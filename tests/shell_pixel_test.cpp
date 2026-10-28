#include "ShellPixelCommand.h"
#include "../libraries/BoardCore/src/PixelGroupLayout.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main(){
  PakarShell::PixelCommand p;
  BoardCore::PixelGroupLayout<80,6,480> table;
  assert(PakarShell::pixelCommand("0",table,p)&&p.index==0&&p.count==1);
  assert(PakarShell::pixelCommand("479 1 00ff00",table,p)&&p.index==479&&p.rgb==0x00ff00);
  assert(PakarShell::pixelCommand("group 80 FF0000",table,p)&&p.index==474&&p.count==6);
  for(const char* bad:{"480","-1","1 6","0 2","0 0","0 256","0 1 xyzxyz","group 0","group 81","group","off 1","0 1 FFFFFF extra","0\n"})
    assert(!PakarShell::pixelCommand(bad,table,p));
  uint8_t overrides[80]{};overrides[0]=5;overrides[1]=7;overrides[79]=4;
  assert(table.configure(overrides,42));
  assert(PakarShell::pixelCommand("group 2 123456",table,p)&&p.index==5&&p.count==7&&p.revision==42);
  assert(PakarShell::pixelCommand("5 7",table,p)&&p.index==5&&p.count==7);
  assert(!PakarShell::pixelCommand("6 6",table,p));
  assert(!PakarShell::pixelCommand("478",table,p)); // Capacity is not used prefix.
  assert(PakarShell::pixelCommand("off",table,p)&&p.off&&p.revision==42);
  BoardCore::PixelGroupLayout<18,12,216> keys;
  assert(PakarShell::pixelCommand("group 2 0000ff",keys,p)&&p.index==12&&p.count==12&&p.rgb==255);
  assert(PakarShell::pixelCommand("group 18",keys,p)&&p.index==204&&p.count==12);
  assert(!PakarShell::pixelCommand("0 6",keys,p));
  assert(!PakarShell::pixelCommand(nullptr,keys,p));
  puts("Shell pixels use the same live topology, group spans and revision as the API/TUI.");
}
