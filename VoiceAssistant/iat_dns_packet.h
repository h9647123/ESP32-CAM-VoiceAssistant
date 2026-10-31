#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static uint16_t iatDns16(const uint8_t* p){return uint16_t(p[0])<<8|p[1];}
static void iatDnsPut16(uint8_t* p,uint16_t v){p[0]=v>>8;p[1]=v;}
static char iatDnsLower(char c){return c>='A'&&c<='Z'?char(c+32):c;}
static bool iatDnsName(const uint8_t* packet,size_t n,size_t& cursor,char* out,size_t capacity) {
  size_t at=cursor,written=0;bool jumped=false;
  for(unsigned hops=0;hops<64;++hops) {
    if(at>=n)return false;
    const uint8_t size=packet[at++];
    if(!size) {if(!jumped)cursor=at;if(written>=capacity)return false;out[written]=0;return true;}
    if((size&0xc0)==0xc0) {
      if(at>=n)return false;const size_t target=((size&0x3f)<<8)|packet[at++];
      if(target>=n)return false;if(!jumped)cursor=at;jumped=true;at=target;continue;
    }
    if(size>63 || at+size>n || written+size+1>=capacity)return false;
    if(written)out[written++]='.';
    for(unsigned i=0;i<size;++i){const char c=char(packet[at++]);if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'))return false;out[written++]=iatDnsLower(c);}
  }
  return false;
}
static size_t iatDnsQuery(const char* host,uint16_t id,uint8_t* out,size_t capacity) {
  if(!host || capacity<18 || !*host || strlen(host)>253)return 0;
  memset(out,0,12);iatDnsPut16(out,id);out[2]=1;out[5]=1;
  size_t at=12;
  for(const char* p=host;*p;) {
    const char* end=strchr(p,'.');const size_t count=end?size_t(end-p):strlen(p);
    if(!count || count>63 || at+count+6>capacity)return 0;
    out[at++]=count;for(size_t i=0;i<count;++i){const char c=p[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'))return 0;out[at++]=uint8_t(c);}
    p+=count;if(*p=='.'){++p;if(!*p)return 0;}
  }
  out[at++]=0;iatDnsPut16(out+at,1);iatDnsPut16(out+at+2,1);return at+4;
}
static bool iatDnsAnswer(const uint8_t* packet,size_t n,const char* host,uint16_t id,uint8_t address[4]) {
  if(n<12 || iatDns16(packet)!=id || (packet[2]&0xfa)!=0x80 || (packet[3]&15) || iatDns16(packet+4)!=1)return false;
  size_t cursor=12;char current[254],name[254];
  if(!iatDnsName(packet,n,cursor,current,sizeof(current)) || cursor+4>n || iatDns16(packet+cursor)!=1 || iatDns16(packet+cursor+2)!=1)return false;
  const size_t hostLength=strlen(host);if(hostLength!=strlen(current))return false;
  for(size_t i=0;i<hostLength;++i)if(iatDnsLower(host[i])!=current[i])return false;
  cursor+=4;
  struct Record{size_t owner,data;uint16_t type,size;};Record records[32];unsigned used=0;
  const unsigned answers=iatDns16(packet+6);if(answers>32)return false;
  for(unsigned i=0;i<answers;++i) {
    const size_t owner=cursor;
    if(!iatDnsName(packet,n,cursor,name,sizeof(name)) || cursor+10>n)return false;
    const uint16_t type=iatDns16(packet+cursor),cls=iatDns16(packet+cursor+2),size=iatDns16(packet+cursor+8);
    cursor+=10;if(size>n-cursor)return false;
    if(cls==1)records[used++]={owner,cursor,type,size};cursor+=size;
  }
  // Follow only the requested name's CNAME chain. Unrelated A/additional
  // records are never accepted; record order may vary between resolvers.
  for(unsigned step=0;step<8;++step) {
    bool alias=false;
    for(unsigned i=0;i<used;++i) {
      size_t at=records[i].owner;if(!iatDnsName(packet,n,at,name,sizeof(name)))return false;
      if(strcmp(name,current))continue;
      if(records[i].type==1 && records[i].size==4) {
        const uint8_t* ip=packet+records[i].data;
        if(!ip[0] || ip[0]>=224 || ip[0]==127)return false;
        memcpy(address,ip,4);return true;
      }
      if(records[i].type==5) {
        at=records[i].data;
        if(!iatDnsName(packet,n,at,name,sizeof(name)) || at!=records[i].data+records[i].size)return false;
        if(!strcmp(name,current))return false;strcpy(current,name);alias=true;break;
      }
    }
    if(!alias)return false;
  }
  return false;
}
