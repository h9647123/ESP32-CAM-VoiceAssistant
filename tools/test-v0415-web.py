from host_toolchain import host_compiler, arduinojson_include
"""Compile actual bounded web-research code with scripted HTTP; no device access."""
from pathlib import Path
import os, subprocess, sys, json
from source_blocks import get
root=Path(__file__).resolve().parents[1]
out=Path(os.environ.get('CAM_TEST_OUTPUT',str(root/'validation/20261102-v0415')))
out.mkdir(parents=True,exist_ok=True)
source=(root/'VoiceAssistant/VoiceAssistant.ino').read_text(encoding='utf-8')
names=['utf8CharLen','camSearchProviderName','camSearchKey','camSearchEndpoint','camBoundSearchQuery','camBuildSearchRequest','camBuildDeepseekDigest','buildTavilyDigestFromJson']
(out/'web_helpers_under_test.h').write_text('\n\n'.join(get(source,n) for n in names),encoding='utf-8')
host=root/'.build-host';host.mkdir(exist_ok=True);env=os.environ.copy()
for key,folder in [('TEMP','tmp'),('TMP','tmp'),('ZIG_GLOBAL_CACHE_DIR','global-cache'),('ZIG_LOCAL_CACHE_DIR','local-cache')]:(host/folder).mkdir(parents=True,exist_ok=True);env[key]=str(host/folder)
exe=out/'test_web.exe'
cmd=[*host_compiler(),'-std=c++17','-I',str(root/'tools/host-tests'),'-I',str(root/'VoiceAssistant'),'-I',str(out),'-I',str(arduinojson_include()),str(root/'tools/host-tests/test_cam_web.cpp'),'-o',str(exe)]
p=subprocess.run(cmd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
(out/'native-build.log').write_text(p.stdout+p.stderr,encoding='utf-8')
if p.returncode:print(p.stdout+p.stderr);sys.exit(p.returncode)
p=subprocess.run([str(exe)],env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
(out/'native-tests.log').write_text(p.stdout+p.stderr,encoding='utf-8')
print(p.stdout+p.stderr)
if p.returncode:sys.exit(p.returncode)
names=['trimChatHistoryToLimit','loadChatHistory','saveChatHistory','addChatMessage','camWebMessageContext','buildLlmRequestBodyBudget']
(out/'web_history_under_test.h').write_text('\n\n'.join(get(source,n) for n in names),encoding='utf-8')
history=(root/'tools/host-tests/test_cam_web_history.cpp').read_text(encoding='utf-8')
# The real display filter is irrelevant to this storage test; no display markers in fixtures.
history=history.replace('#include "web_history_under_test.h"','static String stripDisplayMarkers(const String& text){return text;}\n#include "web_history_under_test.h"')
(out/'test_history.cpp').write_text(history,encoding='utf-8')
cmd[-3]=str(out/'test_history.cpp');cmd[-1]=str(out/'test_history.exe')
p=subprocess.run(cmd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
(out/'history-build.log').write_text(p.stdout+p.stderr,encoding='utf-8')
if p.returncode:print(p.stdout+p.stderr);sys.exit(p.returncode)
p=subprocess.run([str(out/'test_history.exe')],env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
(out/'history-tests.log').write_text(p.stdout+p.stderr,encoding='utf-8')
print(p.stdout+p.stderr)
sys.exit(p.returncode)
