"""Strict CamProbe capture/replay. Offline replay never accesses serial hardware.

Exit: 0 all gates pass; 1 protocol/capture; 2 length/CRC/format/dimensions;
3 JPEG decode; 4 decoder unavailable/exception. Report retains all categories.
Output must be empty. No flashing, NVS access, or image re-encoding.
CRC proves transport only from the returned driver buffer onward.
"""
import argparse
import base64
import json
from pathlib import Path
import re
import shutil
import subprocess
import time
import zlib
from PIL import Image

COMMANDS = ('crc-p20', 'crc-p10', 'crc-d20', 'crc-d10', 'crc-scene-p20', 'crc-bars-p20')
MAX_BYTES = 1024 * 1024
EXPECTED_IDS = {0, 1, 2}


def failure(report, category, message):
    report['errors'].append({'category': category, 'message': message})


def new_report(command):
    return {'schema_version': 2, 'command': command, 'done': False,
            'errors': [], 'frames': [], 'expected_frames': 3,
            'expected_width': 320, 'expected_height': 240, 'expected_format': 'JPEG',
            'crc_selftest_host': f'{zlib.crc32(b"123456789"):08x}'}


def decode_frame(frame, target, report, ffmpeg='ffmpeg'):
    frame['pil_load'] = frame['strict_decode_pass'] = False
    try:
        with Image.open(target) as image:
            frame['decoded_format'] = image.format
            frame['decoded_width'], frame['decoded_height'] = image.size
            frame['format_match'] = image.format == 'JPEG'
            frame['dimensions_match'] = image.size == (320, 240) == (frame.get('width'), frame.get('height'))
            if not frame['format_match'] or not frame['dimensions_match']:
                failure(report, 'integrity', f"Frame {frame.get('frame')}: format/dimensions mismatch")
            image.load()
            frame['pil_load'] = True
    except Exception as exc:
        frame['pil_error'] = f'{type(exc).__name__}: {exc}'
        failure(report, 'decode', f"Frame {frame.get('frame')}: Pillow load failed")
    try:
        result = subprocess.run([ffmpeg, '-hide_banner', '-v', 'error', '-err_detect', 'explode',
            '-i', str(target), '-f', 'null', '-'], capture_output=True, timeout=15)
        frame['ffmpeg_returncode'] = result.returncode
        diagnostic = result.stderr.decode('utf-8', 'replace').strip()
        target.with_suffix('.decode.log').write_text(diagnostic + '\n', encoding='utf-8')
        frame['strict_decode_pass'] = (result.returncode == 0 and not diagnostic and frame['pil_load']
            and frame.get('format_match', False) and frame.get('dimensions_match', False))
        if not frame['strict_decode_pass']:
            failure(report, 'decode', f"Frame {frame.get('frame')}: strict JPEG decode failed")
    except (OSError, subprocess.TimeoutExpired) as exc:
        frame['decode_tool_error'] = f'{type(exc).__name__}: {exc}'
        target.with_suffix('.decode.log').write_text(frame['decode_tool_error'] + '\n', encoding='utf-8')
        failure(report, 'decoder_tool', f"Frame {frame.get('frame')}: {frame['decode_tool_error']}")


def accept(report, output, ffmpeg='ffmpeg'):
    ids = [f.get('frame') for f in report['frames']]
    if not report['done'] or len(ids) != 3 or set(ids) != EXPECTED_IDS:
        failure(report, 'protocol', 'Expected DONE and exactly unique frame IDs 0,1,2')
    for frame in report['frames']:
        target = Path(frame['file'])
        try:
            raw = target.read_bytes()
        except OSError as exc:
            failure(report, 'protocol', f"Frame {frame.get('frame')}: raw file unavailable: {exc}")
            continue
        frame['received_bytes'] = len(raw)
        frame['length_match'] = 4 <= len(raw) <= MAX_BYTES and len(raw) == frame.get('bytes')
        frame['host'] = zlib.crc32(raw)
        crcs = [frame.get(key) for key in ('source', 'copy', 'returned', 'host')]
        frame['crc_match'] = frame['length_match'] and all(type(x) is int for x in crcs) and len(set(crcs)) == 1
        if not frame['length_match'] or not frame['crc_match']:
            failure(report, 'integrity', f"Frame {frame.get('frame')}: length/four-way CRC mismatch")
        if frame.get('format', 'JPEG') != 'JPEG':
            failure(report, 'integrity', f"Frame {frame.get('frame')}: driver format is not JPEG")
        decode_frame(frame, target, report, ffmpeg)
    complete = report['done'] and len(ids) == 3 and set(ids) == EXPECTED_IDS
    report['three_frames_crc_match'] = complete and all(f.get('crc_match', False) for f in report['frames'])
    report['three_frames_decode_pass'] = complete and all(f.get('strict_decode_pass', False) for f in report['frames'])
    report['failure_categories'] = sorted({e['category'] for e in report['errors']})
    report['acceptance_pass'] = not report['errors'] and report['three_frames_crc_match'] and report['three_frames_decode_pass']
    # Priority for mixed failures: protocol, integrity, tool availability, decode.
    codes = {'protocol': 1, 'integrity': 2, 'decoder_tool': 4, 'decode': 3}
    report['exit_code'] = next((codes[c] for c in codes if c in report['failure_categories']), 0)
    if not report['acceptance_pass'] and report['exit_code'] == 0:
        report['exit_code'] = 1
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report['exit_code']


