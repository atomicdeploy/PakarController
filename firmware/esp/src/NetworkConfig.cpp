#include "NetworkConfig.h"
#include "ConfigurationChanges.h"

#include <stddef.h>
#include <string.h>

namespace {
constexpr char kPath[] = "/network.bin";
constexpr char kPending[] = "/network.pending";
constexpr uint32_t kMagic = 0x504B4E54UL;  // PKNT
constexpr uint8_t kSchema = 4;
struct AddressedRecord {
  uint32_t magic;uint8_t schema,stationEnabled,accessPointEnabled;
  char stationSsid[33],stationPassword[64];uint8_t dhcpEnabled,addresses[5][4];uint32_t checksum;
} __attribute__((packed));

struct LegacyRecord {
  uint32_t magic;
  uint8_t schema;
  uint8_t stationEnabled;
  char stationSsid[33];
  char stationPassword[64];
  uint32_t checksum;
} __attribute__((packed));

struct PreviousRecord {
  uint32_t magic;
  uint8_t schema;
  uint8_t stationEnabled;
  uint8_t accessPointEnabled;
  char stationSsid[33];
  char stationPassword[64];
  uint32_t checksum;
} __attribute__((packed));

uint32_t checksumBytes(const uint8_t *bytes, size_t length) {
  uint32_t value = 2166136261UL;
  for (size_t index = 0; index < length; ++index) {
    value ^= bytes[index];
    value *= 16777619UL;
  }
  return value;
}
}

NetworkConfigStore networkConfig;

uint32_t NetworkConfigStore::checksum(const Record &record) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
  return checksumBytes(bytes, offsetof(Record, checksum));
}

void NetworkConfigStore::defaults() {
  memset(&record_, 0, sizeof(record_));
  record_.magic = kMagic;
  record_.schema = kSchema;
  record_.accessPointEnabled = 1;
  record_.dhcpEnabled = 1;
  record_.checksum = checksum(record_);
}

