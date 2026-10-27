#include <Arduino.h>
#include <cassert>
#define _BV(bit) (1U << (bit))
constexpr uint8_t LSBFIRST=0, MSBFIRST=1;
static uint8_t sampled=255;
static uint8_t shifted=0;
inline void shiftOut(uint8_t,uint8_t,uint8_t,uint8_t value) { shifted=value; }
inline uint8_t shiftIn(uint8_t,uint8_t,uint8_t) { return sampled; }
#include "../firmware/avr/ShiftRegisters.cpp"

int main() {
  ShiftRegisters inputs;
  inputs.begin();
  assert(inputs.activeInputs()==0 && inputs.activeOutputs()==0 && shifted==0xff);
  for(uint8_t bit=0;bit<8;++bit) {
    sampled=static_cast<uint8_t>(~(1U<<bit));inputs.service();
    const auto expected=PAKAR_FIRST_WINS_ENABLED?bit:bit<4?3-bit:bit;
    assert(inputs.activeInputs()==(1U<<expected));
  }
  sampled=255;inputs.service();
  for(uint8_t bit=0;bit<8;++bit) {
    inputs.setVirtualInput(bit,true);
    assert(inputs.activeInputs()==(1U<<bit));
    inputs.clearVirtualInputs();
  }
  assert(inputs.activeInputs()==0);
  inputs.setVirtualInput(8,true);assert(inputs.activeInputs()==0);
  inputs.setOutput(2,true);inputs.service();
  assert(inputs.activeOutputs()==(1U<<2) && shifted==static_cast<uint8_t>(~(1U<<2)));
  inputs.setOutput(8,true);assert(inputs.activeOutputs()==(1U<<2));
  inputs.setActiveOutputs(0xa5);inputs.service();
  assert(inputs.activeOutputs()==0xa5 && shifted==0x5a);
  inputs.allOutputsOff();inputs.service();
  assert(inputs.activeOutputs()==0 && shifted==0xff);
}
