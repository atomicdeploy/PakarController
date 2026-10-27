#pragma once
#include <Arduino.h>
#include "HostDemand.h"
#ifndef PAKAR_S3_HOST_TRANSPORT
#define PAKAR_S3_HOST_TRANSPORT 0
#endif
#if PAKAR_S3_HOST_TRANSPORT < 0 || PAKAR_S3_HOST_TRANSPORT > 2
#error "PAKAR_S3_HOST_TRANSPORT must be UART0(0), hardware CDC(1), or TinyUSB CDC(2)"
#endif
#if defined(ARDUINO_ARCH_ESP32) && PAKAR_S3_HOST_TRANSPORT == 2
#include <USB.h>
#include <USBCDC.h>
#include <esp32-hal-tinyusb.h>
#if ARDUINO_USB_MODE || ARDUINO_USB_CDC_ON_BOOT
#error "Owned TinyUSB CDC requires USBMode=default and CDCOnBoot=default"
#endif
#endif

namespace PakarHost {
#if defined(ARDUINO_ARCH_ESP32) && PAKAR_S3_HOST_TRANSPORT == 2
inline USBCDC &port(){static USBCDC value(0);return value;}
inline const char *transport(){return "otg";}
inline bool connected(){return tud_cdc_n_connected(0);}
inline void begin(){
  auto &serial=port();
  serial.enableReboot(false); // BEFORE CDC/USB starts: no DTR or1200-baud reboot.
  USB.productName("PakarController"); // Explicit owned descriptor, SDK VID/PID untouched.
  serial.setRxBufferSize(1024);serial.setTxTimeoutMs(0);serial.begin(115200);
  USB.begin();
}
#elif defined(ARDUINO_ARCH_ESP32) && PAKAR_S3_HOST_TRANSPORT == 1
inline auto &port(){return Serial;}
inline const char *transport(){return "hwcdc";}
inline bool connected(){return bool(Serial);}
inline void begin(){Serial.setRxBufferSize(1024);Serial.setTxTimeoutMs(0);Serial.begin(115200);}
#else
#if defined(ARDUINO_ARCH_ESP32)
inline auto &port(){return Serial0;}
#else
inline auto &port(){return Serial;}
#endif
inline const char *transport(){return "uart0";}
inline bool connected(){return true;} // Bridge DTR/VBUS is not visible at UART0.
inline void begin(){port().begin(115200);}
#endif
inline bool native(){return PAKAR_S3_HOST_TRANSPORT!=0;}
}
