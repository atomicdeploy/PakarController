#pragma once
#include <stdint.h>
#include <string.h>
#include <string>
#define F(value) value
#define strlen_P strlen
inline uint32_t millis() { return 0; }
class String : public std::string {
 public:
  using std::string::string;
  String() = default;
  String(const std::string &value) : std::string(value) {}
  bool isEmpty() const { return empty(); }
  size_t write(uint8_t c) { push_back(static_cast<char>(c)); return 1; }
  size_t write(const uint8_t *p,size_t n) { append(reinterpret_cast<const char*>(p),n); return n; }
  void trim() { const auto first = find_first_not_of(" \r\n\t"); if (first == npos) { clear(); return; } erase(0, first); erase(find_last_not_of(" \r\n\t") + 1); }
  bool startsWith(const char *prefix) const { return rfind(prefix, 0) == 0; }
  int indexOf(char value, size_t start = 0) const { const auto position = find(value, start); return position == npos ? -1 : static_cast<int>(position); }
  void remove(size_t start, size_t count = npos) { erase(start, count); }
  String substring(size_t start, size_t end = npos) const { return String(substr(start, end == npos ? npos : end - start).c_str()); }
  void toCharArray(char *target, size_t capacity) const {
    if (capacity) { strncpy(target, c_str(), capacity - 1); target[capacity - 1] = 0; }
  }
};
