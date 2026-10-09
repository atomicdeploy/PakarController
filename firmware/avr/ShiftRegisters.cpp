#include "ShiftRegisters.h"

#include "BoardPins.h"

ShiftRegisters shiftRegisters;

void ShiftRegisters::begin() {
  pinMode(BoardPins::ShiftDataIn, INPUT_PULLUP);
  pinMode(BoardPins::ShiftDataOut, OUTPUT);
  pinMode(BoardPins::ShiftClock, OUTPUT);
  pinMode(BoardPins::ShiftInputLoad, OUTPUT);
  pinMode(BoardPins::ShiftOutputLatch, OUTPUT);
  pinMode(BoardPins::ShiftMasterReset, OUTPUT);
  pinMode(BoardPins::ShiftClockEnable, OUTPUT);
  pinMode(BoardPins::ShiftOutputEnable, INPUT_PULLUP);

  digitalWrite(BoardPins::ShiftClock, LOW);
  digitalWrite(BoardPins::ShiftInputLoad, HIGH);
  digitalWrite(BoardPins::ShiftMasterReset, HIGH);
  digitalWrite(BoardPins::ShiftClockEnable, HIGH);
  allOutputsOff();
  service();

  pinMode(BoardPins::ShiftOutputEnable, OUTPUT);
  digitalWrite(BoardPins::ShiftOutputEnable, LOW);
}

void ShiftRegisters::service() {
  digitalWrite(BoardPins::ShiftClock, LOW);
  digitalWrite(BoardPins::ShiftOutputLatch, LOW);
  shiftOut(BoardPins::ShiftDataOut, BoardPins::ShiftClock, LSBFIRST, output_);
  digitalWrite(BoardPins::ShiftOutputLatch, HIGH);

  digitalWrite(BoardPins::ShiftClockEnable, HIGH);
  digitalWrite(BoardPins::ShiftInputLoad, LOW);
  delayMicroseconds(5);
  digitalWrite(BoardPins::ShiftInputLoad, HIGH);
  digitalWrite(BoardPins::ShiftClock, HIGH);
  digitalWrite(BoardPins::ShiftClockEnable, LOW);
  input_ = shiftIn(BoardPins::ShiftDataIn, BoardPins::ShiftClock, MSBFIRST);
  digitalWrite(BoardPins::ShiftClockEnable, HIGH);
}