bool NetworkConfigStore::begin(fs::FS &filesystem) {
  filesystem_ = &filesystem;
  defaults();
  File file = filesystem.open(kPath, "r");
  if (!file) return true;
  if(file.size()==sizeof(AddressedRecord)){
    AddressedRecord old{};
    const bool complete=file.read(reinterpret_cast<uint8_t*>(&old),sizeof(old))==sizeof(old);file.close();
    if(!complete||old.magic!=kMagic||old.schema!=3||old.stationEnabled>1||old.accessPointEnabled>1||old.dhcpEnabled>1||
       old.stationSsid[32]||old.stationPassword[63]||(old.stationEnabled&&!old.stationSsid[0])||
       old.checksum!=checksumBytes(reinterpret_cast<const uint8_t*>(&old),offsetof(AddressedRecord,checksum))){
      error_=F("invalid previous network settings");return false;
    }
    memcpy(&record_,&old,offsetof(AddressedRecord,checksum));record_.schema=kSchema;
    if(!dhcpEnabled()&&!setAddressing(false,stationAddress().toString(),stationGateway().toString(),stationSubnet().toString(),stationDns1().toString(),stationDns2().toString())){defaults();return false;}
    return save();
  }
  if (file.size() == sizeof(PreviousRecord)) {
    PreviousRecord previous{};
    const bool complete = file.read(reinterpret_cast<uint8_t *>(&previous), sizeof(previous)) == sizeof(previous);
    file.close();
    if (!complete || previous.magic != kMagic || previous.schema != 2 ||
        previous.stationEnabled > 1 || previous.accessPointEnabled > 1 ||
        (!previous.stationEnabled && !previous.accessPointEnabled) ||
        (previous.stationEnabled && !previous.stationSsid[0]) ||
        previous.stationSsid[32] != '\0' || previous.stationPassword[63] != '\0' ||
        previous.checksum != checksumBytes(reinterpret_cast<const uint8_t *>(&previous), offsetof(PreviousRecord, checksum))) {
      error_ = F("network settings checksum is invalid; AP-only defaults loaded"); return false;
    }
    record_.stationEnabled = previous.stationEnabled;
    record_.accessPointEnabled = previous.accessPointEnabled;
    memcpy(record_.stationSsid, previous.stationSsid, sizeof(record_.stationSsid));
    memcpy(record_.stationPassword, previous.stationPassword, sizeof(record_.stationPassword));
    return save(); // Retain credentials/mode, default newly added addressing to DHCP.
  }
  if (file.size() == sizeof(LegacyRecord)) {
    LegacyRecord legacy{};
    const bool complete = file.read(reinterpret_cast<uint8_t *>(&legacy),
                                    sizeof(legacy)) == sizeof(legacy);
    file.close();
    const bool valid =
        complete && legacy.magic == kMagic && legacy.schema == 1 && legacy.stationEnabled <= 1 &&
        (!legacy.stationEnabled || legacy.stationSsid[0]) &&
        legacy.stationSsid[sizeof(legacy.stationSsid) - 1] == '\0' &&
        legacy.stationPassword[sizeof(legacy.stationPassword) - 1] == '\0' &&
        legacy.checksum == checksumBytes(
                               reinterpret_cast<const uint8_t *>(&legacy),
                               offsetof(LegacyRecord, checksum));
    if (!valid) {
      error_ = F("legacy network settings checksum is invalid; AP-only defaults loaded");
      return false;
    }
    record_.stationEnabled = legacy.stationEnabled;
    memcpy(record_.stationSsid, legacy.stationSsid, sizeof(record_.stationSsid));
    memcpy(record_.stationPassword, legacy.stationPassword,
           sizeof(record_.stationPassword));
    record_.checksum = checksum(record_);
    return save();
  }
  Record stored{};
  const bool exact = file.size() == sizeof(stored);
  const bool complete = exact && file.read(reinterpret_cast<uint8_t *>(&stored),
                                  sizeof(stored)) == sizeof(stored);
  file.close();
  if (!complete || stored.magic != kMagic || stored.schema != kSchema ||
      stored.stationEnabled > 1 || stored.accessPointEnabled > 1 || stored.dhcpEnabled > 1 ||
      (!stored.stationEnabled && !stored.accessPointEnabled && !ethernetBuild()) ||
      (stored.stationEnabled && !stored.stationSsid[0]) ||
      stored.stationSsid[sizeof(stored.stationSsid) - 1] != '\0' ||
      stored.stationPassword[sizeof(stored.stationPassword) - 1] != '\0' ||
      stored.autoSwitch>1 || stored.checksum != checksum(stored)) {
    error_ = F("network settings checksum is invalid; AP-only defaults loaded");
    return false;
  }
  record_ = stored;
  for(uint8_t i=0;i<3;++i)if(record_.fallbackSsid[i][32]||record_.fallbackPassword[i][63]){defaults();error_=F("invalid saved station profile");return false;}
  if (!dhcpEnabled() && !setAddressing(false, stationAddress().toString(),
      stationGateway().toString(), stationSubnet().toString(), stationDns1().toString(), stationDns2().toString())) {
    defaults(); error_ = F("network static addressing is invalid; AP-only defaults loaded"); return false;
  }
  return true;
}

bool NetworkConfigStore::setStation(const String &ssid, const String &password,
                                    bool enabled, bool replacePassword) {
  if(temporaryStation_){error_=F("Temporary station operation owns network settings");return false;}
  if (ssid.length() != strlen(ssid.c_str()) || password.length() != strlen(password.c_str()) ||
      ssid.length() > 32 || password.length() > 63 || (enabled && ssid.length() == 0)) {
    error_ = F("station SSID must be 1..32 and password 0..63 bytes");
    return false;
  }
  if (!replacePassword && ssid != record_.stationSsid) {
    error_ = F("changing station SSID requires an explicit password or Open choice"); return false;
  }
  for(uint8_t i=0;i<3;++i)if(ssid.length()&&ssid==record_.fallbackSsid[i]){error_=F("duplicate station SSID");return false;}
  memset(record_.stationSsid, 0, sizeof(record_.stationSsid));
  ssid.toCharArray(record_.stationSsid, sizeof(record_.stationSsid));
  if (replacePassword) {
    memset(record_.stationPassword, 0, sizeof(record_.stationPassword));
    password.toCharArray(record_.stationPassword, sizeof(record_.stationPassword));
  }
  record_.stationEnabled = enabled ? 1 : 0;
  record_.checksum = checksum(record_);
  error_ = String();
  return true;
}

