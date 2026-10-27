#include "../firmware/esp/src/FilesystemTelemetry.h"
#include <ArduinoJson.h>
#include <cassert>
#include <cstdio>
int main() {
  using namespace PakarFilesystem;
  Observation cache;uint64_t now=100;unsigned probes=0;
  const auto clock=[&]{return now;};
  const auto probe=[&](uint32_t &total,uint32_t &used){++probes;now+=2200;total=8192;used=2048;return true;};
  JsonDocument d;append(d.to<JsonObject>(),cache.snapshot(now));
  assert(d["known"]==false&&d["total_bytes"].isNull()&&d["used_bytes"].isNull());
  assert(d["captured_uptime_ms"].isNull()&&d["age_ms"].isNull());
  assert(!cache.capture(false,true,clock,probe)&&probes==0);
  assert(!cache.capture(true,false,clock,probe)&&probes==0);
  assert(cache.capture(true,true,clock,probe)&&probes==1);
  const auto initial=cache.snapshot(now);assert(initial.known&&initial.capturedAt==2300&&initial.age==0);
  // Thousands of ordinary telemetry snapshots must cause zero additional IO.
  for(unsigned i=0;i<1000;++i) {now+=5;append(d.to<JsonObject>(),cache.snapshot(now));}
  assert(probes==1&&d["known"]==true&&d["total_bytes"]==8192&&d["used_bytes"]==2048);
  assert(d["age_ms"]==5000&&d["observation_scope"]=="cached_observation");
  now=UINT64_C(0x100000000)+2500;
  assert(cache.snapshot(now).age==UINT64_C(0x100000000)+200); // No false freshness at millis wrap.
  const auto failed=[&](uint32_t &,uint32_t &){++probes;return false;};
  assert(!cache.capture(true,true,clock,failed)&&probes==2);
  auto previous=cache.snapshot(now);assert(previous.known&&previous.total==8192&&previous.capturedAt==2300&&previous.error==Error::ReadFailed);
  const auto corrupt=[&](uint32_t &total,uint32_t &used){++probes;total=1;used=2;return true;};
  assert(!cache.capture(true,true,clock,corrupt));previous=cache.snapshot(now);
  assert(previous.known&&previous.total==8192&&previous.error==Error::Invalid);
  RefreshSafety safety;assert(safety.idle());
  bool *owners[]={&safety.outputEnabled,&safety.ota,&safety.reboot,&safety.remote,&safety.artifactReader,
      &safety.upload,&safety.avrUpload,&safety.urboot,&safety.networkChange,&safety.cloudChange,&safety.cloudIo};
  const auto before=probes;
  for(bool *owner:owners) {*owner=true;assert(!safety.idle());assert(!cache.capture(true,safety.idle(),clock,probe));*owner=false;}
  assert(probes==before&&safety.idle());
  assert(cache.capture(true,safety.idle(),clock,probe));append(d.to<JsonObject>(),cache.snapshot(now));
  assert(d["age_ms"]==0&&d["refresh_error"].isNull());
  puts("Actual filesystem telemetry cache: unknown/null, completion time, zero periodic IO, retained last observation, 64-bit age and every maintenance/LED owner fence passed; not filesystem timing proof.");
}
