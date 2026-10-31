#pragma once
#include <Arduino.h>
#include <FS.h>
#include <mbedtls/base64.h>
#include <new>

// Read-only JSON upload: bounded prefix + incremental base64 + bounded suffix.
// JPEG stays in PSRAM, or in an already-open TF file. No complete base64 String.
class CamJpegRequestStream : public Stream {
 public:
  CamJpegRequestStream(const String& prefix, const String& suffix,
      const uint8_t* jpeg, size_t size, File* file = nullptr)
      : prefix_(prefix), suffix_(suffix), jpeg_(jpeg), jpegSize_(size), file_(file),
        total_(prefix.length() + ((size + 2) / 3) * 4 + suffix.length()) {}
  size_t size() const { return total_; }
  bool failed() const { return failed_; }
  int available() override {
    // HTTPClient treats -1 as an ended/error stream. Never spin on zero after I/O failure.
    return failed_ ? -1 : int(total_ - emitted_);
  }
  int peek() override {
    if (peeked_ < 0) peeked_ = next();
    return peeked_;
  }
  int read() override {
    int value;
    if (peeked_ >= 0) { value = peeked_; peeked_ = -1; }
    else value = next();
    if (value >= 0) ++emitted_;
    return value;
  }
  size_t readBytes(char* buffer, size_t length) override {
    size_t count = 0;
    while (count < length) {
      const int value = read();
      if (value < 0) break;
      buffer[count++] = char(value);
    }
    return count;
  }
  size_t readBytes(uint8_t* buffer, size_t length) override {
    return readBytes(reinterpret_cast<char*>(buffer), length);
  }
  size_t write(uint8_t) override { return 0; }
  void flush() override {}

 private:
  int next() {
    if (failed_) return -1;
    if (prefixOffset_ < prefix_.length()) return uint8_t(prefix_[prefixOffset_++]);
    if (encodedOffset_ < encodedSize_) return encoded_[encodedOffset_++];
    if (jpegOffset_ < jpegSize_) {
      const size_t count = min<size_t>(sizeof(raw_), jpegSize_ - jpegOffset_);
      const uint8_t* source = jpeg_ ? jpeg_ + jpegOffset_ : raw_;
      if (!jpeg_ && (!file_ || file_->read(raw_, count) != count)) {
        failed_ = true; return -1;
      }
      if (mbedtls_base64_encode(encoded_, sizeof(encoded_), &encodedSize_, source, count) != 0) {
        failed_ = true; return -1;
      }
      jpegOffset_ += count;
      encodedOffset_ = 1;
      return encoded_[0];
    }
    if (suffixOffset_ < suffix_.length()) return uint8_t(suffix_[suffixOffset_++]);
    return -1;
  }
  const String& prefix_;
  const String& suffix_;
  const uint8_t* jpeg_;
  size_t jpegSize_;
  File* file_;
  size_t total_;
  size_t emitted_ = 0;
  size_t prefixOffset_ = 0;
  size_t suffixOffset_ = 0;
  size_t jpegOffset_ = 0;
  size_t encodedSize_ = 0;
  size_t encodedOffset_ = 0;
  int peeked_ = -1;
  bool failed_ = false;
  uint8_t raw_[768]; // multiple of 3: no padding in intermediate chunks
  uint8_t encoded_[1025];
};

struct CamJpegSource {
  const uint8_t* jpeg=nullptr;
  size_t size=0;
  String path;
};

// Each boundary appears exactly once: prefix, between-image JSON, final suffix.
// Only one file and one 768-byte base64 source buffer are active at a time.
class CamMultiJpegRequestStream : public Stream {
 public:
  CamMultiJpegRequestStream(const String* parts,const CamJpegSource* sources,size_t count,fs::FS& filesystem)
      : parts_(parts),sources_(sources),count_(count),filesystem_(filesystem) {
    if(!parts || !sources || !count || count>6) {failed_=true;return;}
    for(size_t i=0;i<=count;++i)total_+=parts[i].length();
    for(size_t i=0;i<count;++i) {
      if(!sources[i].size || sources[i].size>1024U*1024U || (!sources[i].jpeg && !sources[i].path.length())) {failed_=true;return;}
      total_+=((sources[i].size+2)/3)*4;
    }
    if(total_>12U*1024U*1024U)failed_=true;
  }
  ~CamMultiJpegRequestStream() {delete active_;file_.close();}
  CamMultiJpegRequestStream(const CamMultiJpegRequestStream&)=delete;
  CamMultiJpegRequestStream& operator=(const CamMultiJpegRequestStream&)=delete;
  size_t size()const{return total_;}
  bool failed()const{return failed_;}
  int available()override{return failed_?-1:int(total_-emitted_);}
  int peek()override{if(peeked_<0)peeked_=next();return peeked_;}
  int read()override {
    int value;if(peeked_>=0){value=peeked_;peeked_=-1;}else value=next();
    if(value>=0)++emitted_;return value;
  }
  size_t readBytes(char* data,size_t bytes)override {
    size_t n=0;while(n<bytes){const int v=read();if(v<0)break;data[n++]=char(v);}return n;
  }
  size_t readBytes(uint8_t* data,size_t bytes)override{return readBytes((char*)data,bytes);}
  size_t write(uint8_t)override{return 0;}
  void flush()override{}
 private:
  int next() {
    if(failed_)return -1;
    while(index_<count_) {
      if(!active_) {
        const auto& source=sources_[index_];
        if(!source.jpeg) {
          file_=filesystem_.open(source.path,FILE_READ);
          if(!file_ || file_.size()!=source.size){file_.close();failed_=true;return -1;}
        }
        active_=new(std::nothrow)CamJpegRequestStream(index_?empty_:parts_[0],parts_[index_+1],source.jpeg,source.size,&file_);
        if(!active_){file_.close();failed_=true;return -1;}
      }
      const int value=active_->read();
      if(value>=0)return value;
      if(active_->failed()){failed_=true;return -1;}
      delete active_;active_=nullptr;file_.close();++index_;
    }
    return -1;
  }
  const String* parts_;
  const CamJpegSource* sources_;
  const size_t count_;
  fs::FS& filesystem_;
  String empty_;
  File file_;
  CamJpegRequestStream* active_=nullptr;
  size_t index_=0,total_=0,emitted_=0;
  int peeked_=-1;
  bool failed_=false;
};
