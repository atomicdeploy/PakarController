#pragma once
#include <stdint.h>
#include <string.h>
#include <string>
#define F(value) value
#define strlen_P strlen
class String : public std::string {
 public:
  using std::string::string;
  String() = default;
  bool isEmpty() const { return empty(); }
  void trim() { const auto first = find_first_not_of(" \r\n\t"); if (first == npos) { clear(); return; } erase(0, first); erase(find_last_not_of(" \r\n\t") + 1); }
  bool startsWith(const char *prefix) const { return rfind(prefix, 0) == 0; }
  int indexOf(char value, size_t start = 0) const { const auto position = find(value, start); return position == npos ? -1 : static_cast<int>(position); }
  void remove(size_t start, size_t count = npos) { erase(start, count); }
  String substring(size_t start, size_t end = npos) const { return String(substr(start, end == npos ? npos : end - start).c_str()); }
  void toCharArray(char *target, size_t capacity) const {
    if (capacity) { strncpy(target, c_str(), capacity - 1); target[capacity - 1] = 0; }
  }
};