class ProtocolError(ValueError):
    pass


class CaptureParser:
    """Pure line parser for both live capture and offline protocol tests."""
    def __init__(self, output, report):
        self.output, self.report = output, report
        self.current, self.data = None, None
        self.seen = set()

    def feed(self, line):
        line = line.strip()
        if not line:
            return
        if self.report['done']:
            raise ProtocolError('Data after DONE')
        if self.data is not None and not line.startswith(b'['):
            try:
                self.data.extend(base64.b64decode(line, validate=True))
            except ValueError as exc:
                raise ProtocolError('Invalid base64 payload') from exc
            if len(self.data) > self.current['bytes']:
                raise ProtocolError('Export exceeded announced size')
            return
        decoded = line.decode('ascii', 'replace')
        if line.startswith(b'[PROBE-CRC]'):
            if self.current is not None:
                raise ProtocolError('Overlapping frame metadata')
            fields = dict(re.findall(r'(\w+)=([\w-]+)', decoded))
            try:
                current = {k: int(fields[k], 16 if k in ('source', 'copy', 'returned') else 10)
                    for k in ('frame', 'bytes', 'width', 'height', 'source', 'copy', 'returned')}
            except (KeyError, ValueError) as exc:
                raise ProtocolError('Invalid/incomplete CRC metadata') from exc
            if current['frame'] in self.seen or current['frame'] not in EXPECTED_IDS:
                raise ProtocolError('Duplicate or unexpected frame ID')
            if not 4 <= current['bytes'] <= MAX_BYTES:
                raise ProtocolError('Invalid announced byte count')
            current['format'] = fields.get('format', 'UNREPORTED')
            for k in ('fb_external', 'copy_external'):
                if k in fields:
                    current[k] = int(fields[k])
            self.current = current
        elif line.startswith(b'[CAM-EXPORT-BEGIN]'):
            match = re.fullmatch(rb'\[CAM-EXPORT-BEGIN\] bytes=(\d+) width=(\d+) height=(\d+)', line)
            if self.current is None or self.data is not None or not match:
                raise ProtocolError('Invalid export header')
            if tuple(map(int, match.groups())) != tuple(self.current[k] for k in ('bytes', 'width', 'height')):
                raise ProtocolError('Export header and CRC metadata differ')
            self.data = bytearray()
        elif line == b'[CAM-EXPORT-END]':
            if self.data is None or self.current is None:
                raise ProtocolError('Export end without frame')
            target = self.output / f"frame-{self.current['frame']}.jpg"
            if target.exists():
                raise ProtocolError('Refusing to overwrite raw frame')
            target.write_bytes(self.data)
            self.current['file'] = str(target)
            self.report['frames'].append(self.current)
            self.seen.add(self.current['frame'])
            self.current, self.data = None, None
        elif line.startswith(b'[CAM-EXPORT-ERROR]'):
            failure(self.report, 'protocol', decoded)
        elif line == b'[PROBE] DONE':
            if self.current is not None or self.data is not None:
                raise ProtocolError('DONE during incomplete export')
            self.report['done'] = True
        elif line.startswith(b'[PROBE] deinit='):
            match = re.fullmatch(rb'\[PROBE\] deinit=0x([0-9a-fA-F]+)', line)
            if not match or int(match[1], 16) != 0:
                failure(self.report, 'protocol', decoded)
        elif self.data is not None:
            raise ProtocolError('Unexpected diagnostic during export')

    def preserve_partial(self):
        if self.current is not None:
            partial = self.output / f"frame-{self.current['frame']}.partial.bin"
            partial.write_bytes(self.data or b'')
            self.report['partial_frame'] = {**self.current, 'partial_file': str(partial),
                'received_bytes': len(self.data or b'')}


