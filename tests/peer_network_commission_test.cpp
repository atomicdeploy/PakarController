#define ARDUINO_ARCH_ESP32
#define pdMS_TO_TICKS(value) (value)
#include "../firmware/esp/src/PeerNetworkCommission.h"
#include <cassert>
#include <cstdio>
static bool joinOk=true,joinReady=true,restoreOk=true,restoreReady=true;
static unsigned joinCalls=0,restoreCalls=0;
static bool join(const String &ssid,const String &password,const uint8_t *bssid){
  ++joinCalls;assert(ssid=="keycontroller"&&password=="private-ap-password"&&bssid[0]==0xba);return joinOk;
}
static bool joined(const String&,const uint8_t*){return joinReady;}
static bool restore(){++restoreCalls;return restoreOk;}
static bool restored(){return restoreReady;}
static PeerNetworkCommission::Hooks hooks{join,joined,restore,restored};
static JsonDocument decode(const String &raw){JsonDocument d;assert(!deserializeJson(d,raw));return d;}
static String encode(const JsonDocument &d){String raw;serializeJson(d,raw);return raw;}
static String request(unsigned number=1){JsonDocument d;
  char id[33];snprintf(id,sizeof(id),"%032x",number);d["request_id"]=id;d["confirm"]="configure-peer-network";
  d["board_id"]="esp32s3-b81f3fbb0684";d["hostname"]="keycontroller";d["role"]="first-wins";
  d["ap_ssid"]="keycontroller";d["ap_password"]="private-ap-password";d["ap_bssid"]="ba:1f:3f:bb:06:84";
  d["station_ssid"]="Infrastructure";d["station_password"]="private-station-password";return encode(d);
}
static String info(){return "{\"application\":\"PakarController\",\"version\":\"0.2.8\",\"board_id\":\"esp32s3-b81f3fbb0684\",\"boot_id\":\"1234567890abcdef1234567890abcdef\",\"hostname\":\"keycontroller\"}";}
static void reply(const String &body,int code=200){TestReply r;r.body=body;r.code=code;replies.push_back(r);}
static void goodPrelude(){reply(info());reply("{\"capabilities\":{\"role\":\"first-wins\"}}");reply("{\"station_enabled\":false,\"ap_enabled\":true,\"dhcp\":true,\"station_ssid\":\"\",\"password\":\"must-not-persist\"}");}
static void clean(){requests.clear();replies.clear();joinCalls=restoreCalls=0;joinOk=joinReady=restoreOk=restoreReady=true;testNow=100;}
static void run(PeerNetworkCommission &job){const auto notified=taskNotifications;
  assert(job.start()&&!job.start()&&taskNotifications==notified+1);runRelayWorker();assert(!job.busy());assert(!job.start());assert(restoreCalls==1);}
