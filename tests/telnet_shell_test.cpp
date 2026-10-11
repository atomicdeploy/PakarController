#include "TelnetShell.h"
#include <assert.h>
#include <iostream>
#include <string>
#include <vector>

int main() {
  using namespace PakarShell;
  std::vector<uint8_t> output, input;
  auto write = [&](const uint8_t* bytes, size_t length) { output.insert(output.end(), bytes, bytes + length); };
  auto receive = [&](uint8_t byte) { input.push_back(byte); };
  TelnetParser parser;
  parser.greet(write);
  assert((output == std::vector<uint8_t>{255,251,1,255,251,3,255,253,3}));
  output.clear();
  for (uint8_t byte : {255,253,1,255,253,3,255,251,3}) parser.feed(byte, write, receive);
  assert(output.empty()); // No negotiation loops on greeting acknowledgements.
  for (uint8_t byte : {255,253,42,255,251,24}) parser.feed(byte, write, receive);
  assert((output == std::vector<uint8_t>{255,252,42,255,254,24}));
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
  std::cout << "PU850-derived Telnet negotiation and bounded shell editing passed.\n";
}
