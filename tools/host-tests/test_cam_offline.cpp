#include "cam_media_delete_state.h"
#include "latex_display.h"
#include "asr_capture_limits.h"
#include "service_probe_policy.h"
#include "iat_dns_packet.h"
#include "cam_jpeg_stream.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include "cam_queue_under_test.h"
#include "cam_mode_routes_under_test.h"
int main(int argc,char**argv){
 {
  uint8_t packet[512]={},address[4]={};const char* host="asr.h9647123.cn";
  const size_t q=iatDnsQuery(host,0x1234,packet,sizeof(packet));assert(q>12);
  assert(!iatDnsQuery("a..cn",1,packet,sizeof(packet)));assert(!iatDnsQuery("bad/host",1,packet,sizeof(packet)));
  assert(!iatDnsQuery("trailing.cn.",1,packet,sizeof(packet)));
  assert(iatDnsQuery(host,0x1234,packet,sizeof(packet))==q);packet[2]=0x81;packet[3]=0x80;packet[7]=1;
  const uint8_t answer[]={0xc0,0x0c,0,1,0,1,0,0,1,44,0,4,221,178,62,105};
  memcpy(packet+q,answer,sizeof(answer));size_t n=q+sizeof(answer);
  assert(iatDnsAnswer(packet,n,host,0x1234,address));assert(address[0]==221&&address[3]==105);
  assert(!iatDnsAnswer(packet,n,host,0x1235,address));assert(!iatDnsAnswer(packet,n,"other.example",0x1234,address));
  for(size_t length=0;length<n;++length)assert(!iatDnsAnswer(packet,length,host,0x1234,address));
  packet[2]|=2;assert(!iatDnsAnswer(packet,n,host,0x1234,address));packet[2]&=~2;
  packet[3]=0x83;assert(!iatDnsAnswer(packet,n,host,0x1234,address));packet[3]=0x80;
  packet[q+1]=uint8_t(q); // Compression loop, bounded by the decoder.
  assert(!iatDnsAnswer(packet,n,host,0x1234,address));packet[q+1]=12;
  packet[q+12]=127;assert(!iatDnsAnswer(packet,n,host,0x1234,address));packet[q+12]=221;
  // CNAME -> A, with compressed owner name and compressed target owner.
  packet[7]=2;size_t at=q;memcpy(packet+at,answer,12);iatDnsPut16(packet+at+2,5);
  const uint8_t alias[]={7,'f','r','p','-','p','u','t',3,'c','o','m',0};
  iatDnsPut16(packet+at+10,sizeof(alias));at+=12;size_t aliasAt=at;memcpy(packet+at,alias,sizeof(alias));at+=sizeof(alias);
  memcpy(packet+at,answer,sizeof(answer));packet[at]=0xc0|uint8_t(aliasAt>>8);packet[at+1]=uint8_t(aliasAt);at+=sizeof(answer);
  assert(iatDnsAnswer(packet,at,host,0x1234,address));assert(address[3]==105);
  packet[at-16]=0xc0;packet[at-15]=12; // An A for the unrelated/original owner cannot satisfy the CNAME target.
  assert(!iatDnsAnswer(packet,at,host,0x1234,address));
 }
 assert(serviceNeedsFullProbe(true,false,false));
 assert(serviceNeedsFullProbe(false,true,false));
 assert(!serviceNeedsFullProbe(false,true,true));
 assert(!serviceNeedsFullProbe(false,false,false));
 static_assert(camAsrRecordLimitMs(true,12)==60000,"CUSTOM capture cutoff");
 static_assert(camAsrRecordLimitMs(false,12)==12000,"Legacy provider unchanged");
 static_assert(camMicCapacitySamples(16000,60000,1800)==1020800,"Full PCM plus tail and headroom");
 static_assert(camMicCapacitySamples(16000,20000,0)==352000,"Diagnostics must fit their requested duration");
 static_assert(camAsrUploadLimitBytes(16000,true,12,1800)==2041600,"CUSTOM upload must accept a full recording with tail");
 static_assert(camAsrUploadLimitBytes(16000,false,12,1800)==1920000,"Other providers retain the old upload bound");
 assert(camMicCapacitySamples(16000,60000,1800)>16000*60+16000*1800/1000);
 const uint32_t wrapStart=0xfffffff0U;
 assert(uint32_t(wrapStart+60000U-wrapStart)>=camAsrRecordLimitMs(true,12));

 auto display=[](const std::string& input){std::string out;for(size_t i=0;i<input.size();)if(!latexDisplayToken(input.data(),input.size(),i,out))out+=input[i++];return out;};
 assert(display(R"(\boxed{a\in(-\infty,-1]})")=="a∈(-∞,-1]");
 assert(display(R"(\[\boxed{a\in(-\infty,-1]}\])")=="a∈(-∞,-1]");
 assert(display(R"(\boxed{\sqrt{x^{2}+1}})")=="√(x^{2}+1)");
 assert(display(R"(\infty+\in x)")=="∞+∈ x");
 assert(display(R"(\unknown{x})")==R"(\unknown{x})");
 assert(display(R"(\boxed{x)")==R"(\boxed{x)");
 assert(display("中文上下行 H_{2}O x^{2}")=="中文上下行 H_{2}O x^{2}");

 auto select=[](const char* path,size_t size=1){assert(camStashCurrentPhoto());camPhoto.jpeg=(uint8_t*)queueAlloc();camPhoto.size=size;camPhoto.imagePath=path;camPhoto.confirmed=true;camQuestionRequiresPhoto=true;};
 select("/photo/1.jpg");select("/photo/2.jpg");select("/photo/3.jpg");
 assert(camQuestionPhotoCount()==3);assert(!camStashCurrentPhoto());assert(liveAllocations.size()==3);
 assert(camQuestionPhotoAt(0).imagePath=="/photo/1.jpg");assert(camQuestionPhotoAt(2).imagePath=="/photo/3.jpg");
 camRemoveQuestionPhoto(1);assert(camQuestionPhotoCount()==2);assert(liveAllocations.size()==2);
 assert(camQuestionPhotoAt(0).imagePath=="/photo/1.jpg");assert(camQuestionPhotoAt(1).imagePath=="/photo/3.jpg");
 camRemoveQuestionPhoto(1);assert(camQuestionPhotoCount()==1);assert(camPhoto.imagePath=="/photo/1.jpg");
 assert(camStashCurrentPhoto());assert(!camPhoto.ready());camRestoreLastPhoto();assert(camPhoto.imagePath=="/photo/1.jpg");
 camQuestionReadyForVoice=true;camRemoveQuestionPhoto(0);assert(!camQuestionRequiresPhoto);assert(!camQuestionReadyForVoice);assert(liveAllocations.empty());
 select("/photo/big1.jpg",1024*1024);select("/photo/big2.jpg",1024*1024);select("/photo/big3.jpg",1024*1024);
 assert(!camBoundQuestionPhotoRam());assert(liveAllocations.size()==3); // No saved copy: retain ownership.
 SD_MMC.files["/photo/big1.jpg"]=std::vector<uint8_t>(1024*1024);camSdReady=true;
 assert(camBoundQuestionPhotoRam());assert(liveAllocations.size()==2);assert(!camQuestionPhotoAt(0).jpeg);assert(camQuestionPhotoAt(0).ready());
 camRemoveQuestionPhoto(0);assert(camQuestionPhotoCount()==2);assert(liveAllocations.size()==2);
 camReleasePhoto();assert(camQuestionPhotoCount()==0);assert(liveAllocations.empty());assert(SD_MMC.opens==0);
 camPhotoOnly=true;probeRecordingRoute();assert(modePath==1);
 select("/photo/album.jpg");camQuestionReadyForVoice=true;
 probeRecordingRoute();assert(modePath==3);assert(camPhotoOnly);assert(camQuestionPhotoCount()==1);
 lastFailedQuestion="previous failure";probeRecordingRoute();assert(modePath==3);assert(camQuestionPhotoCount()==1);assert(lastFailedQuestion.empty());
 camVisionEnabled=true;probeRecordingRoute();assert(modePath==3);assert(camPhotoOnly);
 assert(probeAskRoute("photo question"));assert(visionCalls==1);assert(camQuestionPhotoCount()==1);assert(camPhotoOnly);
 camReleasePhoto();probeRecordingRoute();assert(modePath==1);assert(camPhotoOnly);
 hasHistoricalPhotos=true;camQuestionReadyForVoice=true;probeRecordingRoute();assert(modePath==3);
 assert(probeAskRoute("follow-up using retained images"));assert(visionCalls==2);assert(camPhotoOnly);
 camQuestionRequiresPhoto=true;assert(!probeAskRoute("failed retake"));assert(visionCalls==2);camQuestionRequiresPhoto=false;
 camQuestionReadyForVoice=false;probeRecordingRoute();assert(modePath==1);hasHistoricalPhotos=false;
 camPhotoOnly=false;probeRecordingRoute();assert(modePath==2);
 camVoiceOnlyQuestion=true;camQuestionReadyForVoice=true;probeRecordingRoute();assert(modePath==3);camReleasePhoto();
 CamMediaDeleteState d;using A=CamMediaDeleteState::Action;
 assert(d.action('A',true,false)==A::Waiting);
 for(bool video:{false,true}){
  const char* p=video?"/video/V0001.avi":"/photo/P0001.jpg";
  assert(d.begin(video,2,p));assert(d.matches(video,2,p));
  assert(!d.matches(video,1,p));assert(!d.matches(!video,2,p));assert(!d.matches(video,2,"/photo/other.jpg"));
  assert(d.action('C',true,false)==A::Cancel);assert(d.action('B',true,false)==A::Cancel);
  assert(d.action('A',false,true)==A::Cancel);assert(d.action('A',true,true)==A::Cancel);
  assert(d.action('A',false,false)==A::Waiting);assert(d.action('B',false,true)==A::Waiting);
  assert(d.action('A',true,false)==A::Confirm);assert(d.active); // caller must verify SD and locked target
  d.clear();assert(!d.active);assert(d.action('A',true,false)==A::Waiting);
 }
 for(const char* p:{"/photo/../a.jpg","/photo/a/b.jpg","/photo/a\\b.jpg","/photo/a.avi","/video/a.avi","/photo/.jpg","/photo/"})assert(!d.begin(false,0,p));
 assert(!d.begin(false,0,nullptr));assert(!d.begin(false,0,(std::string("/photo/")+std::string(130,'x')+".jpg").c_str()));
 char target[]="/photo/original.jpg";assert(d.begin(false,0,target));target[7]='X';assert(!strcmp(d.path,"/photo/original.jpg"));d.clear();
 fs::FS fs;String parts[]={"[\"data:image/jpeg;base64,","\",\"data:image/jpeg;base64,","\",\"data:image/jpeg;base64,","\"]"};
 std::ofstream output(argv[1]);
 for(size_t n:{1,2,3,767,768,769,1537})for(size_t count:{1,2,3}){
  std::vector<uint8_t> data(n);for(size_t i=0;i<n;++i)data[i]=uint8_t(i*17+3);
  fs.files["/photo/a.jpg"]=data;CamJpegSource sources[]={{data.data(),n,""},{nullptr,n,"/photo/a.jpg"},{nullptr,n,"/photo/a.jpg"}};
  String boundaries[4];for(size_t i=0;i<count;++i)boundaries[i]=parts[i];boundaries[count]="\"]";
  {CamMultiJpegRequestStream stream(boundaries,sources,count,fs);String actual;assert(!stream.failed());
   while(stream.available()>0){const int before=stream.available();int v=stream.peek();assert(v==stream.peek());assert(stream.available()==before);assert(v==stream.read());actual+=char(v);uint8_t block[257];size_t k=stream.readBytes(block,sizeof(block));actual.append((char*)block,k);}
   assert(actual.size()==stream.size());assert(stream.read()==-1);assert(stream.available()==0);assert(!stream.failed());output<<n<<' '<<count<<' '<<actual<<'\n';
  }assert(fs.opens==0);assert(fs.peak<=1);
 }
 CamJpegSource missing[]={{nullptr,5,"/missing.jpg"}};
 {CamMultiJpegRequestStream stream(parts,missing,1,fs);assert(stream.read()==-1);assert(stream.failed());assert(stream.available()==-1);}
 {CamMultiJpegRequestStream stream(parts,missing,0,fs);assert(stream.failed());}
 {CamMultiJpegRequestStream stream(parts,missing,7,fs);assert(stream.failed());}
 {CamJpegSource wrong[]={{nullptr,6,"/photo/a.jpg"}};CamMultiJpegRequestStream stream(parts,wrong,1,fs);assert(stream.read()==-1);assert(stream.failed());}
 // Read failure after a valid open, as with a card/file disappearing during upload.
 {File f=fs.open("/photo/a.jpg",FILE_READ);String a="",b="";CamJpegRequestStream stream(a,b,nullptr,2000,&f);while(stream.read()>=0){}assert(stream.failed());assert(stream.available()==-1);f.close();}
 assert(fs.opens==0);std::cout<<"delete_state_and_stream_checks_passed\n";
}
