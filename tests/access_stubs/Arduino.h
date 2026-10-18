#pragma once
#include <stdint.h>
#include <string.h>
#include <string>
#include <atomic>
#define F(value) value
#define strlen_P strlen
#define memcpy_P memcpy
inline std::atomic<uint32_t> pakarTestMillis{0};
inline bool pakarTestStringConcatAllowed=true;
inline uint32_t millis() { return pakarTestMillis.load(std::memory_order_relaxed); }
class String : public std::string {
#if defined(ARDUINOJSON_ENABLE_ARDUINO_STRING) && ARDUINOJSON_ENABLE_ARDUINO_STRING
  // Real Arduino String is not an STL container. Hide this inherited host-only
  // type when selecting ArduinoJson's actual ArduinoStringReader/Writer path.
  using const_iterator = std::string::const_iterator;
#endif
 public:
  using std::string::string;
  String() = default;
  String(const std::string &value) : std::string(value) {}
  // Actual ArduinoJson ArduinoStringWriter clears with a null const char* and
  // flushes through concat(); generic write() below intentionally appends.
  String& operator=(const char* value) { assign(value ? value : ""); return *this; }
  bool concat(const char* value) { if(!pakarTestStringConcatAllowed)return false;if(value)append(value);return true; }
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
