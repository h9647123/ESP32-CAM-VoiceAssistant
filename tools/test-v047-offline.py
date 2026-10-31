from host_toolchain import host_compiler, arduinojson_include
"""Compile real portable CAM headers on the host; never accesses a serial port."""
from pathlib import Path
import argparse
import base64
import json
import os
import subprocess

root = Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--output',type=Path,default=root/'validation/20261031-v047')
out=parser.parse_args().output.resolve()
out.mkdir(parents=True, exist_ok=True)
host = root / '.build-host'
compiler = host_compiler()
env = os.environ.copy()
for key, folder in [('TEMP', 'tmp'), ('TMP', 'tmp'),
                    ('ZIG_GLOBAL_CACHE_DIR', 'global-cache'),
                    ('ZIG_LOCAL_CACHE_DIR', 'local-cache')]:
    path = host / folder
    path.mkdir(parents=True, exist_ok=True)
    env[key] = str(path)
tests = root / 'tools/host-tests'
exe = out / 'host-tests.exe'
# Compile the actual queue helpers as written in cam_capture.h, excluding only
# ESP32 peripherals. Allocation tracking detects ownership errors/double frees.
capture = (root/'VoiceAssistant/cam_capture.h').read_text(encoding='utf-8')
photo = 'struct CamPhoto {' + capture.split('struct CamPhoto {', 1)[1].split('static CamPhoto camPhoto;', 1)[0]
queue_globals = 'static CamPhoto camPhoto;' + capture.split('static CamPhoto camPhoto;', 1)[1].split('static bool camVisionEnabled', 1)[0]
helpers = 'static size_t camQuestionPhotoCount()' + capture.split('static size_t camQuestionPhotoCount()', 1)[1].split('static void camPrintMemory', 1)[0]
prefix = '''#include <set>
#include <cstdlib>
static std::set<void*> liveAllocations;
static void* queueAlloc(){void* p=malloc(1);assert(p);assert(liveAllocations.insert(p).second);return p;}
static void heap_caps_free(void* p){assert(liveAllocations.erase(p)==1);free(p);}
static uint32_t camPhotoGeneration=0;
static bool camQuestionRequiresPhoto=false,camSdReady=false;
static fs::FS SD_MMC;
'''
(out/'cam_queue_under_test.h').write_text(prefix+photo+queue_globals+helpers, encoding='utf-8')
main = (root/'VoiceAssistant/VoiceAssistant.ino').read_text(encoding='utf-8')
record_route = main.split('static void startRecording() {',1)[1].split('  if(usesIatV2()) {',1)[0]
ask_route = main.split('static bool askAndStoreChat(String question, bool forceSearch) {',1)[1].split('  transientSearchResult = "";',1)[0]
routes = '''
static bool camPhotoOnly=false,camVisionEnabled=false,composingQuestion=false,asrAppendMode=false;
static bool llmTestIsolation=false,lastFailedUseSearch=false;
static bool hasHistoricalPhotos=false;
static bool camHasImageContext(){return hasHistoricalPhotos;}
static String lastFailedQuestion,lastError;
static int modePath=0,visionCalls=0;
enum class AppState { Error };
static void setState(AppState,const String&){}
static void camOpenPreview(){modePath=1;}
static void camOpenDecision(){modePath=2;}
static bool camAskAndStoreVision(const String&){++visionCalls;return true;}
'''
routes += 'static void probeRecordingRoute(){'+record_route+'modePath=3;}\n'
routes += 'static bool probeAskRoute(String question){'+ask_route+'return false;}\n'
(out/'cam_mode_routes_under_test.h').write_text(routes, encoding='utf-8')
subprocess.run([*compiler, '-std=c++17', '-I', str(tests), '-I', str(out), '-I', str(root/'VoiceAssistant'),
                str(tests/'test_cam_offline.cpp'), '-o', str(exe)], env=env, check=True)