def replay(source, output):
    saved = json.loads(source.read_text(encoding='utf-8'))
    report = new_report(saved.get('command', 'offline'))
    report.update(offline=True, source_report=str(source.resolve()), done=saved.get('done') is True)
    for error in saved.get('errors', []):
        failure(report, error.get('category', 'protocol') if isinstance(error, dict) else 'protocol',
            error.get('message', str(error)) if isinstance(error, dict) else str(error))
    for number, metadata in enumerate(saved.get('frames', [])):
        frame = {k: metadata[k] for k in ('frame', 'bytes', 'width', 'height', 'source',
            'copy', 'returned', 'format', 'fb_external', 'copy_external') if k in metadata}
        # Legacy reports omit format. Validate the file's actual JPEG format,
        # recording the missing driver observation rather than inventing it.
        frame['driver_format_reported'] = 'format' in metadata
        original = source.parent / f"frame-{metadata.get('frame')}.jpg"
        if not original.is_file():
            failure(report, 'protocol', f'Missing original raw frame: {original.name}')
            continue
        target = output / f'replay-{number}.jpg'
        shutil.copyfile(original, target)
        frame['file'] = str(target)
        report['frames'].append(frame)
    return report


def capture(port_name, command, output):
    import serial  # Offline replay does not import/open the serial backend.
    report = new_report(command)
    parser = CaptureParser(output, report)
    port = serial.Serial()
    port.port, port.baudrate, port.timeout = port_name, 115200, .3
    port.dtr = port.rts = False
    try:
        with port, (output / 'runtime.log').open('w', encoding='utf-8') as log, (output / 'wire.log').open('wb') as wire:
            time.sleep(1)
            drain_until = time.monotonic() + 8
            while time.monotonic() < drain_until:
                stale = port.readline()
                if not stale or stale.strip() == b'[PROBE] DONE':
                    break
            port.reset_input_buffer()
            port.write((command + '\n').encode('ascii'))
            port.flush()
            deadline = time.monotonic() + 90
            while time.monotonic() < deadline:
                received = port.readline()
                wire.write(received)
                wire.flush()
                line = received.strip()
                if not line:
                    continue
                if parser.data is None or line.startswith(b'['):
                    log.write(line.decode('ascii', 'replace') + '\n')
                    log.flush()
                parser.feed(line)
                if report['done']:
                    break
            if not report['done']:
                failure(report, 'protocol', 'Timed out before DONE')
    except (OSError, ValueError, serial.SerialException) as exc:
        failure(report, 'protocol', f'{type(exc).__name__}: {exc}')
    finally:
        parser.preserve_partial()
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--port', help='Use only with an authorized CamProbe device')
    mode.add_argument('--offline-report', type=Path, help='Re-evaluate adjacent raw files without serial access')
    parser.add_argument('--command', choices=COMMANDS)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--ffmpeg', default='ffmpeg')
    args = parser.parse_args(argv)
    if args.port and not args.command:
        parser.error('--command required for serial capture')
    if args.offline_report and args.command:
        parser.error('--command not used for offline replay')
    if args.output.exists() and any(args.output.iterdir()):
        parser.error('--output must be empty; existing evidence is never overwritten')
    args.output.mkdir(parents=True, exist_ok=True)
    try:
        report = replay(args.offline_report, args.output) if args.offline_report else capture(args.port, args.command, args.output)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        report = new_report(args.command or 'offline')
        failure(report, 'protocol', f'{type(exc).__name__}: {exc}')
    code = accept(report, args.output, args.ffmpeg)
    print(json.dumps({'acceptance_pass': report['acceptance_pass'], 'exit_code': code,
        'failure_categories': report['failure_categories'], 'frames': len(report['frames'])}))
    return code


if __name__ == '__main__':
    raise SystemExit(main())
