#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// The same top-level parsed object must select the command and its operands.
// Raw substring searches can accidentally execute a nested or earlier key that
// the authenticated command admission never authorized.
namespace PakarCommand {
inline String stringValue(JsonObjectConst object, const char *key) {
  if (!object[key].is<const char *>()) return String();
  const JsonString value = object[key].as<JsonString>();
  if (!value.c_str() || strlen(value.c_str()) != value.size()) return String();
  return object[key].as<String>();
}
inline long integerValue(JsonObjectConst object, const char *key, long fallback) {
  return object[key].is<long>() ? object[key].as<long>() : fallback;
}
inline bool booleanValue(JsonObjectConst object, const char *key) {
  return object[key].is<bool>() && object[key].as<bool>();
}
} // namespace PakarCommand
