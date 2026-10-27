#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// An explicitly partial USB receipt is a projection of observed board state,
// never a second state model. Keep complete capabilities/driver/diagnostic
// objects, but omit unrelated telemetry and the 480-pixel RGB string. The
// client must merge only after matching both board and boot identity.
inline bool diagnosticReceiptStatus(const String &observed,const String &frame,
                                    String &result) {
  result=String();
  JsonDocument filter,status,pixel;
  for(const char *field:{"version","compatibility","board_id","boot_id",
      "command_clock_ms","configuration_revision","hostname","capabilities",
      "pixels","led_output","pixel_groups","pixel_corrections_revision",
      "pixel_corrections_active_revision","pixel_corrections_preview"})filter[field]=true;
  if(deserializeJson(status,observed,DeserializationOption::Filter(filter)) ||
      !status["board_id"].is<const char*>() || !status["boot_id"].is<const char*>() ||
      !status["capabilities"].is<JsonObject>() || !status["pixels"].is<uint16_t>() ||
      !status["led_output"]["diagnostic"].is<JsonObject>() ||
      deserializeJson(pixel,frame))return false;
  // A null frame is an honest not-yet-published observation, not a fabricated
  // revision. The client can settle with bounded read-only snapshots.
  if(!pixel.isNull() && (!pixel["revision"].is<uint32_t>() ||
      !pixel["pending"].is<bool>() || !pixel["rgb"].isNull()))return false;
  status["pixel_frame"]=pixel.as<JsonVariantConst>();
  constexpr size_t maximumStatusBytes=7168U; // Leaves 1 KiB for the receipt envelope.
  const size_t bytes=measureJson(status);
  if(!bytes || bytes>maximumStatusBytes)return false;
  result.reserve(bytes+1U);
  if(serializeJson(status,result)!=bytes || result.length()!=bytes) {
    result=String();return false;
  }
  return true;
}
