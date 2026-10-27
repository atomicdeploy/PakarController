#pragma once
#include <stdint.h>
namespace PakarLed {
// ESP8266 SoftwareSerial must finish its reply before IRQ-masked LED output.
// The S3 buffered hardware UART/independent output worker has no such lease.
// Always pass a freshly sampled clock AFTER bounded UART/upgrade service.
inline bool uartWindowReady(bool buffered,uint32_t now,uint32_t resumeAt) {
  return buffered || static_cast<int32_t>(now-resumeAt)>=0;
}
}
