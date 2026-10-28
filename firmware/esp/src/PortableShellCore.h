#pragma once
#include "portable-shell/portable_shell_editor.hpp"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <initializer_list>
#include "ShellNames.h"

// Adapted from portable-shell 67c5dcb8 include/portable_shell.h (PSCommand),
// src/portable_shell.c (command lookup/help) and src/vt100.c. MIT license and
// exact original sources: third_party/portable-shell/upstream/.
// Immutable catalogue replaces the donor's heap-linked registration: no dangling
// callback/name ownership, per-session allocation or duplicate alias registration.
// Execution remains in the existing authenticated, loop-owned command handlers.
namespace PakarShell {
struct Command {
  const char *name, *canonical, *usage;
  bool readOnly;
};
inline constexpr Command commands[] = {
 {"help","help","help : list commands",true},
 {"?","help",nullptr,true},
 {"tui","tui","tui | edit : interactive settings and table",false},
 {"edit","tui",nullptr,false},
 {"properties","properties","properties [KEY] : inspect typed properties",true},
 {"get","get","get KEY : read a property",true},
 {"set","set","set KEY VALUE : validate and save a property",false},
 {"status","status","status : connection and LED state",true},
 {"info","info","info : device information",true},
 {"telemetry","telemetry","telemetry : resource and health snapshot",false},
 {"terminal","terminal","terminal [unicode|color on|off] | terminal size COLS ROWS",true},
 {"game","game","game status | game press 1..9 | game reset",false},
 {"cell","cell","cell ID STATE [EFFECT] : change one cell",false},
 {"cells","cells","cells IDS STATE [EFFECT] : change a cell selection",false},
 {"led-output","led-output","led-output on|off|static|effects",false},
 {"effect","effect","effect NAME : select an available effect",false},
 {"mode","mode","mode NAME : select a supported board mode",false},
 {"brightness","brightness","brightness 1..255",false},
 {"pixel","pixel","pixel INDEX [COUNT] [RRGGBB] : test pixels; INDEX starts at 0",false},
 {"audio","audio","audio CUE : play a supported cue",false},
 {"beep","beep","beep [HZ [MS]] : play a finite tone (1000 Hz, 100 ms)",false},
 {"display","display","display VALUE|stop : set the front panel",false},
 {"network","network","network scan|scan-results|reconnect",false},
 {"clock","clock","clock : UTC clock, Internet observations and probe results",true},
 {"dns","dns","dns HOST : bounded IPv4 DNS query; clock shows result",false},
 {"ping","ping","ping HOST : three ICMP probes; clock shows result",false},
 {"public-ip","public-ip","public-ip : verified HTTPS public IPv4 lookup",false},
 {"http-date","http-date","http-date : check HTTP Date over verified HTTPS",false},
 {"firmware","firmware","firmware status|fetch|apply|cancel ...",false},
 {"device","device","device identity | device rename HOST | device label NAME",false},
 {"reset","reset","reset : reset the current game",false},
 {"clear","clear","clear | cls : clear terminal",false},
 {"cls","clear",nullptr,false},
 {"echo","echo","echo TEXT",false},
 {"hex","hex","hex NUMBER",false},
 {"upper","upper","upper | noupper : echo case",false},
 {"noupper","noupper","noupper : preserve echo case",false},
 {"color","color","color on|off : session ANSI color",false},
 {"watch","watch","watch on|off : live state events",false},
 {"history","history","history : eight non-sensitive commands",true},
 {"exit","exit","exit | quit : close this session",false},
 {"quit","exit",nullptr,false}
};
inline constexpr size_t commandCount = sizeof(commands)/sizeof(commands[0]);
inline const Command* findCommand(const char* name) {
  if (!name) return nullptr;
  for (const auto& command : commands)
    if (sameName(command.name,name)) return &command;
  return nullptr;
}
inline bool commandAvailable(const Command& command,bool firstWins){
  if(sameName(command.canonical,"game"))return firstWins;
  if(sameName(command.canonical,"cell")||sameName(command.canonical,"cells"))return !firstWins;
  return true;
}

inline const char* commandSection(const Command& c){
  const char* n=c.canonical;
  if(sameName(n,"game")||sameName(n,"reset")||sameName(n,"mode"))return "Game";
  if(sameName(n,"cell")||sameName(n,"cells")||sameName(n,"pixel")||sameName(n,"effect")||sameName(n,"brightness")||sameName(n,"led-output"))return "Lighting";
  if(sameName(n,"audio")||sameName(n,"beep")||sameName(n,"display"))return "Sound & display";
  if(sameName(n,"network")||sameName(n,"firmware")||sameName(n,"device")||sameName(n,"info")||sameName(n,"telemetry")||sameName(n,"status"))return "Device";
  return "Workspace";
}
// Adapted portable-shell command metadata/help iteration, grouped and aligned.
// Width counts visible ASCII syntax, never SGR/UTF-8 byte lengths. Long syntax
// or narrow terminals gets its description on a separate wrapped line.
template<class Write> void renderHelp(Write write,bool firstWins,bool ansi,bool unicode,unsigned columns){
  auto out=[&](const char* text){write(reinterpret_cast<const uint8_t*>(text),strlen(text));};
  auto color=[&](const char* text){if(ansi)out(text);};
  const unsigned width=columns<40?40:columns>100?100:columns;
  auto wrapped=[&](const char* text,size_t length,unsigned column,unsigned indent){
    for(size_t at=0;at<length;){
      size_t end=at;while(end<length&&text[end]!=' ')++end;
      const size_t word=end-at;
      if(column+word>=width&&column>indent){out("\r\n");for(unsigned i=0;i<indent;++i)out(" ");column=indent;}
      for(size_t i=at;i<end;++i){
        if(column>=width-1){out("\r\n");for(unsigned n=0;n<indent;++n)out(" ");column=indent;}
        write(reinterpret_cast<const uint8_t*>(text+i),1);++column;
      }
      if(end<length){out(" ");++column;++end;}at=end;
    }
  };
  out("\r\n");color("\x1b[1;32m");out("Usage");color("\x1b[0m");out("\r\n  COMMAND [arguments]\r\n");
  for(const char* section:{"Device","Game","Lighting","Sound & display","Workspace"}){
    out("\r\n");color("\x1b[1;32m");out(unicode?"\xe2\x94\x80 ":"- ");out(section);color("\x1b[0m");out("\r\n");
    for(const auto& item:commands){
      if(!item.usage||!commandAvailable(item,firstWins)||strcmp(commandSection(item),section))continue;
      const char* split=strstr(item.usage," : ");const size_t length=split?size_t(split-item.usage):strlen(item.usage);
      out("  ");color("\x1b[36m");wrapped(item.usage,length,2,4);color("\x1b[0m");
      if(split){
        if(length<22&&width>=72){for(size_t i=length;i<24;++i)out(" ");}
        else out("\r\n    ");
        color("\x1b[90m");wrapped(split+3,strlen(split+3),length<22&&width>=72?26:4,4);color("\x1b[0m");
      }
      out("\r\n");
    }
  }
  out("\r\n");color("\x1b[90m");
  out(unicode?"  \xe2\x86\x91\xe2\x86\x93 history  \xe2\x86\x92/End accept\r\n":"  Up/Down history  Right/End accept\r\n");
  out("  Tab complete\r\n");color("\x1b[0m");
}

// Donor VT100 emitters retain callback-based transport independence. Unlike the
// original, invalid/unbounded movement and color codes cannot escape to a terminal.
using PortableShell::Terminal;
} // namespace PakarShell
