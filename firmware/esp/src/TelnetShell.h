#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "PortableShellCore.h"

// Adapted from atomicdeploy/pu850-esp 4311040 LocalLib/Telnet.cpp and
// Shell.cpp: the IAC/subnegotiation state machine, NVT CR coalescing,
// insert/shift editing, CSI/SS3 cursor keys and prompt redraw. Product command
// callbacks and credentials are deliberately outside this transport/editor.
namespace PakarShell {

// RayanLamp internal/tui/text.go + terminal.go: external strings never own
// terminal controls. Fixed scan/output bounds replace Go's allocating builders.
// Drop complete CSI/OSC/DCS payloads; only our renderer may emit ANSI escapes.
inline size_t terminalText(char* out, size_t capacity, const char* text,
                           size_t maximum = 512) {
  if (!out || !capacity) return 0;
  size_t used=0, i=0;
  while (text && i<maximum && text[i] && used+1<capacity) {
    const uint8_t ch=static_cast<uint8_t>(text[i++]);
    if (ch==27) {
      if (i>=maximum || !text[i]) break;
      const char kind=text[i++];
      if (kind=='[') {
        while (i<maximum && text[i]) { const uint8_t c=text[i++]; if(c>=0x40&&c<=0x7e) break; }
      } else if (kind==']'||kind=='P'||kind=='X'||kind=='^'||kind=='_') {
        while (i<maximum && text[i]) {
          if(text[i]=='\a'){++i;break;}
          if(text[i]==27&&i+1<maximum&&text[i+1]=='\\'){i+=2;break;}
          ++i;
        }
      } else {
        while(i<maximum && text[i]>=0x20 && text[i]<=0x2f) ++i;
        if(i<maximum && text[i]>=0x30 && text[i]<=0x7e) ++i;
      }
    } else if (ch=='\r'||ch=='\n'||ch=='\t') out[used++]=' ';
    else if (ch>=32 && ch<127) out[used++]=static_cast<char>(ch);
    else if (ch>=127) out[used++]='?';
  }
  out[used]=0;return used;
}

class TelnetParser {
 public:
  void reset() {
    state_=Data;negotiation_=0;echo_=sga_=peerSga_=binary_=peerBinary_=false;
    naws_=type_=nawsPending_=typePending_=false;subSize_=0;subOverflow_=false;
    columns_=80;rows_=24;strcpy(terminal_,"unknown");
  }
  uint16_t columns()const{return columns_;}
  uint16_t rows()const{return rows_;}
  const char* terminal()const{return terminal_;}
  bool ansi()const{return strcmp(terminal_,"dumb")&&strcmp(terminal_,"DUMB");}
  bool size(uint16_t columns,uint16_t rows){
    if(columns<40||columns>240||rows<16||rows>100)return false;
    columns_=columns;rows_=rows;return true;
  }
  template <typename Write> void greet(Write write) {
    reset();
    send(write, Will, Echo); send(write, Will, Sga); send(write, Do, Sga);
    echo_ = sga_ = peerSga_ = true;
    send(write,Do,Naws);send(write,Do,Type);nawsPending_=typePending_=true;
    send(write,Will,Binary);send(write,Do,Binary);binary_=peerBinary_=true;
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
        } else if (ch == Sb) {state_=Sub;subSize_=0;subOverflow_=false;}
        else state_ = Data; // Ignore NOP, GA and other simple commands.
        break;
      case Option: negotiate(ch, write); state_ = Data; break;
      case Sub: if (ch == Iac) state_ = SubIac; else append(ch); break;
      case SubIac:
        if(ch==Se){subnegotiation();state_=Data;}
        else {if(ch==Iac)append(ch);else subOverflow_=true;state_=Sub;}
        break;
    }
  }
 private:
  enum State : uint8_t { Data, Command, Option, Sub, SubIac };
  enum : uint8_t { Se = 240, Sb = 250, Will = 251, Wont = 252,
                   Do = 253, Dont = 254, Iac = 255, Binary=0, Echo = 1, Sga = 3, Type=24, Naws=31 };
  void append(uint8_t ch){if(subSize_<sizeof(sub_))sub_[subSize_++]=ch;else subOverflow_=true;}
  void subnegotiation(){
    if(subOverflow_||!subSize_)return;
    if(sub_[0]==Naws&&naws_&&subSize_==5){
      const uint16_t w=uint16_t(sub_[1])<<8|sub_[2],h=uint16_t(sub_[3])<<8|sub_[4];
      // NAWS zero means unspecified. Retain the last useful dimension; cap
      // layout independently so a hostile terminal cannot enlarge allocations.
      if(w)columns_=w>240?240:w;
      if(h)rows_=h>100?100:h;
    }else if(sub_[0]==Type&&type_&&subSize_>2&&sub_[1]==0&&subSize_<=34){
      for(uint8_t i=2;i<subSize_;++i)if(sub_[i]<32||sub_[i]>126)return;
      memcpy(terminal_,sub_+2,subSize_-2);terminal_[subSize_-2]=0;
    }
  }
  template <typename Write> static void send(Write write, uint8_t command, uint8_t option) {
    const uint8_t bytes[] = {Iac, command, option}; write(bytes, sizeof(bytes));
  }
  template <typename Write> void negotiate(uint8_t option, Write write) {
    if (negotiation_ == Do || negotiation_ == Dont) {
      bool* enabled = option == Echo ? &echo_ : option == Sga ? &sga_ : option==Binary?&binary_:nullptr;
      if (!enabled) { if (negotiation_ == Do) send(write, Wont, option); return; }
      const bool accept = negotiation_ == Do;
      if (*enabled != accept) { send(write, accept ? Will : Wont, option); *enabled = accept; }
    } else if (negotiation_ == Will || negotiation_ == Wont) {
      if(option==Naws||option==Type){
        bool &enabled=option==Naws?naws_:type_,&pending=option==Naws?nawsPending_:typePending_;
        const bool accept=negotiation_==Will;
        if(enabled!=accept){if(!pending)send(write,accept?Do:Dont,option);enabled=accept;
          if(accept&&option==Type){const uint8_t request[]={Iac,Sb,Type,1,Iac,Se};write(request,sizeof(request));}}
        pending=false;return;
      }
      if (option != Sga && option!=Binary) { if (negotiation_ == Will) send(write, Dont, option); return; }
      const bool accept = negotiation_ == Will;
      bool &enabled=option==Binary?peerBinary_:peerSga_;
      if (enabled != accept) { send(write, accept ? Do :Dont, option); enabled=accept; }
    }
  }
  State state_ = Data;
  uint8_t negotiation_ = 0;
  bool echo_ = false, sga_ = false, peerSga_ = false;
  bool binary_=false,peerBinary_=false;
  bool naws_=false,type_=false,nawsPending_=false,typePending_=false,subOverflow_=false;
  uint8_t sub_[64]{},subSize_=0;
  uint16_t columns_=80,rows_=24;char terminal_[33]="unknown";
};

