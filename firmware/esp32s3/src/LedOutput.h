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
#if !defined(PAKAR_S3_LED_GPIO_ENABLED) && !defined(PAKAR_S3_LED_RMT_ENABLED) && defined(ARDUINO_ARCH_ESP32)
// Only LED application units depend on this canonical, ignored build profile.
// Changing the backend must not invalidate unrelated SDK/FastLED objects.
#if __has_include(<build/config/esp32s3/LedBackend.h>)
#include <build/config/esp32s3/LedBackend.h>
#endif
#endif
#ifndef PAKAR_S3_LED_GPIO_ENABLED
#define PAKAR_S3_LED_GPIO_ENABLED 0
#endif
#ifndef PAKAR_S3_LED_RMT_ENABLED
#define PAKAR_S3_LED_RMT_ENABLED 0
#endif
static_assert(PAKAR_S3_LED_GPIO_ENABLED==0 || PAKAR_S3_LED_GPIO_ENABLED==1,"LED GPIO inclusion must be boolean");
static_assert(PAKAR_S3_LED_RMT_ENABLED==0 || PAKAR_S3_LED_RMT_ENABLED==1,"LED RMT inclusion must be boolean");
#if PAKAR_S3_LED_RMT_ENABLED
#include "LedSignalCapture.h"
#endif
#if PAKAR_S3_LED_GPIO_ENABLED
#include <sdkconfig.h>
#if CONFIG_PM_ENABLE
#include <esp_pm.h>
#endif
#endif

namespace PakarS3 {
class LedOutput {
 public:
  static constexpr size_t pixels=outputPixels;
  enum class Backend : uint8_t {None=0,Gpio=1,RmtDma=2};
  static constexpr Backend defaultBackend(){
#ifdef PAKAR_S3_LED_DEFAULT_BACKEND
    return static_cast<Backend>(PAKAR_S3_LED_DEFAULT_BACKEND);
#else
    return PAKAR_S3_LED_GPIO_ENABLED?Backend::Gpio:(PAKAR_S3_LED_RMT_ENABLED?Backend::RmtDma:Backend::None);
#endif
  }
  static bool backendSupported(Backend);
  static const char* backendName(Backend);
  static bool parseBackend(const String&,Backend&);
  static String availableBackendsJson();
  bool begin(Backend initial=defaultBackend());
  bool requestBackend(Backend);
  Backend backend()const{return backend_.load();}
  Backend requestedBackend()const{return requestedBackend_.load();}
  bool switchPending()const{return requestedRevision_.load()!=appliedRevision_.load();}
  uint32_t backendRevision()const{return backendRevision_.load();}
  int switchError()const{return switchError_.load();}
  void submit(const CRGB *rgb,uint8_t brightness,bool enabled,uint32_t revision);
  bool canAccept()const{return queue_ && uxQueueMessagesWaiting(queue_)==0;}
  bool presented(uint32_t revision)const{return ready_.load() && revision_.load()==revision;}
  String json(bool compact=false) const;
  String capabilitiesJson() const;
  static bool acquireFlashPause();
  static void releaseFlashPause();
  bool requestSignalCapture(){
#if PAKAR_S3_LED_RMT_ENABLED
    std::lock_guard<std::mutex> lock(wireGate_);
    return backend()==Backend::RmtDma && !switchPending() && ready_.load() && active_.load() && capture_.request();
#else
    return false;
#endif
  }
 private:
  struct Frame {CRGB rgb[pixels];uint32_t revision;uint8_t brightness;bool enabled;};
  static void run(void *context){static_cast<LedOutput*>(context)->work();}
  void work();
  int initializeDriver(Backend);
  int releaseDriver();
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
  // Only the single core1 worker touches handles, payload, PM lock or the pad.
  uint8_t* payload_=nullptr;
  bool pinOwned_=false;
#if PAKAR_S3_LED_RMT_ENABLED
  rmt_channel_handle_t channel_=nullptr;
  rmt_encoder_handle_t encoder_=nullptr;
  bool channelEnabled_=false;
#endif
#if PAKAR_S3_LED_GPIO_ENABLED && CONFIG_PM_ENABLE
  esp_pm_lock_handle_t frequencyLock_=nullptr;
  bool frequencyLocked_=false;
#endif
  std::atomic<Backend> backend_{Backend::None},requestedBackend_{Backend::None};
  std::atomic<uint32_t> requestedRevision_{0},appliedRevision_{0},backendRevision_{0};
  std::atomic<int> switchError_{0};
  std::atomic<bool> ready_{false},active_{false};
  std::atomic<uint32_t> submitted_{0},transmitted_{0},failed_{0},revision_{0};
  std::atomic<int> error_{0};
  std::atomic<uint32_t> doneSymbols_{0};
  // Task-context measurements around real complete-frame transmissions, not
  // host writes/encoded-symbol accounting or physical/optical verification.
  std::atomic<uint32_t> lastFrameMicros_{0},maxFrameMicros_{0};
  std::atomic<uint32_t> lastStartIntervalMicros_{0},lastCompletedAtMicros_{0};
  std::atomic<bool> refillCacheSafe_{false};
#if PAKAR_S3_LED_RMT_ENABLED
  LedSignalCapture capture_;
#endif
  static std::mutex wireGate_;
  static std::atomic<uint32_t> flashHolds_;
  static std::atomic<bool> inFlight_;
};
}
