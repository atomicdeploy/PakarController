#include <assert.h>
#include <stdio.h>
#include <vector>
#include <array>
#include <EEPROM.h>
#include "PcControllerRuntime.h"
#include "PcContractTransport.h"
#include "PcContractAssembly.h"
#include "PcLedContract.h"
#include "SevenSegments.h"
namespace PC = ControllerProtocol;
namespace P = BoardCore::Protocol;
struct Hardware final : PcControllerHardware {
  struct Sent { uint8_t seq; std::vector<uint8_t> p; };
  std::vector<Sent> frames;
  Snapshot live{};
  bool canReceive = true;
  unsigned commands = 0, stops = 0;
  Hardware() { live.brightness = 5; live.cell = 1; }
  Snapshot snapshot() const override { return live; }
  P::Error execute(uint8_t op, const uint8_t *p, uint8_t n, uint32_t) override {
    if (live.programming) return P::kUnsafe;
    if (op == P::kOutputMask && n == 1) live.outputs = p[0];
    else if (op != PC::Buzzer && op != PC::DisplayText && op != PC::Reset) return P::kUnsupported;
    ++commands; return P::kNoError;
  }
  void applySettings(const BoardSettings &s) override { live.brightness = s.displayBrightness; live.muted = s.flags & 1; }
  P::Error menuAction(uint8_t, uint8_t page) override { return page == 2 ? P::kUnsafe : P::kNoError; }
  void showMenu(uint8_t,bool,bool) override {}
  void safeStop() override { ++stops; live.outputs = 0; }
  bool ready() const override { return canReceive; }
  bool send(uint8_t op,uint8_t seq,const uint8_t *p,uint8_t n) override {
    assert(op == P::kPcContractResponse && n <= 48); frames.push_back({seq,{p,p+n}}); return true;
  }
};
void send(PcControllerRuntime &r, Hardware &h, uint8_t op,
          std::initializer_list<uint8_t> bytes = {}, uint32_t now = 100) {
  std::vector<uint8_t> p{op}; p.insert(p.end(),bytes);
  h.frames.clear(); r.handle(7,p.data(),p.size(),now,now*1000);
  assert(!h.frames.empty());
}
void error(Hardware &h, uint8_t op, P::Error e) {
  assert(h.frames[0].p == std::vector<uint8_t>({PC::ErrorResponse,op,e}));
}
void settingsTransactions() {
  EEPROM.reset();
  BoardSettingsStore store; store.begin();
  assert(!store.persisted() && EEPROM.writes == 0);
  auto cfg = store.values(); cfg.displayBrightness = 3; cfg.nameLength=3;
  memcpy(cfg.name,"AVR",3);
  assert(store.replace(cfg,0) && store.persisted());
  const unsigned initialWrites = EEPROM.writes;
  assert(store.replace(cfg,1) && EEPROM.writes == initialWrites);
  const auto original = std::array<uint8_t,1024>{}; (void)original;
  uint8_t good[1024]; memcpy(good,EEPROM.bytes,1024);
  for (int fail = 0; fail < 30; ++fail) {
    memcpy(EEPROM.bytes,good,1024); EEPROM.budget=fail;
    BoardSettingsStore session; session.begin();
    cfg.displayBrightness=7;
    const bool saved = session.replace(cfg,10);
    EEPROM.budget=-1;
    BoardSettingsStore restarted; restarted.begin();
    assert(restarted.persisted());
    assert(restarted.values().displayBrightness == (saved ? 7 : 3));
    assert(EEPROM.bytes[1008] == 0xFF); // Prog record untouched.
  }
  memcpy(EEPROM.bytes,good,1024); EEPROM.budget=-1; store.begin();
  cfg=store.values(); cfg.streamPeriodMs=100;
  assert(store.replace(cfg,0xFFFFFFF0U,false));
  assert(!store.persisted() && !store.service(0xFFFFFFF0U+1499U));
  assert(!store.service(0xFFFFFFF0U+1500U,false));
  assert(store.service(0xFFFFFFF0U+1500U,true));
  cfg.name[3]=0x41; assert(!store.replace(cfg,2000)); // Noncanonical tail.
  cfg=store.values(); cfg.packedOrder[0]=0; assert(!store.replace(cfg,2000));
  cfg=store.values(); cfg.displayBrightness=2;
  assert(store.replace(cfg,3000,false));
  auto rejected=cfg; rejected.displayBrightness=7;
  EEPROM.budget=0; assert(!store.replace(rejected,3100,true)); EEPROM.budget=-1;
  assert(store.values().displayBrightness==2 && !store.persisted());
  assert(store.service(4500) && store.persisted());
}
void dispatcher() {
  EEPROM.reset(); Hardware h; PcControllerRuntime r(h); r.begin(0);
  assert(EEPROM.writes == 0);
  send(r,h,PC::Hello); assert(h.frames[0].p[0] == PC::HelloResponse && h.frames[0].p[1] == 0x50);
  assert(h.frames[0].p[2] == 0 && h.frames[0].p[3] == 0); // Reserved, not contract generations.
  assert(h.frames[0].p[4] == 48);
  send(r,h,PC::GetSettings); assert(h.frames[0].p.size()==23 && h.frames[0].p.back()==0);
  send(r,h,PC::SetSettings,{3}); error(h,PC::SetSettings,P::kBadPayload);
  send(r,h,PC::SetStreamPeriod,{99,0}); error(h,PC::SetStreamPeriod,P::kBadPayload);
  send(r,h,PC::SetStreamPeriod,{100,0}); assert(h.frames[0].p[0]==PC::Ack);
  send(r,h,PC::FrontPanelGet); assert(h.frames[0].p.size()==48 && h.frames[0].p[9]==0x80);
  send(r,h,PC::MenuList,{0}); assert(h.frames[0].p[4]==6);
  send(r,h,PC::MenuLayoutSet,{2,6,1,0,0x00,0x32,0x54}); error(h,PC::MenuLayoutSet,P::kBadPayload);
  send(r,h,PC::MenuLayoutSet,{2,6,1,0,0x10,0x32,0x54}); assert(h.frames[0].p[0]==PC::Ack);
  send(r,h,PC::MenuSetPage,{1}); error(h,PC::MenuSetPage,P::kBadPayload);
  send(r,h,PC::MenuSetPage,{0}); assert(r.menuActive());
  send(r,h,PC::RemoteKeyGesture,{3,5}); assert(h.frames[0].p[0]==PC::Ack);
  send(r,h,PC::ProgramState,{1,0xAA}); assert(r.running());
  // Every hardware-mismatched operation fails without mutation, including the
  // donor PWM 0x11: this MUST NOT invoke Pakar's legacy audio-cue opcode.
  const uint8_t disabled[] = {PC::TemperatureList,PC::PwmSet,PC::PwmAllOff,PC::PwmGet,
    PC::StatusRgb,PC::StatusEffect,PC::StatusProfileGet,PC::StatusProfileSet,
    PC::AddressableLed,PC::RadioTransmit,PC::RadioLearnStart,PC::RadioLearnCancel,
    PC::RadioLearnClear,PC::RadioLearnList,PC::RadioLearnRemove,PC::RadioLearnReplace,
    PC::RelaySet,PC::RelaySide,PC::RelayAllOff,PC::RelayTest,PC::I2cTransfer};
  const unsigned commands = h.commands;
  for (auto op : disabled) { send(r,h,op); error(h,op,P::kHardwareUnavailable); }
  assert(h.commands==commands);
  for (auto op : {PC::HostMenuDirectory,PC::HostMenuContent,PC::HostMenuStateGet}) {
    send(r,h,op); error(h,op,P::kUnsupported);
  }
  h.live.programming=true;
  send(r,h,PC::Buzzer,{0,0,1,0}); error(h,PC::Buzzer,P::kUnsafe);
  send(r,h,PC::GetSettings); assert(h.frames[0].p[0]==PC::SettingsResponse);
  h.live.programming=false;
  send(r,h,PC::Reset,{0}); assert(h.frames[0].p[0]==PC::Ack);
  send(r,h,PC::ProgramState,{1},200); h.live.outputs=255;
  r.service(5199,5199000); assert(r.running());
  r.service(5200,5200000); assert(!r.running() && h.live.outputs==0);
  for (uint8_t n=0;n<48;++n) {
    uint8_t bad[48]={PC::SetSettings};
    h.frames.clear(); r.handle(8,bad,n,6000,6000000);
    assert(!h.frames.empty() && h.frames[0].p[0]==PC::ErrorResponse);
  }
}
void macroAndAutomations() {
  EEPROM.reset(); Hardware h; PcControllerRuntime r(h); r.begin(0);
  send(r,h,PC::MacroStart,{1,0,1,0});
  // One timestamped BUZZER step [deltaUs32,opcode,len,frequency16,duration16].
  send(r,h,PC::MacroStep,{0,0,0,1,0,100,0,0,0,PC::Buzzer,4,0xE8,3,20,0});
  send(r,h,PC::MacroStep,{1});
  r.service(100,100099); assert(h.commands==0);
  r.service(101,100100); assert(h.commands==1);
  send(r,h,PC::MacroStep,{2});
  assert(h.frames[0].p[2]==ControllerCore::MacroRing::Completed);
  send(r,h,PC::MacroStart,{2,0,0,0}); send(r,h,PC::MacroStep,{1});
  send(r,h,PC::MacroStep,{2}); assert(h.frames[0].p[2]==ControllerCore::MacroRing::Completed);
  send(r,h,PC::MacroStep,{3,9}); h.live.outputs=3; r.service(110,110000);
  send(r,h,PC::MacroStep,{4}); send(r,h,PC::MacroStep,{5,0,0});
  assert(h.frames[0].p[2]==0x80);
  send(r,h,PC::MacroStep,{6}); r.service(120,120000);
  assert(h.live.outputs==0); r.service(121,130000); assert(h.live.outputs==3);
  send(r,h,PC::MacroStep,{7});
  AutomationRecord rec{0xFF,1,static_cast<uint8_t>(AutomationEventKind::Key),5,0xFF,
      static_cast<uint8_t>(AutomationActionKind::Buzzer),0,440,25,0};
  uint8_t p[13] = {PC::AutomationPut}; memcpy(p+1,&rec,sizeof(rec));
  h.frames.clear(); r.handle(9,p,sizeof(p),130,130000);
  assert(h.frames[0].p[0]==PC::AutomationRecordResponse);
  const unsigned before=h.commands;
  r.key(0,KeyEvent::Down,0,131,131000); assert(h.commands==before+1);
  // Wildcard rule action budgets and recursion guards are actual imported code.
  for(unsigned i=0;i<10;++i) r.key(0,KeyEvent::Down,0,132,132000);
  assert(h.commands<=before+8);
  rec.eventKind=static_cast<uint8_t>(AutomationEventKind::Door); rec.eventValue=0; memcpy(p+1,&rec,sizeof(rec));
  h.frames.clear(); r.handle(9,p,sizeof(p),140,140000); error(h,PC::AutomationPut,P::kHardwareUnavailable);
  send(r,h,PC::AutomationList,{0}); assert(h.frames[0].p[4]==1);
  rec.id=0xFF; rec.eventKind=static_cast<uint8_t>(AutomationEventKind::Key); rec.eventValue=0x85;
  memcpy(p+1,&rec,sizeof(rec)); h.frames.clear(); r.handle(9,p,sizeof(p),145,145000);
  assert(h.frames[0].p[0]==PC::AutomationRecordResponse);
  const unsigned ninthBefore=h.commands;
  r.key(8,KeyEvent::Down,0,1200,1200000); assert(h.commands==ninthBefore+1);
  send(r,h,PC::AutomationRemove,{1},1201);
  rec.id=0; rec.eventValue=5; rec.value=440; memcpy(p+1,&rec,sizeof(rec));
  uint8_t good[1024]; memcpy(good,EEPROM.bytes,1024);
  rec.eventKind=static_cast<uint8_t>(AutomationEventKind::Key); rec.eventValue=5; rec.id=0; rec.value=880;
  for(int fail=0;fail<160;++fail) {
    memcpy(EEPROM.bytes,good,1024); EEPROM.budget=fail; boardAutomations.begin();
    const bool saved=boardAutomations.put(rec);
    EEPROM.budget=-1; boardAutomations.begin(); AutomationRecord restored;
    assert(boardAutomations.get(0,restored)); assert(restored.value==(saved?880:440));
    assert(EEPROM.bytes[1008]==0xFF);
  }
}
void transport() {
  using T=BoardCore::PcContractTransport;
  T mux; uint8_t p[48]={}; uint8_t n=0;
  assert(T::decodeHex("00fF",p,n) && n==2 && p[1]==255);
  for(const char *bad:{"0","zz"," 00","0x10"}) assert(!T::decodeHex(bad,p,n));
  const auto a=mux.enqueue(PC::GetSettings,nullptr,0,0,true,42);
  const auto b=mux.enqueue(PC::Buzzer,p,4,0); assert(a && b && a!=b);
  auto *first=mux.next(); assert(first->id==a); mux.sent(*first,true,0);
  P::Frame f{}; f.opcode=P::kPcContractResponse; f.sequence=first->wireSequence;
  f.payloadLength=3; f.payload[0]=PC::Ack; f.payload[1]=PC::Buzzer;
  assert(!mux.accept(f,10)); // Wrong request may not complete another client.
  f.payload[1]=PC::GetSettings; f.payload[2]=P::kBusy; assert(!mux.accept(f,10));
  f.payload[0]=PC::ErrorResponse; assert(mux.accept(f,10)->hostSequence==42);
  mux.releaseUsb(*first);
  auto *second=mux.next(); assert(second->id==b); mux.sent(*second,true,0);
  unsigned expired=0; mux.expire(749,[&](auto &){++expired;}); assert(!expired);
  mux.expire(750,[&](auto &){++expired;}); assert(expired==1 && mux.get(b)->state==T::OutcomeUnknown);
  f.sequence=second->wireSequence; f.payload[0]=PC::Ack; f.payload[1]=PC::Buzzer; f.payload[2]=0;
  assert(!mux.accept(f,751)); // No replay or late success after uncertain timeout.
  for(unsigned i=0;i<7;++i) assert(mux.enqueue(PC::Hello,nullptr,0,800));
  assert(!mux.enqueue(PC::Hello,nullptr,0,800));
  mux.expire(3000,[&](auto &){++expired;});
  assert(!mux.enqueue(PC::Hello,nullptr,0,3001)); // Receipts remain readable for 60s.
  assert(mux.enqueue(PC::Hello,nullptr,0,63001));
  T owned;
  for(unsigned i=0;i<32;++i) {
    const auto id=owned.enqueue(PC::Buzzer,p,4,1000+i); assert(id);
    assert(!owned.consume(id,PC::Buzzer)); // Pending cannot be deleted.
    auto *entry=owned.next(); owned.sent(*entry,true,1000+i);
    f.sequence=entry->wireSequence; f.payload[1]=PC::Buzzer;
    assert(owned.accept(f,1001+i));
    assert(!owned.consume(id,PC::DisplayText)); // Wrong request cannot consume.
    assert(owned.consume(id,PC::Buzzer) && !owned.get(id));
    assert(!owned.consume(id,PC::Buzzer)); // No double consumption.
    assert(!owned.accept(f,1002+i)); // Late duplicate quarantined.
  }
  T selectors;
  for(uint8_t action=0;action<8;++action) {
    const auto id=selectors.enqueue(PC::MacroStep,&action,1,2000+action); assert(id);
    auto *entry=selectors.next(); selectors.sent(*entry,true,2000+action);
    f.sequence=entry->wireSequence; f.payloadLength=3;
    f.payload[0]=PC::ErrorResponse; f.payload[1]=PC::MacroStep; f.payload[2]=0;
    assert(!selectors.accept(f,2001+action)); // ERROR must contain a real error.
    const bool query=action==2 || action==5;
    f.payload[0]=query ? PC::Ack : PC::MacroStatusResponse;
    assert(!selectors.accept(f,2001+action)); // Wrong response shape cannot complete.
    f.payload[0]=query ? PC::MacroStatusResponse : PC::Ack;
    assert(selectors.accept(f,2001+action));
    assert(selectors.consume(id,PC::MacroStep));
  }
  const auto queryId=selectors.enqueue(PC::GetSettings,nullptr,0,3000);
  auto *entry=selectors.next(); selectors.sent(*entry,true,3000);
  f.sequence=entry->wireSequence; f.payload[0]=PC::Ack; f.payload[1]=PC::GetSettings;
  assert(!selectors.accept(f,3001)); // Read-only settings requires SETTINGS, not ACK.
  f.payload[0]=PC::SettingsResponse; assert(selectors.accept(f,3001));
  assert(selectors.consume(queryId,PC::GetSettings));
}
struct Strip {
  uint8_t pixels[480][3] = {};
  uint8_t level=255; unsigned commits=0;
  void clear() { memset(pixels,0,sizeof(pixels)); }
  void commit() { ++commits; }
  void brightness(uint8_t v) { level=v; }
  bool stage(uint8_t start,const uint8_t *p,uint8_t count) {
    if (start+count>100) return false;
    memcpy(pixels[start],p,count*3); return true;
  }
};
void stripAndChunks() {
  Strip strip; BoardCore::PcLedContract<Strip> led(strip);
  uint8_t fill[]={0xFF,10,20,30,200};
  assert(led.handle(fill,5)==P::kNoError && strip.level==200 && strip.commits==1);
  assert(strip.pixels[99][2]==30 && strip.pixels[100][2]==0);
  uint8_t bad[]={0xFE,101}; assert(led.handle(bad,2)==P::kBadPayload && strip.commits==1);
  uint8_t configure[]={0xFE,2}; assert(led.handle(configure,2)==P::kNoError && led.count()==2);
  uint8_t stage[]={0xFD,1,1,2,3}; assert(led.handle(stage,5)==P::kNoError && strip.commits==2);
  stage[1]=2; assert(led.handle(stage,5)==P::kBadPayload && strip.commits==2);
  uint8_t commit[]={0xFC}; assert(led.handle(commit,1)==P::kNoError && strip.commits==3);
  BoardCore::PcContractAssembly assembly;
  uint8_t start[48]={0,PC::DisplayText,48}, finish[4]={1,45,46,47};
  for(unsigned i=0;i<45;++i) start[i+3]=i;
  assert(!assembly.accept(1,start,48,0));
  assert(!assembly.accept(2,finish,4,1)); // Mismatched client/sequence cancels.
  assert(!assembly.accept(1,start,48,10));
  assert(!assembly.accept(1,finish,4,210)); // Expired start cannot mutate.
  assert(!assembly.accept(1,start,48,UINT32_MAX-10));
  assert(assembly.accept(1,finish,4,10));
  assert(assembly.data()[0]==PC::DisplayText && assembly.data()[48]==47);
  assert(!assembly.accept(1,finish,4,11)); // No duplicate final execution.
}
std::vector<KeyEvent> gestures;
void gesture(uint8_t bit,KeyEvent e,void*) { assert(bit==8); gestures.push_back(e); }
void keysAndDisplay() {
  Key key; key.begin(8); key.setEventCallback(gesture);
  key.updateSample(false,0); key.updateSample(true,5); key.updateSample(true,24);
  assert(gestures.empty()); key.updateSample(true,25); assert(gestures.back()==KeyEvent::Down);
  key.updateSample(true,625); assert(gestures.back()==KeyEvent::HoldStart);
  key.updateSample(true,775); assert(gestures.back()==KeyEvent::HoldRepeat);
  key.updateSample(false,780); key.updateSample(false,800);
  assert(gestures[gestures.size()-2]==KeyEvent::Up && gestures.back()==KeyEvent::HoldRelease);
  SevenSegments d; mockReads=0; mockAck=false; d.begin(5); d.showInteger(1234);
  assert(!d.detected()); uint8_t frame[4]; memcpy(frame,d.rawSegments(),4);
  mockAck=true; const auto reads=mockReads; d.service(999); assert(mockReads==reads);
  d.service(1000); assert(d.detected() && !memcmp(frame,d.rawSegments(),4) && mockReads>reads);
  mockFailRead=mockReads+3; d.service(2000); assert(!d.detected()); // NACK before final control ACK.
  mockFailRead=0; d.service(3000); assert(d.detected());
  d.setBrightness(0); assert(d.brightness()==0);
  mockAck=false; d.service(4000); assert(!d.detected());
  mockAck=true; d.service(5000); assert(d.detected() && d.brightness()==0);
}
void polling() {
  EEPROM.reset(); Hardware h; h.canReceive=false; PcControllerRuntime r(h); r.begin(0);
  send(r,h,PC::Hello); h.frames.clear(); r.service(1,1000); assert(h.frames.empty());
  send(r,h,PC::GetStatus); assert(h.frames.size()==2 && h.frames[1].p[0]==PC::SegmentChanged);
  h.live.audioStartedMicros = 0xFFFFFE00UL;
  send(r,h,PC::GetStatus); assert(h.frames.size()==2 && h.frames[1].p[0]==PC::BuzzerChanged && h.frames[1].p.size()==14);
  // Appended actual musical start and observation clocks, little endian;
  // existing frequency/duration/mute bytes retain their offsets.
  assert(h.frames[1].p[6] == 0 && h.frames[1].p[7] == 0xFE && h.frames[1].p[8] == 0xFF && h.frames[1].p[9] == 0xFF);
  h.live.muted=true;
  send(r,h,PC::GetStatus); assert(h.frames.size()==2 && h.frames[1].p[0]==PC::BuzzerChanged && h.frames[1].p[5]==1);
  r.key(8,KeyEvent::Down,0,110,110000);
  send(r,h,PC::GetStatus,{},111); assert(h.frames.size()==2 && h.frames[1].p[0]==PC::Event && h.frames[1].p==std::vector<uint8_t>({PC::Event,1,8,5,0,0xFF}));
}
void savedMenuPage() {
  EEPROM.reset(); Hardware h; PcControllerRuntime r(h); r.begin(0);
  BoardSettings cfg; cfg.flags|=4;
  uint8_t p[22]={PC::SetSettings}; memcpy(p+1,&cfg,sizeof(cfg));
  r.handle(7,p,sizeof(p),100,100000);
  send(r,h,PC::MenuSetPage,{4},200);
  assert(r.settings().values().defaultPage==4 && !r.settings().persisted());
  r.service(1700,1700000); assert(r.settings().persisted());
  PcControllerRuntime restarted(h); restarted.begin(1800); assert(restarted.menuPage()==4);
}
int main() {
  settingsTransactions(); dispatcher(); macroAndAutomations(); transport(); keysAndDisplay(); stripAndChunks(); polling(); savedMenuPage();
  puts("PCController profile: dispatch, safety, settings/automation torn writes, macros, transport, keys, TM1637 passed.");
}
