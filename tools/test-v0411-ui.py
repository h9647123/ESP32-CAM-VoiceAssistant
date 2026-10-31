from host_toolchain import host_compiler, arduinojson_include
"""Exercise actual fade functions on the host and check menu/sleep routes."""
from pathlib import Path
import json
import os
import subprocess

root = Path(__file__).resolve().parents[1]
out = root/'validation/20261031-v0411'
out.mkdir(parents=True, exist_ok=True)
source = (root/'VoiceAssistant/VoiceAssistant.ino').read_text(encoding='utf-8')
fade = 'static void animatePageTransition(bool forward) {' + source.split(
    'static void animatePageTransition(bool forward) {', 1)[1].split(
    'static void flashClearChatAccepted()', 1)[0]
prefix = '''#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>
using std::max;
static bool screenSleeping=false,recording=false,transitionDimmed=false;
static int screenBrightness=100,pageTransitionCount=0;
static unsigned elapsed=0;
static void delay(unsigned ms){elapsed+=ms;}
static int map(int x,int a,int b,int c,int d){return (x-a)*(d-c)/(b-a)+c;}
struct Display{std::vector<int> values;void setBrightness(int value){values.push_back(value);}};
static Display tft;
'''
checks = '''
int main(){
 for(int brightness: {1,5,20,50,100}) {
  screenBrightness=brightness;tft.values.clear();elapsed=0;
  const int expected=map(brightness,1,100,8,255);
  animatePageTransition(true);assert(transitionDimmed);assert(tft.values.size()==4);
  int prior=expected;for(int value:tft.values){assert(value<=prior && value>=1);prior=value;}
  animatePageTransition(false);assert(tft.values.size()==4);
  finishPageTransition();assert(!transitionDimmed);assert(tft.values.size()==8);
  for(unsigned i=4;i<8;++i)assert(tft.values[i]>=tft.values[i-1]);
  assert(tft.values.back()==expected);assert(elapsed==84);
  finishPageTransition();assert(tft.values.size()==8);
 }
 for(bool sleeping: {false,true}) {
  screenSleeping=sleeping;recording=!sleeping;tft.values.clear();
  animatePageTransition(true);assert(tft.values.empty()&&!transitionDimmed);
 }
}
'''
# Execute the new camera B/C adjustment branches and the actual A edit route.
adjust = source.split('static void changeMenuValue(int delta, bool fast) {',1)[1]
adjust = '  if (item == MenuVideoSize) {' + adjust.split('  if (item == MenuVideoSize) {',1)[1].split(
    '  } else if (item == MenuBrightness) {',1)[0] + '  }\n'
route = source.split('static void handleButtonA(bool click, bool longPress, bool repeat, bool released) {',1)[1]
route = '  if (item == MenuPhotoOnly ||' + route.split('  if (item == MenuPhotoOnly ||',1)[1].split(
    '  if (item == MenuProvider) {',1)[0]
prefix += '''
enum MenuItem{MenuPhotoOnly,MenuVideoSize,MenuCamera,MenuQuestionMode,MenuEnableThinking,
 MenuThinkingDisplay,MenuAsrProvider,MenuAsr,MenuSearch};
static bool camPhotoOnly=false,camFullPreview=false,camVisionEnabled=false,menuEditing=false;
static uint8_t camVideoResolution=1;
static int cameraSaves=0,runtimeSaves=0,cancels=0;
static int camQuestionPhotoCount(){return 0;}
static void camCancelDraft(){++cancels;}
static void camSaveSettings(){++cameraSaves;}
static void saveRuntimeConfig(){++runtimeSaves;}
static void drawPage2(){}
'''
prefix += 'static void adjust(MenuItem item,int delta){'+adjust+'}\n'
prefix += 'static void select(MenuItem item){'+route+'}\n'
checks = checks.replace('int main(){','''int main(){
 for(MenuItem item: {MenuPhotoOnly,MenuVideoSize,MenuCamera,MenuQuestionMode,MenuEnableThinking,
     MenuThinkingDisplay,MenuAsrProvider,MenuAsr,MenuSearch}) {
  int saved=cameraSaves+runtimeSaves;
  select(item);assert(menuEditing);assert(cameraSaves+runtimeSaves==saved);
  select(item);assert(!menuEditing);assert(cameraSaves+runtimeSaves==saved+1);
 }
 adjust(MenuVideoSize,-1);assert(camVideoResolution==0);
 adjust(MenuVideoSize,-1);assert(camVideoResolution==2);
 adjust(MenuVideoSize,1);assert(camVideoResolution==0);
 adjust(MenuCamera,1);assert(camFullPreview);adjust(MenuCamera,-1);assert(!camFullPreview);
 adjust(MenuPhotoOnly,1);assert(camPhotoOnly&&cancels==1);
 adjust(MenuPhotoOnly,-1);assert(!camPhotoOnly);
 adjust(MenuQuestionMode,1);assert(camVisionEnabled&&cancels==2);
''')
test = out/'menu-transition-test.cpp'
test.write_text(prefix+fade+checks, encoding='utf-8')
host = root/'.build-host'
env = os.environ.copy()
for key, folder in [('TEMP','tmp'),('TMP','tmp'),('ZIG_GLOBAL_CACHE_DIR','global-cache'),('ZIG_LOCAL_CACHE_DIR','local-cache')]:
    (host/folder).mkdir(parents=True,exist_ok=True); env[key] = str(host/folder)
exe = out/'menu-transition-test.exe'
subprocess.run([*host_compiler(),'-std=c++17',str(test),'-o',str(exe)], env=env, check=True)
subprocess.run([str(exe)],check=True)
assert 'static constexpr uint8_t SLEEP_MINUTES_MAX = 30;' in source
assert source.count('sleepMinutes = constrain(sleepMinutes, 0, SLEEP_MINUTES_MAX);') == 2
assert 'sleepMinutes = clampInt(int(sleepMinutes) + delta, 0, SLEEP_MINUTES_MAX);' in source
assert 'constrain(debugWeb.arg("sleep").toInt(), 0, SLEEP_MINUTES_MAX)' in source
assert '(value < 0 || value > SLEEP_MINUTES_MAX)' in source
assert 'runtimePrefs.putUChar("sleep_min", sleepMinutes);' in source
assert 'uint32_t(sleepMinutes) * 60000UL' in source
assert 'menuReturnRoot();\n      animatePageTransition(true);' in source
assert 'if (menuGroup == -1) {\n    animatePageTransition(true);\n    menuGroup=menuIndex;' in source
assert 'if (menuGroup >= 0) { animatePageTransition(false); menuReturnRoot(); drawPage2(); return; }' in source
report = dict(actual_fade_functions_host_test_passed=True,brightness_values_tested=[1,5,20,50,100],fade_total_ms=84,
    actual_a_edit_routes_tested=9,actual_camera_bc_adjustment_branches_passed=True,
    home_to_menu_and_root_to_submenu_share_transition=True,sleep_max_minutes=30,
    sleep_menu_serial_web_import_load_and_save_checked=True,hardware_tested=False,flashed=False)
(out/'ui-tests.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,indent=2))
