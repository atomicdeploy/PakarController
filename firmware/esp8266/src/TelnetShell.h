#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Adapted from atomicdeploy/pu850-esp 4311040 LocalLib/Telnet.cpp and
// Shell.cpp: the IAC/subnegotiation state machine, NVT CR coalescing,
// insert/shift editing, CSI/SS3 cursor keys and prompt redraw. Product command
// callbacks and credentials are deliberately outside this transport/editor.
namespace PakarShell {

class TelnetParser {
 public:
  void reset() { state_ = Data; negotiation_ = 0; echo_ = sga_ = peerSga_ = false; }
  template <typename Write> void greet(Write write) {
    reset();
    send(write, Will, Echo); send(write, Will, Sga); send(write, Do, Sga);
    echo_ = sga_ = peerSga_ = true;
  }
  template <typename Write, typename Input>
  void feed(uint8_t ch, Write write, Input input) {
    switch (state_) {
      case Data:
        if (ch == Iac) state_ = Command; else input(ch);
        break;
      case Command:
        if (ch == Iac) { state_ = Data; input(ch); }
        else if (ch == Do || ch == Dont || ch == Will || ch == Wont) {
          negotiation_ = ch; state_ = Option;
        } else if (ch == Sb) state_ = Sub;
        else state_ = Data; // Ignore NOP, GA and other simple commands.
        break;
      case Option: negotiate(ch, write); state_ = Data; break;
      case Sub: if (ch == Iac) state_ = SubIac; break;
      case SubIac: state_ = ch == Se ? Data : Sub; break;
    }
  }
 private:
  enum State : uint8_t { Data, Command, Option, Sub, SubIac };
  enum : uint8_t { Se = 240, Sb = 250, Will = 251, Wont = 252,
                   Do = 253, Dont = 254, Iac = 255, Echo = 1, Sga = 3 };
  template <typename Write> static void send(Write write, uint8_t command, uint8_t option) {
    const uint8_t bytes[] = {Iac, command, option}; write(bytes, sizeof(bytes));
  }
  template <typename Write> void negotiate(uint8_t option, Write write) {
    if (negotiation_ == Do || negotiation_ == Dont) {
      bool* enabled = option == Echo ? &echo_ : option == Sga ? &sga_ : nullptr;
      if (!enabled) { if (negotiation_ == Do) send(write, Wont, option); return; }
      const bool accept = negotiation_ == Do;
      if (*enabled != accept) { send(write, accept ? Will : Wont, option); *enabled = accept; }
    } else if (negotiation_ == Will || negotiation_ == Wont) {
      if (option != Sga) { if (negotiation_ == Will) send(write, Dont, option); return; }
      const bool accept = negotiation_ == Will;
      if (peerSga_ != accept) { send(write, accept ? Do : Dont, option); peerSga_ = accept; }
    }
  }
  State state_ = Data;
  uint8_t negotiation_ = 0;
  bool echo_ = false, sga_ = false, peerSga_ = false;
};

class LineEditor {
 public:
  // Full bounded HTTPS endpoint (383), SHA-256 and target fit without truncation.
  static constexpr size_t kCapacity = 512;
  enum Event : uint8_t { None, Submit, Cancel, TooLong };
  const char* line() const { return command_; }
  size_t cursor() const { return cursor_; }
  void reset() { clear(); crPending_ = false; echo_ = false; }
  void clear() {
    // Scrub password bytes after authentication, disconnect or cancellation.
    volatile char* p = command_;
    for (size_t i = 0; i < sizeof(command_); ++i) p[i] = 0;
    cursor_ = 0; escape_ = attribute_ = 0; overflow_ = false;
  }
  void setEcho(bool enabled) { echo_ = enabled; }
  template <typename Write> Event feed(uint8_t ch, Write write) {
    if (crPending_ && (ch == '\n' || ch == 0)) { crPending_ = false; return None; }
    if (ch == '\r' || ch == '\n') {
      crPending_ = ch == '\r'; emit(write, "\r\n");
      return overflow_ ? TooLong : Submit;
    }
    crPending_ = false;
    if (ch == 3) { clear(); emit(write, "^C\r\n"); return Cancel; }
    if (overflow_) return None; // Never execute a silently truncated command.
    if (escape_) { escape(ch, write); return None; }
    if (ch == 27) { escape_ = 1; return None; }
    if (ch == 12) {
      if (echo_) { emit(write, "\x1b[H\x1b[2J"); redraw(write); }
      return None;
    }
    if (ch == 8 || ch == 127) {
      if (cursor_ == 0) bell(write);
      else { --cursor_; shiftLeft(); if (echo_) { emit(write, "\x1b[D"); fixCursor(write, true); } }
      return None;
    }
    if (ch < 32 || ch >= 127) { bell(write); return None; }
    if (strlen(command_) >= kCapacity) { overflow_ = true; bell(write); return None; }
    // Donor ShellShiftRight/ShellShiftLeft, with a fixed extra NUL slot.
    for (size_t i = kCapacity; i > cursor_; --i) command_[i] = command_[i - 1];
    command_[cursor_++] = static_cast<char>(ch);
    if (echo_) { const char text[] = {static_cast<char>(ch), 0}; emit(write, text); fixCursor(write, false); }
    return None;
  }
 private:
  template <typename Write> static void emit(Write write, const char* text) {
    write(reinterpret_cast<const uint8_t*>(text), strlen(text));
  }
  template <typename Write> void bell(Write write) { if (echo_) emit(write, "\a"); }
  void shiftLeft() { for (size_t i = cursor_; i < kCapacity; ++i) command_[i] = command_[i + 1]; }
  template <typename Write> void fixCursor(Write write, bool clearAfter) {
    const size_t length = strlen(command_);
    if (cursor_ != length) {
      emit(write, "\x1b[s"); emit(write, command_ + cursor_);
      emit(write, "\x1b[K\x1b[u");
    } else if (clearAfter) emit(write, "\x1b[K");
  }
  template <typename Write> void move(uint8_t direction, Write write) {
    const size_t length = strlen(command_);
    if (direction == 'D') {
      if (cursor_ == 0) { bell(write); return; }
      --cursor_; if (echo_) emit(write, "\x1b[D");
    } else if (direction == 'C') {
      if (cursor_ == length) { bell(write); return; }
      ++cursor_; if (echo_) emit(write, "\x1b[C");
    } else if (direction == 'H' || direction == 'F') {
      const size_t target = direction == 'H' ? 0 : length;
      while (cursor_ != target) move(cursor_ > target ? 'D' : 'C', write);
    } else bell(write);
  }
  template <typename Write> void escape(uint8_t ch, Write write) {
    if (escape_ == 1) {
      if (ch == '[' || ch == 'O') { escape_ = 2; return; }
      escape_ = 0; move(ch, write); return;
    }
    if (escape_ == 2 && ch >= '0' && ch <= '9') { attribute_ = ch; escape_ = 3; return; }
    if (escape_ == 3) {
      if (ch == '~') {
        if (attribute_ == '3') {
          if (cursor_ < strlen(command_)) { shiftLeft(); if (echo_) fixCursor(write, true); }
          else bell(write);
        } else if (attribute_ == '1' || attribute_ == '4') move(attribute_ == '1' ? 'H' : 'F', write);
        else if (attribute_ != '2') bell(write);
      } else bell(write);
    } else move(ch, write);
    escape_ = attribute_ = 0;
  }
  template <typename Write> void redraw(Write write) {
    emit(write, "pakar> "); emit(write, command_);
    for (size_t i = cursor_; i < strlen(command_); ++i) emit(write, "\x1b[D");
  }
  char command_[kCapacity + 1] = {};
  size_t cursor_ = 0;
  uint8_t escape_ = 0, attribute_ = 0;
  bool echo_ = false, crPending_ = false, overflow_ = false;
};
} // namespace PakarShell