class LineEditor {
 public:
  // Full bounded HTTPS endpoint (383), SHA-256 and target fit without truncation.
  static constexpr size_t kCapacity = 512;
  static constexpr size_t kHistory = 8;
  enum Event : uint8_t { None, Submit, Cancel, TooLong, Close, Complete };
  ~LineEditor() { reset(); } // Disconnect/destruction never leaves partial secrets in freed storage.
  const char* line() const { return command_; }
  size_t cursor() const { return cursor_; }
  size_t historySize() const { return historyCount_; }
  // Completion never executes, records history, or operates on password input.
  // Only an end-of-line token may be replaced; text to the right stays untouched.
  template<class Candidate,class Write>
  void complete(size_t start,size_t count,Candidate candidate,Write write) {
    if(!echo_ || overflow_ || escape_ || cursor_!=strlen(command_) || start>cursor_)return;
    const size_t typed=cursor_-start;
    const char* first=nullptr;size_t matches=0,common=0;
    for(size_t i=0;i<count;++i){
      const char* name=candidate(i);
      if(!name || !namePrefix(name,command_+start,typed))continue;
      if(!matches){first=name;common=strlen(name);}
      else{size_t n=0;while(n<common && name[n] && name[n]==first[n])++n;common=n;}
      ++matches;
    }
    if(!matches){bell(write);return;}
    const size_t extra=matches==1?1:0;
    if(start+common+extra>kCapacity){bell(write);return;}
    if(common>typed || matches==1){
      memcpy(command_+start,first,common);cursor_=start+common;
      if(extra)command_[cursor_++]=' ';
      command_[cursor_]=0;historyOffset_=0;redraw(write);return;
    }
    emit(write,"\r\n");
    for(size_t i=0;i<count;++i){const char* name=candidate(i);
      if(name && namePrefix(name,command_+start,typed)){emit(write,name);emit(write,"  ");}}
    emit(write,"\r\n");redraw(write);
  }
  void reset() { clear(); clearHistory(); crPending_ = false; echo_ = upper_ = false; }
  void clear() {
    // Scrub password bytes after authentication, disconnect or cancellation.
    volatile char* p = command_;
    for (size_t i = 0; i < sizeof(command_); ++i) p[i] = 0;
    scrub(draft_,sizeof(draft_));draftCursor_=historyOffset_=0;
    cursor_ = 0; escape_ = attribute_ = escapeBytes_ = 0; overflow_ = false;
  }
  void setEcho(bool enabled) { echo_ = enabled; }
  void setUpper(bool enabled) { upper_ = enabled; }
  void setPrompt(const char* label, bool ansi=true) {
    terminalText(prompt_,sizeof(prompt_),label,128);ansi_=ansi;
  }
  void clearHistory() {
    scrub(reinterpret_cast<char*>(history_),sizeof(history_));
    scrub(draft_,sizeof(draft_));historyNext_=historyCount_=historyOffset_=draftCursor_=0;
  }
  // Caller must explicitly classify the command before a command parser mutates
  // it. Hidden password input and sensitive argument names are never retained.
  bool remember(bool allowed) {
    if(!allowed || !echo_ || overflow_ || !safeHistory(command_)) return false;
    const size_t latest=(historyNext_+kHistory-1)%kHistory;
    if(historyCount_ && !strcmp(history_[latest],command_)) return false;
    scrub(history_[historyNext_],sizeof(history_[historyNext_]));
    memcpy(history_[historyNext_],command_,strlen(command_)+1);
    historyNext_=(historyNext_+1)%kHistory;if(historyCount_<kHistory)++historyCount_;
    historyOffset_=0;return true;
  }
  template<typename Write> void printHistory(Write write) const {
    if(!echo_)return;
    for(size_t i=0;i<historyCount_;++i){
      const size_t slot=(historyNext_+kHistory-historyCount_+i)%kHistory;
      char number[12],safe[kCapacity+1];
      snprintf(number,sizeof(number),"%u  ",static_cast<unsigned>(i+1));
      terminalText(safe,sizeof(safe),history_[slot],kCapacity);
      emit(write,number);emit(write,safe);emit(write,"\r\n");
    }
  }
  template <typename Write> void prompt(Write write) const {
    if(!echo_)return;
    if(ansi_){emit(write,"\x1b[1;36m");}emit(write,prompt_);emit(write,"> ");
    if(ansi_)emit(write,"\x1b[0m");
  }
  template <typename Write> void redraw(Write write) const {
    if(!echo_)return;
    Terminal::clearLine(write,ansi_);prompt(write);emitInput(write,command_);
    cursorBack(write,strlen(command_)-cursor_);
  }
  template <typename Write> void redrawEvent(const char* text, Write write) const {
    char safe[257];terminalText(safe,sizeof(safe),text);
    if(echo_)emit(write,ansi_?"\r\x1b[2K":"\r\n");
    emit(write,safe);emit(write,"\r\n");if(echo_)redraw(write);
  }
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
    if (ch == '\t') return echo_ ? Complete : None;
    if(ch==1 || ch==5){move(ch==1?'H':'F',write);return None;}
    if(ch==21 || ch==23) {
      size_t target=cursor_;
      if(ch==21)target=0;
      else {while(target && command_[target-1]==' ')--target;while(target && command_[target-1]!=' ')--target;}
      if(target<cursor_){memmove(command_+target,command_+cursor_,strlen(command_+cursor_)+1);cursor_=target;redraw(write);}
      return None;
    }
    if(ch==4) {
      if(command_[0]==0)return Close;
      if(cursor_<strlen(command_)){shiftLeft();if(echo_)fixCursor(write,true);}else bell(write);
      return None;
    }
    if (ch == 12) {
      if (echo_) { Terminal::clear(write,ansi_); redraw(write); }
      return None;
    }
    if (ch == 8 || ch == 127) {
      if (cursor_ == 0) bell(write);
      else { --cursor_; shiftLeft(); if (echo_) { cursorBack(write,1); fixCursor(write, true); } }
      return None;
    }
    if (ch < 32 || ch >= 127) { bell(write); return None; }
    if (strlen(command_) >= kCapacity) { overflow_ = true; bell(write); return None; }
    // Donor ShellShiftRight/ShellShiftLeft, with a fixed extra NUL slot.
    for (size_t i = kCapacity; i > cursor_; --i) command_[i] = command_[i - 1];
    command_[cursor_++] = static_cast<char>(ch);
    if (echo_) { const char text[] = {static_cast<char>(ch), 0}; emitInput(write, text); fixCursor(write, false); }
    return None;
  }
 private:
  static void scrub(char* text,size_t size){volatile char* p=text;while(size--)*p++=0;}
  static bool safeHistory(const char* text) {
    if(!text || !text[0])return false;
    bool nonspace=false;
    const char* forbidden[]={"password","passwd","passphrase","token","secret","credential","authorization","bearer","psk","auth","login","username"};
    for(size_t i=0;text[i] && i<kCapacity;++i) {
      if(text[i]!=' ')nonspace=true;
      if(text[i]=='?' || text[i]=='@')return false; // URL query/user-info may hide credentials.
      for(const char* word:forbidden) {
        size_t n=0;while(word[n] && text[i+n]){
          const char c=text[i+n]>='A'&&text[i+n]<='Z'?text[i+n]+('a'-'A'):text[i+n];
          if(c!=word[n]){break;}++n;
        }
        if(!word[n])return false;
      }
    }
    return nonspace;
  }
  template <typename Write> static void emit(Write write, const char* text) {
    write(reinterpret_cast<const uint8_t*>(text), strlen(text));
  }
  template <typename Write> void emitInput(Write write,const char* text) const {
    if(!upper_){emit(write,text);return;}
    for(;*text;++text){const uint8_t c=*text>='a'&&*text<='z'?*text-('a'-'A'):*text;write(&c,1);}
  }
  template <typename Write> void bell(Write write) { if (echo_) emit(write, "\a"); }
  template <typename Write> void cursorBack(Write write,size_t count) const {
    if(!echo_||!count)return;
    Terminal::cursorLeft(write,count,ansi_);
  }
  void shiftLeft() { for (size_t i = cursor_; i < kCapacity; ++i) command_[i] = command_[i + 1]; }
  template <typename Write> void fixCursor(Write write, bool clearAfter) {
    const size_t length = strlen(command_);
    if (cursor_ != length) {
      if(ansi_){emit(write, "\x1b[s"); emitInput(write, command_ + cursor_);emit(write, "\x1b[K\x1b[u");}
      else {emitInput(write,command_+cursor_);if(clearAfter)emit(write," ");cursorBack(write,length-cursor_+(clearAfter?1:0));}
    } else if (clearAfter) {if(ansi_)emit(write, "\x1b[K");else emit(write," \b");}
  }
  template <typename Write> void history(bool older,Write write) {
    if(!echo_ || !historyCount_){bell(write);return;}
    if(older) {
      if(historyOffset_==historyCount_){bell(write);return;}
      if(!historyOffset_){scrub(draft_,sizeof(draft_));memcpy(draft_,command_,strlen(command_)+1);draftCursor_=cursor_;}
      ++historyOffset_;
    }else{
      if(!historyOffset_){bell(write);return;}--historyOffset_;
    }
    if(historyOffset_){memcpy(command_,history_[(historyNext_+kHistory-historyOffset_)%kHistory],sizeof(command_));cursor_=strlen(command_);}
    else{memcpy(command_,draft_,sizeof(command_));cursor_=draftCursor_;}
    redraw(write);
  }
  template <typename Write> void move(uint8_t direction, Write write) {
    const size_t length = strlen(command_);
    if (direction == 'D') {
      if (cursor_ == 0) { bell(write); return; }
      --cursor_; cursorBack(write,1);
    } else if (direction == 'C') {
      if (cursor_ == length) { bell(write); return; }
      if(echo_){if(ansi_)emit(write,"\x1b[C");else{const char c[]={command_[cursor_],0};emitInput(write,c);}}++cursor_;
    } else if (direction == 'H' || direction == 'F') {
      const size_t target = direction == 'H' ? 0 : length;
      while (cursor_ != target) move(cursor_ > target ? 'D' : 'C', write);
    } else if(direction=='A'||direction=='B')history(direction=='A',write);
    else bell(write);
  }
  template <typename Write> void escape(uint8_t ch, Write write) {
    if (escape_ == 1) {
      if (ch == '[' || ch == 'O') { escape_ = 2; return; }
      escape_ = 0; move(ch, write); return;
    }
    if (++escapeBytes_ > 16) {escape_=attribute_=escapeBytes_=0;bell(write);return;}
    if ((escape_==2||escape_==3) && ((ch>='0'&&ch<='9') || ch==';')) {
      if(!attribute_){attribute_=ch;}escape_=3;return;
    }
    if (escape_ == 3) {
      if (ch == '~') {
        if (attribute_ == '3') {
          if (cursor_ < strlen(command_)) { shiftLeft(); if (echo_) fixCursor(write, true); }
          else bell(write);
        } else if (attribute_ == '1' || attribute_ == '4' || attribute_=='7' || attribute_=='8') move(attribute_ == '1'||attribute_=='7' ? 'H' : 'F', write);
        else if (attribute_ != '2') bell(write);
      } else if(ch>=0x40 && ch<=0x7e)move(ch,write);
      else {return;}
    } else move(ch, write);
    escape_ = attribute_ = escapeBytes_=0;
  }
  char command_[kCapacity + 1] = {};
  char history_[kHistory][kCapacity+1] = {},draft_[kCapacity+1]={};
  char prompt_[49]="pakar";
  size_t historyNext_=0,historyCount_=0,historyOffset_=0,draftCursor_=0;
  size_t cursor_ = 0;
  uint8_t escape_ = 0, attribute_ = 0,escapeBytes_=0;
  bool echo_ = false, crPending_ = false, overflow_ = false,ansi_=true,upper_=false;
};
} // namespace PakarShell
