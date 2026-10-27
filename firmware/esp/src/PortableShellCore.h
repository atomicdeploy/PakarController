#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
 {"cell","cell","cell ID STATE [EFFECT] : change one cell",false},
 {"cells","cells","cells IDS STATE [EFFECT] : change a cell selection",false},
 {"led-output","led-output","led-output on|off|static|effects",false},
 {"effect","effect","effect NAME : select an available effect",false},
 {"mode","mode","mode NAME : select a supported board mode",false},
 {"brightness","brightness","brightness 1..255",false},
 {"pixel","pixel","pixel INDEX[0..479] [1|6] [RRGGBB] | pixel off",false},
 {"audio","audio","audio CUE : play a supported cue",false},
 {"display","display","display VALUE|stop | display text once|loop|interval SPEED HOLD PAUSE SCROLL TEXT",false},
 {"network","network","network scan|scan-results|reconnect",false},
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
    if (strcmp(command.name,name)==0) return &command;
  return nullptr;
}

// Donor VT100 emitters retain callback-based transport independence. Unlike the
// original, invalid/unbounded movement and color codes cannot escape to a terminal.
struct Terminal {
  template<class Write> static void text(Write write,const char* value) {
    write(reinterpret_cast<const uint8_t*>(value),strlen(value));
  }
  template<class Write> static void clear(Write write,bool ansi=true) {
    text(write,ansi?"\x1b[H\x1b[2J":"\r\n");
  }
  template<class Write> static void clearLine(Write write,bool ansi=true) {
    text(write,ansi?"\r\x1b[2K":"\r");
  }
  template<class Write> static void cursorLeft(Write write,size_t count,bool ansi=true) {
    if (!count || count>512) return;
    if (!ansi) { while(count--)text(write,"\b"); return; }
    char buf[20];snprintf(buf,sizeof(buf),"\x1b[%uD",static_cast<unsigned>(count));text(write,buf);
  }
  template<class Write> static void color(Write write,int fg,int bg,bool ansi=true) {
    if (!ansi || fg<30 || fg>37 || bg<40 || bg>47) return;
    char buf[32];snprintf(buf,sizeof(buf),"\x1b[%d;%dm",fg,bg);text(write,buf);
  }
  template<class Write> static void reset(Write write,bool ansi=true) {
    if(ansi)text(write,"\x1b[0m");
  }
};
} // namespace PakarShell
