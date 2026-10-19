#pragma once
#include <Arduino.h>
#include "NtfyNotice.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#endif

class NtfyService {
 public:
  static constexpr bool supported() {
#if defined(ARDUINO_ARCH_ESP32)
    return true;
#else
    return false; // No blocking HTTPS added to the ESP8266 LED/UART loop.
#endif
  }
  void observe(const PakarUpdate::Lifecycle &l,uint32_t now);
  void configurationChanged();
  void service(bool networkReady,bool timeReady,bool maintenance,const char *hostname,uint32_t now);
  bool pending() const {return notices_.pending();}
  int status() const;
  uint32_t accepted() const;
  uint32_t failed() const;
 private:
  PakarUpdate::NtfyNotice notices_;
#if defined(ARDUINO_ARCH_ESP32)
  struct Job {uint32_t generation;char host[64],topic[65],token[65],message[256];};
  static void worker(void *owner);
  QueueHandle_t queue_=nullptr;
  std::atomic<uint32_t> generation_{0},accepted_{0},failed_{0};
  std::atomic<int> status_{0};
#endif
};
extern NtfyService ntfyService;
