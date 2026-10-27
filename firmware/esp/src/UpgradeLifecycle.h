#pragma once
#include <stdint.h>
#include <string.h>

// PCController UPDATE-PROGRESS stage truth + PU AsyncOTA finite notices.
// Loop-owned presentation only: no command replay, EEPROM or flash admission.
namespace PakarUpdate {
enum class Source : uint8_t { None, Local, Remote, Avr };
enum class Visual : uint8_t { None, Startup, Progress, Shutdown, Success, Failure };
inline const char *sourceName(Source source) {
  switch(source) {case Source::Local:return "local-ota";case Source::Remote:return "remote-firmware";
    case Source::Avr:return "avr-maintenance";default:return "boot";}
}
class Lifecycle {
 public:
  void boot(uint32_t now,bool animate=true) {bootAt_=now;booting_=animate;}
  bool observe(Source source,uint32_t operation,const char *phase,uint32_t done,
      uint32_t total,bool active,bool verified,uint32_t now,bool currentTerminal=false) {
    if(!phase || !operation || strlen(phase)>=sizeof(phase_))return false;
    const bool terminal=!strcmp(phase,"complete") || !strcmp(phase,"failed") || !strcmp(phase,"aborted") || !strcmp(phase,"cancelled");
    const bool same=seen_ && source_==source && operation_==operation;
    // Old persisted completed/failed jobs are a baseline, never a new chime.
    if(!same && (!active || terminal) && !currentTerminal)return false;
    const bool stageChanged=!same || strcmp(phase_,phase);
    const bool changed=stageChanged || done_!=done || total_!=total || active_!=active || verified_!=verified;
    if(!changed)return false;
    if(!same) {startedAt_=now;shutdownNotified_=terminalNotified_=false;}
    if(stageChanged) {stageAt_=now;phaseNotice_=true;strcpy(phase_,phase);}
    seen_=true;source_=source;operation_=operation;done_=done;total_=total;
    active_=active;verified_=terminal && !strcmp(phase,"complete") && verified;
    dirty_=true;updatedAt_=now;
    return true;
  }
  bool known()const {
    if(!total_ || done_>total_)return false;
    const char *measured[]={"receiving","download_esp","download_avr","backup_flash","backup_eeprom",
        "verify_flash","verify_eeprom","verify_esp","program_avr","program_esp"};
    for(const char *p:measured)
      if(!strcmp(phase_,p))return true;
    return false;
  }
  uint8_t percent()const{return known()?uint8_t(uint64_t(done_)*100/total_):0;}
  Visual visual(uint32_t now)const {
    if(seen_ && (!strcmp(phase_,"awaiting_reboot") || !strcmp(phase_,"ready-to-reboot")))return Visual::Shutdown;
    if(active_) {
      // Hold the final black frame until the owner explicitly advances. A
      // time-only transition would skip alpha=0 at the shutdown boundary.
      if(!strcmp(phase_,"prepare"))return Visual::Shutdown;
      return Visual::Progress;
    }
    if(seen_ && uint32_t(now-stageAt_)<1600U) {
      if(verified_)return Visual::Success;
      if(!strcmp(phase_,"failed") || !strcmp(phase_,"aborted"))return Visual::Failure;
    }
    return booting_ && uint32_t(now-bootAt_)<startupMs?Visual::Startup:Visual::None;
  }
  uint8_t sceneAlpha(uint32_t now)const {
    const auto view=visual(now);
    if(view==Visual::Startup)return uint8_t(uint32_t(now-bootAt_)*255U/startupMs);
    if(view==Visual::Shutdown)return uint8_t(255U-(uint32_t(now-stageAt_)>=shutdownMs?255U:uint32_t(now-stageAt_)*255U/shutdownMs));
    if(view==Visual::Success || view==Visual::Failure)return uint8_t(uint32_t(now-stageAt_)*255U/1600U);
    return 0;
  }
  uint32_t cellRgb(uint8_t cell,uint32_t now)const {
    if(visual(now)==Visual::Success)return 0x20c070;
    if(visual(now)==Visual::Failure)return 0xc02018;
    if(known())return uint64_t(cell+1)*total_<=uint64_t(done_)*80?0x20a8e0:0x020810;
    const uint8_t cursor=uint32_t(now-stageAt_)/100U%80U;
    const uint8_t distance=uint8_t((cell+80U-cursor)%80U);
    return distance<3?0x187ca0:0x020810;
  }
  bool takeShutdownNotice(uint32_t now) {
    if(visual(now)!=Visual::Shutdown || shutdownNotified_)return false;
    shutdownNotified_=true;return true;
  }
  int8_t takeTerminalNotice() {
    if(!seen_ || terminalNotified_)return 0;
    const int8_t result=verified_?1:(!strcmp(phase_,"failed") || !strcmp(phase_,"aborted")?-1:0);
    if(result)terminalNotified_=true;
    return result;
  }
  bool readyToReboot(uint32_t now,bool blackPresented)const {
    return visual(now)==Visual::Shutdown && uint32_t(now-stageAt_)>=shutdownMs &&
      (blackPresented || uint32_t(now-stageAt_)>=2000U);
  }
  bool takeEvent(uint32_t now,bool &phaseChanged) {
    if(!dirty_ || (!phaseNotice_ && uint32_t(now-eventAt_)<250U))return false;
    phaseChanged=phaseNotice_;dirty_=phaseNotice_=false;eventAt_=now;++revision_;return true;
  }
  Source source()const{return source_;} uint32_t operation()const{return operation_;}
  const char *phase()const{return phase_;} bool active()const{return active_;}
  bool verified()const{return verified_;} uint32_t done()const{return done_;}
  uint32_t total()const{return total_;} uint32_t revision()const{return revision_;}
  uint32_t startedAt()const{return startedAt_;} uint32_t updatedAt()const{return updatedAt_;}
  static constexpr uint32_t shutdownMs=700,startupMs=900;
 private:
  Source source_=Source::None;uint32_t operation_=0,done_=0,total_=0,revision_=0;
  uint32_t startedAt_=0,stageAt_=0,updatedAt_=0,eventAt_=0,bootAt_=0;
  char phase_[32]="idle";
  bool seen_=false,active_=false,verified_=false,dirty_=false,phaseNotice_=false;
  bool shutdownNotified_=false,terminalNotified_=false,booting_=false;
};
}
