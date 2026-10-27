#pragma once
#include <stdint.h>

// Diagnostic cache only. Never use it to admit a firmware download/write:
// FirmwarePlatform::freeStorage retains its independent fresh storage check.
namespace PakarFilesystem {
struct RefreshSafety {
  bool outputEnabled=false,ota=false,reboot=false,remote=false,artifactReader=false;
  bool upload=false,avrUpload=false,urboot=false,networkChange=false,cloudChange=false,cloudIo=false;
  bool idle()const {
    return !outputEnabled&&!ota&&!reboot&&!remote&&!artifactReader&&!upload&&
        !avrUpload&&!urboot&&!networkChange&&!cloudChange&&!cloudIo;
  }
};
enum class Error:uint8_t {None,NotMounted,Busy,ReadFailed,Invalid};
inline const char *code(Error error) {
  switch(error) {
    case Error::None:return nullptr;
    case Error::NotMounted:return "not_mounted";
    case Error::Busy:return "maintenance_busy";
    case Error::ReadFailed:return "read_failed";
    default:return "invalid_observation";
  }
}
struct Snapshot {
  bool mounted=false,known=false,attempted=false;
  uint32_t total=0,used=0;
  uint64_t capturedAt=0,age=0,lastAttemptAt=0;
  Error error=Error::None;
};
class Observation {
 public:
  // Explicit caller only: there is no automatic refresh/service timer. The
  // injected clock is sampled AFTER a successful slow probe, not stale ingress.
  template<class Clock,class Probe>
  bool capture(bool mounted,bool safe,Clock clock,Probe probe) {
    mounted_=mounted;
    if(!mounted){error_=Error::NotMounted;return false;}
    if(!safe){error_=Error::Busy;return false;}
    attempted_=true;lastAttemptAt_=clock();
    uint32_t total=0,used=0;
    if(!probe(total,used)){error_=Error::ReadFailed;return false;}
    if(!total||used>total){error_=Error::Invalid;return false;}
    total_=total;used_=used;capturedAt_=clock();known_=true;error_=Error::None;return true;
  }
  bool mounted()const{return mounted_;}
  Snapshot snapshot(uint64_t now)const {
    Snapshot value;value.mounted=mounted_;value.known=known_;value.attempted=attempted_;
    value.total=total_;value.used=used_;value.capturedAt=capturedAt_;
    value.age=known_&&now>=capturedAt_?now-capturedAt_:0;
    value.lastAttemptAt=lastAttemptAt_;value.error=error_;return value;
  }
 private:
  bool mounted_=false,known_=false,attempted_=false;
  uint32_t total_=0,used_=0;
  uint64_t capturedAt_=0,lastAttemptAt_=0;
  Error error_=Error::None;
};
template<class Object> void append(Object object,const Snapshot &value) {
  object["mounted"]=value.mounted;object["known"]=value.known;
  object["observation_scope"]="cached_observation";
  if(value.known) {
    object["total_bytes"]=value.total;object["used_bytes"]=value.used;
    object["captured_uptime_ms"]=value.capturedAt;object["age_ms"]=value.age;
  } else {
    object["total_bytes"]=nullptr;object["used_bytes"]=nullptr;
    object["captured_uptime_ms"]=nullptr;object["age_ms"]=nullptr;
  }
  if(value.attempted)object["last_attempt_uptime_ms"]=value.lastAttemptAt;
  else object["last_attempt_uptime_ms"]=nullptr;
  object["refresh_error"]=code(value.error);
}
} // namespace PakarFilesystem
