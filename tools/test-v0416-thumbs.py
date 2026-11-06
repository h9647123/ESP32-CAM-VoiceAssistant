from host_toolchain import host_compiler, arduinojson_include
from pathlib import Path
import os,subprocess,sys
from source_blocks import get
root=Path(__file__).resolve().parents[1];out=Path(os.environ.get('CAM_TEST_OUTPUT',str(root/'validation/20261102-v0416')));out.mkdir(parents=True,exist_ok=True)
source=(root/'VoiceAssistant/VoiceAssistant.ino').read_text(encoding='utf-8')
(out/'thumb_lines_under_test.h').write_text(get(source,'appendChatMessageLines'),encoding='utf-8')
host=root/'.build-host';host.mkdir(exist_ok=True);env=os.environ.copy()
for key,folder in [('TEMP','tmp'),('TMP','tmp'),('ZIG_GLOBAL_CACHE_DIR','global-cache'),('ZIG_LOCAL_CACHE_DIR','local-cache')]:(host/folder).mkdir(parents=True,exist_ok=True);env[key]=str(host/folder)
exe=out/'thumb-tests.exe'
cmd=[*host_compiler(),'-std=c++17','-I',str(root/'tools/host-tests'),'-I',str(root/'VoiceAssistant'),'-I',str(out),str(root/'tools/host-tests/test_cam_thumbs.cpp'),'-o',str(exe)]
p=subprocess.run(cmd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/'native-build.log').write_text(p.stdout+p.stderr,encoding='utf-8')
if p.returncode:print(p.stdout+p.stderr);sys.exit(p.returncode)
p=subprocess.run([str(exe)],env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/'native-tests.log').write_text(p.stdout+p.stderr,encoding='utf-8');print(p.stdout+p.stderr);sys.exit(p.returncode)
