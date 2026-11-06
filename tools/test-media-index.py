from host_toolchain import host_compiler, arduinojson_include
"""Exercise the firmware's cache state machine with controlled I/O failures."""
from pathlib import Path
import os,subprocess
from source_blocks import get
root=Path(__file__).resolve().parents[1];out=root/'validation/20261107-v0426/index-tests';out.mkdir(parents=True,exist_ok=True)
s=(root/'VoiceAssistant/cam_album.h').read_text(encoding='utf-8')
a=s.index('struct CamMediaIndex {');b=s.index('static bool camMediaIndexBegin',a)
code=r'''
#include "Arduino.h"
#include <algorithm>
#include <vector>
#include <cassert>
#include <cstdio>
#include <sys/stat.h>
#include "cam_photo_order.h"
struct CamAlbumItem{String path;CamPhotoKey key;};
static bool camSdReady=true,failNames=false,failStat=false,failRaw=false;
static uint32_t camPhotoIndexRevision=1,camVideoIndexRevision=1;static const unsigned CAM_PHOTO_ARCHIVE_LIMIT=200;
static std::vector<String> manifest;static unsigned listings=0,metadata=0;
static unsigned millis(){static unsigned at=0;return ++at;}static void delay(unsigned){}static void yield(){}
struct Console{template<class... A>void printf(const char*,A...){}}Serial;
struct Disk{bool readRAW(uint8_t*,unsigned){return !failRaw;}}SD_MMC;
static bool camScanMediaNames(const char*,const char*,std::vector<String>& names){++listings;if(failNames)return false;names=manifest;return true;}
static int camTestStat(const char*,struct stat* info){if(failStat)return -1;info->st_mode=S_IFREG;info->st_mtime=1;return 0;}
static String hiddenPath;static bool camPhotoHidden(const String& path,bool& ok){ok=true;return path==hiddenPath;}
static CamPhotoKey camReadMediaKey(const String& p,long){++metadata;CamPhotoKey key;key.order=atoi(p.c_str()+7);return key;}
'''+s[a:b]
for name in ['camMediaIndexBegin','camMediaIndexStep','camScanMedia']:
 code+=get(s,name).replace('stat(native.c_str(),&info)','camTestStat(native.c_str(),&info)')+'\n'
code+=r'''
int main(){
 for(unsigned i=1;i<=99;++i)manifest.push_back(String("/photo/")+String(i)+".jpg");
 std::vector<CamAlbumItem> shown;assert(camScanMedia("/photo",".jpg",shown));assert(shown.size()==99&&shown.front().key.order==99);
 const auto reads=metadata,dirs=listings;for(unsigned i=0;i<10;++i)assert(camScanMedia("/photo",".jpg",shown));assert(metadata==reads&&listings==dirs);
 ++camVideoIndexRevision;assert(camScanMedia("/photo",".jpg",shown));assert(metadata==reads&&listings==dirs); // saving a clip cannot invalidate photos
 ++camPhotoIndexRevision;manifest.push_back("/photo/100.jpg");assert(camScanMedia("/photo",".jpg",shown));assert(shown.size()==100&&shown.front().key.order==100);
 ++camPhotoIndexRevision;hiddenPath="/photo/100.jpg";assert(camScanMedia("/photo",".jpg",shown));assert(shown.size()==99&&camPhotoIndexCache.maxOrder==100&&camPhotoIndexCache.hiddenCount==1);
 ++camPhotoIndexRevision;hiddenPath="";assert(camScanMedia("/photo",".jpg",shown));assert(shown.size()==100);
 // A failed refresh must not publish an empty or partial list.
 ++camPhotoIndexRevision;failStat=true;assert(!camScanMedia("/photo",".jpg",shown));assert(shown.size()==100);failStat=false;
 assert(camScanMedia("/photo",".jpg",shown));failRaw=true;assert(!camScanMedia("/photo",".jpg",shown));assert(shown.size()==100);failRaw=false;
 // Removal/reinsertion invalidates cached identities even with equal file counts.
 camSdReady=false;assert(!camScanMedia("/photo",".jpg",shown));camSdReady=true;++camPhotoIndexRevision;manifest={"/photo/500.jpg"};assert(camScanMedia("/photo",".jpg",shown));assert(shown.size()==1&&shown[0].key.order==500);
 ++camPhotoIndexRevision;manifest.clear();assert(camScanMedia("/photo",".jpg",shown));assert(shown.empty());
 puts("PASS: cached re-entry avoids all metadata reads; media revision rebuilds; newest ordering; stat/raw/removal failures retain published list; reinsertion and genuine empty card.");
}
'''
(out/'cache.cpp').write_text(code,encoding='utf-8')
host=root/'.build-host';host.mkdir(exist_ok=True);env=os.environ.copy()
for key,folder in [('TEMP','tmp'),('TMP','tmp'),('ZIG_GLOBAL_CACHE_DIR','global-cache'),('ZIG_LOCAL_CACHE_DIR','local-cache')]:(host/folder).mkdir(parents=True,exist_ok=True);env[key]=str(host/folder)
exe=out/'cache.exe'
r=subprocess.run([*host_compiler(),'-std=c++17','-I',str(root/'tools/host-tests'),'-I',str(root/'VoiceAssistant'),str(out/'cache.cpp'),'-o',str(exe)],env=env,capture_output=True,text=True)
(out/'compile.log').write_text(r.stdout+r.stderr,encoding='utf-8')
if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(exe)],capture_output=True,text=True);(out/'tests.log').write_text(r.stdout+r.stderr,encoding='utf-8');print(r.stdout+r.stderr);raise SystemExit(r.returncode)
