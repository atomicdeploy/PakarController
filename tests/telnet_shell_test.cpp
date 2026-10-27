#include "TelnetShell.h"
#include <assert.h>
#include <iostream>
#include <new>
#include <string>
#include <vector>

int main() {
  using namespace PakarShell;
  std::vector<uint8_t> output, input;
  auto write = [&](const uint8_t* bytes, size_t length) { output.insert(output.end(), bytes, bytes + length); };
  auto receive = [&](uint8_t byte) { input.push_back(byte); };
  TelnetParser parser;
  parser.greet(write);
  assert((output == std::vector<uint8_t>{255,251,1,255,251,3,255,253,3,255,253,31,255,253,24,255,251,0,255,253,0}));
  output.clear();
  for (uint8_t byte : {255,253,1,255,253,3,255,251,3,255,253,0,255,251,0}) parser.feed(byte, write, receive);
  assert(output.empty()); // No negotiation loops on greeting acknowledgements.
  for (uint8_t byte : {255,253,42,255,251,24}) parser.feed(byte, write, receive);
  assert((output == std::vector<uint8_t>{255,252,42,255,250,24,1,255,240}));
  output.clear();
  for(uint8_t byte:{255,251,24,255,251,31,255,251,31})parser.feed(byte,write,receive);
  assert(output.empty());
  for(uint8_t byte:{255,250,31,0,120,0,40,255,240})parser.feed(byte,write,receive);
  assert(parser.columns()==120&&parser.rows()==40);
  for(uint8_t byte:{255,250,24,0,100,117,109,98,255,240})parser.feed(byte,write,receive);
  assert(!parser.ansi()&&std::string(parser.terminal())=="dumb");
  // Escaped IAC is data inside NAWS; zero dimensions keep their prior value.
  for(uint8_t byte:{255,250,31,0,255,255,0,0,255,240})parser.feed(byte,write,receive);
  assert(parser.columns()==240&&parser.rows()==40);
  for(uint8_t byte:{255,250,24,0,27,91,50,74,255,240})parser.feed(byte,write,receive);
  assert(std::string(parser.terminal())=="dumb");
  for(uint8_t byte:{255,250,24,0})parser.feed(byte,write,receive);
  for(unsigned i=0;i<200;++i)parser.feed('x',write,receive);
  parser.feed(255,write,receive);parser.feed(240,write,receive);
  assert(std::string(parser.terminal())=="dumb");
  assert(!parser.size(1,1)&&parser.size(80,24));
  for (uint8_t byte : {255,250,24,65,255,255,66,255,240,115,255,255,255,241,116}) parser.feed(byte, write, receive);
  assert((input == std::vector<uint8_t>{115,255,116}));
  parser.feed(255, write, receive); parser.reset(); parser.feed('X', write, receive);
  assert(input.back() == 'X');

  LineEditor editor;
  auto feed = [&](const std::string& bytes) { for (unsigned char byte : bytes) editor.feed(byte, write); };
  output.clear(); feed("secret\bT");
  assert(std::string(editor.line()) == "secreT" && output.empty()); // Password not echoed.
  assert(editor.feed('\r', write) == LineEditor::Submit);
  editor.clear(); editor.setEcho(true);
  assert(editor.feed('\n', write) == LineEditor::None); // Split CR LF across authentication.
  feed("sttus\x1b[D\x1b[D\x1b[D" "a\x1b[F");
  assert(std::string(editor.line()) == "status" && editor.cursor() == 6);
  feed("\x1b[H\x1b[C\x1b[3~");
  assert(std::string(editor.line()) == "satus");
  feed("\x1b[1~\x1b[4~\x1bOD\x7f");
  assert(std::string(editor.line()) == "sats" && editor.cursor() == 3);
  const auto cursor = editor.cursor(); const auto line = std::string(editor.line());
  feed("\x0c");
  assert(std::string(editor.line()) == line && editor.cursor() == cursor);
  assert(editor.feed(3, write) == LineEditor::Cancel && std::string(editor.line()).empty());
  feed(std::string(LineEditor::kCapacity, 'a'));
  assert(strlen(editor.line()) == LineEditor::kCapacity);
  editor.feed('b', write); editor.feed(8, write);
  assert(editor.feed('\n', write) == LineEditor::TooLong); // Never execute truncated input.
  editor.clear(); feed("ok");
  assert(editor.feed('\r', write) == LineEditor::Submit);
  editor.clear(); assert(editor.feed(0, write) == LineEditor::None); // CR NUL.
  feed("z"); editor.reset(); assert(editor.line()[0] == 0 && editor.cursor() == 0);
  LineEditor other; other.feed('q', write);
  assert(std::string(other.line()) == "q" && std::string(editor.line()).empty());

  // Eight full-length commands, recalled by COPY, never an editable pointer
  // into stored history. A partial input + exact cursor survive Up/Down.
  editor.reset();editor.setEcho(true);output.clear();
  for(unsigned i=0;i<10;++i){editor.clear();feed("echo "+std::to_string(i)+std::string(506,'x'));assert(editor.remember(true));}
  assert(editor.historySize()==8);
  output.clear();editor.printHistory(write);const std::string listed(output.begin(),output.end());
  assert(listed.find("1  echo 2")==0&&listed.find("8  echo 9")!=std::string::npos);
  assert(listed.size()<=8*(LineEditor::kCapacity+5));
  editor.clear();feed("draft\x1b[D\x1b[D");
  const auto draftCursor=editor.cursor();
  feed("\x1b[A");assert(std::string(editor.line()).find("echo 9")==0);
  feed("\x1b[H\x04" "Q");assert(editor.line()[0]=='Q');
  feed("\x1b[B");assert(std::string(editor.line())=="draft"&&editor.cursor()==draftCursor);
  feed("\x1b[A");assert(std::string(editor.line()).find("echo 9")==0);
  for(unsigned i=0;i<7;++i)feed("\x1b[A");
  assert(std::string(editor.line()).find("echo 2")==0);
  feed("\x1b[A");assert(std::string(editor.line()).find("echo 2")==0);
  editor.clear();feed("status");assert(editor.remember(true));assert(!editor.remember(true));

  for(const auto& secret: {"set password private", "SET MQTT_TOKEN private", "auth off", "login david", "fetch https://example/a?key=private", "fetch https://user:private@example/a"}) {
    editor.clear();feed(secret);const auto count=editor.historySize();assert(!editor.remember(true));assert(editor.historySize()==count);
  }
  editor.clear();editor.setEcho(false);feed("private password input");assert(!editor.remember(true));
  editor.clear();editor.setEcho(true);feed("set wifi private");assert(!editor.remember(false));
  editor.reset();assert(editor.historySize()==0);

  // Emacs controls do not leak hidden input, execute, or corrupt the cursor.
  editor.setEcho(true);feed("one two tail");feed("\x01\x05\x17");
  assert(std::string(editor.line())=="one two "&&editor.cursor()==8);
  feed("\x01\x1b[C\x1b[C\x1b[C\x15");assert(std::string(editor.line())==" two "&&editor.cursor()==0);
  assert(editor.feed(4,write)==LineEditor::None&&std::string(editor.line())=="two ");
  editor.clear();assert(editor.feed(4,write)==LineEditor::Close);
  editor.setPrompt("ledboard\x1b]52;c;private\a",true);feed("status\x1b[D\x1b[D");
  const auto beforeEvent=std::string(editor.line());const auto beforeCursor=editor.cursor();
  output.clear();editor.redrawEvent("online\x1b[2J\x1b]52;c;private\a\r\nOK",write);
  const std::string visible(output.begin(),output.end());
  assert(visible.find("private")==std::string::npos);
  assert(visible.find("ledboard")!=std::string::npos);
  assert(visible.find("online OK")!=std::string::npos || visible.find("online  OK")!=std::string::npos);
  assert(std::string(editor.line())==beforeEvent&&editor.cursor()==beforeCursor);
  editor.setPrompt("plain",false);output.clear();editor.prompt(write);
  assert(std::string(output.begin(),output.end())=="plain> ");
  char safe[16];terminalText(safe,sizeof(safe),"a\x1bPprivate\x1b\\b\x1b[31mc");assert(std::string(safe)=="abc");

  // Inspect the caller-owned backing bytes after an explicit placement
  // destructor: no live-object access, network, or allocator reuse assumptions.
  alignas(LineEditor) unsigned char storage[sizeof(LineEditor)]{};
  auto *dying=new(storage) LineEditor;
  dying->setEcho(true);for(unsigned char ch:std::string("echo durable-marker"))dying->feed(ch,write);
  assert(dying->remember(true));dying->clear();
  for(unsigned char ch:std::string("draft-marker"))dying->feed(ch,write);
  for(unsigned char ch:std::string("\x1b[A"))dying->feed(ch,write); // Separate draft/history copies.
  dying->clear();dying->setEcho(false);
  for(unsigned char ch:std::string("unsubmitted-secret-marker"))dying->feed(ch,write);
  const size_t beforeDestruction=output.size();dying->~LineEditor();
  assert(output.size()==beforeDestruction);
  const std::string erased(reinterpret_cast<const char*>(storage),sizeof(storage));
  for(const auto* marker:{"durable-marker","draft-marker","unsubmitted-secret-marker"})assert(erased.find(marker)==std::string::npos);
  std::cout << "PU850-derived Telnet negotiation and bounded shell editing passed.\n";
}
