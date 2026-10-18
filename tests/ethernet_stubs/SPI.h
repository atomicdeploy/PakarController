#pragma once
#include "Arduino.h"
#define FSPI 0
#define HSPI 1
struct SPIClass {
  explicit SPIClass(int host) : bus(host) {}
  int bus;
  inline static bool beginResult = true;
  inline static unsigned starts = 0, stops = 0;
  inline static int usedBus = -1, usedSck = -1, usedMiso = -1, usedMosi = -1, usedSs = -2;
  bool begin(int sck, int miso, int mosi, int ss) {
    ++starts; usedBus = bus; usedSck = sck; usedMiso = miso; usedMosi = mosi; usedSs = ss;
    return beginResult;
  }
  void end() { ++stops; }
};
