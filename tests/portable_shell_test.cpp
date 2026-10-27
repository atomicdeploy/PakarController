// Adapted donor test_shell.c scenarios: independent contexts, command lookup,
// built-ins, editing and callback output. No network or production board access.
#include "TelnetShell.h"
#include <cassert>
#include <string>
#include <iostream>
int main(){
  using namespace PakarShell;
  std::string output;
  auto write=[&](const uint8_t* p,size_t n){output.append(reinterpret_cast<const char*>(p),n);};
  for(size_t i=0;i<commandCount;++i){
    assert(findCommand(commands[i].name)==&commands[i]);
    const auto* canonical=findCommand(commands[i].canonical);
    assert(canonical && canonical->usage && canonical->readOnly==commands[i].readOnly);
    for(size_t j=0;j<i;++j)assert(strcmp(commands[i].name,commands[j].name));
  }
  assert(!findCommand(nullptr)&&!findCommand("unknown")&&!findCommand("setx"));
  assert(std::string(findCommand("edit")->canonical)=="tui");
  assert(std::string(findCommand("cls")->canonical)=="clear");
  assert(!findCommand("set")->readOnly&&!findCommand("reset")->readOnly);
  Terminal::clear(write);Terminal::clearLine(write);Terminal::cursorLeft(write,12);
  Terminal::color(write,32,40);Terminal::reset(write);
  assert(output=="\x1b[H\x1b[2J\r\x1b[2K\x1b[12D\x1b[32;40m\x1b[0m");
  output.clear();Terminal::cursorLeft(write,513);Terminal::color(write,123,40);Terminal::color(write,32,99);Terminal::reset(write,false);
  assert(output.empty());Terminal::clear(write,false);Terminal::cursorLeft(write,2,false);assert(output=="\r\n\b\b");
  LineEditor a,b;
  auto feed=[&](LineEditor& editor,const std::string& text){for(unsigned char c:text)editor.feed(c,write);};
  auto candidates=[](size_t i){return commands[i].name;};
  feed(a,"sta");output.clear();a.complete(0,commandCount,candidates,write);
  assert(std::string(a.line())=="sta"&&output.empty()); // hidden password
  assert(a.feed('\t',write)==LineEditor::None);
  a.clear();a.setEcho(true);feed(a,"sta");output.clear();
  assert(a.feed('\t',write)==LineEditor::Complete);
  a.complete(0,commandCount,candidates,write);
  assert(std::string(a.line())=="status "&&a.historySize()==0&&b.line()[0]==0);
  a.clear();feed(a,"cell");a.complete(0,commandCount,candidates,write);
  assert(std::string(a.line())=="cell"); // ambiguous cell/cells: do not execute
  assert(output.find("cells")!=std::string::npos);
  a.clear();feed(a,"co");a.complete(0,commandCount,candidates,write);assert(std::string(a.line())=="color ");
  a.clear();feed(a,"sta\x1b[D");a.complete(0,commandCount,candidates,write);assert(std::string(a.line())=="sta"&&a.cursor()==2);
  // Fragmented escape sequences still belong to the editor, never completion.
  a.clear();feed(a,"sta\x1b[");a.complete(0,commandCount,candidates,write);assert(std::string(a.line())=="sta");
  a.feed('D',write);assert(a.cursor()==2);
  const char* props[]={"network.mode","network.ssid",nullptr};
  auto property=[&](size_t i){return props[i];};
  a.clear();feed(a,"set net");a.complete(4,3,property,write);assert(std::string(a.line())=="set network.");
  feed(a,"mo");a.complete(4,3,property,write);assert(std::string(a.line())=="set network.mode ");
  a.clear();feed(a,"unknown");output.clear();a.complete(0,commandCount,candidates,write);assert(output=="\a");
  a.clear();feed(a,std::string(LineEditor::kCapacity,'x'));
  const auto full=std::string(a.line());a.complete(0,1,[&](size_t){return full.c_str();},write);
  assert(std::string(a.line())==full&&a.cursor()==LineEditor::kCapacity);
  // All sessions share descriptors, not mutable history/input or IO buffers.
  b.setEcho(true);feed(b,"edi");b.complete(0,commandCount,candidates,write);
  assert(std::string(b.line())=="edit "&&std::string(a.line())==full);
  std::cout<<"Portable-shell catalogue, VT100 and bounded completion passed.\n";
}
