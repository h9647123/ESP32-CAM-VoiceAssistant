from host_toolchain import host_compiler, arduinojson_include
from pathlib import Path
import os,subprocess,sys
from source_blocks import get
root=Path(__file__).resolve().parents[1];out=root/'validation/20261107-v0427/history';out.mkdir(parents=True,exist_ok=True)
s=(root/'VoiceAssistant/VoiceAssistant.ino').read_text(encoding='utf-8')
names=['trimChatHistoryToLimit','loadChatHistory','saveChatHistory','addChatMessage','camWebMessageContext','buildLlmRequestBodyBudget']
(out/'web_history_under_test.h').write_text('\n'.join(get(s,n) for n in names),encoding='utf-8')
f=(root/'tools/host-tests/test_cam_web_history.cpp').read_text(encoding='utf-8').replace('#include "web_history_under_test.h"','static String stripDisplayMarkers(const String& text){return text;}\n#include "web_history_under_test.h"')
(out/'test.cpp').write_text(f,encoding='utf-8');host=root/'.build-host';host.mkdir(exist_ok=True);env=os.environ.copy()
for k,v in [('TEMP','tmp'),('TMP','tmp'),('ZIG_GLOBAL_CACHE_DIR','global-cache'),('ZIG_LOCAL_CACHE_DIR','local-cache')]:(host/v).mkdir(parents=True,exist_ok=True);env[k]=str(host/v)
cmd=[*host_compiler(),'-std=c++17','-I',str(root/'tools/host-tests'),'-I',str(root/'VoiceAssistant'),'-I',str(out),'-I',arduinojson_include(),str(out/'test.cpp'),'-o',str(out/'test.exe')]
p=subprocess.run(cmd,env=env,capture_output=True,text=True);(out/'compile.log').write_text(p.stdout+p.stderr,encoding='utf-8');print(p.stdout+p.stderr);p.check_returncode()
p=subprocess.run([str(out/'test.exe')],capture_output=True,text=True);(out/'tests.log').write_text(p.stdout+p.stderr,encoding='utf-8');print(p.stdout+p.stderr);
p.check_returncode()
