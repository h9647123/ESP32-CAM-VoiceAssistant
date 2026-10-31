"""Convert a CAM MJPEG/PCM AVI to H.264/AAC MP4 without altering the source."""
import argparse,json,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('input',type=Path);p.add_argument('--output',type=Path);a=p.parse_args()
source=a.input.resolve();target=(a.output or source.with_suffix('.mp4')).resolve()
if not source.is_file() or source==target or target.exists():raise SystemExit('Input missing, output equals input, or output already exists.')
ffmpeg=shutil.which('ffmpeg');ffprobe=shutil.which('ffprobe')
if not ffmpeg or not ffprobe:raise SystemExit('Install FFmpeg and add ffmpeg/ffprobe to PATH first.')
subprocess.run([ffmpeg,'-hide_banner','-loglevel','error','-n','-i',str(source),'-map','0:v:0','-map','0:a:0',
    '-c:v','libx264','-preset','veryfast','-crf','23','-pix_fmt','yuv420p','-c:a','aac','-b:a','64k','-movflags','+faststart',str(target)],check=True)
data=json.loads(subprocess.check_output([ffprobe,'-v','error','-show_streams','-show_format','-of','json',str(target)],encoding='utf-8'))
assert any(s['codec_name']=='h264' for s in data['streams']) and any(s['codec_name']=='aac' for s in data['streams'])
print(json.dumps(dict(output=str(target),bytes=target.stat().st_size,duration=data['format'].get('duration'),codecs=[s['codec_name'] for s in data['streams']]),ensure_ascii=False,indent=2))
