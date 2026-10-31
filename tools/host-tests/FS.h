#pragma once
#include "Arduino.h"
#include <map>
#include <memory>
#include <vector>
#define FILE_READ "r"
namespace fs {class FS;}
class File {
 struct Handle {std::vector<uint8_t> data;size_t pos=0;int* opens;Handle(std::vector<uint8_t> d,int* o):data(d),opens(o){++*opens;}~Handle(){--*opens;}};
 std::shared_ptr<Handle> h;
 friend class fs::FS;
public:
 explicit operator bool()const{return bool(h);}
 size_t size()const{return h?h->data.size():0;}
 size_t read(uint8_t* out,size_t n){if(!h)return 0;n=min(n,h->data.size()-h->pos);std::copy_n(h->data.data()+h->pos,n,out);h->pos+=n;return n;}
 void close(){h.reset();}
};
namespace fs {
class FS {
public:
 std::map<String,std::vector<uint8_t>> files;
 int opens=0,peak=0;
 File open(const String& path,const char*){File f;auto i=files.find(path);if(i!=files.end()){f.h=std::make_shared<File::Handle>(i->second,&opens);peak=std::max(peak,opens);}return f;}
};
}
