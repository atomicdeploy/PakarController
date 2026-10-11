#pragma once
#include "Arduino.h"
#include <map>
#include <memory>
#include <vector>
class File {
 public:
  struct Data { std::vector<uint8_t> bytes; };
  File() = default;
  File(std::shared_ptr<Data> data, bool failWrite = false) : data_(data), failWrite_(failWrite) {}
  explicit operator bool() const { return bool(data_); }
  size_t size() const { return data_ ? data_->bytes.size() : 0; }
  size_t read(uint8_t *bytes, size_t length) {
    if (!data_ || length > size()) return 0;
    memcpy(bytes, data_->bytes.data(), length); return length;
  }
  size_t write(const uint8_t *bytes, size_t length) {
    if (!data_ || failWrite_) return 0;
    data_->bytes.assign(bytes, bytes + length); return length;
  }
  void close() { data_.reset(); }
  const char *name() const { return "fixture"; }
 private:
  std::shared_ptr<Data> data_;
  bool failWrite_ = false;
};
namespace fs {
class FS {
 public:
  bool failWrite = false, failRename = false;
  std::map<std::string, std::shared_ptr<File::Data>> files;
  bool exists(const char *path) const { return files.count(path); }
  File open(const char *path, const char *mode) {
    if (mode[0] == 'w') files[path] = std::make_shared<File::Data>();
    return exists(path) ? File(files[path], failWrite) : File();
  }
  bool rename(const char *from, const char *to) {
    if (failRename || !exists(from)) return false;
    files[to] = files[from]; files.erase(from); return true;
  }
};
}
