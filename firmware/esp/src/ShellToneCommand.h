#pragma once
#include "ShellWire.h"
namespace PakarShell {
struct ToneCommand {uint16_t frequency=1000,duration=100;};
// PU850's Request_Beep(args[0]) used an unparsed ASCII byte. Retain the useful
// finite-tone command, but validate human numeric units before the shared AVR
// command owner handles ACK/readiness/maintenance. No delays or direct GPIO.
inline bool toneCommand(const char* args,ToneCommand& tone){
  tone=ToneCommand{};if(!args||!*args)return true;
  const char* split=strchr(args,' ');char frequency[6]{};
  const size_t length=split?size_t(split-args):strlen(args);
  if(!length||length>=sizeof(frequency))return false;
  memcpy(frequency,args,length);uint32_t hz=0,ms=100;
  if(!unsignedNumber(frequency,12000,hz)||hz<20)return false;
  if(split&&(!unsignedNumber(split+1,5000,ms)||!ms))return false;
  tone.frequency=hz;tone.duration=ms;return true;
}
}
