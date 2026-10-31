#pragma once
#include <stdint.h>
#include <string.h>
// Single-stream MJPEG AVI; RIFF layout follows Microsoft's AVI RIFF reference.
// This helper also compiles on the host so ffprobe tests use these exact bytes.
static uint32_t aviRead32(const uint8_t* b) { return uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24); }
static void aviPut32(uint8_t* b,uint32_t v) { for(int i=0;i<4;++i)b[i]=v>>(8*i); }
static constexpr uint32_t CAM_AVI_HEADER=324,CAM_AVI_MOVI=320;
static void aviHeader(uint8_t* b,uint32_t w,uint32_t h,uint32_t frames,uint32_t ms,uint32_t moviBytes,uint32_t largest,bool index,uint32_t audioBytes=0,uint32_t indexEntries=0) {
  memset(b,0,CAM_AVI_HEADER); if(!ms)ms=1;
  memcpy(b,"RIFF",4); aviPut32(b+4,CAM_AVI_HEADER-8+moviBytes+(index?8+indexEntries*16:0)); memcpy(b+8,"AVI LIST",8);
  aviPut32(b+16,292);memcpy(b+20,"hdrlavih",8);aviPut32(b+28,56);
  aviPut32(b+32,frames?uint64_t(ms)*1000/frames:100000);aviPut32(b+36,frames?uint64_t(moviBytes)*1000/ms:0);
  aviPut32(b+44,index?0x10:0);aviPut32(b+48,frames);aviPut32(b+56,2);aviPut32(b+60,largest);
  aviPut32(b+64,w);aviPut32(b+68,h);
  memcpy(b+88,"LIST",4);aviPut32(b+92,116);memcpy(b+96,"strlstrh",8);aviPut32(b+104,56);
  memcpy(b+108,"vidsMJPG",8);aviPut32(b+128,ms);aviPut32(b+132,frames?frames*1000:1000);
  aviPut32(b+140,frames);aviPut32(b+144,largest);aviPut32(b+148,0xffffffff);
  b[160]=w;b[161]=w>>8;b[162]=h;b[163]=h>>8;
  memcpy(b+164,"strf",4);aviPut32(b+168,40);aviPut32(b+172,40);aviPut32(b+176,w);aviPut32(b+180,h);
  b[184]=1;b[186]=24;memcpy(b+188,"MJPG",4);aviPut32(b+192,w*h*3);
  memcpy(b+212,"LIST",4);aviPut32(b+216,92);memcpy(b+220,"strlstrh",8);aviPut32(b+228,56);
  memcpy(b+232,"auds",4);aviPut32(b+252,2);aviPut32(b+256,32000);aviPut32(b+264,audioBytes/2);
  aviPut32(b+268,6400);aviPut32(b+272,0xffffffff);aviPut32(b+276,2);
  memcpy(b+288,"strf",4);aviPut32(b+292,16);b[296]=1;b[298]=1;aviPut32(b+300,16000);aviPut32(b+304,32000);b[308]=2;b[310]=16;
  memcpy(b+312,"LIST",4);aviPut32(b+316,4+moviBytes);memcpy(b+320,"movi",4);
}