bool NetworkConfigStore::setAccessPointEnabled(bool enabled) {
  if(temporaryStation_){error_=F("Temporary station operation owns network settings");return false;}
  if (!enabled && !stationEnabled() && !ethernetBuild()) {
    error_ = F("AP and station cannot both be disabled");
    return false;
  }
  record_.accessPointEnabled = enabled ? 1 : 0;
  record_.checksum = checksum(record_);
  error_ = String();
  return true;
}

bool NetworkConfigStore::save() {
  if(temporaryStation_){error_=F("Temporary station operation owns network settings");return false;}
  if (!stationEnabled() && !accessPointEnabled() && !ethernetBuild()) {
    error_ = F("AP and station cannot both be disabled"); return false;
  }
  if (filesystem_ == nullptr) {
    error_ = F("LittleFS is not mounted");
    return false;
  }
  record_.checksum = checksum(record_);
  // Skip identical writes. Stage, verify and rename instead of truncating the
  // only credential copy; a failed candidate never replaces the active record.
  File current = filesystem_->open(kPath, "r"); Record previous{};
  const bool identical = current && current.size() == sizeof(record_) &&
      current.read(reinterpret_cast<uint8_t *>(&previous), sizeof(previous)) == sizeof(previous) &&
      memcmp(&previous, &record_, sizeof(previous)) == 0;
  if (current) current.close();
  if (identical) { error_ = String(); return true; }
  File file = filesystem_->open(kPending, "w");
  if (!file) {
    error_ = F("cannot create network settings");
    return false;
  }
  const bool complete = file.write(reinterpret_cast<const uint8_t *>(&record_),
                                   sizeof(record_)) == sizeof(record_);
  file.close();
  File verify = filesystem_->open(kPending, "r"); Record readback{};
  const bool verified = verify && verify.size() == sizeof(record_) &&
      verify.read(reinterpret_cast<uint8_t *>(&readback), sizeof(readback)) == sizeof(readback) &&
      memcmp(&readback, &record_, sizeof(readback)) == 0;
  if (verify) verify.close();
  if (!complete || !verified || !filesystem_->rename(kPending, kPath)) {
    error_ = F("network settings commit failed; previous settings preserved"); return false;
  }
  error_ = String(); PakarConfiguration::changes().saved(PakarConfiguration::Network); return true;
}

bool NetworkConfigStore::setAddressing(bool dhcp, const String &address,
    const String &gateway, const String &subnet, const String &dns1, const String &dns2) {
  if(temporaryStation_){error_=F("Temporary station operation owns network settings");return false;}
  uint8_t parsed[5][4]{};
  if (!dhcp) {
    const String values[] = {address, gateway, subnet, dns1, dns2};
    uint32_t numbers[5]{};
    for (unsigned index = 0; index < 5; ++index) {
      IPAddress ip;
      if ((index >= 3 && !values[index].length())) continue;
      if (values[index].length() != strlen(values[index].c_str()) || !ip.fromString(values[index])) {
        error_ = F("static address, gateway, subnet and DNS must be IPv4 values"); return false;
      }
      for (unsigned byte = 0; byte < 4; ++byte) {
        parsed[index][byte] = ip[byte]; numbers[index] = (numbers[index] << 8) | ip[byte];
      }
    }
    const uint32_t hostMask = ~numbers[2];
    if (hostMask < 3 || hostMask == UINT32_MAX || (hostMask & (hostMask + 1)) ||
        (numbers[0] >> 24) == 0 || (numbers[0] >> 24) >= 224 ||
        (numbers[0] & hostMask) == 0 || (numbers[0] & hostMask) == hostMask ||
        (numbers[1] >> 24) == 0 || (numbers[1] >> 24) >= 224 ||
        (numbers[0] & numbers[2]) != (numbers[1] & numbers[2]) ||
        (numbers[1] & hostMask) == 0 || (numbers[1] & hostMask) == hostMask ||
        numbers[0] == numbers[1] || (numbers[3] >> 24) >= 224 || (numbers[4] >> 24) >= 224) {
      error_ = F("invalid static subnet, host, gateway or DNS address"); return false;
    }
  }
  record_.dhcpEnabled = dhcp ? 1 : 0;
  // Preserve the last validated static profile while DHCP is active so the
  // front panel can switch back without re-entering every address.
  if (!dhcp) memcpy(record_.addresses, parsed, sizeof(parsed));
  record_.checksum = checksum(record_); error_ = String(); return true;
}

