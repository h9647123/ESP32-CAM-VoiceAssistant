"""Export an idle CAM home render (or reversible local thumbnail fixture) over UART."""
import argparse,base64,pathlib,re,time,serial
from PIL import Image
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--port',default='COM5')
p.add_argument('--command',default='ui screenshot',help='ui screenshot, ui thumbtest 1-3|missing, or ui mathtest 1-9 [scroll-row]')
p.add_argument('--output',type=pathlib.Path,required=True)
a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True)
if not re.fullmatch(r'(cam view export|ui (screenshot|thumbtest (1|2|3|missing)|mathtest [1-9]( [0-9]{1,4})?))',a.command):p.error('unsupported read-only screenshot command')
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.3;s.dtr=s.rts=False
redact=re.compile(r'api[ _-]?key|password|authorization|bearer|secret|appid|signa=|sk-[A-Za-z0-9]',re.I)
rows=[];log=[];started=False;done=False;end=time.monotonic()+80
with s:
 s.reset_input_buffer();s.write((a.command+'\n').encode());s.flush()
 while time.monotonic()<end:
  line=s.readline().strip()
  if not line:continue
  if line.startswith(b'[CAM-VIEW-BEGIN]'):
   assert b'width=240 height=240 format=RGB888' in line
   started=True;continue
  if line==b'[CAM-VIEW-END]':done=True;break
  if started:
   try:data=base64.b64decode(line,validate=True)
   except Exception:data=b''
   if len(data)==720:rows.append(data);continue
  text=line.decode('utf-8','replace')
  if redact.search(text):text='[credential-bearing line redacted]'
  log.append(text)
 finish=time.monotonic()+2
 while time.monotonic()<finish:
  text=s.readline().decode('utf-8','replace').strip()
  if text and not redact.search(text):log.append(text)
a.output.with_suffix('.log').write_text('\n'.join(log)+'\n',encoding='utf-8')
assert done and len(rows)==240,f'Incomplete frame: done={done} rows={len(rows)}; see diagnostic log'
Image.frombytes('RGB',(240,240),b''.join(rows)).save(a.output)
print(f'Saved {a.output} (240x240, actual device render buffers)')
for text in log:
 if text.startswith(('[CHAT-THUMB','[UI-SCREENSHOT]')):print(text)
