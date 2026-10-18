#pragma once
#include "Arduino.h"
#include <map>
#include <memory>
#include <vector>

struct CloudFsFaults {
  bool failOpenWrite = false, failWrite = false, shortWrite = false;
  bool failVerifyRead = false, corruptVerify = false, failRename = false;
  size_t writeOpens = 0, writes = 0, bytesWritten = 0, renames = 0;
};
class File {
 public:
  struct Data { std::vector<uint8_t> bytes; };
  File() = default;
  File(std::shared_ptr<Data> data, std::shared_ptr<CloudFsFaults> faults, bool pending)
      : data_(data), faults_(faults), pending_(pending) {}
  explicit operator bool() const { return bool(data_); }
  size_t size() const { return data_ ? data_->bytes.size() : 0; }
  size_t read(uint8_t *out, size_t length) {
    if (!data_ || length > size() || (pending_ && faults_->failVerifyRead)) return 0;
    memcpy(out, data_->bytes.data(), length);
    if (pending_ && faults_->corruptVerify && length) out[0] ^= 0xff;
    return length;
  }
  size_t write(const uint8_t *value, size_t length) {
    if (!data_) return 0;
    ++faults_->writes;
    if (faults_->failWrite) return 0;
    const size_t written = faults_->shortWrite && length ? length - 1 : length;
    data_->bytes.assign(value, value + written);
    faults_->bytesWritten += written;
    return written;
  }
  void close() { data_.reset(); }
 private:
  std::shared_ptr<Data> data_;
  std::shared_ptr<CloudFsFaults> faults_;
  bool pending_ = false;
};
namespace fs {
class FS {
 public:
  std::shared_ptr<CloudFsFaults> faults = std::make_shared<CloudFsFaults>();
  std::map<std::string, std::shared_ptr<File::Data>> files;
  File open(const char *path, const char *mode) {
    if (mode[0] == 'w') {
      ++faults->writeOpens;
      if (faults->failOpenWrite) return File();
      files[path] = std::make_shared<File::Data>();
    }
    const auto entry = files.find(path);
    return entry == files.end() ? File() : File(entry->second, faults, std::string(path) == "/cloud.pending");
  }
  bool rename(const char *from, const char *to) {
    ++faults->renames;
    const auto entry = files.find(from);
    if (faults->failRename || entry == files.end()) return false;
    files[to] = entry->second; files.erase(entry); return true;
  }
};
}
