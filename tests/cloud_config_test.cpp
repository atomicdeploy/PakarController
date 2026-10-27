#include <cassert>
#include <cstddef>
#include <iostream>
#include "../firmware/esp/src/CloudConfig.h"
#include "../firmware/esp/src/ConfigurationChanges.h"

namespace {
struct Stored {
  uint32_t magic;
  uint8_t schema, mqttEnabled, mqttTls;
  uint16_t mqttPort;
  char mqttHost[64], mqttUser[33], mqttPassword[65], mqttBaseTopic[65];
  uint8_t webSocketEnabled, webSocketTls;
  uint16_t webSocketPort;
  char webSocketHost[64], webSocketPath[97], webSocketToken[65];
  uint16_t telemetrySeconds;
  uint32_t checksum;
  uint8_t ntfyEnabled;
  char ntfyHost[64],ntfyTopic[65],ntfyToken[65];
  uint32_t fullChecksum;
} __attribute__((packed));
void digest(Stored &r) {
  r.fullChecksum = 2166136261U;
  const auto *bytes = reinterpret_cast<const uint8_t *>(&r);
  for (size_t i=0;i<offsetof(Stored,fullChecksum);++i) r.fullChecksum=(r.fullChecksum^bytes[i])*16777619U;
}
void writeRecord(fs::FS &filesystem, Stored r) {
  digest(r);
  auto data = std::make_shared<File::Data>();
  const auto *bytes = reinterpret_cast<const uint8_t *>(&r);
  data->bytes.assign(bytes,bytes+sizeof(r)); filesystem.files["/cloud.bin"] = data;
}
Stored record(const std::vector<uint8_t> &bytes) {
  assert(bytes.size()==sizeof(Stored)); Stored result{};
  memcpy(&result,bytes.data(),sizeof(result)); return result;
}
void assertOld(fs::FS &filesystem,const std::vector<uint8_t> &bytes) {
  assert(filesystem.files.at("/cloud.bin")->bytes == bytes);
  CloudConfigStore loaded; assert(loaded.begin(filesystem));
  assert(String(loaded.mqttHost())=="mqtt.fixture.invalid");
  assert(String(loaded.mqttPassword())==u8"رمز fixture\npassword");
  assert(String(loaded.webSocketToken())==u8"توکن-fixture");
  assert(loaded.telemetrySeconds()==17);
}
}

