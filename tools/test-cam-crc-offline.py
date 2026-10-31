"""Real decoder and protocol regression tests, never serial/hardware access."""
import argparse
import base64
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import zlib
from unittest.mock import patch
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    root = Path(__file__).resolve().parents[1]
    script = root / 'tools/capture-cam-crc.py'
    spec = importlib.util.spec_from_file_location('cam_crc', script)
    tool = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(tool)
    results = []
    def check(name, condition, **details):
        results.append({'name': name, 'passed': bool(condition), **details})
        if not condition:
            raise AssertionError(name)

    def cli(name, fixture, expected, extra=()):
        target = out / name
        proc = subprocess.run([sys.executable, str(script), '--offline-report', str(fixture),
            '--output', str(target), *extra], capture_output=True, text=True)
        (out / (name + '.stdout.log')).write_text(proc.stdout + proc.stderr, encoding='utf-8')
        report = json.loads((target / 'report.json').read_text())
        check(name, proc.returncode == expected and report['exit_code'] == expected
            and report['acceptance_pass'] == (expected == 0), exit_code=proc.returncode,
            categories=report['failure_categories'])
        return report

    old = root / 'validation/20261024-crc'
    historical = [p for group in ('psram20-clean', 'dram20', 'psram10', 'dram10')
                  for p in sorted((old / group).glob('frame-*.jpg'))]
    before = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in historical}
    for group in ('psram20-clean', 'dram20', 'psram10', 'dram10'):
        if not (old / group / 'report.json').is_file():
            continue  # Private historical captures are intentionally not published.
        report = cli('bad-' + group, old / group / 'report.json', 3)
        check('bad-' + group + '-all-three-rejected', len(report['frames']) == 3
            and all(not f['strict_decode_pass'] and f['crc_match'] for f in report['frames']))

    fixture = out / 'generated-good'
    fixture.mkdir()
    saved = tool.new_report('offline-known-good')
    saved['done'] = True
    for i in range(3):
        image = Image.new('RGB', (320, 240))
        image.putdata([((x + i * 20) % 256, y % 256, (x ^ y) % 256)
                       for y in range(240) for x in range(320)])
        target = fixture / f'frame-{i}.jpg'
        image.save(target, 'JPEG', quality=90)
        raw = target.read_bytes()
        crc = zlib.crc32(raw)
        saved['frames'].append({'frame': i, 'bytes': len(raw), 'width': 320, 'height': 240,
            'format': 'JPEG', 'source': crc, 'copy': crc, 'returned': crc})
    source = fixture / 'report.json'
    source.write_text(json.dumps(saved), encoding='utf-8')
    cli('known-good', source, 0)
    cli('decoder-missing', source, 4, ('--ffmpeg', str(out / 'missing-ffmpeg.exe')))

    mutations = [
        ('missing-done', lambda s: s.update(done=False), 1),
        ('missing-frame', lambda s: s['frames'].pop(), 1),
        ('duplicate-frame', lambda s: s['frames'].__setitem__(2, copy.deepcopy(s['frames'][0])), 1),
        ('wrong-id', lambda s: s['frames'][2].update(frame=9), 1),
        ('length-mismatch', lambda s: s['frames'][0].update(bytes=10), 2),
        ('crc-mismatch', lambda s: s['frames'][0].update(copy=0), 2),
        ('dimensions-mismatch', lambda s: s['frames'][0].update(width=160), 2),
        ('driver-format-mismatch', lambda s: s['frames'][0].update(format='RGB565'), 2),
        ('transport-error', lambda s: s['errors'].append('Export interrupted'), 1),
    ]
    for name, mutate, expected in mutations:
        changed = copy.deepcopy(saved)
        mutate(changed)
        source.write_text(json.dumps(changed), encoding='utf-8')
        cli(name, source, expected)
    source.write_text(json.dumps(saved), encoding='utf-8')
    original_bytes = (fixture / 'frame-0.jpg').read_bytes()
    Image.new('RGB', (320, 240), 'red').save(fixture / 'frame-0.jpg', 'PNG')
    raw = (fixture / 'frame-0.jpg').read_bytes()
    changed = copy.deepcopy(saved)
    changed['frames'][0].update(bytes=len(raw), **{k: zlib.crc32(raw) for k in ('source', 'copy', 'returned')})
    source.write_text(json.dumps(changed), encoding='utf-8')
    cli('actual-file-not-jpeg', source, 2)
    (fixture / 'frame-0.jpg').write_bytes(original_bytes)
    source.write_text(json.dumps(saved), encoding='utf-8')

    # Feed the same real, independently-generated JPEG through the wire parser.
    raw = original_bytes
    crc = zlib.crc32(raw)
    def header(index=0):
        return f'[PROBE-CRC] frame={index} bytes={len(raw)} width=320 height=240 format=JPEG source={crc:08x} copy={crc:08x} returned={crc:08x}'.encode()
    begin = f'[CAM-EXPORT-BEGIN] bytes={len(raw)} width=320 height=240'.encode()
    wire_dir = out / 'wire-good'; wire_dir.mkdir()
    wire = tool.new_report('offline-wire')
    parsed = tool.CaptureParser(wire_dir, wire)
    for i in range(3):
        for line in (header(i), begin, base64.b64encode(raw), b'[CAM-EXPORT-END]'):
            parsed.feed(line)
    parsed.feed(b'[PROBE] DONE')
    check('wire-real-jpeg-pass', tool.accept(wire, wire_dir) == 0)
    for name, lines in (
        ('bad-base64', [header(), begin, b'%%%']),
        ('overlapping-metadata', [header(), header()]),
        ('header-disagreement', [header(), b'[CAM-EXPORT-BEGIN] bytes=4 width=320 height=240']),
        ('end-without-frame', [b'[CAM-EXPORT-END]']),
        ('incomplete-done', [header(), begin, b'[PROBE] DONE']),
        ('payload-overrun', [header(), begin, base64.b64encode(raw + b'overrun')]),
        ('duplicate-wire-id', [header(), begin, base64.b64encode(raw), b'[CAM-EXPORT-END]', header()]),
    ):
        case_dir = out / name; case_dir.mkdir()
        state = tool.new_report('offline-wire')
        obj = tool.CaptureParser(case_dir, state)
        caught = False
        try:
            for line in lines:
                obj.feed(line)
        except tool.ProtocolError:
            caught = True
        obj.preserve_partial()
        check(name, caught)
        if name == 'payload-overrun':
            check('partial-raw-preserved', (case_dir / 'frame-0.partial.bin').read_bytes() == raw + b'overrun')
    # Exercise exceptional decoder execution without invoking hardware.
    state = tool.new_report('offline-timeout')
    timeout_dir = out / 'decoder-timeout'; timeout_dir.mkdir()
    with patch.object(tool.subprocess, 'run', side_effect=subprocess.TimeoutExpired('ffmpeg', 15)):
        tool.decode_frame(saved['frames'][0].copy(), fixture / 'frame-0.jpg', state)
    check('decoder-timeout-classified', any(e['category'] == 'decoder_tool' for e in state['errors']))
    for label, line, expected_errors in (
        ('deinit-zero', b'[PROBE] deinit=0x0', 0),
        ('deinit-padded-zero', b'[PROBE] deinit=0x0000', 0),
        ('deinit-failed', b'[PROBE] deinit=0x105', 1),
        ('deinit-malformed', b'[PROBE] deinit=invalid', 1),
    ):
        state = tool.new_report('offline-deinit')
        tool.CaptureParser(out, state).feed(line)
        check(label, len(state['errors']) == expected_errors)
    # Assert offline CLI cannot accidentally fall through to serial access.
    with patch.object(tool, 'capture', side_effect=AssertionError('Hardware access forbidden')):
        check('offline-no-capture-call', tool.main(['--offline-report', str(source),
            '--output', str(out / 'offline-no-hardware')]) == 0)
    check('historical-originals-unchanged', before == {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in historical})
    summary = {'passed': all(x['passed'] for x in results), 'checks': results,
               'count': len(results), 'historical_bad_frames': len(historical),
               'hardware_accessed': False}
    (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(f"PASS: {len(results)} offline checks; {len(historical)} available historical bad JPEGs checked; generated JPEGs accepted")


if __name__ == '__main__':
    main()