int main(){
  const String self="esp32s3-907069eaaf34";
  {
    clean();fs::FS fs;PeerNetworkCommission job;assert(job.begin(fs,true,hooks));
    assert(decode(job.submit(request(),self))["accepted"]==true&&job.busy());
    assert(joinCalls==0&&requests.empty()); // Acceptance alone never switches Wi-Fi.
    goodPrelude();reply("{\"accepted\":true}",202);reply("{\"station_enabled\":true,\"ap_enabled\":true,\"dhcp\":true,\"station_ssid\":\"Infrastructure\"}");run(job);
    auto result=decode(job.snapshot());assert(result["phase"]=="complete"&&result["settings_verified"]==true&&result["restore_ready"]==true);
    assert(requests.size()==5&&requests[3].method=="POST"&&requests[3].headers["X-Pakar-Boot"]=="1234567890abcdef1234567890abcdef");
    assert(requests[3].headers["Host"]=="keycontroller.local"&&requests[3].address=="192.168.4.1");
    assert(requests[3].body.find("password=%70%72%69%76%61%74%65")!=String::npos);
    assert(job.snapshot().find("private-")==String::npos&&job.snapshot().find("must-not-persist")==String::npos);
    const auto sent=requests.size();assert(decode(job.submit(request(),self))["phase"]=="complete"&&requests.size()==sent);
    PeerNetworkCommission reboot;assert(reboot.begin(fs,true,hooks));assert(decode(reboot.submit(request(),self))["phase"]=="complete"&&!reboot.busy());
    restoreCalls=0;
    assert(decode(reboot.submit(request(2),self))["accepted"]==true);
    goodPrelude();reply("{\"accepted\":true}",202);reply("{\"station_enabled\":true,\"ap_enabled\":true,\"dhcp\":true,\"station_ssid\":\"Infrastructure\"}");run(reboot);
    assert(decode(reboot.submit(request(),self))["error"]=="receipt_retired_do_not_replay");
    assert(reboot.snapshot().find("private-")==String::npos);
  }
  {
    clean();fs::FS fs;PeerNetworkCommission job;job.begin(fs,true,hooks);job.submit(request(),self);
    goodPrelude();reply("",-1);run(job);auto result=decode(job.snapshot());
    assert(result["phase"]=="uncertain"&&result["mutation_dispatched"]==true&&requests.size()==4);
    job.submit(request(),self);assert(requests.size()==4); // Uncertain POST never replayed.
  }
  {
    clean();fs::FS fs;PeerNetworkCommission job;job.begin(fs,true,hooks);job.submit(request(),self);
    reply(info());reply("{\"capabilities\":{\"role\":\"table-selection\"}}");run(job);
    assert(decode(job.snapshot())["error"]=="peer_role_not_verified"&&requests.size()==2);
  }
  {
    clean();fs::FS fs;PeerNetworkCommission job;job.begin(fs,true,hooks);job.submit(request(),self);
    TestReply wrong;wrong.body=info();wrong.board="different";replies.push_back(wrong);run(job);
    assert(decode(job.snapshot())["error"]=="peer_identity_not_verified"&&requests.size()==1);
  }
  {
    clean();fs::FS fs;PeerNetworkCommission job;job.begin(fs,true,hooks);joinReady=false;
    job.submit(request(),self);run(job);assert(requests.empty()&&testNow<51000);
    assert(decode(job.snapshot())["error"]=="target_ap_join_timeout");
  }
  {
    clean();fs::FS fs;PeerNetworkCommission job;job.begin(fs,true,hooks);job.submit(request(),self);
    joinOk=false;restoreReady=false;run(job);auto result=decode(job.snapshot());
    assert(result["restore_requested"]==true&&result["restore_ready"]==false&&testNow<51000);
  }
  {
    clean();fs::FS fs;PeerNetworkCommission job;job.begin(fs,true,hooks);fs.failWrite=true;
    assert(decode(job.submit(request(),self))["error"]=="receipt_storage_failed"&&!job.busy()&&joinCalls==0);
  }
  {
    clean();fs::FS fs;PeerNetworkCommission job;job.begin(fs,true,hooks);
    auto invalid=decode(request());invalid["ap_bssid"]="ff:ff:ff:ff:ff:ff";
    assert(decode(job.submit(encode(invalid),self))["error"]=="invalid_commissioning_target");
    invalid=decode(request());invalid["hostname"]="keycontroller.local";
    assert(decode(job.submit(encode(invalid),self))["error"]=="invalid_commissioning_target");
    assert(!job.busy()&&joinCalls==0&&requests.empty());
  }
  {
    clean();fs::FS fs;PeerNetworkCommission job;job.begin(fs,true,hooks);job.submit(request(),self);
    // Simulated reset after durable admission does not notify/execute a worker.
    PeerNetworkCommission reboot;assert(reboot.begin(fs,true,hooks));auto result=decode(reboot.snapshot());
    assert(result["phase"]=="interrupted"&&result["busy"]==false&&result["restart_resumed"]==false);
    assert(!reboot.busy()&&joinCalls==0&&requests.empty());
  }
  for(const size_t bytes:{size_t(0),size_t(4097)}){
    clean();fs::FS fs;File journal=fs.open("/peer-commission.json","w");
    journal.print(String(std::string(bytes,'x')));journal.close();
    PeerNetworkCommission job;assert(!job.begin(fs,true,hooks));
    assert(decode(job.submit(request(),self))["error"]=="commissioning_unavailable");
    assert(!job.busy()&&joinCalls==0&&requests.empty());
  }
  puts("Detached peer network commissioning fixtures passed");
}
