#pragma once
#include <cstddef>
inline int mbedtls_base64_encode(unsigned char* out,size_t capacity,size_t* written,const unsigned char* in,size_t n){
 const char* table="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 *written=((n+2)/3)*4;if(capacity<*written+1)return -1;
 size_t k=0;for(size_t i=0;i<n;i+=3){unsigned x=unsigned(in[i])<<16;if(i+1<n)x|=unsigned(in[i+1])<<8;if(i+2<n)x|=in[i+2];out[k++]=table[(x>>18)&63];out[k++]=table[(x>>12)&63];out[k++]=i+1<n?table[(x>>6)&63]:'=';out[k++]=i+2<n?table[x&63]:'=';}out[k]=0;return 0;
}