bool NetworkConfigStore::configure(const char *hostname,
                                   const char *apPassword) {
  if(temporaryStation_){error_=F("Temporary station operation owns network settings");return false;}
  if (!hostname || !*hostname || strlen(hostname)>63) {
    error_ = F("network hostname must be 1..63 bytes");return false;
  }
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
#if !defined(ESP32)
  WiFi.setAutoConnect(false);
#endif
  WiFi.mode(WIFI_OFF);
  WiFi.disconnect(true);
  WiFi.softAPdisconnect(true);
  delay(10);
  const bool apEnabled = accessPointEnabled();
  if (!apEnabled && !stationEnabled()) {
    // Do not accidentally select WIFI_STA for a persisted Ethernet-only mode.
    // A build without Ethernet cannot load/save such a record in the first place.
    error_ = String();
    return false; // AP is genuinely not running, not a network failure.
  }
#if defined(ESP32)
  // ASA0002E WiFi_Setup applies its persisted identity BEFORE enabling Wi-Fi.
  // Arduino-ESP32 mode() copies Network's default into the new STA netif.
  if (!WiFi.setHostname(hostname)) { error_ = F("Cannot set network hostname"); return false; }
  // Set the SDK's retained policy before STA starts, and reapply below once
  // started: mode changes/reconnects must not restore implicit modem sleep.
  WiFi.setSleep(PAKAR_S3_WIFI_MODEM_SLEEP != 0);
#endif
  if (!WiFi.mode(apEnabled ? (stationEnabled() ? WIFI_AP_STA : WIFI_AP) : WIFI_STA)) {
    error_ = F("Cannot start requested Wi-Fi mode"); return false;
  }
#if defined(ESP32)
  const bool sleepPolicyApplied = WiFi.setSleep(PAKAR_S3_WIFI_MODEM_SLEEP != 0);
  // Network's global buffer is only 32 bytes in SDK 3.3.12. Apply the full
  // validated DNS label to each netif before addressing/association as well.
  if ((stationEnabled() && (!WiFi.STA.setHostname(hostname) ||
          !WiFi.STA.getHostname() || strcmp(WiFi.STA.getHostname(), hostname) != 0)) ||
      (apEnabled && (!WiFi.softAPsetHostname(hostname) ||
          !WiFi.softAPgetHostname() || strcmp(WiFi.softAPgetHostname(), hostname) != 0))) {
    WiFi.mode(WIFI_OFF);
    error_ = F("Wi-Fi hostname was not applied; interfaces stopped"); return false;
  }
#else
  if (!WiFi.hostname(hostname)) { error_ = F("Cannot set station hostname"); return false; }
#endif
  bool ap = false;
  // DNS host labels permit63bytes, IEEE802.11 SSIDs only32. The SDK copies32
  // but uses strlen(input) for ssid_len; pass an explicitly bounded AP name.
  // The full persisted hostname/netif/mDNS identity is never silently renamed.
  const String apSsid=String(hostname).substring(0,32);
  if (apEnabled) ap = WiFi.softAP(apSsid.c_str(), apPassword);
  if (stationEnabled()) {
    activeProfile_=0;attemptAt_=millis();scanAt_=attemptAt_;wasConnected_=false;switchScan_=false;
    const bool addressed = dhcpEnabled()
        ? WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0))
        : WiFi.config(stationAddress(), stationGateway(), stationSubnet(), stationDns1(), stationDns2());
    if (!addressed) { error_ = F("station IP configuration failed"); return ap; }
    WiFi.begin(record_.stationSsid, record_.stationPassword);
    WiFi.setAutoReconnect(true);
#if !defined(ESP32)
    WiFi.setAutoConnect(true);
#endif
  }
  if (apEnabled && !ap) error_ = F("access point startup failed");
#if defined(ESP32)
  else if (!sleepPolicyApplied) error_ = F("Wi-Fi latency policy was not applied");
#endif
  else error_ = String();
  // Return AP availability for existing callers; STA-only is not an AP startup.
  return ap;
}