int main() {
  PakarConfiguration::changes().reset();PakarConfiguration::Journal::Event change;
  fs::FS filesystem; CloudConfigStore settings;
  assert(settings.begin(filesystem));
  assert(!settings.mqttEnabled()&&!settings.webSocketEnabled());
  assert(!settings.ntfyEnabled() && String(settings.ntfyHost())=="ntfy.sh" && !settings.ntfyTokenSet());
  assert(settings.setMqtt(true,true,"mqtt.fixture.invalid",8883,u8"کاربر",u8"رمز fixture\npassword","fixture/base",true));
  assert(settings.setWebSocket(true,true,"ws.fixture.invalid",443,"/ws?fixture=1",u8"توکن-fixture",true));
  assert(settings.setTelemetrySeconds(17)); assert(settings.save());
  assert(PakarConfiguration::changes().peek(change)&&change.scopes==PakarConfiguration::Cloud);
  assert(PakarConfiguration::changes().retire(change));
  const auto active = filesystem.files.at("/cloud.bin")->bytes;
  const auto opens=filesystem.faults->writeOpens, writes=filesystem.faults->writes,
      bytes=filesystem.faults->bytesWritten, renames=filesystem.faults->renames;
  assert(settings.save()); assert(settings.save());
  assert(!PakarConfiguration::changes().peek(change));
  assert(filesystem.faults->writeOpens==opens&&filesystem.faults->writes==writes&&
      filesystem.faults->bytesWritten==bytes&&filesystem.faults->renames==renames);
  assertOld(filesystem,active);

  CloudConfigStore candidate=settings; assert(candidate.setTelemetrySeconds(23));
  filesystem.faults->failOpenWrite=true; assert(!candidate.save()); assertOld(filesystem,active);
  filesystem.faults->failOpenWrite=false; filesystem.faults->failWrite=true;
  assert(!candidate.save()); assertOld(filesystem,active);
  filesystem.faults->failWrite=false; filesystem.faults->shortWrite=true;
  assert(!candidate.save()); assertOld(filesystem,active);
  filesystem.faults->shortWrite=false; filesystem.faults->failVerifyRead=true;
  assert(!candidate.save()); assertOld(filesystem,active);
  filesystem.faults->failVerifyRead=false; filesystem.faults->corruptVerify=true;
  assert(!candidate.save()); assertOld(filesystem,active);
  filesystem.faults->corruptVerify=false; filesystem.faults->failRename=true;
  assert(!candidate.save()); assertOld(filesystem,active);
  assert(settings.telemetrySeconds()==17); // Publish only a committed copy.
  assert(!PakarConfiguration::changes().peek(change)); // All six failure paths remain silent.
  filesystem.faults->failRename=false; assert(candidate.save());
  CloudConfigStore committed; assert(committed.begin(filesystem));
  assert(committed.telemetrySeconds()==23);
  assert(committed.setMqtt(true,true,"mqtt.fixture.invalid",8883,u8"کاربر","ignored","fixture/base",false));
  assert(String(committed.mqttPassword())==u8"رمز fixture\npassword");

  // Setters reject injection/embedded NUL without partially changing the record.
  assert(!settings.setMqtt(true,true,"mqtt\r\nInjected",8883,"user","","fixture",false));
  assert(!settings.setMqtt(true,true,"mqtt.fixture.invalid",8883,"user","","bad/#",false));
  assert(!settings.setMqtt(true,true,"mqtt.fixture.invalid",8883,String("bad\0user",8),"","fixture",false));
  assert(!settings.setMqtt(true,true,"mqtt.fixture.invalid",8883,"user",String("bad\0secret",10),"fixture",true));
  assert(!settings.setMqtt(true,true,"mqtt.fixture.invalid",8883,"user",String("bad\0secret",10),"fixture",false));
  assert(!settings.setMqtt(true,true,String("bad\0host",8),8883,"user","","fixture",false));
  assert(!settings.setMqtt(true,true,"mqtt.fixture.invalid",8883,"user","",String("bad\0topic",9),false));
  assert(!settings.setMqtt(true,true,"mqtt.fixture.invalid",0,"user","","fixture",false));
  assert(!settings.setWebSocket(true,true,"ws.fixture.invalid",443,"/ws\r\nInjected","",false));
  assert(!settings.setWebSocket(true,true,"ws.fixture.invalid",443,"/ws", "x\r\nHeader: value",true));
  assert(!settings.setWebSocket(true,true,"ws.fixture.invalid",443,"/ws",String("bad\0token",9),true));
  assert(!settings.setWebSocket(true,true,"ws.fixture.invalid",443,"/ws",String("bad\0token",9),false));
  assert(!settings.setWebSocket(true,true,"ws.fixture.invalid",443,"/ws","x\r\nHeader: value",false));
  assert(!settings.setWebSocket(true,true,"ws.fixture.invalid",443,String("/bad\0path",9),"",false));
  assert(!settings.setWebSocket(true,true,String("bad\0host",8),443,"/ws","",false));
  assert(!settings.setTelemetrySeconds(0)&&!settings.setTelemetrySeconds(3601));
  assert(String(settings.mqttHost())=="mqtt.fixture.invalid"&&settings.telemetrySeconds()==17);

  // Valid checksums cannot make malformed bools/strings/ports authoritative.
  const Stored good=record(active);
  // Old-layout migration is read-only until the next explicit save, preserves
  // all existing credentials, and cannot opt a board in to notifications.
  {
    Stored old=good;old.schema=1;old.checksum=2166136261U;
    auto *raw=reinterpret_cast<uint8_t*>(&old);
    for(size_t i=0;i<offsetof(Stored,checksum);++i)old.checksum=(old.checksum^raw[i])*16777619U;
    fs::FS legacy;auto data=std::make_shared<File::Data>();
    data->bytes.assign(raw,raw+offsetof(Stored,ntfyEnabled));legacy.files["/cloud.bin"]=data;
    CloudConfigStore loaded;assert(loaded.begin(legacy));
    assert(!loaded.ntfyEnabled() && legacy.faults->writes==0);
    assert(String(loaded.mqttPassword())==u8"رمز fixture\npassword" && String(loaded.webSocketToken())==u8"توکن-fixture");
    assert(loaded.setNtfy(true,"ntfy.sh","fixture-private","tk_fixture",true));assert(loaded.save());
    CloudConfigStore restored;assert(restored.begin(legacy));assert(restored.ntfyEnabled() && restored.ntfyTokenSet());
    assert(restored.setNtfy(false,"ntfy.sh","fixture-private","",false));assert(restored.ntfyTokenSet());
    assert(restored.setNtfy(false,"ntfy.sh","fixture-private","",true));assert(!restored.ntfyTokenSet());
  }
  for(const char *bad:{"a/b","bad?x","with space","bad\r\nHeader:x"})
    assert(!settings.setNtfy(true,"ntfy.sh",bad,"",false));
  assert(!settings.setNtfy(true,"https://ntfy.sh","ok","",false));
  assert(!settings.setNtfy(true,"ntfy.sh","","",false));
  assert(!settings.setNtfy(true,"ntfy.sh","ok","bad\r\nHeader",true));
  for (unsigned fault=0;fault<14;++fault) {
    Stored damaged=good;
    switch(fault) {
      case 0: damaged.mqttEnabled=2; break;
      case 1: damaged.mqttTls=2; break;
      case 2: damaged.webSocketEnabled=2; break;
      case 3: damaged.webSocketTls=2; break;
      case 4: damaged.mqttPort=0; break;
      case 5: damaged.webSocketPort=0; break;
      case 6: memset(damaged.mqttPassword,'x',sizeof(damaged.mqttPassword)); break;
      case 7: damaged.mqttUser[0]='\0'; damaged.mqttUser[1]='x'; break;
      case 8: damaged.webSocketToken[0]='\r'; break;
      case 9: damaged.webSocketPath[1]='\n'; break;
      case 10: damaged.mqttHost[0]=' '; break;
      case 11: damaged.mqttBaseTopic[0]='#'; break;
      case 12: damaged.telemetrySeconds=0; break;
      case 13: damaged.telemetrySeconds=3601; break;
    }
    fs::FS bad; writeRecord(bad,damaged);
    const auto old=bad.files.at("/cloud.bin")->bytes;
    CloudConfigStore rejected; assert(!rejected.begin(bad));
    assert(!rejected.mqttEnabled()&&!rejected.webSocketEnabled());
    assert(bad.files.at("/cloud.bin")->bytes==old&&bad.faults->writeOpens==0);
    fs::FS absent; assert(rejected.begin(absent)); assert(String(rejected.lastError()).empty());
  }
  // An orphan pending candidate is never automatically applied after restart.
  fs::FS orphan; orphan.files["/cloud.pending"]=filesystem.files.at("/cloud.bin");
  CloudConfigStore fresh; assert(fresh.begin(orphan)); assert(!fresh.mqttEnabled());
  std::cout << "Cloud config transactional persistence, faults, no-op writes and validation passed\n";
}
