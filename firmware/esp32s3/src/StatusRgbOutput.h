// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
// SPDX-License-Identifier: Apache-2.0
// Adapted from Arduino-ESP32 3.3.12 cores/esp32/esp32-hal-rgb-led.c.
// Preserve its WS2812/GRB encoding, but expose actual admission/completion errors
// and a finite wait instead of a void writer with RMT_WAIT_FOR_EVER.
#pragma once
#include <esp32-hal-rmt.h>
#include "StatusRgb.h"
namespace PakarS3 {
inline bool writeStatusPixel(int pin,uint8_t red,uint8_t green,uint8_t blue) {
  if(!StatusRgb::supportedPin(pin))return false;
  if(!rmtInit(pin,RMT_TX_MODE,RMT_MEM_NUM_BLOCKS_1,10000000))return false;
  rmt_data_t symbols[24]{};const uint8_t channels[]={green,red,blue};
  unsigned index=0;
  for(uint8_t channel:channels)for(uint8_t mask=0x80;mask;mask>>=1) {
    auto &symbol=symbols[index++];const bool one=(channel&mask)!=0;
    symbol.level0=1;symbol.duration0=one?8:4;
    symbol.level1=0;symbol.duration1=one?4:8;
  }
  return rmtWrite(pin,symbols,RMT_SYMBOLS_OF(symbols),10);
}
}