bool NetworkConfigStore::setFallback(uint8_t slot,const String &ssid,const String &password){
  if(temporaryStation_){error_=F("Temporary station operation owns network settings");return false;}
  if(slot>=3||ssid.length()>32||password.length()>63||ssid.length()!=strlen(ssid.c_str())||password.length()!=strlen(password.c_str())||
      (password.length()&&password.length()<8)||(!ssid.length()&&password.length())){error_=F("invalid fallback credentials");return false;}
  if(ssid.length())for(uint8_t i=0;i<4;++i)if(i!=slot+1&&ssid==profileSsid(i)){error_=F("duplicate station SSID");return false;}
  memset(record_.fallbackSsid[slot],0,33);memset(record_.fallbackPassword[slot],0,64);
  ssid.toCharArray(record_.fallbackSsid[slot],33);password.toCharArray(record_.fallbackPassword[slot],64);return true;
}

void NetworkConfigStore::service(uint32_t now){
  if(temporaryStation_)return;
  if(!stationEnabled())return;
  uint8_t next=activeProfile_;
  if(stationConnected()){
    wasConnected_=true;attemptAt_=now;
    if(!autoSwitch()||!activeProfile_)return;
    // Opt-in return to an earlier priority only after observing its SSID.
    // Reuse the SDK async scan; never block the game loop or stop the AP.
    if(!switchScan_){if(uint32_t(now-scanAt_)<60000U)return;scanAt_=now;
      if(WiFi.scanComplete()==-1)return;
      WiFi.scanNetworks(true,true);switchScan_=true;return;}
    const int count=WiFi.scanComplete();if(count==-1)return;switchScan_=false;
    for(uint8_t priority=0;priority<activeProfile_;++priority){
      if(!profileSsid(priority)[0])continue;
      for(int i=0;i<count;++i)if(WiFi.SSID(i)==profileSsid(priority)){next=priority;break;}
      if(next!=activeProfile_)break;
    }
    if(next==activeProfile_)return;
  }else{
    if(wasConnected_){wasConnected_=false;attemptAt_=now;}
    if(uint32_t(now-attemptAt_)<15000U)return;
    for(uint8_t step=1;step<=4;++step){const uint8_t candidate=(activeProfile_+step)%4;if(profileSsid(candidate)[0]){next=candidate;break;}}
  }
  activeProfile_=next;attemptAt_=now;switchScan_=false;
  WiFi.disconnect(false);
  WiFi.begin(profileSsid(next),next?record_.fallbackPassword[next-1]:record_.stationPassword);
}

#if defined(ESP32)
bool NetworkConfigStore::beginTemporaryStation(const String &ssid,const String &password,const uint8_t bssid[6]) {
  if(temporaryStation_||!bssid||ssid.isEmpty()||ssid.length()>32||password.length()>63||
      ssid.length()!=strlen(ssid.c_str())||password.length()!=strlen(password.c_str())||
      (password.length()&&password.length()<8))return false;
  uint8_t any=0;for(unsigned i=0;i<6;++i)any|=bssid[i];
  if(!any||(bssid[0]&1U))return false;
  temporaryStation_=true;
  WiFi.persistent(false);WiFi.setAutoReconnect(false);
  // Both boards normally own 192.168.4.1 on their AP. Disable ONLY the
  // commissioning board's AP temporarily to avoid routing to itself.
  WiFi.disconnect(false);WiFi.softAPdisconnect(true);
  if(!WiFi.mode(WIFI_STA)||!WiFi.config(IPAddress(0,0,0,0),IPAddress(0,0,0,0),IPAddress(0,0,0,0))){
    error_=F("Temporary station setup failed; restore required");return false;
  }
  WiFi.setSleep(PAKAR_S3_WIFI_MODEM_SLEEP!=0);
  WiFi.begin(ssid.c_str(),password.c_str(),0,bssid,true);
  error_=String();return true;
}
bool NetworkConfigStore::temporaryStationReady(const String &ssid,const uint8_t bssid[6])const {
  return temporaryStation_&&bssid&&WiFi.status()==WL_CONNECTED&&WiFi.SSID()==ssid&&
      WiFi.BSSID()&&!memcmp(WiFi.BSSID(),bssid,6);
}
bool NetworkConfigStore::restoreSavedNetwork(const char *hostname,const char *apPassword) {
  // Safe even when the temporary setup failed before association. configure()
  // uses only the untouched persisted record, never commissioning credentials.
  temporaryStation_=false;
  configure(hostname,apPassword);
  return error_.isEmpty();
}
#endif