vectors = out / 'stream-vectors.txt'
run = subprocess.run([str(exe), str(vectors)], capture_output=True, text=True, check=True)
(out/'native-tests.log').write_text(run.stdout + run.stderr, encoding='utf-8')
lines = vectors.read_text().splitlines()
assert len(lines) == 21
for line in lines:
    n, count, payload = line.split(' ', 2)
    n, count = int(n), int(count)
    values = json.loads(payload)
    assert len(values) == count
    for item in values:
        assert item.startswith('data:image/jpeg;base64,')
        assert base64.b64decode(item.split(',', 1)[1], validate=True) == bytes((i*17+3)%256 for i in range(n))
context = main.split('struct CamChatImage {', 1)[1].split('static size_t chatHistoryBytes', 1)[0]
(out/'cam_context_under_test.h').write_text('static constexpr size_t CAM_JPEG_MAX_BYTES=1024*1024;\nstruct CamChatImage {'+context, encoding='utf-8')
vision = (root/'VoiceAssistant/cam_vision.h').read_text(encoding='utf-8')
parts = vision.split('static bool camBuildVisionParts(', 1)[1].split('static bool camVisionOnce(', 1)[0]
(out/'cam_vision_parts_under_test.h').write_text('static bool camBuildVisionParts('+parts, encoding='utf-8')
think = main.split('static size_t thinkTagSuffixLength(', 1)[1].split('static bool streamChunkSuggestsBreak(', 1)[0]
(out/'cam_think_helpers_under_test.h').write_text('static size_t thinkTagSuffixLength('+think, encoding='utf-8')
sink = main.split('class LlmSseSink : public Stream {', 1)[1].split('// Deterministic replay exercises', 1)[0]
(out/'cam_sse_under_test.h').write_text('class LlmSseSink : public Stream {'+sink, encoding='utf-8')
json_include = arduinojson_include()
context_exe = out/'context-stream-tests.exe'
subprocess.run([*compiler, '-std=c++17', '-I', str(tests), '-I', str(out), '-I', str(root/'VoiceAssistant'),
                '-I', str(json_include), str(tests/'test_cam_context_stream.cpp'), '-o', str(context_exe)], env=env, check=True)
context_vectors = out/'context-vectors.txt'
context_run = subprocess.run([str(context_exe), str(context_vectors)], capture_output=True, text=True, check=True)
(out/'context-native-tests.log').write_text(context_run.stdout + context_run.stderr, encoding='utf-8')
context_lines = context_vectors.read_text(encoding='utf-8').splitlines()
assert len(context_lines) == 6
for line in context_lines:
    groups, streamed, payload = line.split(' ', 2)
    request = json.loads(payload)
    assert request['stream'] == bool(int(streamed))
    expected_groups = range(max(0, int(groups)-2), int(groups))
    expected = [bytes([g, j, 255]) for g in expected_groups for j in range(3)]
    actual = []
    for message in request['messages']:
        assert '_cam_history_index' not in message
        if isinstance(message['content'], list):
            for item in message['content'][1:]:
                actual.append(base64.b64decode(item['image_url']['url'].split(',',1)[1], validate=True))
    assert actual == expected
report = dict(hardware_tested=False, flashed=False, native_delete_state_pass=True, native_dns_packet_validation_pass=True,
              native_stream_pass=True, native_photo_queue_ownership_pass=True,
              photo_only_album_question_routes_pass=True, latex_display_samples_pass=7,
              independent_base64_json_vectors=len(lines), max_simultaneously_open_files=1,
              context_round_trip_and_message_image_order_pass=True, context_json_vectors=len(context_lines),
              reasoning_parser_stress_input_bytes=1500*1024, live_layout_input_max_bytes=771,
              inline_think_parser_stress_input_bytes=1000*1024,
              reasoning_retained_max_bytes=8192, visible_reply_limit_bytes=49152,
              real_device_ui_timing_tested=False)
(out/'offline-tests.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
print(json.dumps(report, indent=2))
