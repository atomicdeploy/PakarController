#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include <atomic>
#include <mutex>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <driver/rmt_tx.h>
#include <driver/rmt_encoder.h>
#include "BoardPins.h"
#include "LedWire.h"
#if !defined(PAKAR_S3_LED_USE_DMA) && defined(ARDUINO_ARCH_ESP32)
// Only LED application units depend on this canonical, ignored build profile.
// Changing the backend must not invalidate unrelated SDK/FastLED objects.
#if __has_include(<build/config/esp32s3/LedBackend.h>)
#include <build/config/esp32s3/LedBackend.h>
#endif
#endif
#ifndef PAKAR_S3_LED_USE_DMA
#define PAKAR_S3_LED_USE_DMA 0
#endif
static_assert(PAKAR_S3_LED_USE_DMA==0 || PAKAR_S3_LED_USE_DMA==1,"LED DMA selection must be boolean");
#if PAKAR_S3_LED_USE_DMA
#include "LedSignalCapture.h"
#endif

namespace PakarS3 {
class LedOutput {
 public:
  static constexpr size_t pixels=outputPixels;
  bool begin();
  void submit(const CRGB *rgb,uint8_t brightness,bool enabled,uint32_t revision);
  bool canAccept()const{return queue_ && uxQueueMessagesWaiting(queue_)==0;}
  bool presented(uint32_t revision)const{return ready_.load() && revision_.load()==revision;}
  String json() const;
  String capabilitiesJson() const;
  static bool acquireFlashPause();
  static void releaseFlashPause();
  bool requestSignalCapture(){
#if PAKAR_S3_LED_USE_DMA
    return ready_.load() && active_.load() && capture_.request();
#else
    return false;
#endif
  }
 private:
  struct Frame {CRGB rgb[pixels];uint32_t revision;uint8_t brightness;bool enabled;};
  static void run(void *context){static_cast<LedOutput*>(context)->work();}
  void work();
  static bool onDone(rmt_channel_handle_t,const rmt_tx_done_event_data_t *,void *);
  QueueHandle_t queue_=nullptr;
  StaticQueue_t queueState_{};
  alignas(4) uint8_t queueBytes_[sizeof(Frame)]{};
  // Persistent producer scratch, never a ~1.5 KiB frame on the loop/TLS stack.
  // Serialize complete copies until xQueueOverwrite has copied the mailbox.
  std::mutex producerGate_;
  Frame producer_{};
  // Worker-owned frame: producers can only overwrite the separate mailbox.
  Frame current_{};
  std::atomic<bool> ready_{false},active_{false};
  std::atomic<uint32_t> submitted_{0},transmitted_{0},failed_{0},revision_{0};
  std::atomic<int> error_{0};
  std::atomic<uint32_t> doneSymbols_{0};
  // Task-context measurements around real complete-frame transmissions, not
  // host writes/encoded-symbol accounting or physical/optical verification.
  std::atomic<uint32_t> lastFrameMicros_{0},maxFrameMicros_{0};
  std::atomic<uint32_t> lastStartIntervalMicros_{0},lastCompletedAtMicros_{0};
  std::atomic<bool> refillCacheSafe_{false};
#if PAKAR_S3_LED_USE_DMA
  LedSignalCapture capture_;
#endif
  static std::mutex wireGate_;
  static std::atomic<uint32_t> flashHolds_;
  static std::atomic<bool> inFlight_;
};
}
