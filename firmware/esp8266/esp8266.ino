#include <Arduino.h>
#include <ArduinoJson.h>
#include <utility>
#include <memory>
#include <new>
#if defined(ARDUINO_ARCH_ESP32)
#include <sys/socket.h>
#include <errno.h>
#endif
#include "src/ShellSettings.h"
#include "src/ShellBuild.h"
#include "src/ShellWire.h"
#include "src/ShellTui.h"
#include <CommandReceipts.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <esp_wifi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <Update.h>
#include <firmware/esp32s3/src/BoardPins.h>
#include <firmware/esp32s3/src/AvrUart.h>
#include <firmware/esp32s3/src/Discovery.h>
#include <firmware/esp32s3/src/DnsRecovery.h>
#include <firmware/esp32s3/src/StatusRgb.h>
#include <firmware/esp32s3/src/StatusRgbConfig.h>
#include <firmware/esp32s3/src/StatusRgbOutput.h>
#include <firmware/esp32s3/src/LcdPort.h>
#include <firmware/esp32s3/src/BleService.h>
#else
#include <ESP8266SSDP.h>
#include <ESP8266NetBIOS.h>
#include <ESPAsyncTCP.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <SoftwareSerial.h>
#include <Updater.h>
#include <coredecls.h>
#endif
#include "src/MndpService.h"
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFiUdp.h>
#include <time.h>
#include "src/SystemPlatform.h"
#include "src/FirmwarePlatform.h"
#include "src/UpdateProgress.h"
#include "src/UpgradeLifecycle.h"
#include "src/LocalOtaReceipt.h"
#include "src/EspFrontPanel.h"
#include "src/SemanticPanel.h"

#include <BuildInfo.h>
#include <PeerCompatibility.h>
#include <AudioCues.h>
#include <AttentionCues.h>
#include <Protocol.h>
#include <AvrPeerHealth.h>
#include <LedOutputControl.h>
#include <DeviceContext.h>
#include <PcContractTransport.h>
#include <PcContractAssembly.h>
#include <PcLedContract.h>
#include <SerialLink.h>

#include "src/AvrIsp.h"
#include "src/RemoteFirmware.h"
#include "src/ClientTelemetry.h"
#include "src/AsyncHttpService.h"
#include "src/WebAccess.h"
#include "src/FirmwareTransfer.h"
#include "src/CloudConfig.h"
#include "src/CloudServices.h"
#include "src/NtfyService.h"
#include "src/CloudAnimationProgress.h"
#include "src/LedGrid.h"
#include "src/LedRenderAdmission.h"
#include "src/LoopMessages.h"
#include "src/HostFrameIngress.h"
#include "src/LedPreferences.h"
#include "src/SceneStore.h"
#include "src/LedScene.h"
#include "src/AvrSceneSync.h"
#include "src/NetworkConfig.h"
#include "src/NetworkPath.h"
#include "src/CommandValues.h"
#if defined(ARDUINO_ARCH_ESP32)
#include "src/EthernetConfig.h"
#endif
#include "src/DeviceIdentity.h"
#include "src/ApplicationIdentity.h"
#include "src/BusyLine.h"
#include "src/TelnetShell.h"
#if defined(ARDUINO_ARCH_ESP8266)
#include "src/WebUiCompressed.h"
#else
#include "src/WebUi.h"
#endif
#include "src/BasicUi.h"
#include "src/FontAssets.h"

using namespace BoardCore::Protocol;

#ifndef PAKAR_USE_BUSY_WIRE
#if defined(ARDUINO_ARCH_ESP32)
#define PAKAR_USE_BUSY_WIRE 0
#else
#define PAKAR_USE_BUSY_WIRE 1
#endif
#endif

#if PAKAR_USE_BUSY_WIRE != 0 && PAKAR_USE_BUSY_WIRE != 1
#error "PAKAR_USE_BUSY_WIRE must be 0 or 1"
#endif

#ifndef PAKAR_AVR_LINK_ENABLED
#define PAKAR_AVR_LINK_ENABLED 1
#endif
#if PAKAR_AVR_LINK_ENABLED != 0 && PAKAR_AVR_LINK_ENABLED != 1
#error "PAKAR_AVR_LINK_ENABLED must be 0 or 1"
#endif
#if defined(ARDUINO_ARCH_ESP32)
#include <firmware/esp32s3/src/PassiveAvrRx.h>
#endif

namespace {

constexpr uint32_t kBaud = 38400;
constexpr uint16_t kStreamPort = 7890;
#if defined(ARDUINO_ARCH_ESP32)
constexpr uint8_t kBusyPin = 15; // Unused by the S3 buffered profile.
constexpr uint8_t kAvrResetPin = PAKAR_S3_ISP_RESET;
constexpr uint8_t kAvrRxPin = PAKAR_S3_AVR_RX;
PakarS3::Discovery s3Discovery;
PakarS3::StatusRgb statusRgb;
PakarS3::StatusRgbConfig statusRgbConfig;
PakarS3::LcdPort lcdPort;
PakarS3::BleService bleService;
#if !PAKAR_AVR_LINK_ENABLED
PakarS3::PassiveAvrRx passiveAvrRx;
std::atomic<bool> passiveCaptureGuard{false};
#endif
bool writeStatusRgb(int pin, uint8_t red, uint8_t green, uint8_t blue) {
  return PakarS3::writeStatusPixel(pin, red, green, blue);
}
#else
constexpr uint8_t kBusyPin = D3;  // GPIO0 -> AVR D10/PB2, HIGH blocks AVR TX.
constexpr uint8_t kAvrResetPin = D1;
constexpr uint8_t kAvrRxPin = D4;
#endif
constexpr char kDevelopmentPassword[] = "pakarcontroller";
constexpr char kAvrImagePath[] = "/avr/staged.bin";
constexpr char kBackupDirectory[] = "/avr/backup";
constexpr char kLedPreferencesPath[] = "/led.cfg";
constexpr char kCellMappingPath[] = "/cells.map";

bool passiveCaptureActive() {
#if defined(ARDUINO_ARCH_ESP32) && !PAKAR_AVR_LINK_ENABLED
  return passiveCaptureGuard.load(std::memory_order_acquire);
#else
  return false;
#endif
}

struct CellMappingRecord {
  uint8_t magic0;
  uint8_t magic1;
  uint8_t schema;
  uint8_t physical[LedGrid::kCells];
  char names[LedGrid::kCells][16];
  uint16_t check;
};

AsyncHttpService server(80);
AsyncWebSocket webSocket("/ws");
WebAccess webAccess;
WiFiUDP stream;
WiFiServer telnetServer(23);
ShellSettings shellSettings;
#if defined(ARDUINO_ARCH_ESP32)
PakarS3::AvrUart avrUart;
BoardCore::SerialLink &link = avrUart.frames();
#else
SoftwareSerial avrSerial(D4, D0);
BoardCore::SerialLink link(static_cast<Stream &>(avrSerial));
#endif
BoardCore::SerialLink &avrLink = link; // Unambiguous outside namespace vs POSIX link().
#if defined(ARDUINO_ARCH_ESP32) && !defined(PAKAR_S3_HOST_NATIVE_USB)
// CH343/UART socket remains a real independent PC path on UART0; UART1 is AVR.
BoardCore::SerialLink hostLink(Serial0);
#else
BoardCore::SerialLink hostLink(Serial);
#endif
HostFrameIngress hostFrameIngress;
bool hostCallerAdmitted=false;
AvrIsp avrIsp(kAvrResetPin);
RemoteFirmware remoteFirmware;
PakarHttpDetail::Processing urbootBusy;
bool avrRecoveryHelloPending=false;
uint8_t avrRecoveryHelloSequence=0;
bool urbootQueued=false,urbootRawOwned=false,urbootDetected=false;
bool urbootProgrammed=false,urbootVerified=false,urbootRecovered=false,avrWriteAttempted=false;
uint32_t urbootJobId=0,urbootDone=0,urbootTotal=0,urbootPublishedAt=0;
const char *urbootPhase="idle";
String urbootOperation,urbootError,urbootImageSha,avrMaintenanceMethod="urboot";
String avrMaintenanceRequest,avrMaintenanceSeen[64];
uint8_t avrMaintenanceSeenCount=0;
uint64_t avrMaintenanceDeadline=0;
String urbootJobJson();
bool urbootSupported();
#if defined(ARDUINO_ARCH_ESP32)
std::atomic<unsigned> artifactReaders{0};
#else
unsigned artifactReaders=0; // Cooperative single-core owner; no unsupported LX106 atomic RMW.
#endif
struct FirmwareReaderLease {
  FirmwareReaderLease(){++artifactReaders;}
  ~FirmwareReaderLease(){--artifactReaders;}
};
BusyLine<PAKAR_USE_BUSY_WIRE != 0> busyLine(kBusyPin);
LedGrid grid;
SceneStore<fs::FS> sceneStore;
AvrSceneSync avrSceneSync;
LoopMessages loopMessages;
File uploadFile;
uint32_t avrUploadOwner = 0;
String hostname;
bool avrOnline = false;
bool avrVersionKnown = false;
bool avrRoleKnown=false,avrRoleCompatible=true;
uint8_t avrGameRole=0;
uint8_t avrAudioCueCount = 0;
uint16_t avrCapabilities = 0;
uint32_t avrDisplayStatusAt = 0;
bool avrDisplayStatusKnown = false;
bool avrPcSupported = false;
uint16_t avrPcFeatures = 0;
BoardCore::PcContractTransport pcTransport;
String pcBootId;
String pcBoardId;
uint32_t lastAvrPingAt = 0;
uint8_t avrVersion[3] = {};
bool apStarted = false;
GameMode mode = static_cast<GameMode>(BoardCore::GameRole::mode);
uint8_t winner = 0;
uint8_t selectedCell = 1;
uint8_t selectedState = kCellWhite;
uint8_t sequence = 1;
uint32_t lastAvrFrameAt = 0;
uint32_t lastHelloAt = 0;
uint32_t lastStatusPollAt = 0;
uint32_t lastDiagnosticAt = 0;
uint32_t ledResumeAt = 0;
uint16_t avrUartErrors = 0;
bool tm1637Ack = false;
uint8_t tm1637Segments[4] = {};
uint8_t tm1637Brightness = 0;
bool tm1637HostActive = false;
bool avrMaintenancePanel = false;
bool avrToneTimestamps = false, avrPanelKnown = false;
uint32_t avrSession = 0, mirrorSequence = 0;
uint8_t avrMenuPage = 0;
bool avrMenuActive = false, avrRunning = false, avrCaptured = false;
uint16_t avrPanelMeta = 0;
bool gameStateInitialized = false;
uint8_t lastCellActionRevision = 0;
uint8_t lastResetRevision = 0;
bool programmingLatched = false;
uint8_t activeInputs = 0;
bool activeInput9 = false;
uint8_t activeOutputs = 0;
bool awaitedReplySeen = false;
bool awaitedReplyOk = false;
uint8_t awaitedSequence = 0;
uint8_t awaitedOpcode = 0;
String consoleLine;
bool consoleLineMode = false;
bool networkApplyPending = false;
uint32_t networkApplyAt = 0;
bool cloudApplyPending = false;
uint32_t cloudApplyAt = 0;
bool ethernetApplyOnly = false;
#if defined(ARDUINO_ARCH_ESP32)
EthernetConfigStore ethernetConfig;
#endif
uint32_t lastWebSocketBroadcastAt = 0;
bool ntpConfigured = false;
bool ntpObserved = false;
uint32_t lastClockSetAt = 0;
const char *clockSource = "unset";
uint8_t avrTelemetry[46] = {};
uint32_t avrTelemetryAt = 0, lastTelemetryRequestAt = 0;
bool avrTelemetryKnown = false;
bool avrResetTelemetryKnown = false, avrBootCountPersisted = false;
uint8_t avrResetCause = 0;
uint32_t avrBootCount = 0, avrResetTelemetryAt = 0;
bool mdnsStarted = false;
bool ssdpStarted = false;
MndpService mndpService;
uint32_t discoveryAttemptAt = 0;
bool netbiosStarted = false;
IPAddress discoveryAddress;
String stagedStationSsid;
String stagedStationPassword;
String stagedAccessUser;
String stagedAccessPassword;
String stagedDeviceName;
String stagedDisplayName;
#if PAKAR_SNAKE_ENABLED
uint8_t snakeBody[LedGrid::kCells] = {};
uint8_t snakeLength = 0;
uint8_t snakeFood = 0;
uint8_t snakeDirection = 1;
uint32_t lastSnakeAt = 0;
bool snakeAlive = false;
#else
constexpr uint8_t snakeLength = 0;
constexpr bool snakeAlive = false;
#endif
std::atomic<bool> otaActive{false};
BoardCore::Attention::Upload uploadAttention;
BoardCore::Attention::Remote remoteAttention;
bool remoteCueLead=false;
uint32_t remoteCueUntil=0;
std::atomic<uint32_t> otaRebootAt{0};
size_t otaUploadedBytes = 0;
uint32_t otaOwner = 0;
PakarUpdate::Progress otaProgress;
PakarUpdate::Completion otaCompletion;
PakarPanel::Presenter semanticPanel;
bool semanticPanelSending=false;
PakarUpdate::Lifecycle upgradeLifecycle;
LocalOtaReceipt<fs::FS> localOtaReceipt;
bool localBootNoticePending=false,localBootCuePending=false;
uint32_t localBootCueUntil=0;
uint32_t localObservedOperation=0;
bool servicingUpgrade=false;
uint32_t otaNextOwner=0;
String pendingUpgradeCloud;
String pendingDisplayCloud;
uint32_t displayCloudCoalesced=0,lastDisplayCloudAt=0;
const char *localUpgradeSound="none";
uint32_t otaExpectedBytes = 0;
CellMappingRecord cellMapping{};

void saveLedPreferences();
void serviceScenePersistence(bool flush = false);
void loadCellMapping();
bool scheduleDeviceRename(const String &name);
bool scheduleDeviceIdentity(const String &name, const String &displayName);
uint32_t identityRevision();
String applicationIdentityJson();
void appendJsonEscaped(String &body, const char *text);
String deviceNameJson(bool accepted = false);
bool saveCellMapping();
String buildStatusJson();
String buildStatusJson(bool compact);
String panelStatusJson();
void serviceSemanticPanel(uint32_t now);
void closeDeviceShells();
void beginDeviceShells();
void serviceDeviceShells();
unsigned shellConnectionCount();
uint16_t shellAdvertisedPort();
String shellStatusJson();
bool handleShellFrame(const BoardCore::Protocol::Frame &frame);
bool updateShellConfiguration(JsonObjectConst patch,String &result);
void observePanelAction(uint8_t kind,uint16_t value,uint8_t detail);
#if defined(ARDUINO_ARCH_ESP32)
String bleStatusJson();
String bleRequest(const String &message,uint32_t epoch);
void serviceBle();
void serviceBleReadOnly();
void registerBleApi();
void publishBleEvent(const String &event);
#endif
String localUpdateJson();
String upgradeJson(bool event=false,bool phaseChanged=false);
void serviceUpgrade();
bool flushUpgradeScene();
void upgradeBackground();
bool upgradeRebootReady();
uint32_t runningImageSize();
String runningImageMd5();
bool runningImageIntegrity();
String ethernetStatusJson(bool compact);
String lcdStatusJson();
String buildUsbTableSnapshot();
String buildTelemetryJson();
String avrVersionText();
bool recordClientReport(const String &report,const String &peer,const String &agent=String(),const String &version=String());
void registerMaintenanceRoutes();
bool prepareRemotePair(const RemoteFirmware::Plan&,String&);
bool programRemoteAvr(const char*,const String&,String&);
int8_t finishRemotePair(const RemoteFirmware::Plan&,bool,String&);
bool handleCloudCommand(const String &message, String &result);
bool remoteCommand(const JsonDocument &request,String &result);
bool handleMaintenanceShell(const String&,String&);
bool strictU32(const String&,uint32_t&);
void startSnake();
bool setSnakeDirection(uint8_t direction);
void serviceSnake(uint32_t now);
void serviceDiscovery();
void registerAsyncOta();
bool sendToAvr(uint8_t opcode, uint8_t requestSequence,
               const uint8_t *payload = nullptr, uint8_t payloadLength = 0);
String pcReceiptJson(const BoardCore::PcContractTransport::Receipt &receipt);
void publishPcEvent(const String &event,bool displaySnapshot=false) {
#if defined(ARDUINO_ARCH_ESP32)
  publishBleEvent(event);
#endif
  webSocket.textAll(event);
  // Raw masks are latest-state snapshots, not ordered tone edges. Never let
  // an Internet publish stall AVR RX/LED admission for every marquee window.
  if(displaySnapshot) {
    if(pendingDisplayCloud.length())++displayCloudCoalesced;
    pendingDisplayCloud=event;
  } else cloudServices.publishEvent(event); // Preserve ordered command/tone semantics.
}
void servicePcContracts();
void serviceAvrFor(uint32_t durationMs);
void setAvrTransmitBlocked(bool blocked);
EspFrontPanel espFrontPanel;
bool wifiScanQueued = false;
uint32_t lastEspPanelOfferAt = 0;
class EspPanelSettings : public EspFrontPanel::Settings {
 public:
  bool read(BoardCore::Protocol::EspPanel::Snapshot &state) const override {
    if (!deviceIdentity.ready()) return false;
    state.mode = (networkConfig.accessPointEnabled() ? 1 : 0) | (networkConfig.stationEnabled() ? 2 : 0);
    state.dhcp = networkConfig.dhcpEnabled();
    state.stationConfigured = strlen(networkConfig.stationSsid()) != 0;
    state.stationConnected = networkConfig.stationConnected();
    // Bounded physical-display preview only; never used as a network identity.
    strlcpy(state.hostname, deviceIdentity.hostname(), sizeof(state.hostname));
    return state.mode != 0;
  }
  Error setMode(uint8_t value) override {
    if (busy()) return kBusy;
    NetworkConfigStore candidate = networkConfig;
    if (!candidate.setStation(candidate.stationSsid(), String(), (value & 2) != 0, false) ||
        !candidate.setAccessPointEnabled((value & 1) != 0)) return kUnsafe;
    return commit(candidate);
  }
  Error setDhcp(bool enabled) override {
    if (busy()) return kBusy;
    NetworkConfigStore candidate = networkConfig;
    if (!candidate.setAddressing(enabled, candidate.stationAddress().toString(), candidate.stationGateway().toString(),
        candidate.stationSubnet().toString(), candidate.stationDns1().toString(), candidate.stationDns2().toString())) return kUnsafe;
    return commit(candidate);
  }
  Error reconnect() override {
    if (busy()) return kBusy;
    networkApplyPending = true; networkApplyAt = millis() + 250U; return kNoError;
  }
  Error scan() override {
    if (busy()) return kBusy;
    wifiScanQueued = true; return kNoError; // Queued, never a claim that scan completed.
  }
 private:
  static bool busy() { return urbootBusy.get() || remoteFirmware.busy() || otaActive || otaRebootAt || avrUploadOwner || programmingLatched || networkApplyPending || cloudApplyPending; }
  static Error commit(NetworkConfigStore &candidate) {
    if (!candidate.save()) return kHardwareUnavailable;
    networkConfig = candidate;
    networkApplyPending = true; networkApplyAt = millis() + 250U; return kNoError;
  }
} espPanelSettings;
class PcStripOutput {
 public:
  bool available() { return grid.canStage(0); }
  void clear() { grid.fill(kCellOff); }
  bool stage(uint8_t index, const uint8_t *rgb, uint8_t count) { return grid.stagePixels(index, rgb, count); }
  void brightness(uint8_t value) { grid.setBrightness(value); }
  bool commit() {
    setAvrTransmitBlocked(true);
#if !defined(ARDUINO_ARCH_ESP32)
    serviceAvrFor(18);
#endif
    const bool committed=grid.commitStaged(0);
    setAvrTransmitBlocked(false);
    return committed;
  }
} pcStripOutput;
BoardCore::PcLedContract<PcStripOutput> pcStrip(pcStripOutput);

uint16_t currentFeatures() {
  uint16_t available = BoardCore::Context::features(avrOnline && avrVersionKnown,
      avrCapabilities, tm1637Ack, avrDisplayStatusKnown &&
      static_cast<uint32_t>(millis() - avrDisplayStatusAt) < 3000U, avrPcSupported);
  if(avrRoleKnown&&!avrRoleCompatible)available&=~(BoardCore::Context::Inputs|BoardCore::Context::Outputs);
#if defined(ARDUINO_ARCH_ESP32) && !PAKAR_S3_ISP_ELECTRICAL_VERIFIED
  available &= ~BoardCore::Context::Isp;
#endif
#if defined(ARDUINO_ARCH_ESP32)
  if(lcdPort.display().available())available|=BoardCore::Context::Lcd;
#endif
  return available;
}

bool tm1637DetectionKnown() {
  return BoardCore::Context::displayObservationFresh(avrOnline&&avrVersionKnown,
      avrCapabilities,avrDisplayStatusKnown,avrDisplayStatusAt,millis());
}

bool contextAllows(uint8_t opcode, const uint8_t *payload, uint8_t length) {
  using namespace BoardCore::Context;
  const uint16_t available = currentFeatures();
  switch (opcode) {
    case kSetMode: return length == 1 && payload[0] < 4 && (modes(available) & (1U << payload[0]));
    case kGameReset: return available & Table;
    case kCellAction: case kCellsAction: case kCellAssignments: case kCellLock:
      return PAKAR_TABLE_SELECTION && (available & Table);
    case kBuzzer: case kAudioCue: return available & Buzzer;
    case kDisplayText: return available & (length > 4 ? Text : Display);
    case kOutputMask: return available & Outputs;
    default: return true;
  }
}

void sendHostStatus(uint8_t requestSequence) {
  const time_t currentTime = time(nullptr);
  const uint16_t errors = link.errors();
  const uint16_t available = currentFeatures();
  const uint8_t payload[] = {
      static_cast<uint8_t>(mode), winner, selectedCell, grid.brightness(),
      static_cast<uint8_t>(grid.pattern()), static_cast<uint8_t>(avrOnline),
      static_cast<uint8_t>(tm1637DetectionKnown()&&tm1637Ack), static_cast<uint8_t>(programmingLatched),
      activeInputs, static_cast<uint8_t>(activeInput9), activeOutputs,
      static_cast<uint8_t>(errors), static_cast<uint8_t>(errors >> 8),
      static_cast<uint8_t>(avrUartErrors), static_cast<uint8_t>(avrUartErrors >> 8),
      static_cast<uint8_t>(networkConfig.stationConnected()),
      static_cast<uint8_t>(apStarted),
      static_cast<uint8_t>(currentTime > 1700000000),
      static_cast<uint8_t>(PAKAR_USE_BUSY_WIRE),
      // Append-only extension: preserve the original 19-byte status prefix.
      tm1637Segments[0], tm1637Segments[1], tm1637Segments[2], tm1637Segments[3],
      tm1637Brightness, static_cast<uint8_t>(tm1637HostActive),
      avrVersion[0], avrVersion[1], avrVersion[2],
      BoardCore::AudioCues::supportedCount(avrOnline, (avrCapabilities & kCapBuzzer) != 0, avrAudioCueCount),
      // Living context: reserved zero followed by explicit acknowledged features.
      0, static_cast<uint8_t>(available), static_cast<uint8_t>(available >> 8),
      BoardCore::Context::modes(available), static_cast<uint8_t>(PAKAR_TABLE_ENABLED),
      10, 8, 6, static_cast<uint8_t>(identityRevision()),
      // Configured state is distinct from observed station/AP link state above.
      static_cast<uint8_t>(networkConfig.stationEnabled()),
      static_cast<uint8_t>(networkConfig.accessPointEnabled()),
      static_cast<uint8_t>(networkConfig.dhcpEnabled()),
      static_cast<uint8_t>(PAKAR_AVR_LINK_ENABLED),
      static_cast<uint8_t>(grid.outputEnabled()),
      static_cast<uint8_t>(grid.staticMode()?0:1), // Optional byte43; absent means unknown.
      static_cast<uint8_t>(PAKAR_TABLE_ENABLED), // Optional byte44: frozen USB table snapshot/boot-fenced commands.
      static_cast<uint8_t>(PAKAR_TABLE_ENABLED), // Optional byte45: immediate same-sequence post-ACK table state.
  };
  hostLink.send(kStatusResponse, requestSequence, payload, sizeof(payload));
}

void onHostFrame(const BoardCore::Protocol::Frame &frame, void *) {
  static String introduction;
  static uint16_t introductionTotal=0;
  static uint32_t introductionAt=0;
  if(frame.opcode==kHello) {
    // Every new caller resets admission. An empty HELLO may inspect identity,
    // but is never permission for an unintroduced/unsupported app to control.
    hostCallerAdmitted=false;introduction=String();introductionTotal=0;
    if(frame.payloadLength) {
      if(frame.payloadLength!=4 || frame.payload[0]!=kRoleHost) {
        hostLink.error(frame.sequence,kHello,kBadPayload);return;
      }
      char version[16];snprintf(version,sizeof(version),"%u.%u.%u",frame.payload[1],frame.payload[2],frame.payload[3]);
      hostCallerAdmitted=BoardCore::PeerCompatibility::appAllowed(version);
      if(!hostCallerAdmitted){hostLink.error(frame.sequence,kHello,kUnsupported);return;}
    }
  }
  const bool bootstrap=frame.opcode==kHello || frame.opcode==kGetStatus || frame.opcode==kGetIdentity || frame.opcode==kPing;
  if(BoardCore::PeerCompatibility::enforced && !hostCallerAdmitted && !bootstrap) {
    hostLink.error(frame.sequence,frame.opcode,kUnsupported);return;
  }
  if(frame.opcode==kPeerReport) {
    if(frame.payloadLength<=4){hostLink.error(frame.sequence,frame.opcode,kBadPayload);return;}
    const uint16_t total=frame.payload[0]|uint16_t(frame.payload[1])<<8;
    const uint16_t offset=frame.payload[2]|uint16_t(frame.payload[3])<<8;
    const uint16_t count=frame.payloadLength-4;
    if(!total || total>1536 || offset>total || count>total-offset) {
      introduction=String();introductionTotal=0;hostLink.error(frame.sequence,frame.opcode,kBadPayload);return;
    }
    if(!offset){introduction=String();introductionTotal=total;introduction.reserve(total);introductionAt=millis();}
    if(total!=introductionTotal || offset!=introduction.length() || uint32_t(millis()-introductionAt)>5000) {
      introduction=String();introductionTotal=0;hostLink.error(frame.sequence,frame.opcode,kBadPayload);return;
    }
    introduction.concat(reinterpret_cast<const char*>(frame.payload+4),count);
    if(introduction.length()!=offset+count) {introduction=String();hostLink.error(frame.sequence,frame.opcode,kBusy);return;}
    if(introduction.length()==total) {
      const bool accepted=recordClientReport(introduction,"usb");
      introduction=String();introductionTotal=0;
      if(!accepted){hostCallerAdmitted=false;hostLink.error(frame.sequence,frame.opcode,kBadPayload);return;}
    }
    hostLink.ack(frame.sequence,frame.opcode);return;
  }
  const bool shellPoll=frame.opcode==kShellRequest && frame.payloadLength==7 && frame.payload[0]==PakarShell::SerialChannel::Poll;
  // Cloud write progress may drain read-only USB requests, never re-enter a
  // mutation or programming operation while a cloud packet is being emitted.
  if(cloudServices.ioActive() && frame.opcode!=kGetStatus && frame.opcode!=kGetIdentity &&
      frame.opcode!=kGetTableSnapshot && frame.opcode!=kHello && frame.opcode!=kPing && !shellPoll) {
    hostLink.error(frame.sequence,frame.opcode,kBusy);return;
  }
  if (!contextAllows(frame.opcode, frame.payload, frame.payloadLength)) {
    const bool roleCommand=frame.opcode==kCellAction||frame.opcode==kCellsAction||frame.opcode==kCellAssignments||frame.opcode==kCellLock;
    const uint8_t error[] = {frame.opcode,static_cast<uint8_t>(roleCommand&&!PAKAR_TABLE_SELECTION?kUnsupportedBoardRole:kHardwareUnavailable)};
    hostLink.send(kErrorResponse, frame.sequence, error, sizeof(error)); return;
  }
  static BoardCore::PcContractAssembly usbPcAssembly;
  if ((urbootBusy.get() || remoteFirmware.busy() || otaActive || otaRebootAt || avrUploadOwner) && frame.opcode != kGetStatus && frame.opcode != kGetIdentity && frame.opcode != kGetTableSnapshot && frame.opcode != kHello && frame.opcode != kPing && !shellPoll) {
      hostLink.error(frame.sequence, frame.opcode, kBusy); return;
  }
  if(handleShellFrame(frame))return;
#if defined(ARDUINO_ARCH_ESP32) && !PAKAR_AVR_LINK_ENABLED
  if (passiveAvrRx.snapshot().active && frame.opcode!=kAvrRxDiagnostic &&
      frame.opcode!=kGetStatus && frame.opcode!=kGetIdentity && frame.opcode!=kGetTableSnapshot && frame.opcode!=kHello && frame.opcode!=kPing) {
    hostLink.error(frame.sequence,frame.opcode,kBusy);return;
  }
#endif
  switch (frame.opcode) {
    case kGetTableSnapshot: {
      static String captured;
      static uint32_t token=0,capturedAt=0;
      static uint8_t capturedNonce=0;
      if(!PAKAR_TABLE_ENABLED || frame.payloadLength!=7){hostLink.error(frame.sequence,frame.opcode,kBadPayload);break;}
      const uint8_t nonce=frame.payload[0];
      const uint16_t offset=frame.payload[1]|static_cast<uint16_t>(frame.payload[2])<<8;
      uint32_t requested=0;memcpy(&requested,frame.payload+3,4);
      if(!requested && !offset){
        captured=buildUsbTableSnapshot();
        if(!captured.length() || captured.length()>16384){captured=String();hostLink.error(frame.sequence,frame.opcode,kHardwareUnavailable);break;}
        if(++token==0)++token;capturedNonce=nonce;capturedAt=millis();
      }else if(!requested || requested!=token || nonce!=capturedNonce || !captured.length() ||
          static_cast<uint32_t>(millis()-capturedAt)>=15000U){hostLink.error(frame.sequence,frame.opcode,kUnsafe);break;}
      if(offset>=captured.length()){hostLink.error(frame.sequence,frame.opcode,kBadPayload);break;}
      const uint16_t total=captured.length();
      const uint8_t count=min<unsigned>(39,total-offset);
      uint8_t response[48]={nonce};memcpy(response+1,&token,4);memcpy(response+5,&total,2);
      memcpy(response+7,&offset,2);memcpy(response+9,captured.c_str()+offset,count);
      hostLink.send(kTableSnapshotResponse,frame.sequence,response,count+9);break;
    }
    case kTableCommand: {
      if(frame.payloadLength<17 || pcBootId.length()!=32 || !webAccess.peerFenceReady()){
        hostLink.error(frame.sequence,frame.opcode,kUnsafe);break;}
      bool matches=true;
      for(uint8_t i=0;i<16;++i){
        const char a=pcBootId[i*2],b=pcBootId[i*2+1];
        const int8_t hi=a>='0'&&a<='9'?a-'0':a>='a'&&a<='f'?a-'a'+10:-1;
        const int8_t lo=b>='0'&&b<='9'?b-'0':b>='a'&&b<='f'?b-'a'+10:-1;
        matches&=hi>=0&&lo>=0&&frame.payload[i]==static_cast<uint8_t>((hi<<4)|lo);
      }
      const uint8_t inner=frame.payload[16];
      const bool allowed=inner==kSetMode || inner==kCellAction || inner==kCellsAction || inner==kCellAssignments || inner==kCellLock || inner==kGameReset ||
          inner==kSetBrightness || inner==kSetLedPattern || inner==kSetLedOutput;
      if(!matches || !allowed){hostLink.error(frame.sequence,frame.opcode,kUnsafe);break;}
      BoardCore::Protocol::Frame command{};command.opcode=inner;command.sequence=frame.sequence;
      command.payloadLength=frame.payloadLength-17;memcpy(command.payload,frame.payload+17,command.payloadLength);
      onHostFrame(command,nullptr);
      // Keep the original ACK/error unchanged for all existing clients. This
      // additive readback is observed AFTER dispatch on the same loop owner;
      // it is not an optimistic copy of the request or a physical LED receipt.
      // A rejected inner command may still report state; without its matching
      // success ACK clients must not complete that operation.
      uint8_t state[45]={inner};memcpy(state+1,frame.payload,16);
      state[17]=static_cast<uint8_t>(mode);state[18]=winner;state[19]=selectedCell;
      state[20]=grid.brightness();state[21]=static_cast<uint8_t>(grid.pattern());
      state[22]=grid.outputEnabled();state[23]=grid.staticMode()?0:1;
      for(uint8_t i=0;i<LedGrid::kCells;++i)state[24+i/4]|=static_cast<uint8_t>(grid.state(i+1))<<((i%4)*2);
      state[44]=grid.redLock();
      hostLink.send(kTableCommandState,frame.sequence,state,sizeof(state));break;
    }
    case kGetIdentity: {
      if (frame.payloadLength != 3) { hostLink.error(frame.sequence, frame.opcode, kBadPayload); break; }
      String json = F("{\"revision\":"); json += identityRevision();
      json += F(",\"board_id\":\"");json+=pcBoardId;
      json += F("\",\"boot_id\":\"");json+=pcBootId;json+='"';
      json += F(",\"hostname\":\""); json += deviceIdentity.hostname();
      json += F("\",\"display_name\":\""); appendJsonEscaped(json, deviceIdentity.displayName());
      json += F("\",\"application\":"); json += applicationIdentityJson();
      json += F(",\"version\":\"");json+=BoardCore::BuildInfo::kVersion;
      json += F("\",\"compatibility\":");json+=PakarPeer::policyJson();json+='}';
      const uint16_t offset = frame.payload[1] | static_cast<uint16_t>(frame.payload[2]) << 8;
      if (json.length() > 8192 || offset >= json.length()) { hostLink.error(frame.sequence, frame.opcode, kBadPayload); break; }
      const uint32_t revision = identityRevision();
      uint8_t response[48] = {frame.payload[0], static_cast<uint8_t>(revision), static_cast<uint8_t>(revision >> 8),
        static_cast<uint8_t>(revision >> 16), static_cast<uint8_t>(revision >> 24),
        static_cast<uint8_t>(json.length()), static_cast<uint8_t>(json.length() >> 8), frame.payload[1], frame.payload[2]};
      const uint8_t count = min(static_cast<size_t>(39), json.length() - offset);
      memcpy(response + 9, json.c_str() + offset, count);
      hostLink.send(kIdentityResponse, frame.sequence, response, count + 9); break;
    }
    case kAvrRxDiagnostic: {
#if defined(ARDUINO_ARCH_ESP32) && !PAKAR_AVR_LINK_ENABLED
      if (!frame.payloadLength || frame.payload[0]>3 ||
          (frame.payloadLength != ((frame.payload[0]==1 || frame.payload[0]==3) ? 2 : 1)) ||
          (frame.payload[0]==1 && frame.payload[1]>1) ||
          (frame.payload[0]==3 && frame.payload[1]>128)) {
        hostLink.error(frame.sequence,frame.opcode,kBadPayload);break;
      }
      passiveAvrRx.service(millis());
      passiveCaptureGuard.store(passiveAvrRx.snapshot().active,std::memory_order_release);
      const uint8_t action=frame.payload[0];
      if(action==3) {
        uint8_t response[36]={action,kNoError,frame.payload[1],0};
        response[3]=passiveAvrRx.readChunk(frame.payload[1],32,response+4);
        hostLink.send(kAvrRxDiagnosticResponse,frame.sequence,response,response[3]+4);break;
      }
      if(action==1 && (programmingLatched || networkApplyPending)) {
        hostLink.error(frame.sequence,frame.opcode,kBusy);break;
      }
      const bool started=action!=1 || passiveAvrRx.begin(frame.payload[1] ? 115200U : 38400U,millis());
      if(action==2)passiveAvrRx.stop();
      const auto state=passiveAvrRx.snapshot();
      passiveCaptureGuard.store(state.active,std::memory_order_release);
      uint8_t response[27]={action,static_cast<uint8_t>(started?kNoError:kUnsafe),
          static_cast<uint8_t>(state.active),static_cast<uint8_t>(state.baud==38400 ? 0 : state.baud==115200 ? 1 : 255),
          static_cast<uint8_t>(state.pinsSafe),static_cast<uint8_t>(state.error),
          static_cast<uint8_t>(state.stopReason),static_cast<uint8_t>(state.stored)};
      const uint32_t counts[]={state.received,state.dropped,state.hardwareErrors,state.elapsedMs};
      for(uint8_t i=0;i<4;++i)for(uint8_t byte=0;byte<4;++byte)response[8+i*4+byte]=counts[i]>>(byte*8);
      response[24]=digitalRead(PAKAR_S3_AVR_RX);
      response[25]=digitalRead(PAKAR_S3_ISP_RESET);
      gpio_io_config_t resetConfig{};
      response[26]=gpio_get_io_config(static_cast<gpio_num_t>(PAKAR_S3_ISP_RESET),&resetConfig)==ESP_OK ? resetConfig.oe : 255;
      hostLink.send(kAvrRxDiagnosticResponse,frame.sequence,response,sizeof(response));
#else
      hostLink.error(frame.sequence,frame.opcode,kUnsupported);
#endif
      break;
    }
    case kPcContractChunk:
      if (usbPcAssembly.accept(frame.sequence, frame.payload, frame.payloadLength, millis())) {
        const uint8_t *request = usbPcAssembly.data();
        if (request[0] != ControllerProtocol::AddressableLed && (!avrOnline || !avrPcSupported))
          hostLink.error(frame.sequence, kPcContract, kHardwareUnavailable);
        else if (!pcTransport.enqueue(request[0], request + 1, 48, millis(), true, frame.sequence))
          hostLink.error(frame.sequence, kPcContract, kBusy);
      }
      break;
    case kPcContract:
      if ((!frame.payloadLength || frame.payload[0] != ControllerProtocol::AddressableLed) &&
          (!avrOnline || !avrPcSupported)) {
        hostLink.error(frame.sequence, frame.opcode, kHardwareUnavailable); break;
      }
      if (!frame.payloadLength || !pcTransport.enqueue(frame.payload[0],
          frame.payload + 1, frame.payloadLength - 1, millis(), true, frame.sequence)) {
        hostLink.error(frame.sequence, frame.opcode, frame.payloadLength ? kBusy : kBadPayload);
      }
      // No fabricated ESP ACK: return the actual AVR inner response later.
      break;
    case kDeviceName: {
      if (programmingLatched || networkApplyPending) { hostLink.error(frame.sequence, frame.opcode, kBusy); break; }
      if (frame.payloadLength == 1 && frame.payload[0] == 0) { stagedDeviceName = String(); stagedDisplayName = String(); }
      else if (frame.payloadLength >= 2 && (frame.payload[0] == 1 || frame.payload[0] == 3)) {
        String &target = frame.payload[0] == 1 ? stagedDeviceName : stagedDisplayName;
        if (target.length() + frame.payloadLength - 1 > (frame.payload[0] == 1 ? 69U : 63U)) { hostLink.error(frame.sequence, frame.opcode, kBadPayload); break; }
        for (uint8_t i = 1; i < frame.payloadLength; ++i) {
          if (!frame.payload[i]) { stagedDeviceName = String(); stagedDisplayName = String(); hostLink.error(frame.sequence, frame.opcode, kBadPayload); return; }
          target += static_cast<char>(frame.payload[i]);
        }
      } else if (frame.payloadLength == 1 && frame.payload[0] == 2) {
        const bool ok = scheduleDeviceIdentity(stagedDeviceName, stagedDisplayName.length() ? stagedDisplayName : String(deviceIdentity.displayName()));
        stagedDeviceName = String(); stagedDisplayName = String();
        if (!ok) { hostLink.error(frame.sequence, frame.opcode, kBadPayload); break; }
      } else { hostLink.error(frame.sequence, frame.opcode, kBadPayload); break; }
      hostLink.ack(frame.sequence, frame.opcode); break;
    }
    case kAccessConfig: {
      if (frame.payloadLength == 1 && frame.payload[0] == 0) {
        stagedAccessUser = String(); stagedAccessPassword = String();
      } else if (frame.payloadLength >= 2 && (frame.payload[0] == 1 || frame.payload[0] == 2)) {
        String &target = frame.payload[0] == 1 ? stagedAccessUser : stagedAccessPassword;
        const size_t limit = frame.payload[0] == 1 ? 32 : 64;
        if (target.length() + frame.payloadLength - 1 > limit) { hostLink.error(frame.sequence, frame.opcode, kBadPayload); break; }
        for (uint8_t i = 1; i < frame.payloadLength; ++i) target += static_cast<char>(frame.payload[i]);
      } else if (frame.payloadLength == 1 && frame.payload[0] == 3) {
        const bool ok = accessConfig.provisionFromPhysicalUsb(stagedAccessUser, stagedAccessPassword);
        stagedAccessUser = String(); stagedAccessPassword = String();
        if (!ok) { hostLink.error(frame.sequence, frame.opcode, kBadPayload); break; }
        webAccess.revokeAll(); webSocket.closeAll(1008, "Credentials changed");
        closeDeviceShells();
      } else { hostLink.error(frame.sequence, frame.opcode, kBadPayload); break; }
      hostLink.ack(frame.sequence, frame.opcode); break;
    }
    case kHello: {
      uint16_t capabilities =
          kCapLedGrid | kCapOta | kCapLittleFs | kCapApi |
          kCapWebSocket | kCapNtp | kCapHostSerial |
          (PAKAR_USE_BUSY_WIRE ? kCapBusyWire : 0U);
#if !defined(ARDUINO_ARCH_ESP32) || PAKAR_S3_ISP_ELECTRICAL_VERIFIED
      capabilities |= kCapIsp;
#endif
      uint8_t payload[25] = {
          0, kRoleEsp, // Reserved zero, not a protocol generation.
          static_cast<uint8_t>(capabilities),
          static_cast<uint8_t>(capabilities >> 8), LedGrid::kCells, 9,
          BoardCore::BuildInfo::kVersionMajor,
          BoardCore::BuildInfo::kVersionMinor,
          BoardCore::BuildInfo::kVersionPatch,
      };
      for(uint8_t i=0;i<16&&pcBootId.length()==32;++i){char byte[3]={pcBootId[i*2],pcBootId[i*2+1],0};payload[9+i]=strtoul(byte,nullptr,16);}
      hostLink.send(kHelloResponse, frame.sequence, payload, sizeof(payload));
      break;
    }
    case kGetStatus:
      sendHostStatus(frame.sequence);
      break;
    case kSetMode:
      if (frame.payloadLength != 1 || !BoardCore::GameRole::accepts(frame.payload[0])) {
        hostLink.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      if(!grid.resetWhite()){hostLink.error(frame.sequence,frame.opcode,kRedCellsLocked);break;}
      mode = static_cast<GameMode>(BoardCore::GameRole::mode);
      winner = 0;
      sendToAvr(kSetMode, sequence++, frame.payload, 1);
      hostLink.ack(frame.sequence, frame.opcode);
      break;
    case kCellAction:
      if (frame.payloadLength < 2 || frame.payloadLength > 3 ||
          frame.payload[0] < 1 || frame.payload[0] > LedGrid::kCells ||
          frame.payload[1] > kCellOff ||
          (frame.payloadLength == 3 && frame.payload[2] > kEffectPulse)) {
        hostLink.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      if(!grid.setCell(frame.payload[0], static_cast<CellState>(frame.payload[1]),
                   frame.payloadLength == 3
                       ? static_cast<Effect>(frame.payload[2])
                       : kEffectFade)){hostLink.error(frame.sequence,frame.opcode,kRedCellsLocked);break;}
      selectedCell = frame.payload[0];
      selectedState = frame.payload[1];
      sendToAvr(kCellAction, sequence++, frame.payload, frame.payloadLength);
      hostLink.ack(frame.sequence, frame.opcode);
      break;
    case kCellsAction: {
      BoardCore::CellBatch selection;
      if(!BoardCore::CellBatch::decode(frame.payload,frame.payloadLength,selection)){hostLink.error(frame.sequence,frame.opcode,kBadPayload);break;}
      if(!grid.setCells(selection,static_cast<CellState>(frame.payload[0]),static_cast<Effect>(frame.payload[1]))){hostLink.error(frame.sequence,frame.opcode,kRedCellsLocked);break;}
      selectedCell=selection.first();selectedState=frame.payload[0];
      hostLink.ack(frame.sequence,frame.opcode);break;
    }
    case kCellAssignments: {
      if(frame.payloadLength!=30){hostLink.error(frame.sequence,frame.opcode,kInvalidCellAssignments);break;}
      uint8_t ids[80],states[80],count=0;
      for(uint8_t i=0;i<80;++i)if(frame.payload[i/8]&(1U<<(i%8))){
        ids[count]=i+1;states[count++]=(frame.payload[10+i/4]>>((i%4)*2))&3U;
      }
      bool invalid=!count,locked=false;
      for(uint8_t i=0;i<count;++i){invalid|=states[i]>kCellOff;locked|=states[i]<=kCellOff&&!grid.permits(ids[i],static_cast<CellState>(states[i]));}
      if(invalid||locked||!grid.setAssignments(ids,states,count)){hostLink.error(frame.sequence,frame.opcode,invalid?kInvalidCellAssignments:kRedCellsLocked);break;}
      hostLink.ack(frame.sequence,frame.opcode);break;
    }
    case kCellLock:
      if(frame.payloadLength!=1||frame.payload[0]>1){hostLink.error(frame.sequence,frame.opcode,kBadPayload);break;}
      grid.setRedLock(frame.payload[0]);hostLink.ack(frame.sequence,frame.opcode);break;
    case kGameReset:
      if(!grid.resetWhite()){hostLink.error(frame.sequence,frame.opcode,kRedCellsLocked);break;}
      winner = 0;
      sendToAvr(kGameReset, sequence++);
      hostLink.ack(frame.sequence, frame.opcode);
      break;
    case kSetBrightness:
      if (frame.payloadLength != 1 || frame.payload[0] == 0) {
        hostLink.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      grid.setBrightness(frame.payload[0]);
      saveLedPreferences();
      hostLink.ack(frame.sequence, frame.opcode);
      break;
    case kSetLedOutput: {
      BoardCore::LedOutputControl requested;
      if(!BoardCore::parseLedOutputControl(frame.payload,frame.payloadLength,requested)){
        hostLink.error(frame.sequence,frame.opcode,kBadPayload);break;}
      if(requested.modeOnly)grid.setStaticMode(!requested.value);
      else grid.setOutputEnabled(requested.value);
      hostLink.ack(frame.sequence,frame.opcode);break;
    }
    case kSetLedPattern:
      if (frame.payloadLength != 1 || frame.payload[0] > LedGrid::kPatternCellChase) {
        hostLink.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      if(!grid.setPattern(static_cast<LedGrid::Pattern>(frame.payload[0]))){hostLink.error(frame.sequence,frame.opcode,kRedCellsLocked);break;}
      saveLedPreferences();
      hostLink.ack(frame.sequence, frame.opcode);
      break;
    case kNetworkConfig:
      if (frame.payloadLength < 1) {
        hostLink.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      if (frame.payload[0] == 0) {
        stagedStationSsid = String();
        stagedStationPassword = String();
      } else if (frame.payload[0] == 1 || frame.payload[0] == 2) {
        String &target = frame.payload[0] == 1 ? stagedStationSsid
                                               : stagedStationPassword;
        const size_t limit = frame.payload[0] == 1 ? 32U : 63U;
        if (target.length() + frame.payloadLength - 1U > limit) {
          hostLink.error(frame.sequence, frame.opcode, kBadPayload);
          break;
        }
        for (uint8_t index = 1; index < frame.payloadLength; ++index) {
          target += static_cast<char>(frame.payload[index]);
        }
      } else if (frame.payload[0] == 3 && frame.payloadLength == 3) {
        if (programmingLatched || networkApplyPending || frame.payload[1]>1 || frame.payload[2]>1) {
          hostLink.error(frame.sequence, frame.opcode, kUnsafe); break;
        }
        const bool stationEnabled = frame.payload[1] != 0;
        const bool apEnabled = frame.payload[2] != 0;
        NetworkConfigStore candidate = networkConfig;
        const bool saved = candidate.setStation(stagedStationSsid,
                                      stagedStationPassword,
                                      stationEnabled) &&
            candidate.setAccessPointEnabled(apEnabled) && candidate.save();
        stagedStationSsid = String(); stagedStationPassword = String();
        if (!saved) {
          hostLink.error(frame.sequence, frame.opcode, kBadPayload);
          break;
        }
        networkConfig = candidate;
        networkApplyAt = millis() + 250U;
        networkApplyPending = true;
      } else {
        hostLink.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      hostLink.ack(frame.sequence, frame.opcode);
      break;
    case kOutputMask:
    case kBuzzer:
    case kAudioCue:
    case kDisplayText:
      if (frame.opcode == kAudioCue &&
          (frame.payloadLength != 1 || frame.payload[0] >=
           BoardCore::AudioCues::supportedCount(avrOnline, (avrCapabilities & kCapBuzzer) != 0, avrAudioCueCount))) {
        hostLink.error(frame.sequence, frame.opcode, kBadPayload); break;
      }
      if (!sendToAvr(frame.opcode, sequence++, frame.payload,
                     frame.payloadLength)) {
        hostLink.error(frame.sequence, frame.opcode, kHardwareUnavailable);
      } else {
        hostLink.ack(frame.sequence, frame.opcode);
      }
      break;
    case kPing:
      hostLink.send(kPong, frame.sequence, frame.payload, frame.payloadLength);
      break;
    default:
      hostLink.error(frame.sequence, frame.opcode, kUnsupported);
      break;
  }
}

void rejectHostFrame(const BoardCore::Protocol::Frame& frame,
                     BoardCore::Protocol::Error error,void*) {
  hostLink.error(frame.sequence,frame.opcode,error);
}
void revokeHostAdmission(void*) {hostCallerAdmitted=false;}
void observeHostIngressFence() {
  // Job identities catch a maintenance operation that began and ended between
  // two services, including async OTA. Only this loop owner mutates the FIFO.
  static uint32_t generation=0,httpEpoch=0,otaOperation=0,remoteJob=0,avrJob=0,uploadOwner=0;
  const uint32_t nextHttpEpoch=server.ingressEpoch(),nextOtaOperation=otaProgress.snapshot().operation;
  const uint32_t nextRemoteJob=remoteFirmware.jobId(),nextAvrJob=urbootJobId,nextUploadOwner=avrUploadOwner;
  if(httpEpoch!=nextHttpEpoch || otaOperation!=nextOtaOperation || remoteJob!=nextRemoteJob ||
      avrJob!=nextAvrJob || uploadOwner!=nextUploadOwner) {
    ++generation;httpEpoch=nextHttpEpoch;otaOperation=nextOtaOperation;
    remoteJob=nextRemoteJob;avrJob=nextAvrJob;uploadOwner=nextUploadOwner;
  }
  hostFrameIngress.observeFence(generation,urbootBusy.get() || remoteFirmware.busy() ||
      otaActive || otaRebootAt || avrUploadOwner || urbootRawOwned);
}
void onHostFrameIngress(const BoardCore::Protocol::Frame& frame,void*) {
  observeHostIngressFence();
  hostFrameIngress.receive(frame,millis(),cloudServices.ioActive());
}
void serviceHostLink() {
  // SerialLink's callback runs before its RX scratch is retired. A command
  // that publishes cloud output must not recursively enter that same parser.
  static bool servicing=false;if(servicing)return;servicing=true;
  observeHostIngressFence();
  hostFrameIngress.service(millis(),cloudServices.ioActive());
  hostLink.service();
  observeHostIngressFence();
  hostFrameIngress.service(millis(),cloudServices.ioActive());
  servicing=false;
}

void setAvrTransmitBlocked(bool blocked) {
  busyLine.setBlocked(blocked);
}

uint16_t cellMappingCheck(const CellMappingRecord &value) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&value);
  const size_t length = offsetof(CellMappingRecord, check);
  uint16_t hash = 0x4B50U;
  for (size_t i = 0; i < length; ++i) {
    hash = static_cast<uint16_t>((hash << 5) | (hash >> 11));
    hash ^= bytes[i];
  }
  return hash;
}

uint8_t defaultPhysicalCell(uint8_t logicalCell) {
  return BoardCore::TableCellMapping::physicalForLogical(logicalCell);
}

void resetCellMapping() {
  memset(&cellMapping, 0, sizeof(cellMapping));
  cellMapping.magic0 = 0x50;
  cellMapping.magic1 = 0x4D;
  cellMapping.schema = 1;
  for (uint8_t logical = 1; logical <= LedGrid::kCells; ++logical) {
    cellMapping.physical[logical - 1] = defaultPhysicalCell(logical);
  }
  cellMapping.check = cellMappingCheck(cellMapping);
}

bool mappingIsValid(const CellMappingRecord &value) {
  if (value.magic0 != 0x50 || value.magic1 != 0x4D || value.schema != 1 ||
      value.check != cellMappingCheck(value)) return false;
  bool seen[LedGrid::kCells] = {};
  for (uint8_t i = 0; i < LedGrid::kCells; ++i) {
    const uint8_t physical = value.physical[i];
    if (physical < 1 || physical > LedGrid::kCells || seen[physical - 1]) {
      return false;
    }
    seen[physical - 1] = true;
  }
  return true;
}

void loadCellMapping() {
  resetCellMapping();
  File file = LittleFS.open(kCellMappingPath, "r");
  if (file) {
    CellMappingRecord stored{};
    const bool complete =
        file.read(reinterpret_cast<uint8_t *>(&stored), sizeof(stored)) ==
        sizeof(stored);
    file.close();
    if (complete && mappingIsValid(stored)) cellMapping = stored;
  }
#if PAKAR_LED_START_CELL_CHASE
  // Explicit test build uses the user's confirmed cable order without
  // rewriting names or flash. The preset API can persist it independently.
  for(uint8_t logical=1;logical<=LedGrid::kCells;++logical)
    cellMapping.physical[logical-1]=defaultPhysicalCell(logical);
  cellMapping.check=cellMappingCheck(cellMapping);
#endif
  grid.setPhysicalMapping(cellMapping.physical, LedGrid::kCells);
}

bool saveCellMapping() {
  cellMapping.check = cellMappingCheck(cellMapping);
  File file = LittleFS.open(kCellMappingPath, "w");
  if (!file) return false;
  const bool complete =
      file.write(reinterpret_cast<const uint8_t *>(&cellMapping),
                 sizeof(cellMapping)) == sizeof(cellMapping);
  file.close();
  return complete;
}

void appendJsonEscaped(String &body, const char *value) {
  while (value != nullptr && *value != '\0') {
    if (*value == '\\' || *value == '"') body += '\\';
    const uint8_t byte = static_cast<uint8_t>(*value++);
    if (byte >= 0x20U) body += static_cast<char>(byte);
  }
}

void saveLedPreferences() {
  // Admission is not a durable-save receipt. The loop coalesces all ingress
  // paths (including USB, physical keypad and cloud), not just REST settings.
  serviceScenePersistence();
}

void loadLedPreferences() {
  // Read-only migration of the old brightness-only record; never rewrite it.
  File file = LittleFS.open(kLedPreferencesPath, "r");
  if (!file) return;
  LedPreferences value{};
  const bool complete = file.read(reinterpret_cast<uint8_t *>(&value),
                                  sizeof(value)) == sizeof(value);
  file.close();
  if (complete) applyLedBootPreferences(grid, value);
}

void serviceScenePersistence(bool flush) {
  const SceneState next = captureLedScene(sceneStore.state(), grid,
      static_cast<uint8_t>(mode), selectedCell, selectedState, winner);
  sceneStore.observe(next, millis());
  sceneStore.service(millis(), !otaActive && !remoteFirmware.busy() &&
      !urbootBusy.get() && !uploadFile && !avrUploadOwner, flush);
}
bool flushUpgradeScene() {
  // Loop-owned final semantic checkpoint, never rendered/overlay pixels.
  if(otaActive || uploadFile || avrUploadOwner)return false;
  const SceneState next=captureLedScene(sceneStore.state(),grid,
    static_cast<uint8_t>(mode),selectedCell,selectedState,winner);
  sceneStore.observe(next,millis());sceneStore.service(millis(),true,true);
  return sceneStore.saved();
}

void stopDiscovery() {
#if defined(ARDUINO_ARCH_ESP32)
  s3Discovery.end();
#else
  MDNS.close();
  SSDP.end();
  mndpService.stop();
  NBNS.end();
#endif
  mdnsStarted = false;
  ssdpStarted = false;
  netbiosStarted = false;
  discoveryAddress = IPAddress();
}

void startDiscovery() {
  discoveryAttemptAt = millis();
  stopDiscovery();
#if defined(ARDUINO_ARCH_ESP32)
  if (!NetworkPath::ready() && !apStarted) return;
  discoveryAddress = NetworkPath::ready() ? NetworkPath::localIP() : WiFi.softAPIP();
  s3Discovery.begin(hostname.c_str(), deviceIdentity.displayName(), SystemPlatform::chipId(), kStreamPort,shellAdvertisedPort());
  mdnsStarted = s3Discovery.mdnsReady();
  ssdpStarted = s3Discovery.ssdpReady();
  netbiosStarted = s3Discovery.netbiosReady();
#else
  if (!networkConfig.stationConnected()) return;

  discoveryAddress = WiFi.localIP();
  mdnsStarted = MDNS.begin(hostname.c_str());
  if (mdnsStarted) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "path", "/");
    MDNS.addServiceTxt("http", "tcp", "app", APP_NAME);
    MDNS.addServiceTxt("http", "tcp", "app_id", APP_ID);
    MDNS.addServiceTxt("http", "tcp", "version",
                       BoardCore::BuildInfo::kVersion);
    MDNS.addService("pakarcontroller", "tcp", 80);
    MDNS.addServiceTxt("pakarcontroller", "tcp", "api", "/api/status");
    MDNS.addServiceTxt("pakarcontroller", "tcp", "websocket", "/ws");
    MDNS.addServiceTxt("pakarcontroller","tcp","version",BoardCore::BuildInfo::kVersion);
    MDNS.addServiceTxt("pakarcontroller","tcp","role",BoardCore::GameRole::name);
    MDNS.addServiceTxt("pakarcontroller","tcp","minimum_app",BoardCore::PeerCompatibility::minimumApp);
    MDNS.addServiceTxt("pakarcontroller","tcp","minimum_board",BoardCore::PeerCompatibility::minimumBoard);
    MDNS.addServiceTxt("pakarcontroller","tcp","version_check",BoardCore::PeerCompatibility::enforced?"true":"false");
    MDNS.addService("pakar-stream", "udp", kStreamPort);
    if(shellAdvertisedPort())MDNS.addService("telnet", "tcp", shellAdvertisedPort());
  }

  SSDP.setSchemaURL("description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName(deviceIdentity.displayName());
  SSDP.setSerialNumber(ESP.getChipId());
  SSDP.setURL("/");
  SSDP.setModelName(APP_NAME);
  SSDP.setModelNumber(BoardCore::BuildInfo::kVersion);
  SSDP.setManufacturer("AtomicDeploy");
  char uuid[37];
  snprintf(uuid, sizeof(uuid), "38323636-4558-4dda-9188-cda0e6%06x", ESP.getChipId());
  SSDP.setUUID(uuid);
  ssdpStarted = SSDP.begin();
  mndpService.begin();
  // NetBIOS labels are limited to 15; never advertise a silently truncated alias.
  netbiosStarted = hostname.length() <= 15 && NBNS.begin(hostname.c_str());
#endif
}

void serviceDiscovery() {
#if defined(ARDUINO_ARCH_ESP32)
  const bool connected = NetworkPath::ready() || apStarted;
  const IPAddress address = NetworkPath::ready() ? NetworkPath::localIP() : apStarted ? WiFi.softAPIP() : IPAddress();
#else
  const bool connected = networkConfig.stationConnected();
  const IPAddress address = connected ? WiFi.localIP() : IPAddress();
#endif
  if ((connected && (address != discoveryAddress ||
      (!mdnsStarted && static_cast<uint32_t>(millis()-discoveryAttemptAt)>=10000UL))) ||
      (!connected && (mdnsStarted || ssdpStarted))) {
    startDiscovery();
  }
#if defined(ARDUINO_ARCH_ESP32)
  s3Discovery.service(pcBoardId.c_str());
  mdnsStarted = s3Discovery.mdnsReady(); ssdpStarted = s3Discovery.ssdpReady();
  netbiosStarted = s3Discovery.netbiosReady();
#else
  if (mdnsStarted) MDNS.update();
  mndpService.service(hostname.c_str(),pcBoardId.c_str());
#endif
}

void applyEthernetConfig() {
#if defined(ARDUINO_ARCH_ESP32) && PAKAR_S3_ETHERNET_ENABLED
  cloudServices.suspend();
  NetworkPath::setEthernetDhcp(ethernetConfig.dhcpEnabled());
  if(!ethernetConfig.enabled()) {
    NetworkPath::ethernet().end();NetworkPath::service();ntpConfigured=false;return;
  }
  if(!NetworkPath::ethernet().ensureInitialized(hostname.c_str()))return;
  if(!NetworkPath::ethernet().configureAddressing(ethernetConfig.dhcpEnabled(),
      ethernetConfig.address(),ethernetConfig.gateway(),ethernetConfig.subnet(),
      ethernetConfig.dns1(),ethernetConfig.dns2()))return;
  NetworkPath::ethernet().setLldp(ethernetConfig.lldpEnabled(),ethernetConfig.lldpReceive());
  NetworkPath::service();ntpConfigured=false;
#endif
}

void applyNetworkConfig() {
  cloudServices.suspend();
  if (hostname != deviceIdentity.hostname()) {
    cloudServices.renameDevice(deviceIdentity.hostname());
    hostname = deviceIdentity.hostname();
    webSocket.closeAll(1012, "Device name changed");
    closeDeviceShells();
  }
  stopDiscovery();
  apStarted = networkConfig.configure(hostname.c_str(), kDevelopmentPassword);
#if defined(ARDUINO_ARCH_ESP32) && PAKAR_S3_ETHERNET_ENABLED
  NetworkPath::begin(hostname.c_str(),ethernetConfig.enabled());
#else
  NetworkPath::begin(hostname.c_str());
#endif
  NetworkPath::service();
  ntpConfigured = false;
}

bool scheduleDeviceRename(const String &name) {
  return scheduleDeviceIdentity(name, deviceIdentity.displayName());
}
bool scheduleDeviceIdentity(const String &name, const String &displayName) {
  const String previousLabel = deviceIdentity.displayName();
  if (!deviceIdentity.configure(name, displayName)) return false;
  if (hostname != deviceIdentity.hostname()) {
    networkApplyAt = millis() + 1000U; networkApplyPending = true;
  } else if (previousLabel != deviceIdentity.displayName()) startDiscovery();
  return true;
}

String applicationIdentityJson() {
  String body = F("{\"id\":\""); appendJsonEscaped(body, APP_ID);
  body += F("\",\"name\":\""); appendJsonEscaped(body, APP_NAME);
  body += F("\",\"title\":\""); appendJsonEscaped(body, APP_TITLE);
  body += F("\",\"icon_png_base64\":\""); body += FPSTR(APP_ICON_PNG_BASE64); body += F("\"}");
  return body;
}
uint32_t identityRevision() {
  // Change detector, not an authenticity claim. Metadata has no credentials.
  uint32_t hash = 2166136261UL;
  for (const char *value : {deviceIdentity.hostname(), deviceIdentity.displayName(), APP_ID, APP_NAME, APP_TITLE}) {
    for (; *value; ++value) hash = (hash ^ static_cast<uint8_t>(*value)) * 16777619UL;
    hash = (hash ^ 0U) * 16777619UL;
  }
  const size_t length = strlen_P(APP_ICON_PNG_BASE64);
  for (size_t i = 0; i < length; ++i) hash = (hash ^ pgm_read_byte(APP_ICON_PNG_BASE64 + i)) * 16777619UL;
  return hash;
}
String deviceNameJson(bool accepted) {
  String body = F("{\"hostname\":\""); body += deviceIdentity.hostname();
  body += F("\",\"fqdn\":\""); body += deviceIdentity.hostname();
  body += F(".local\",\"canonical_url\":\"http://"); body += deviceIdentity.hostname();
  body += F(".local\",\"active_hostname\":\""); body += hostname;
  body += F("\",\"persisted\":"); body += deviceIdentity.ready() ? F("true") : F("false");
  body += F(",\"display_name\":\""); appendJsonEscaped(body, deviceIdentity.displayName());
  body += F("\",\"revision\":"); body += identityRevision();
  body += F(",\"application\":"); body += applicationIdentityJson();
  body += F(",\"version\":\"");body+=BoardCore::BuildInfo::kVersion;body+='"';
  body += F(",\"compatibility\":");body+=PakarPeer::policyJson();
  body += F(",\"accepted\":"); body += accepted ? F("true") : F("false");
  body += F(",\"apply_pending\":"); body += networkApplyPending ? F("true") : F("false");
  body += F(",\"apply_after_ms\":"); body += networkApplyPending ? 1000 : 0;
  body += F(",\"error\":\""); appendJsonEscaped(body, deviceIdentity.error().c_str()); body += F("\"}");
  return body;
}

bool sendToAvr(uint8_t opcode, uint8_t requestSequence,
               const uint8_t *payload, uint8_t payloadLength) {
  if (!PAKAR_AVR_LINK_ENABLED || urbootRawOwned) return false;
#if defined(ARDUINO_ARCH_ESP32)
  if (!avrUart.started()) return false;
#endif
  const uint8_t introduction[]={kRoleEsp,BoardCore::BuildInfo::kVersionMajor,
      BoardCore::BuildInfo::kVersionMinor,BoardCore::BuildInfo::kVersionPatch};
  if(opcode==kHello && !payloadLength){payload=introduction;payloadLength=sizeof(introduction);}
  const bool sent = link.send(opcode, requestSequence, payload, payloadLength);
  if (sent) {
    if(opcode==kDisplayText && !semanticPanelSending)semanticPanel.manual(payload,payloadLength,millis());
    // The longest routine response is below 6 ms at 38400 baud. Keep a full
    // margin so FastLED cannot mask interrupts after only part of it arrived.
    ledResumeAt = millis() + (opcode == kPcContract || opcode == kGetTelemetry ? 32U : 12U);
  }
  return sent;
}

String pcReceiptJson(const BoardCore::PcContractTransport::Receipt &receipt) {
  using T = BoardCore::PcContractTransport;
  String body = F("{\"type\":\"pc-contract\",\"boot_id\":\"");
  body += pcBootId;
  body += F("\",\"request_id\":"); body += receipt.id;
  body += F(",\"request_opcode\":"); body += receipt.request[0];
  body += F(",\"state\":\"");
  body += receipt.state == T::Queued ? F("queued") : receipt.state == T::Awaiting ?
      F("awaiting") : receipt.state == T::Responded ? F("responded") : F("outcome_unknown");
  body += F("\",\"response_opcode\":");
  if (receipt.state == T::Responded && receipt.responseLength) body += receipt.response[0];
  else body += F("null");
  body += F(",\"payload_hex\":\"");
  constexpr char digits[] = "0123456789abcdef";
  for (uint8_t i = 1; i < receipt.responseLength; ++i) {
    body += digits[receipt.response[i] >> 4]; body += digits[receipt.response[i] & 15];
  }
  body += F("\"}"); return body;
}

void servicePcContracts() {
  using T = BoardCore::PcContractTransport;
  pcTransport.expire(millis(), [](T::Receipt &receipt) {
    if (receipt.usb) {
      // Transport timeout is uncertainty, not an AVR rejection or safe stop.
      hostLink.error(receipt.hostSequence, kPcContract, kBusy);
      pcTransport.releaseUsb(receipt);
    } else publishPcEvent(pcReceiptJson(receipt));
  });
  if (urbootBusy.get() || remoteFirmware.busy() || otaActive || otaRebootAt || avrUploadOwner) return;
  auto *request = pcTransport.next();
  if (!request) return;
  if (request->request[0] == ControllerProtocol::AddressableLed) {
    pcTransport.sent(*request, true, millis());
    const Error error = programmingLatched ? kUnsafe :
        pcStrip.handle(request->request + 1, request->requestLength - 1);
    BoardCore::Protocol::Frame response{};
    response.opcode = kPcContractResponse; response.sequence = request->wireSequence;
    response.payloadLength = 3;
    response.payload[0] = error == kNoError ? ControllerProtocol::Ack : ControllerProtocol::ErrorResponse;
    response.payload[1] = ControllerProtocol::AddressableLed; response.payload[2] = error;
    auto *receipt = pcTransport.accept(response, millis());
    if (receipt && receipt->usb) {
      hostLink.send(kPcContractResponse, receipt->hostSequence, response.payload, response.payloadLength);
      pcTransport.releaseUsb(*receipt);
    } else if (receipt) publishPcEvent(pcReceiptJson(*receipt));
    return;
  }
  if (!avrOnline || !avrPcSupported) return;
  setAvrTransmitBlocked(false);
  bool sent;
  if (request->requestLength == 49) {
    uint8_t start[48] = {0, request->request[0], 48};
    memcpy(start + 3, request->request + 1, 45);
    uint8_t finish[4] = {1};
    memcpy(finish + 1, request->request + 46, 3);
    sent = sendToAvr(kPcContractChunk, request->wireSequence, start, sizeof(start)) &&
           sendToAvr(kPcContractChunk, request->wireSequence, finish, sizeof(finish));
    ledResumeAt = millis() + 32;
  } else sent = sendToAvr(kPcContract, request->wireSequence,
                          request->request, request->requestLength);
  pcTransport.sent(*request, sent, millis());
  if(sent && request->requestLength>1 && request->request[0]==ControllerProtocol::DisplayText)
    semanticPanel.manual(request->request+1,request->requestLength-1,millis());
  if (!sent && !request->usb) publishPcEvent(pcReceiptJson(*request));
}

void serviceAvrFor(uint32_t durationMs) {
  // Disabled AVR means no UART drain/reply can arrive. In particular, do not
  // impose the legacy 18 ms drain window on every remote-download 1 KiB chunk.
  if (!PAKAR_AVR_LINK_ENABLED) return;
#if defined(ARDUINO_ARCH_ESP32) && PAKAR_UART_BUFFERED
  // UART1 owns a driver RX ring; service its bounded entry occupancy and let
  // the main loop continue. The ESP8266's timed interrupt-safety drain would
  // add 18 ms to every firmware-download chunk without protecting this UART.
  link.service();
  return;
#else
  const uint32_t startedAt = millis();
  do {
    link.service();
    delay(1);
    yield();
  } while (static_cast<uint32_t>(millis() - startedAt) < durationMs);
#endif
}

bool requestAndWaitForAck(uint8_t opcode, const uint8_t *payload = nullptr,
                          uint8_t payloadLength = 0,
                          uint16_t timeoutMs = 750) {
  awaitedSequence = sequence++;
  awaitedOpcode = opcode;
  awaitedReplySeen = false;
  awaitedReplyOk = false;
  if (!sendToAvr(opcode, awaitedSequence, payload, payloadLength)) return false;
  const uint32_t startedAt = millis();
  while (!awaitedReplySeen &&
         static_cast<uint32_t>(millis() - startedAt) < timeoutMs) {
    link.service();
    delay(1);
    yield();
  }
  return awaitedReplySeen && awaitedReplyOk;
}

bool waitForAvr(uint16_t timeoutMs = 2500) {
  avrOnline = false;
  avrVersionKnown = false;
  const uint32_t startedAt = millis();
  uint32_t lastHello = 0;
  while (!avrOnline && static_cast<uint32_t>(millis() - startedAt) < timeoutMs) {
    const uint32_t now = millis();
    if (lastHello == 0 || static_cast<uint32_t>(now - lastHello) >= 250U) {
      lastHello = now;
      sendToAvr(kHello, sequence++);
    }
    link.service();
    delay(1);
    yield();
  }
  return avrOnline;
}

bool updateCueAvailable(uint8_t cue) {
  return PAKAR_AVR_LINK_ENABLED && avrOnline && avrVersionKnown && (avrCapabilities & kCapBuzzer) &&
      avrAudioCueCount>cue && !programmingLatched && !avrUploadOwner &&
      !urbootRawOwned && !passiveCaptureActive() && static_cast<uint32_t>(millis()-lastAvrFrameAt)<=3000U;
}
bool playUpdateStartCue() {
  const uint8_t cue = kCueUpdateStart;
  return updateCueAvailable(cue) && sendToAvr(kAudioCue, sequence++, &cue, 1);
}
bool playUpdateSuccessCue() {
  const uint8_t cue=kCueUpdateSuccess;
  return updateCueAvailable(cue) && sendToAvr(kAudioCue,sequence++,&cue,1);
}
bool playUpdateFailureCue() {
  const uint8_t cue=kCueError;
  return updateCueAvailable(cue) && sendToAvr(kAudioCue,sequence++,&cue,1);
}
String upgradeJson(bool event,bool phaseChanged) {
  JsonDocument d;
  if(event) {d["type"]="update-progress";d["event"]=phaseChanged?"update.phase":"update.progress";}
  d["board_id"]=pcBoardId;d["boot_id"]=pcBootId;
  d["revision"]=upgradeLifecycle.revision();
  d["source"]=PakarUpdate::sourceName(upgradeLifecycle.source());
  d["operation_id"]=upgradeLifecycle.operation();d["phase"]=upgradeLifecycle.phase();
  d["source_boot_id"]=upgradeLifecycle.source()==PakarUpdate::Source::Local &&
    upgradeLifecycle.operation()==localOtaReceipt.uploadId()?localOtaReceipt.sourceBoot().c_str():pcBootId.c_str();
  d["active"]=upgradeLifecycle.active();d["completion_verified"]=upgradeLifecycle.verified();
  d["received_bytes"]=upgradeLifecycle.done();d["expected_bytes"]=upgradeLifecycle.total();
  d["progress_known"]=upgradeLifecycle.known();
  if(upgradeLifecycle.known())d["progress_percent"]=upgradeLifecycle.percent();else d["progress_percent"]=nullptr;
  d["started_uptime_ms"]=upgradeLifecycle.startedAt();d["updated_uptime_ms"]=upgradeLifecycle.updatedAt();
  using V=PakarUpdate::Visual;const auto view=upgradeLifecycle.visual(millis());
  d["presentation"]=view==V::Startup?"startup":view==V::Shutdown?"shutdown":view==V::Progress?"progress":
    view==V::Success?"complete":view==V::Failure?"failed":"none";
  d["matrix_output_enabled"]=grid.outputEnabled();
  d["black_frame_transmitted"]=grid.outputEnabled() && grid.maintenancePresented();
  d["matrix_presentation_paused"]=urbootRawOwned;
  d["completion_sound"]=localUpgradeSound; // Enqueue/availability, not audible proof.
  d["scene_saved"]=sceneStore.saved();d["scene_error"]=sceneStore.error();
  d["automatic_retry"]=false;
  const auto source=upgradeLifecycle.source();
  d["error"]=source==PakarUpdate::Source::Remote?remoteFirmware.error():
    source==PakarUpdate::Source::Avr?urbootError:String(localOtaReceipt.error());
  String out;serializeJson(d,out);return out;
}
#if defined(ARDUINO_ARCH_ESP32)
String statusRgbObservationJson() {
  const auto &rgb=statusRgb.snapshot();const auto &config=statusRgbConfig.configuration();JsonDocument d;
  d["available"]=rgb.available;d["enabled"]=rgb.enabled;d["have_output"]=rgb.haveOutput;
  d["pin"]=rgb.pin;d["pin_confirmed"]=config.pinConfirmed;d["brightness"]=rgb.brightness;
  d["rgb"].add(rgb.red);d["rgb"].add(rgb.green);d["rgb"].add(rgb.blue);
  d["state"]=PakarS3::StatusRgb::name(rgb.condition);d["effect"]=rgb.effect;
  d["effect_name"]=PakarS3::StatusRgb::effectName(rgb.effect);
  if(rgb.progress<=100)d["progress_percent"]=rgb.progress;else d["progress_percent"]=nullptr;
  d["persisted"]=statusRgbConfig.persisted();d["error"]=statusRgbConfig.lastError();
  d["revision"]=rgb.revision;d["updated_uptime_ms"]=rgb.updatedAt;
  String result;serializeJson(d,result);return result;
}
void serviceStatusRgb(uint32_t now) {
  using Engine=PakarS3::StatusRgb;using Visual=PakarUpdate::Visual;
  const auto visual=upgradeLifecycle.visual(now);Engine::Observations observed;
  observed.fault=!accessConfig.ready()||visual==Visual::Failure;
  observed.updating=otaActive||otaRebootAt||remoteFirmware.busy()||urbootBusy.get();
  observed.booting=visual==Visual::Startup;
  observed.shutdown=visual==Visual::Shutdown;observed.success=visual==Visual::Success;
  observed.warning=statusRgbConfig.lastError()[0]!=0;
  observed.stationEnabled=networkConfig.stationEnabled();observed.networkReady=NetworkPath::ready();
  observed.accessPoint=apStarted;observed.peerRequired=PAKAR_AVR_LINK_ENABLED;observed.peerOnline=avrOnline;
  statusRgb.setState(Engine::selectState(observed),now);
  statusRgb.setProgress(observed.updating&&upgradeLifecycle.known()?upgradeLifecycle.percent():255);
  statusRgb.service(now,writeStatusRgb);
  // Compact latest output observation, not a synthetic client animation or a
  // full hardware-freshness renewal. BLE retains its separately bounded feed.
  static uint32_t sentAt=0,sentRevision=0;
  const auto revision=statusRgb.snapshot().revision;
  if(revision!=sentRevision && uint32_t(now-sentAt)>=50U) {
    sentAt=now;sentRevision=revision;JsonDocument head;
    head["type"]="status-rgb";head["board_id"]=pcBoardId;head["boot_id"]=pcBootId;head["revision"]=revision;
    String event;serializeJson(head,event);event.remove(event.length()-1);
    event+=F(",\"status_rgb\":");event+=statusRgbObservationJson();event+='}';webSocket.textAll(event);
  }
}
#endif
void serviceUpgrade() {
  observeHostIngressFence();
  if(servicingUpgrade)return;
  servicingUpgrade=true;
  using S=PakarUpdate::Source;
  const uint32_t now=millis();
  const auto local=otaProgress.snapshot();
  const bool localCurrent=local.operation && (local.operation!=localObservedOperation ||
    otaActive || otaRebootAt || upgradeLifecycle.source()==S::Local);
  PakarUpdate::Completion::Intent accepted;
  if(otaCompletion.take(accepted)) {
    flushUpgradeScene();
    localOtaReceipt.awaitingReboot(pcBootId.c_str(),accepted.operation,accepted.bytes,accepted.md5);
  }
  if(urbootBusy.get() || (urbootJobId && upgradeLifecycle.source()==S::Avr &&
      !localCurrent && !otaActive && !otaRebootAt && !remoteFirmware.busy())) {
    upgradeLifecycle.observe(S::Avr,urbootJobId,urbootPhase,urbootDone,urbootTotal,
      urbootBusy.get(),!strcmp(urbootPhase,"complete") && urbootRecovered &&
      (urbootOperation!="program" || urbootVerified),now);
  } else {
    if(localCurrent) {
      localObservedOperation=local.operation;
      upgradeLifecycle.observe(S::Local,local.operation,PakarUpdate::name(local.phase),local.received,local.expected,
        otaActive || otaRebootAt,false,now,true);
    } else if(localBootNoticePending) {
      // Terminal notices require the durable exact-image/new-boot receipt.
      upgradeLifecycle.observe(S::Local,localOtaReceipt.uploadId(),"verify_esp",0,0,true,false,now);
      upgradeLifecycle.observe(S::Local,localOtaReceipt.uploadId(),localOtaReceipt.phase(),0,0,false,
        localOtaReceipt.completionVerified(),now);
      localBootNoticePending=false;localBootCuePending=true;localBootCueUntil=now+2500U;localUpgradeSound="pending";
    }
    if(!otaActive && !otaRebootAt && remoteFirmware.busy()) {
      const auto measured=remoteFirmware.progress();
      upgradeLifecycle.observe(S::Remote,remoteFirmware.jobId(),remoteFirmware.progressStage(),
        measured.received,measured.expected,true,false,now);
    } else if(upgradeLifecycle.source()==S::Remote && !otaActive && !otaRebootAt) {
      const auto measured=remoteFirmware.progress();
      upgradeLifecycle.observe(S::Remote,remoteFirmware.jobId(),remoteFirmware.progressStage(),
        measured.received,measured.expected,false,remoteFirmware.completionVerified(),now);
    }
  }
  if(!urbootRawOwned) {
#if !defined(ARDUINO_ARCH_ESP32)
    setAvrTransmitBlocked(true);serviceAvrFor(8U);
#endif
    grid.presentMaintenance(upgradeLifecycle,millis());
#if !defined(ARDUINO_ARCH_ESP32)
    setAvrTransmitBlocked(false);
#endif
  }
  if(upgradeLifecycle.takeShutdownNotice(now) &&
      (upgradeLifecycle.source()!=S::Avr || urbootOperation=="program"))playUpdateStartCue();
  const int8_t terminalNotice=upgradeLifecycle.takeTerminalNotice();
  if(upgradeLifecycle.source()!=S::Local &&
      (upgradeLifecycle.source()!=S::Avr || urbootOperation=="program")) {
    if(terminalNotice>0)playUpdateSuccessCue();else if(terminalNotice<0)playUpdateFailureCue();
  }
  const uint8_t completionCue=localOtaReceipt.completionVerified()?kCueUpdateSuccess:kCueError;
  if(localBootCuePending && (updateCueAvailable(completionCue) || static_cast<int32_t>(now-localBootCueUntil)>=0)) {
    const auto notice=localOtaReceipt.takeNotice();localBootCuePending=false;
    bool queued=false;
    if(notice==LocalOtaReceipt<fs::FS>::Notice::Complete)queued=playUpdateSuccessCue();
    else if(notice==LocalOtaReceipt<fs::FS>::Notice::Failed)queued=playUpdateFailureCue();
    localUpgradeSound=queued?"enqueued":"unavailable";
  }
  bool phaseChanged=false;
  if(upgradeLifecycle.takeEvent(now,phaseChanged)) {
    if(upgradeLifecycle.source()!=S::Avr || urbootOperation=="program")
      ntfyService.observe(upgradeLifecycle,now);
    const String notice=upgradeJson(true,phaseChanged);
    // Loop-only, finite phase/progress notices, never from async flash callbacks.
    webSocket.textAll(notice);pendingUpgradeCloud=notice;
#if defined(ARDUINO_ARCH_ESP32)
    publishBleEvent(notice);
#endif
  }
  // A TLS/MQTT write may block. Never do one while Urboot owns raw UART.
  // Coalesce its progress to the latest snapshot; do not replay stale stages.
  if(!urbootRawOwned && pendingUpgradeCloud.length()) {
    cloudServices.publishEvent(pendingUpgradeCloud);pendingUpgradeCloud="";
  }
#if defined(ARDUINO_ARCH_ESP32)
  serviceStatusRgb(millis());
#endif
  servicingUpgrade=false;
}
void upgradeBackground() {
  serviceUpgrade();serviceHostLink();
  serviceSemanticPanel(millis());
#if defined(ARDUINO_ARCH_ESP32)
  serviceBleReadOnly();
#endif
}
bool upgradeRebootReady() {
  serviceUpgrade();return upgradeLifecycle.readyToReboot(millis(),grid.maintenancePresented());
}
void serviceAttentionCues() {
  using Cue=BoardCore::Attention::Cue;
  const auto dispatch=[](Cue value) {
    // Consume once even if no companion is available; never replay an old
    // upgrade sound when AVR reconnects. Serial enqueue is not audible proof.
    if(value==Cue::Start) {playUpdateStartCue();return;}
    if(value==Cue::Success) {playUpdateSuccessCue();return;}
    if(value==Cue::Failure && PAKAR_AVR_LINK_ENABLED && avrOnline && avrVersionKnown &&
       (avrCapabilities & kCapBuzzer) && avrAudioCueCount>kCueError &&
       !programmingLatched && !avrUploadOwner && !passiveCaptureActive() &&
       static_cast<uint32_t>(millis()-lastAvrFrameAt)<=3000U) {
      const uint8_t cue=kCueError;sendToAvr(kAudioCue,sequence++,&cue,1);
    }
  };
  const auto upload=uploadAttention.take();
  // Upload completion only schedules reboot, never a verified-success cue.
  if(upload==Cue::Failure)dispatch(upload);
  const auto remote=remoteAttention.next(remoteFirmware.jobId(),remoteFirmware.phase());
  if(remote==Cue::Start) {
    // Let the finite AVR melody finish before Prog deliberately silences it.
    // This is a cooperative lead-in, never a blocking UART/audio wait.
    remoteCueLead=true;remoteCueUntil=millis()+PakarUpdate::Lifecycle::shutdownMs;
    serviceUpgrade();
  } // Lifecycle owns actual terminal notices, not a second melody producer.
}

bool authenticate() {
  if(urbootBusy.get() && server.request() && (server.request()->method()!=HTTP_GET ||
      (server.request()->url()!=F("/api/status") && server.request()->url()!=F("/api/telemetry") &&
       server.request()->url()!=F("/info") && server.request()->url()!=F("/api/avr/maintenance") &&
       server.request()->url()!=F("/api/update/lifecycle") &&
       server.request()->url()!=F("/api/auth/info") && server.request()->url()!=F("/api/firmware/remote")))) {
    server.send(409,"text/plain",F("Urboot owns board maintenance; inspect its job, do not replay"));return false;
  }
  if ((remoteFirmware.busy() || artifactReaders) && server.request() &&
      !server.request()->url().startsWith("/api/firmware/") &&
      server.request()->url()!=F("/file/cancel") &&
      (server.request()->method()!=HTTP_GET || server.request()->url()==F("/api/avr/probe"))) {
    server.send(409,"text/plain",F("Remote firmware operation owns board maintenance")); return false;
  }
  if ((otaActive || otaRebootAt) && (!server.request() ||
      server.request()->method()!=HTTP_GET ||
      (server.request()->url()!=F("/api/status") && server.request()->url()!=F("/api/firmware/remote")))) {
    server.send(503, "text/plain", F("ESP update in progress")); return false;
  }
  if (avrUploadOwner && server.request() && server.request()->method() != HTTP_GET) {
    server.send(409, "text/plain", F("AVR image upload in progress")); return false;
  }
  if (server.request() && webAccess.allowed(server.request())) {
    // Middleware admits async requests before loop dispatch. Recheck ownership
    // at the hardware boundary as well, not only when entering the queue.
    if (!webAccess.peerAllowed(server.request())) {
      server.send(409, "application/json", F("{\"error\":\"peer_changed\",\"accepted\":false}")); return false;
    }
    const String path = server.request()->url();
#if defined(ARDUINO_ARCH_ESP32) && !PAKAR_AVR_LINK_ENABLED
    if (passiveAvrRx.snapshot().active && (server.request()->method()!=HTTP_GET ||
        path==F("/api/wifi/scan") || path==F("/hostname/check") || path==F("/api/avr/probe"))) {
      server.send(409,"text/plain",F("Bounded passive UART capture owns diagnostics"));return false;
    }
#endif
    using namespace BoardCore::Context;
    uint16_t required = 0;
    if (path == F("/api/cell") || path == F("/api/cells/mapping") ||
        path == F("/api/game/reset") || path == F("/api/snake/direction")) required = Table;
    if(path==F("/api/cells"))required=CellBatch;
    if (path == F("/api/display")) required = server.hasArg("text") ? Text : Display;
    if (path == F("/api/audio") || path == F("/api/buzzer")) required = Buzzer;
    if (path == F("/api/outputs")) required = Outputs;
    bool allowed = !required || (currentFeatures() & required);
    if(path==F("/api/cell")||path==F("/api/cells")||path==F("/api/cells/assignments")||path==F("/api/cells/lock")) {
      if(!PAKAR_TABLE_SELECTION){server.send(409,"application/json",F("{\"accepted\":false,\"error\":\"unsupported_board_role\",\"message\":\"This firmware has a different baked-in game role.\"}"));return false;}
      allowed=currentFeatures() & Table;
    }
    if (path == F("/api/mode")) {
      allowed = false;
      for (uint8_t code = 0; code < 4; ++code)
        if (server.arg("value") == BoardCore::Context::modeName(code) && (modes(currentFeatures()) & (1U << code))) allowed = true;
    }
    if (!allowed) { server.send(409, "application/json", F("{\"error\":\"capability_not_acknowledged\"}")); return false; }
    return true;
  }
  server.send(401, "application/json", F("{\"error\":\"login_required\"}"));
  return false;
}

#if PAKAR_SNAKE_ENABLED
uint8_t snakeCell(uint8_t position) {
  const uint8_t fromRight = position % 10U;
  const uint8_t y = position / 10U;
  return static_cast<uint8_t>(
      fromRight * 8U + (fromRight % 2U == 0U ? 8U - y : y + 1U));
}

void renderSnake() {
  grid.fill(kCellOff);
  if (snakeFood < LedGrid::kCells) {
    grid.setCell(snakeCell(snakeFood), kCellRed, kEffectImmediate);
  }
  for (uint8_t index = 0; index < snakeLength; ++index) {
    grid.setCell(snakeCell(snakeBody[index]), kCellWhite, kEffectImmediate);
  }
}

void placeSnakeFood() {
  uint8_t candidate = static_cast<uint8_t>((snakeFood * 17U + 23U) % 80U);
  for (uint8_t attempts = 0; attempts < 80; ++attempts) {
    bool occupied = false;
    for (uint8_t index = 0; index < snakeLength; ++index) {
      if (snakeBody[index] == candidate) {
        occupied = true;
        break;
      }
    }
    if (!occupied) {
      snakeFood = candidate;
      return;
    }
    candidate = static_cast<uint8_t>((candidate + 1U) % 80U);
  }
}

void startSnake() {
  snakeLength = 3;
  snakeBody[0] = 44;
  snakeBody[1] = 43;
  snakeBody[2] = 42;
  snakeFood = 7;
  snakeDirection = 1;
  snakeAlive = true;
  lastSnakeAt = millis();
  placeSnakeFood();
  renderSnake();
}

bool setSnakeDirection(uint8_t direction) {
  if (direction > 3 || (direction ^ 2U) == snakeDirection) return false;
  snakeDirection = direction;
  return true;
}

void serviceSnake(uint32_t now) {
  if (mode != kModeSnake || !snakeAlive ||
      static_cast<uint32_t>(now - lastSnakeAt) < 250U) {
    return;
  }
  lastSnakeAt = now;
  const int8_t x = static_cast<int8_t>(snakeBody[0] % 10U);
  const int8_t y = static_cast<int8_t>(snakeBody[0] / 10U);
  const int8_t nextX = x + (snakeDirection == 1 ? 1 : snakeDirection == 3 ? -1 : 0);
  const int8_t nextY = y + (snakeDirection == 2 ? 1 : snakeDirection == 0 ? -1 : 0);
  if (nextX < 0 || nextX >= 10 || nextY < 0 || nextY >= 8) {
    snakeAlive = false;
  }
  const uint8_t next = static_cast<uint8_t>(nextY * 10 + nextX);
  const bool eating = snakeAlive && next == snakeFood;
  const uint8_t collisionLimit =
      eating ? snakeLength : static_cast<uint8_t>(snakeLength - 1U);
  for (uint8_t index = 0; snakeAlive && index < collisionLimit; ++index) {
    if (snakeBody[index] == next) snakeAlive = false;
  }
  if (!snakeAlive) {
    const uint8_t cue = kCueLost;
    sendToAvr(kAudioCue, sequence++, &cue, 1);
    grid.fill(kCellRed);
    return;
  }
  if (eating && snakeLength < LedGrid::kCells) ++snakeLength;
  for (uint8_t index = snakeLength - 1U; index > 0; --index) {
    snakeBody[index] = snakeBody[index - 1U];
  }
  snakeBody[0] = next;
  if (eating) {
    const uint8_t cue = kCueIncrease;
    sendToAvr(kAudioCue, sequence++, &cue, 1);
    placeSnakeFood();
  }
  renderSnake();
}

#else
void startSnake() {}
bool setSnakeDirection(uint8_t) { return false; }
void serviceSnake(uint32_t) {}
#endif

const char *modeName() {
  switch (mode) {
    case kModeCellSelect: return "cell";
    case kModeFirstWins: return "first-wins";
    case kModeSnake: return "snake";
    default: return "unknown";
  }
}

const char *cellName(CellState state) {
  switch (state) {
    case kCellRed: return "red";
    case kCellOff: return "off";
    default: return "white";
  }
}

String cellAssignmentsJson(bool accepted=false) {
  String body=accepted?F("{\"accepted\":true,\"assignments\":["):F("{\"assignments\":[");
  for(uint8_t cell=1;cell<=LedGrid::kCells;++cell){
    if(cell>1)body+=',';
    body+=F("{\"cell\":");body+=cell;body+=F(",\"state\":\"");body+=cellName(grid.state(cell));body+=F("\"}");
  }
  body+=F("],\"red_lock\":");body+=grid.redLock()?F("true}"):F("false}");return body;
}

bool applyCellAssignments(JsonVariantConst value,String &error) {
  if(!PAKAR_TABLE_SELECTION){error=F("unsupported_board_role");return false;}
  if(!value.is<JsonArrayConst>() || !value.size() || value.size()>LedGrid::kCells){error=F("invalid_cell_assignments");return false;}
  uint8_t ids[80],states[80],count=0;BoardCore::CellBatch seen;
  for(JsonObjectConst row:value.as<JsonArrayConst>()) {
    if(!row["cell"].is<uint8_t>() || !row["state"].is<const char*>()){error=F("invalid_cell_assignments");return false;}
    const uint8_t id=row["cell"].as<uint8_t>();const String state=row["state"].as<String>();
    if(id<1||id>80||seen.contains(id)||state.length()!=row["state"].as<JsonString>().size()||(state!="red"&&state!="white"&&state!="off")){error=F("invalid_cell_assignments");return false;}
    seen.add(id);ids[count]=id;states[count++]=state=="red"?kCellRed:state=="off"?kCellOff:kCellWhite;
  }
  if(!grid.setAssignments(ids,states,count)){error=F("red_cells_locked");return false;}
  return true;
}

uint64_t commandClock() {
  static uint32_t previous=0;static uint64_t high=0;
  const uint32_t now=millis();if(now<previous)high+=1ULL<<32;previous=now;
  return high|now; // Queried every telemetry tick; old intents cannot revive at millis wrap.
}
#include "src/SemanticPanelService.h"
String buildStatusJson() { return buildStatusJson(false); }
String buildStatusJson(bool compact) {
  String body;
  // Reserve the complete document before TLS/WebSocket activity fragments the
  // ESP8266 heap. Arduino String otherwise fails mid-JSON without an error.
  if(!body.reserve(compact ? 10240 : 2600))return String();
  body += F("{\"version\":\""); body += BoardCore::BuildInfo::kVersion;
  body += F("\",\"compatibility\":");body+=PakarPeer::policyJson();
  body += F(",\"board_id\":\""); body += pcBoardId;
  body += F("\",\"boot_id\":\""); body += pcBootId;
  body += F("\",\"http_peer_fencing\":"); body += webAccess.peerFenceReady() ? F("true") : F("false");
  char commandTime[24];snprintf(commandTime,sizeof(commandTime),"%llu",static_cast<unsigned long long>(commandClock()));
  body += F(",\"command_clock_ms\":"); body += commandTime;
  body += F(",\"hostname\":\""); body += hostname;
  body += F("\",\"uplink\":\""); body += NetworkPath::interfaceName();
  body += F("\",\"uplink_ipv4_ready\":"); body += NetworkPath::ready() ? F("true") : F("false");
  if(!compact) {
  body += F(",\"uplink_route_changes\":");body+=NetworkPath::state().routeChanges;
  body += F(",\"uplink_resolver_changes\":");body+=NetworkPath::state().resolverChanges;
  }
  body += F(",\"cloud_command_fencing\":true,\"cloud_command_deadline_seconds\":60");
  body += F(",\"ethernet\":");body+=ethernetStatusJson(compact);
#if defined(ARDUINO_ARCH_ESP32)
  if(!compact) {body += F(",\"ble\":");body+=bleStatusJson();}
  wifi_ps_type_t observedSleep=WIFI_PS_NONE;
  const bool sleepKnown=esp_wifi_get_ps(&observedSleep)==ESP_OK;
  body += F(",\"wifi_modem_sleep\":");
  body += sleepKnown ? (observedSleep==WIFI_PS_NONE ? F("false") : F("true")) : F("null");
  body += F(",\"wifi_modem_sleep_requested\":");body+=PAKAR_S3_WIFI_MODEM_SLEEP?F("true"):F("false");
#endif
  body += F(",\"ip\":\""); body += (NetworkPath::ready() ? NetworkPath::localIP() : WiFi.softAPIP()).toString();
  body += F("\",\"mode\":\""); body += modeName();
  body += F("\",\"led_output\":");body+=grid.outputJson(compact);
  if(!compact) {
  body += F(",\"state_persistence\":{\"restored\":");body+=sceneStore.restored()?F("true"):F("false");
  body += F(",\"pending\":");body+=sceneStore.pending()?F("true"):F("false");
  body += F(",\"saved\":");body+=sceneStore.saved()?F("true"):F("false");
  body += F(",\"writes\":");body+=sceneStore.writes();
  body += F(",\"save_delay_ms\":");body+=SceneStore<fs::FS>::saveDelayMs;
  body += F(",\"error\":");if(sceneStore.error()){body+='"';body+=sceneStore.error();body+='"';}else body+=F("null");
  body += '}';
  body += F(",\"ws_queue_rejected\":");body+=loopMessages.rejected();
  body += F(",\"ws_queue_ready\":");body+=loopMessages.ready()?F("true"):F("false");
  }
  body += F(",\"update\":");body+=localUpdateJson();
  body += F(",\"upgrade\":");body+=upgradeJson();
  if(!compact) {body += F(",\"local_ota_receipt\":");body+=localOtaReceipt.json();}
  body += F(",\"avr_maintenance\":");body+=urbootJobJson();
  // Cheap stress/health metric; unlike maintenance metadata this never hashes
  // the running sketch or pauses the LED transmitter for flash reads.
  body += F(",\"free_heap_bytes\":");body+=ESP.getFreeHeap();
#if defined(ARDUINO_ARCH_ESP32)
  body += F(",\"internal_heap_free_bytes\":");body+=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  body += F(",\"internal_heap_largest_block_bytes\":");body+=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  body += F(",\"internal_heap_minimum_free_bytes\":");body+=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
#endif
  body += F(",\"avr_online\":"); body += avrOnline ? F("true") : F("false");
  body += F(",\"avr_role\":");if(avrOnline&&avrRoleKnown){body+='"';body+=avrGameRole==1?F("table-selection"):avrGameRole==2?F("first-wins"):F("unsupported");body+='"';}else body+=F("null");
  body += F(",\"avr_role_compatible\":");body+=avrOnline&&avrRoleKnown?(avrRoleCompatible?F("true"):F("false")):F("null");
  body += F(",\"avr_version\":");
  if (avrOnline && avrVersionKnown) {
    body += '"'; body += avrVersion[0]; body += '.'; body += avrVersion[1];
    body += '.'; body += avrVersion[2]; body += '"';
  } else body += F("null");
  body += F(",\"audio_cues\":[");
  const uint8_t cueCount = BoardCore::AudioCues::supportedCount(avrOnline, (avrCapabilities & kCapBuzzer) != 0, avrAudioCueCount);
  for (uint8_t i = 0; i < cueCount; ++i) {
    if (i) body += ',';
    body += '"'; body += BoardCore::AudioCues::name(i); body += '"';
  }
  // Contract support is peer-advertised, not inferred from ESP source flags.
  body += ']';
  body += F(",\"ap_started\":"); body += apStarted ? F("true") : F("false");
  body += F(",\"ap_enabled\":"); body += networkConfig.accessPointEnabled() ? F("true") : F("false");
  body += F(",\"ap_ip\":\""); body += WiFi.softAPIP().toString(); body += '"';
  body += F(",\"station_enabled\":"); body += networkConfig.stationEnabled() ? F("true") : F("false");
  body += F(",\"station_connected\":"); body += networkConfig.stationConnected() ? F("true") : F("false");
  body += F(",\"station_ssid\":\""); appendJsonEscaped(body,networkConfig.stationSsid()); body += '"';
  body += F(",\"station_ip\":\""); body += WiFi.localIP().toString(); body += '"';
  body += F(",\"station_rssi\":"); body += networkConfig.stationConnected() ? WiFi.RSSI() : 0;
  body += F(",\"mdns_started\":"); body += mdnsStarted ? F("true") : F("false");
  body += F(",\"ssdp_started\":"); body += ssdpStarted ? F("true") : F("false");
  body += F(",\"mndp_started\":");
#if defined(ARDUINO_ARCH_ESP32)
  body += s3Discovery.mndpReady() ? F("true") : F("false");
  body += F(",\"status_rgb\":");body+=statusRgbObservationJson();
  body += F(",\"isp_available\":"); body += PAKAR_S3_ISP_ELECTRICAL_VERIFIED ? F("true") : F("false");
#else
  body += mndpService.running() ? F("true") : F("false");
#endif
  body += F(",\"netbios_started\":"); body += netbiosStarted ? F("true") : F("false");
#if defined(ARDUINO_ARCH_ESP32)
  body += F(",\"netbios_state\":\"");body += s3Discovery.netbiosState();body += '"';
  body += F(",\"netbios_interfaces\":");body += s3Discovery.netbiosInterfaces();
#endif
  body += F(",\"canonical_url\":\"http://"); body += hostname; body += F(".local\"");
  body += F(",\"mdns_url\":\"http://"); body += hostname; body += F(".local\"");
  body += F(",\"winner\":"); body += winner;
  body += F(",\"selected_cell\":"); body += selectedCell;
  body += F(",\"snake_alive\":"); body += snakeAlive ? F("true") : F("false");
  body += F(",\"snake_length\":"); body += snakeLength;
  body += F(",\"red_lock\":"); body += grid.redLock() ? F("true") : F("false");
  body += F(",\"brightness\":"); body += grid.brightness();
  body += F(",\"effect\":\""); body += grid.patternName(); body += '"';
  body += F(",\"esp_uart_errors\":"); body += link.errors();
  body += F(",\"avr_uart_errors\":"); body += avrUartErrors;
  body += F(",\"tm1637_detection_known\":");body += tm1637DetectionKnown()?F("true"):F("false");
  body += F(",\"tm1637_detected\":");body += !tm1637DetectionKnown()?F("null"):(tm1637Ack?F("true"):F("false"));
  body += F(",\"tm1637_ack\":"); body += tm1637DetectionKnown()&&tm1637Ack ? F("true") : F("false");
  body += F(",\"lcd\":");body += lcdStatusJson();
  body += F(",\"tm1637_segments\":[");
  for (uint8_t i = 0; i < 4; ++i) {
    if (i != 0) body += ',';
    body += tm1637Segments[i];
  }
  body += F("]");
  body += F(",\"tm1637_brightness\":"); body += tm1637Brightness;
  body += F(",\"tm1637_host_active\":"); body += tm1637HostActive ? F("true") : F("false");
  body += F(",\"mirroring\":{\"avr_session\":"); body += avrSession;
  body += F(",\"buzzer_timestamps\":"); body += avrOnline && avrToneTimestamps ? F("true") : F("false");
  body += F(",\"front_panel\":"); body += avrOnline && avrPanelKnown ? F("true") : F("false");
  body += F("}");
  if (avrOnline && avrPanelKnown) {
    body += F(",\"front_panel\":{\"menu_page\":"); body += avrMenuPage;
    body += F(",\"menu_active\":"); body += avrMenuActive ? F("true") : F("false");
    body += F(",\"running\":"); body += avrRunning ? F("true") : F("false");
    body += F(",\"captured\":"); body += avrCaptured ? F("true") : F("false");
    body += F(",\"meta\":"); body += avrPanelMeta; body += '}';
  }
  body += F(",\"programming_latch\":"); body += programmingLatched ? F("true") : F("false");
  body += F(",\"link_mode\":\"");
#if defined(PAKAR_UART_BUFFERED) && PAKAR_UART_BUFFERED
  body += F("buffered");
#else
  body += PAKAR_USE_BUSY_WIRE ? F("busy-wire") : F("polling");
#endif
  body += '"';
  body += F(",\"avr_link_enabled\":"); body += PAKAR_AVR_LINK_ENABLED ? F("true") : F("false");
#if defined(ARDUINO_ARCH_ESP32)
  body += F(",\"avr_uart_ready\":"); body += avrUart.started() ? F("true") : F("false");
  body += F(",\"avr_uart_hardware_errors\":"); body += avrUart.hardwareErrors();
#endif
  body += F(",\"active_inputs\":"); body += activeInputs;
  body += F(",\"input_9\":"); body += activeInput9 ? F("true") : F("false");
  body += F(",\"active_outputs\":"); body += activeOutputs;
  const time_t currentTime = time(nullptr);
  body += F(",\"ntp_synced\":");
  body += ntpObserved && currentTime > 1700000000 ? F("true") : F("false");
  body += F(",\"utc_epoch\":");
  body += currentTime > 1700000000 ? static_cast<uint32_t>(currentTime) : 0U;
  body += F(",\"websocket_clients\":");
  body += webSocket.count();
  body += F(",\"telnet_connected\":");
  body += shellConnectionCount() ? F("true") : F("false");
  if(!compact) {body += F(",\"shell\":");body += shellStatusJson();}
  body += F(",\"mqtt_enabled\":");
  body += cloudConfig.mqttEnabled() ? F("true") : F("false");
  body += F(",\"mqtt_connected\":");
  body += cloudServices.mqttConnected() ? F("true") : F("false");
  body += F(",\"mqtt_state\":"); body += cloudServices.mqttState();
  if(!compact) {
  const auto &mqttTiming=cloudServices.mqttConnectionStats();
  body += F(",\"mqtt_connection\":{\"attempts\":");body += mqttTiming.attempts;
  body += F(",\"failures\":");body += mqttTiming.failures;
  body += F(",\"last_ms\":");body += mqttTiming.lastDurationMs;
  body += F(",\"max_ms\":");body += mqttTiming.maxDurationMs;body += '}';
  const auto &cloudIo=cloudServices.ioStats();
  body += F(",\"cloud_io\":{\"services\":");body+=cloudIo.services;
  body += F(",\"last_service_ms\":");body+=cloudIo.lastServiceMs;
  body += F(",\"max_service_ms\":");body+=cloudIo.maxServiceMs;
  body += F(",\"publishes\":");body+=cloudIo.publishes;
  body += F(",\"last_publish_ms\":");body+=cloudIo.lastPublishMs;
  body += F(",\"max_publish_ms\":");body+=cloudIo.maxPublishMs;
  body += F(",\"suspend_closes\":");body+=cloudIo.suspendCloses;
  body += F(",\"configuration_closes\":");body+=cloudIo.configurationCloses;
  body += F(",\"clean_closes\":");body+=cloudIo.cleanCloses;
  body += F(",\"unclean_closes\":");body+=cloudIo.uncleanCloses;
  body += F(",\"write_closes\":");body+=cloudIo.writeCloses;body+='}';
  body += F(",\"mqtt_messages_received\":");
  body += cloudServices.mqttMessagesReceived();
  }
  body += F(",\"cloud_websocket_enabled\":");
  body += cloudConfig.webSocketEnabled() ? F("true") : F("false");
  body += F(",\"cloud_websocket_connected\":");
  body += cloudServices.webSocketConnected() ? F("true") : F("false");
  if(!compact) {body += F(",\"cloud_websocket_messages_received\":");
  body += cloudServices.webSocketMessagesReceived();}
  body += F(",\"remote_firmware\":"); body += remoteFirmware.json();
  String cloudError = cloudServices.lastError();
  cloudError.replace("\\", "\\\\");
  cloudError.replace("\"", "\\\"");
  body += F(",\"cloud_error\":\""); body += cloudError; body += '"';
  if(!compact) {
  body += F(",\"uart_bytes_received\":"); body += link.bytesReceived();
  body += F(",\"uart_frames_received\":"); body += link.framesReceived();
  body += F(",\"usb_serial_bytes_received\":"); body += hostLink.bytesReceived();
  body += F(",\"usb_serial_frames_received\":"); body += hostLink.framesReceived();
  }
  body += F(",\"usb_serial_errors\":"); body += hostLink.errors();
  body += F(",\"pc_contract_supported\":");
  body += avrOnline && avrPcSupported ? F("true") : F("false");
  body += F(",\"pc_contract_features\":");
  if (avrOnline && avrPcSupported) body += avrPcFeatures;
  else body += F("null");
  body += F(",\"capabilities\":{\"board_kind\":\"");
  body += PAKAR_TABLE_ENABLED ? F("table") : F("controller");
  body += F("\",\"role\":\""); body += BoardCore::GameRole::name;
  body += F("\",\"name\":\""); appendJsonEscaped(body, deviceIdentity.displayName());
  body += F("\",\"hostname\":\""); body += deviceIdentity.hostname();
  body += F("\",\"application\":"); body += applicationIdentityJson();
  body += F(",\"acknowledged_features\":[\"command-ack\",\"pixel-diagnostic\",\"panel-events\"");
#if PAKAR_TABLE_SELECTION
  body += F(",\"cell-assignments\",\"cell-lock\"");
#endif
#if defined(ARDUINO_ARCH_ESP32)
  body += F(",\"status-rgb\""); // Settings capability is distinct from a confirmed/enabled physical pixel.
#endif
  const uint16_t available = currentFeatures();
  bool separator = true;
  for (uint8_t bit = 0; bit < 16; ++bit) if (available & (1U << bit)) {
    if (separator) body += ',';
    body += '"'; body += BoardCore::Context::name(bit); body += '"'; separator = true;
  }
  body += F("]");
  if(available & BoardCore::Context::Leds) {
    body += F(",\"led_output\":");body += grid.outputCapabilitiesJson();
  }
  body += F(",\"modes\":["); separator = false;
  for (uint8_t code = 0; code < 4; ++code) if (BoardCore::Context::modes(available) & (1U << code)) {
    if (separator) body += ',';
    body += F("{\"id\":\""); body += BoardCore::Context::modeName(code);
    body += F("\",\"code\":"); body += code;
    body += F(",\"label\":\""); body += BoardCore::Context::modeLabel(code);
    body += F("\",\"kind\":\""); body += code == 2 ? F("game") : F("presentation");
    body += F("\"}"); separator = true;
  }
  body += F("]");
  if (PAKAR_TABLE_ENABLED) body += F(",\"table\":{\"columns\":10,\"rows\":8,\"pixels_per_cell\":6}");
  body += F("}");
  body += F(",\"pixels\":"); body += LedGrid::kPixels;
  body += F(",\"cells\":[");
  for (uint8_t cell = 1; cell <= LedGrid::kCells; ++cell) {
    if (cell != 1) body += ',';
    body += '\"'; body += cellName(grid.state(cell)); body += '\"';
  }
  char panelText[5];semanticPanel.view(millis(),panelText);
  body += F("],\"panel_presentation\":{\"requested_text\":\"");appendJsonEscaped(body,panelText);
  body += F("\",\"event_id\":");body+=semanticPanel.revision();body+=F(",\"hardware_confirmed\":false}}");
  return body;
}

#if defined(ARDUINO_ARCH_ESP32)
String buildCloudStatusJson() {
  // Diagnostics need the exact published frame, not a locally inferred color.
  // Keep routine cloud traffic unchanged; only this S3 diagnostic path allocates
  // the additional frame. Compact status retains peer/capability/driver receipts.
  const bool diagnostic=grid.diagnosticActive();
  String body=buildStatusJson(diagnostic);
  if(!diagnostic || !body.endsWith("}"))return body;
  const String frame=grid.pixelFrameJson();
  if(!frame.length() || frame[0]!='{' || !frame.endsWith("}"))return body;
  constexpr char fields[]=",\"pixel_preview_requested\":true,\"pixel_frame\":";
  const size_t bytes=body.length()+sizeof(fields)-1U+frame.length();
  // Failure leaves a valid ordinary status, without claiming a pixel preview.
  if(bytes>16384U || !body.reserve(bytes))return body;
  body.remove(body.length()-1U);
  body+=fields;body+=frame;body+='}';
  if(body.length()!=bytes)return String(); // Never publish truncated JSON.
  return body;
}
#endif

void sendStatusJson() {
  server.send(200, "application/json", buildStatusJson());
}

// A capture is assembled once on the loop owner. Its chunks never rebuild
// mutable cell state or reuse identityRevision() as a scene revision.
String buildUsbTableSnapshot() {
  // PCController sessionsnapshot's explicit projection/bounded-encode pattern:
  // keep live identity, controls and receipts, not unbounded diagnostic detail.
  // 10240 + 6104 worst-case escaped mappings/published frame = 16344 bytes.
  // Never truncate identity, names or pixels to fit the transport's 16384 cap.
  String status=buildStatusJson(true);
  if(!status.length() || status.length()>10240)return String();
  String body;if(!body.reserve(16384))return String();
  body=F("{\"status\":");body+=status;status=String();body+=F(",\"physical\":[");
  for(uint8_t i=0;i<LedGrid::kCells;++i){if(i)body+=',';body+=cellMapping.physical[i];}
  body+=F("],\"names\":[");
  for(uint8_t i=0;i<LedGrid::kCells;++i){if(i)body+=',';body+='\"';
    if(cellMapping.names[i][0])appendJsonEscaped(body,cellMapping.names[i]);else body+=i+1;
    body+='\"';}
  body+=F("],\"pixels\":");body+=grid.pixelFrameJson();body+='}';
  if(body.length()>16384)return String();
  return body;
}

void configureNtp() {
  NetworkPath::serviceDns(millis(), !ntpConfigured);
  if (!NetworkPath::ready() || ntpConfigured) return;
  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
  ntpConfigured = true;
}

bool applyStreamPacket(const uint8_t *packet, size_t length,uint64_t owner=0) {
  if(urbootBusy.get())return false;
  if (packet == nullptr || length < 10U || length > 1300U ||
      memcmp(packet, "PKAR", 4) != 0 || packet[4] != 0 || packet[5] != 0 ||
      (packet[9] & ~1U) != 0) return false;
  const uint16_t offset = packet[6] | static_cast<uint16_t>(packet[7]) << 8;
  const uint8_t count = packet[8];
  const uint8_t flags = packet[9];
  if(!count && !(flags&1U))return false;
  if (length != static_cast<size_t>(10U + count * 3U)) return false;
  if (!grid.stagePixels(offset, packet + 10, count,owner)) return false;
  // Publish this exact complete frame now; a later packet cannot alter a
  // ready frame while it waits for the wire worker. No I/O in network callbacks.
  if ((flags & 0x01U) != 0 && !grid.commitStaged(owner)) return false;
  return true;
}

BoardCore::CommandReceipts commandReceipts;
bool parseCellsAction(const String &,const String &,const String &,BoardCore::CellBatch &,CellState &,Effect &);
String commandReply(const String &message) {
  JsonDocument request, reply;
  reply["type"]="command-result";reply["board_id"]=pcBoardId;reply["boot_id"]=pcBootId;
  reply["accepted"]=false;
  if(message.length()>LoopMessages::kMaximumMessage || deserializeJson(request,message) || !request.is<JsonObject>()) {
    reply["error"]="invalid_command";
  } else {
    const JsonObjectConst object=request.as<JsonObjectConst>();
    const String id=PakarCommand::stringValue(object,"request_id");
    const String command=PakarCommand::stringValue(object,"command");
    reply["request_id"]=id;
    const bool supported=command=="cells"||command=="cell"||command=="effect"||
        command=="mode"||command=="brightness"||command=="led-output"||command=="pixel-diagnostic"||command=="panel-notice"||command=="shell-config"||command=="configure-shell"||command=="cell-assignments"||command=="cell-lock";
    if(PakarCommand::stringValue(object,"board_id")!=pcBoardId ||
       PakarCommand::stringValue(object,"boot_id")!=pcBootId || !webAccess.peerFenceReady()) {
      reply["error"]="peer_changed";
    } else if(!supported || !request["expires_at_ms"].is<uint64_t>()) {
      reply["error"]="unsupported_command";
    } else {
      FirmwarePlatform::Sha256 hash;hash.add(message.c_str(),message.length());
      const String digest=hash.hex();BoardCore::CommandReceipts::Entry *entry=nullptr;
      const auto admission=commandReceipts.reserve(id.c_str(),digest.c_str(),request["expires_at_ms"],commandClock(),entry);
      if(admission==BoardCore::CommandReceipts::Fresh) {
        String result;
        entry->accepted=!programmingLatched && !otaActive && !otaRebootAt && !avrUploadOwner &&
            !remoteFirmware.busy() && !networkApplyPending && !cloudApplyPending && handleCloudCommand(message,result);
        if(!entry->accepted)entry->failure=result==F("red_cells_locked")?BoardCore::CommandReceipts::RedCellsLocked:
            result==F("invalid_cell_assignments")?BoardCore::CommandReceipts::InvalidCellAssignments:
            result==F("unsupported_board_role")?BoardCore::CommandReceipts::UnsupportedBoardRole:BoardCore::CommandReceipts::NoFailure;
        if(entry->accepted && command=="cells") {
          BoardCore::CellBatch cells;CellState state;Effect effect;
          if(parseCellsAction(request["ids"].as<String>(),request["state"].as<String>(),request["effect"].as<String>(),cells,state,effect))entry->count=cells.count();
        }
      }
      if(admission==BoardCore::CommandReceipts::Fresh || admission==BoardCore::CommandReceipts::Cached) {
        reply["accepted"]=entry->accepted;reply["count"]=entry->count;
        reply["cached"]=admission==BoardCore::CommandReceipts::Cached;
        if(!entry->accepted){reply["error"]=BoardCore::CommandReceipts::failureCode(entry->failure);reply["message"]=BoardCore::CommandReceipts::failureMessage(entry->failure);}
      } else reply["error"]=admission==BoardCore::CommandReceipts::Conflict?"request_id_conflict":
          admission==BoardCore::CommandReceipts::Full?"command_capacity":
          admission==BoardCore::CommandReceipts::Expired?"command_expired":"invalid_request_id";
    }
  }
  String body;serializeJson(reply,body);
  if(reply["accepted"]==true) {
    body.remove(body.length()-1);body+=F(",\"status\":");
    String status=buildStatusJson();
    if(request["command"]=="pixel-diagnostic") {
      status.remove(status.length()-1);status+=F(",\"pixel_frame\":");
      status+=grid.pixelFrameJson();status+='}';
    }
    body+=status;body+='}';
  }
  return body;
}

void handleWebSocketMessage(uint32_t clientId, uint8_t opcode,
                           const uint8_t *payload, size_t length) {
  // Only an immutable ID crosses the async -> loop boundary. SDK lookup/use
  // must remain under its own client-list lock; client(id) returns no lease.
  if (!webAccess.socketAllowed(clientId)) { webSocket.close(clientId, 1008, "Sign in again"); return; }
  if(passiveCaptureActive()) {
    webSocket.text(clientId, "{\"accepted\":false,\"error\":\"passive_uart_capture_active\"}");return;
  }
  if (otaActive || otaRebootAt || urbootBusy.get()) {
    if (opcode==WS_TEXT && length<=1024U) {
      JsonDocument request;
      if(!deserializeJson(request,payload,length) && request.is<JsonObject>()) {
        if(request["type"]=="status") webSocket.text(clientId,buildStatusJson());
        else if(request["type"]=="ping") webSocket.text(clientId,"{\"type\":\"pong\"}");
        else webSocket.text(clientId,"{\"accepted\":false,\"error\":\"esp_update_in_progress\"}");
      }
    }
    return;
  }
  if (opcode == WS_BINARY) { if(!remoteFirmware.busy())applyStreamPacket(payload, length, (1ULL << 63) | clientId); return; }
  if (opcode != WS_TEXT || length > LoopMessages::kMaximumMessage) return;
  String message;
  message.reserve(length);
  message.concat(reinterpret_cast<const char *>(payload), length);
  JsonDocument parsed;
  if(deserializeJson(parsed,message) || !parsed.is<JsonObject>()) return;
  if(parsed["type"]=="peer-introduction") {
    String report;serializeJson(parsed["report"],report);
    // Connection address is retained by WebAccess at the actual HTTP upgrade,
    // never accepted from a caller-supplied JSON field.
    const bool accepted=recordClientReport(report,webAccess.socketAddress(clientId),webAccess.socketAgent(clientId),webAccess.socketVersion(clientId));
    webSocket.text(clientId,accepted?"{\"type\":\"peer-introduction\",\"accepted\":true}":"{\"type\":\"peer-introduction\",\"accepted\":false,\"error\":\"invalid_report_or_client_limit\"}");
    return;
  }
  if(length>LoopMessages::kMaximumMessage)return;
  if(parsed["type"]=="command") {
    webSocket.text(clientId,commandReply(message));
    // Other connected clients see the changed state immediately too.
    webSocket.textAll(buildStatusJson());
    return;
  }
  if(parsed["type"]=="telemetry") {webSocket.text(clientId, buildTelemetryJson());return;}
  if(parsed["command"].as<String>().startsWith("firmware-") || parsed["command"]=="telemetry") {
    String result;const bool accepted=remoteCommand(parsed,result);
    JsonDocument reply;reply["type"]="firmware-result";reply["accepted"]=accepted;
    JsonDocument details;
    if(accepted && !deserializeJson(details,result)) reply["job"]=details;
    else reply["error"]=result;
    String body;serializeJson(reply,body);webSocket.text(clientId, body);return;
  }
  if (message.indexOf(F("\"type\":\"status\"")) >= 0) {
    String status = buildStatusJson();
    webSocket.text(clientId, status);
  } else if (message.indexOf(F("\"type\":\"ping\"")) >= 0) {
    webSocket.text(clientId, "{\"type\":\"pong\"}");
  } else {
    JsonDocument request;
    if (deserializeJson(request, message)) return;
    const String command = request["command"].as<String>();
    if (command != F("pc-contract") && command != F("led-output")) return;
    String result;
    const bool accepted = handleCloudCommand(message, result);
    result.replace("\\", "\\\\"); result.replace("\"", "\\\"");
    webSocket.text(clientId, String(F("{\"accepted\":")) + (accepted ? "true" : "false") +
                 F(",\"message\":\"") + result + F("\"}"));
  }
}

String localUpdateJson() {
  const auto progress=otaProgress.snapshot();
  String body=F("{\"target\":\"esp\",\"phase\":\"");body+=PakarUpdate::name(progress.phase);
  body+=F("\",\"received_bytes\":");body+=progress.received;
  body+=F(",\"operation_id\":");body+=progress.operation;
  body+=F(",\"expected_bytes\":");body+=progress.expected;
  body+=F(",\"progress_known\":");body+=progress.known()?F("true"):F("false");
  body+=F(",\"progress_percent\":");
  if(progress.known())body+=progress.percent();else body+=F("null");
  body+=F(",\"completion_verified\":false}"); // Reboot/readback belongs to the client.
  return body;
}
struct WsAssembly { size_t used; size_t length; uint32_t epoch; uint8_t opcode; uint8_t data[1]; };
void onWebSocketEvent(AsyncWebSocket *, AsyncWebSocketClient *client,
                      AwsEventType type, void *argument, uint8_t *payload, size_t length) {
  const auto closeSocket = [&](uint16_t code, const char *reason) {
    // A close handshake need not disconnect immediately. Revoke queued-command
    // admission before marking the callback-owned client as closing.
    webAccess.forgetSocket(client->id()); client->close(code, reason);
  };
  const uint32_t ingressEpoch=server.ingressEpoch();
  if (type == WS_EVT_DISCONNECT || type == WS_EVT_ERROR) {
    webAccess.forgetSocket(client->id());
    const uint8_t unused = 0;
    // A full queue may drop this advisory release; the stage-owner lease still
    // expires, and immediately revoked auth rejects already queued data.
    loopMessages.push(client->id(), 0xfe, &unused, 0,ingressEpoch);
    if (client->_tempObject) { free(client->_tempObject); client->_tempObject = nullptr; }
    return;
  }
  if (type == WS_EVT_CONNECT) {
    if (webSocket.count() > 8) { closeSocket(1013, "Client limit reached"); return; }
    if (!webAccess.bindSocket(client->id(), static_cast<AsyncWebServerRequest *>(argument), client->remoteIP().toString())) {
      closeSocket(1008, "Sign in required"); return;
    }
    const uint8_t statusRequest[]="{\"type\":\"status\"}";
    if(!loopMessages.push(client->id(),WS_TEXT,statusRequest,sizeof(statusRequest)-1,ingressEpoch))closeSocket(1013,"Command queue full");
    return;
  }
  if (type != WS_EVT_DATA) return;
  if (!webAccess.socketAllowed(client->id())) { closeSocket(1008, "Sign in again"); return; }
  AwsFrameInfo *info = static_cast<AwsFrameInfo *>(argument);
  if (!info || !info->final || info->num != 0 || info->len > LoopMessages::kMaximumMessage ||
      info->index + length > info->len) { closeSocket(1009, "Message exceeds stream bounds"); return; }
  if (info->index == 0 && info->len == length) {
    if(!loopMessages.push(client->id(),info->opcode,payload,length,ingressEpoch))closeSocket(1013,"Command queue full");return;
  }
  if (info->index == 0) {
    if (client->_tempObject) free(client->_tempObject);
    auto *assembly = static_cast<WsAssembly *>(malloc(sizeof(WsAssembly) + info->len));
    client->_tempObject = assembly;
    if (!assembly) { closeSocket(1013, "Message buffer unavailable"); return; }
    assembly->used = 0; assembly->length = info->len; assembly->opcode = info->opcode;
    assembly->epoch=ingressEpoch;
  }
  auto *assembly = static_cast<WsAssembly *>(client->_tempObject);
  if (!assembly || assembly->epoch!=ingressEpoch || assembly->used != info->index || assembly->length != info->len ||
      assembly->used + length > assembly->length) { closeSocket(1002, "Invalid message sequence"); return; }
  memcpy(assembly->data + assembly->used, payload, length); assembly->used += length;
  if (assembly->used == assembly->length) {
    const bool queued=loopMessages.push(client->id(),assembly->opcode,assembly->data,assembly->length,assembly->epoch);
    free(assembly); client->_tempObject = nullptr;
    if(!queued)closeSocket(1013,"Command queue full");
  }
}

GameMode parseMode(const String &value) {
  if (value == F("snake")) return kModeSnake;
  if (value == F("first-wins")) return kModeFirstWins;
  if (value == F("cell")) return kModeCellSelect;
  return kModeCellSelect;
}

CellState parseCellState(const String &value) {
  if (value == F("red")) return kCellRed;
  if (value == F("off")) return kCellOff;
  return kCellWhite;
}

Effect parseEffect(const String &value) {
  if (value == F("pulse")) return kEffectPulse;
  if (value == F("immediate")) return kEffectImmediate;
  return kEffectFade;
}

bool parseCellsAction(const String &ids,const String &stateName,const String &effectName,
                      BoardCore::CellBatch &selection,CellState &state,Effect &effect) {
  if((stateName!=F("white")&&stateName!=F("red")&&stateName!=F("off"))||
      (!effectName.isEmpty()&&effectName!=F("immediate")&&effectName!=F("fade")&&effectName!=F("pulse"))||
      !BoardCore::CellBatch::parse(ids.c_str(),ids.length(),selection))return false;
  state=parseCellState(stateName);effect=effectName.isEmpty()?kEffectImmediate:parseEffect(effectName);return true;
}

LedGrid::Pattern parsePattern(const String &value) {
  if (value == F("rainbow")) return LedGrid::kPatternRainbow;
  if (value == F("fire")) return LedGrid::kPatternFire;
  if (value == F("cell-chase")) return LedGrid::kPatternCellChase;
  return LedGrid::kPatternNone;
}

bool parseAudioCue(const String &value, uint8_t &cue) {
  return BoardCore::AudioCues::parse(value.c_str(), cue) &&
         cue < BoardCore::AudioCues::supportedCount(avrOnline, (avrCapabilities & kCapBuzzer) != 0, avrAudioCueCount);
}

bool submitDisplayText(const String &text, long speed, long hold,
                       const String &repeat, bool forceScroll, long interval,
                       String &result) {
  if ((!repeat.isEmpty() && repeat != F("once") && repeat != F("loop") &&
       repeat != F("interval")) || text.length() > 40 || speed < 80 ||
      speed > 5000 || hold < 0 || hold > 60000 || interval < 1 || interval > 255) {
    result = F("invalid display text scheduling values");
    return false;
  }
  for (size_t i = 0; i < text.length(); ++i) {
    if (static_cast<uint8_t>(text[i]) < 0x20U || static_cast<uint8_t>(text[i]) > 0x7EU) {
      result = F("display text must be printable ASCII");
      return false;
    }
  }
  uint8_t payload[48] = {};
  payload[0] = 5;
  payload[1] = static_cast<uint8_t>(speed);
  payload[2] = static_cast<uint8_t>(speed >> 8);
  payload[3] = static_cast<uint8_t>(text.length());
  const uint8_t repeatMode = repeat == F("loop") ? 1 : repeat == F("interval") ? 2 : 0;
  payload[4] = static_cast<uint8_t>(repeatMode | (forceScroll ? 0x80U : 0));
  payload[5] = static_cast<uint8_t>(hold);
  payload[6] = static_cast<uint8_t>(hold >> 8);
  payload[7] = static_cast<uint8_t>(interval);
  memcpy(payload + 8, text.c_str(), text.length());
  if(!sendToAvr(kDisplayText, sequence++, payload, static_cast<uint8_t>(8 + text.length()))) {
    result=F("display transport unavailable; not submitted");return false;
  }
  result = F("display text submitted");
  return true;
}

#include "src/CloudManagement.h"
#include "src/EthernetManagement.h"
#include "src/LcdManagement.h"

// Form and cloud surfaces share typed validation and atomic persistence. Do
// not coerce malformed booleans or numeric prefixes into destructive settings.
bool configurationForm(JsonDocument &document, bool cloud, String &error) {
  auto patch = document.to<JsonObject>();
  const char *cloudText[] = {"mqtt_host","mqtt_user","mqtt_password","mqtt_base_topic",
      "websocket_host","websocket_path","websocket_token","ntfy_host","ntfy_topic","ntfy_token"};
  const char *networkText[] = {"ssid","password","address","gateway","subnet","dns1","dns2"};
  const char *cloudBool[] = {"mqtt_enabled","mqtt_tls","websocket_enabled","websocket_tls","ntfy_enabled"};
  const char *networkBool[] = {"station_enabled","ap_enabled","dhcp"};
  const char *numbers[] = {"mqtt_port","websocket_port","telemetry_seconds"};
  for (unsigned i=0; i<(cloud?10U:7U); ++i) {
    const char *key = cloud ? cloudText[i] : networkText[i];
    if (server.hasArg(key)) patch[key] = server.arg(key);
  }
  for (unsigned i=0; i<(cloud?5U:3U); ++i) {
    const char *key = cloud ? cloudBool[i] : networkBool[i];
    if (!server.hasArg(key)) continue;
    const String value = server.arg(key);
    if (value!="true" && value!="false" && value!="1" && value!="0") {
      error=F("Invalid configuration boolean");return false;
    }
    patch[key] = value=="true" || value=="1";
  }
  if (cloud) for (const char *key:numbers) {
    if (!server.hasArg(key)) continue;
    const String value=server.arg(key);uint32_t number=0;
    if (!value.length() || value.length()>5) {error=F("Invalid configuration number");return false;}
    for (size_t i=0;i<value.length();++i) {
      if(value[i]<'0'||value[i]>'9') {error=F("Invalid configuration number");return false;}
      number=number*10U+value[i]-'0';
    }
    patch[key]=number;
  }
  return !document.overflowed();
}

bool handleCloudCommand(const String &message, String &result) {
  if(urbootBusy.get()) {result=F("Urboot owns board maintenance; no command dispatched");return false;}
  if(passiveCaptureActive()) {result=F("Bounded passive UART capture owns diagnostics");return false;}
  JsonDocument commandDocument;
  if(message.length()>LoopMessages::kMaximumMessage || deserializeJson(commandDocument,message) || !commandDocument.is<JsonObject>()) {
    result=F("Invalid command JSON");return false;
  }
  const JsonObjectConst commandRequest=commandDocument.as<JsonObjectConst>();
  const String command=PakarCommand::stringValue(commandRequest,"command");
  if(command=="shell-config"){result=shellStatusJson();return true;}
  if(command=="configure-shell")return updateShellConfiguration(commandRequest["settings"].as<JsonObjectConst>(),result);
  if (command=="cloud-config" || command=="network-config" || command=="ethernet-config" ||
      command=="configure-cloud" || command=="configure-network" || command=="configure-ethernet" || command=="device-identity") {
    JsonDocument request;
    if(message.length()>1024 || deserializeJson(request,message) || !request.is<JsonObject>()) {
      result=F("Invalid management JSON");return false;
    }
    if(command=="cloud-config") {result=cloudConfigurationJson();return result.length()>0;}
    if(command=="network-config") {result=networkConfigurationJson();return result.length()>0;}
    if(command=="ethernet-config") {result=ethernetConfigurationJson();return result.length()>0;}
    if(command=="device-identity") {
      if(!request["hostname"].is<const char*>() ||
          (!request["display_name"].isNull()&&!request["display_name"].is<const char*>()) ||
          remoteFirmware.busy() || otaActive || otaRebootAt || avrUploadOwner ||
          programmingLatched || networkApplyPending || cloudApplyPending) {
        result=F("Invalid identity or device busy");return false;
      }
      const bool accepted=scheduleDeviceIdentity(request["hostname"].as<String>(),
          request["display_name"].isNull()?String(deviceIdentity.displayName()):request["display_name"].as<String>());
      result=accepted?F("Identity saved; applying after response"):F("Invalid identity or save failed");
      return accepted;
    }
    if(!request["settings"].is<JsonObject>()) {result=F("settings object required");return false;}
    if(command=="configure-ethernet")return updateEthernetConfiguration(request["settings"].as<JsonObjectConst>(),result);
    return command=="configure-cloud" ?
        updateCloudConfiguration(request["settings"].as<JsonObjectConst>(),result,true) :
        updateNetworkConfiguration(request["settings"].as<JsonObjectConst>(),result);
  }
  using namespace BoardCore::Context;
  uint16_t required = 0;
  if (command == F("cell") || command == F("reset") || command == F("snake-direction") || command == F("mode")) required = Table;
  if(command==F("cells"))required=BoardCore::Context::CellBatch;
  if((command=="cell"||command=="cells"||command=="cell-assignments"||command=="cell-lock")&&!PAKAR_TABLE_SELECTION){result=F("unsupported_board_role");return false;}
  if (command == F("display")) required = !commandRequest["text"].isUnbound() ? Text : Display;
  if (command == F("panel-notice")) required = Display;
  if (command == F("audio") || command == F("buzzer")) required = Buzzer;
  if (command == F("outputs")) required = Outputs;
  if (required && !(currentFeatures() & required)) { result = F("Capability not acknowledged by this board"); return false; }
  if(command=="cell-assignments") {
    if(!applyCellAssignments(commandRequest["assignments"],result))return false;
    result=F("Cell assignments applied atomically");return true;
  }
  if(command=="cell-lock") {
    if(!commandRequest["enabled"].is<bool>()){result=F("enabled must be boolean");return false;}
    grid.setRedLock(commandRequest["enabled"].as<bool>());result=F("Red-cell lock updated");return true;
  }
  if(command==F("panel-notice")) {
    const String text=PakarCommand::stringValue(commandRequest,"text");
    const String blink=commandRequest["blink"].isNull()?String("steady"):PakarCommand::stringValue(commandRequest,"blink");
    const auto duration=commandRequest["duration_ms"];
    if(!text.length()||text.length()>32 || (!duration.isNull()&&(!duration.is<uint16_t>() || duration.as<uint16_t>()<100 || duration.as<uint16_t>()>10000)) ||
      (blink!="steady"&&blink!="slow"&&blink!="double-fast")) {result=F("Invalid panel notice");return false;}
    for(size_t i=0;i<text.length();++i)if(text[i]<32||text[i]>126){result=F("Printable ASCII required");return false;}
    semanticPanel.post(PakarPanel::State,text.c_str(),0,millis(),60,
      blink=="slow"?PakarPanel::Slow:blink=="double-fast"?PakarPanel::DoubleFast:PakarPanel::Steady,
      duration.isNull()?2000:duration.as<uint16_t>());
    result=F("panel notice queued; segment observation remains separate");return true;
  }
  if (command == F("mode")) {
    const String value = PakarCommand::stringValue(commandRequest, "value");
    bool allowed = false;
    for (uint8_t code = 0; code < 4; ++code)
      if (value == BoardCore::Context::modeName(code) && (modes(currentFeatures()) & (1U << code))) allowed = true;
    if (!allowed) { result = F("Mode not advertised by this board"); return false; }
  }
  if(command=="telemetry") {
    cloudServices.requestTelemetry();result=F("Complete checksummed telemetry snapshot queued");return true;
  }
  if(command.startsWith("firmware-") || command=="telemetry") {
    JsonDocument d;
    if(message.length()>1024 || deserializeJson(d,message) || !d.is<JsonObject>()) {
      result=F("Invalid firmware/telemetry JSON");return false;
    }
    return remoteCommand(d,result);
  }
  if (remoteFirmware.busy() && command!=F("firmware-cancel") &&
      command!=F("firmware-status") && command!=F("telemetry") && command!=F("status")) {
    result=F("Remote firmware operation owns board maintenance");return false;
  }
  if (command == F("pc-contract")) {
    JsonDocument request;
    if (message.length() > 512 || deserializeJson(request, message) ||
        !request.is<JsonObject>() || !request["opcode"].is<int>() ||
        (!request["payload_hex"].isNull() &&
         !request["payload_hex"].is<const char *>())) {
      result = F("bad PCController contract request"); return false;
    }
    const int op = request["opcode"].as<int>();
    uint8_t p[48], n = 0;
    if (op < 0 || op > 127 || !BoardCore::PcContractTransport::decodeHex(
        request["payload_hex"] | "", p, n)) {
      result = F("bad PCController opcode or payload"); return false;
    }
    if (otaActive || otaRebootAt || avrUploadOwner ||
        (op != ControllerProtocol::AddressableLed && (!avrOnline || !avrPcSupported))) {
      result = F("contract endpoint unavailable"); return false;
    }
    const uint32_t id = pcTransport.enqueue(op, p, n, millis());
    if (!id) { result = F("contract queue full"); return false; }
    result = pcReceiptJson(*pcTransport.get(id)); return true;
  }
  if(command=="pixel-diagnostic") {
    const auto index=commandRequest["index"],count=commandRequest["count"],rgb=commandRequest["rgb"],off=commandRequest["off"];
    if(!(currentFeatures()&Leds) || !index.is<uint16_t>() || !count.is<uint8_t>() ||
       !rgb.is<uint32_t>() || !off.is<bool>() || programmingLatched || otaActive || otaRebootAt ||
       !grid.diagnostic(index.as<uint16_t>(),count.as<uint8_t>(),rgb.as<uint32_t>(),off.as<bool>())) {
      result=F("Invalid or busy pixel diagnostic");return false;
    }
    // No AVR command, EEPROM write, animation or intermediate white scene.
    winner=0;result=F("Complete diagnostic frame published");return true;
  }
  if (command == F("status") ||
      PakarCommand::stringValue(commandRequest, "type") == F("status")) {
    result = F("status will be published");
    return true;
  }
  if (command == F("mode")) {
    const String value = PakarCommand::stringValue(commandRequest, "value");
    if (value != F("cell") &&
        value != F("first-wins") && value != F("snake")) {
      result = F("mode must be cell, first-wins, or snake");
      return false;
    }
    if(!BoardCore::GameRole::accepts(parseMode(value))){result=F("Mode is selected at firmware build time");return false;}
    if(!grid.resetWhite()){result=F("red_cells_locked");return false;}
    mode = static_cast<GameMode>(BoardCore::GameRole::mode);
    winner = 0;
    const uint8_t payload[] = {static_cast<uint8_t>(mode)};
    sendToAvr(kSetMode, sequence++, payload, sizeof(payload));
    result = F("mode updated");
    return true;
  }
  if(command==F("cells")){
    JsonDocument document;BoardCore::CellBatch selection;CellState state;Effect effect;
    if(deserializeJson(document,message)||!document.is<JsonObject>()||
        !document["ids"].is<const char*>()||!document["state"].is<const char*>()||
        (!document["effect"].isNull()&&!document["effect"].is<const char*>())||
        !parseCellsAction(document["ids"].as<String>(),document["state"].as<String>(),
                          document["effect"].as<String>(),selection,state,effect)){
      result=F("cells needs ids 1..80, state white/red/off and effect immediate/fade/pulse");return false;
    }
    if(!grid.setCells(selection,state,effect)){result=F("red_cells_locked");return false;}
    selectedCell=selection.first();selectedState=state;result=F("cell set accepted atomically");return true;
  }
  if (command == F("cell")) {
    const long id = PakarCommand::integerValue(commandRequest, "id", 0);
    const String stateName = PakarCommand::stringValue(commandRequest, "state");
    const String effectName = PakarCommand::stringValue(commandRequest, "effect");
    if (id < 1 || id > LedGrid::kCells ||
        (stateName != F("white") && stateName != F("red") &&
         stateName != F("off"))) {
      result = F("cell needs id 1..80 and state white, red, or off");
      return false;
    }
    const Effect effect = parseEffect(effectName);
    if(!grid.setCell(static_cast<uint8_t>(id),parseCellState(stateName),effect)){result=F("red_cells_locked");return false;}
    selectedCell = static_cast<uint8_t>(id);
    selectedState = parseCellState(stateName);
    const uint8_t payload[] = {selectedCell, selectedState,
                               static_cast<uint8_t>(effect)};
    sendToAvr(kCellAction, sequence++, payload, sizeof(payload));
    result = F("cell updated");
    return true;
  }
  if (command == F("brightness")) {
    const long value = PakarCommand::integerValue(commandRequest, "value", 0);
    if (value < 1 || value > 255) {
      result = F("brightness must be 1..255");
      return false;
    }
    grid.setBrightness(static_cast<uint8_t>(value));
    saveLedPreferences();
    result = F("brightness updated");
    return true;
  }
  if(command==F("led-output")){
    JsonDocument request;if(deserializeJson(request,message)){
      result=F("LED output requires valid JSON");return false;}
    const bool enabledGiven=!request["enabled"].isUnbound(),modeGiven=!request["mode"].isUnbound();
    const String outputMode=request["mode"].is<const char *>()?request["mode"].as<String>():String();
    if((!enabledGiven&&!modeGiven)||(enabledGiven&&!request["enabled"].is<bool>())||
       (modeGiven&&outputMode!="static"&&outputMode!="effects")){
      result=F("LED output requires enabled boolean and/or mode static or effects");return false;}
    // Validate the complete command before changing either independent field.
    if(modeGiven)grid.setStaticMode(outputMode=="static");
    if(enabledGiven)grid.setOutputEnabled(request["enabled"].as<bool>());
    result=grid.outputJson();return true;
  }
  if (command == F("effect")) {
    const String value = PakarCommand::stringValue(commandRequest, "value");
    const LedGrid::Pattern pattern = parsePattern(value);
    if (pattern == LedGrid::kPatternNone && value != F("none")) {
      result = F("effect must be none, rainbow, fire, or cell-chase");
      return false;
    }
    if(!grid.setPattern(pattern)){result=F("red_cells_locked");return false;}
    saveLedPreferences();
    result = F("effect updated");
    return true;
  }
  if (command == F("outputs")) {
    const long mask = PakarCommand::integerValue(commandRequest, "mask", -1);
    if (mask < 0 || mask > 255) {
      result = F("output mask must be 0..255");
      return false;
    }
    const uint8_t payload[] = {static_cast<uint8_t>(mask)};
    sendToAvr(kOutputMask, sequence++, payload, sizeof(payload));
    result = F("outputs updated");
    return true;
  }
  if (command == F("display")) {
    if (PakarCommand::booleanValue(commandRequest, "release")) {
      const uint8_t payload[] = {4, 0, 0, 0};
      sendToAvr(kDisplayText, sequence++, payload, sizeof(payload));
      result = F("display released");
      return true;
    }
    if (!commandRequest["text"].isUnbound()) {
      return submitDisplayText(PakarCommand::stringValue(commandRequest, "text"),
          PakarCommand::integerValue(commandRequest, "speed_ms", 260), PakarCommand::integerValue(commandRequest, "hold_ms", 1500),
          PakarCommand::stringValue(commandRequest, "repeat"), PakarCommand::booleanValue(commandRequest, "force_scroll"),
          PakarCommand::integerValue(commandRequest, "interval_seconds", 5), result);
    }
    const long value = PakarCommand::integerValue(commandRequest, "value", 10000);
    if (value < -999 || value > 9999) {
      result = F("display value must be -999..9999");
      return false;
    }
    const int16_t signedValue = static_cast<int16_t>(value);
    const uint8_t payload[] = {static_cast<uint8_t>(signedValue),
                               static_cast<uint8_t>(signedValue >> 8)};
    sendToAvr(kDisplayText, sequence++, payload, sizeof(payload));
    result = F("display updated");
    return true;
  }
  if (command == F("buzzer")) {
    const long frequency = PakarCommand::integerValue(commandRequest, "frequency", 0);
    const long duration = PakarCommand::integerValue(commandRequest, "duration_ms", 0);
    if (frequency < 20 || frequency > 12000 ||
        duration < 1 || duration > 5000) {
      result = F("buzzer frequency or duration is invalid");
      return false;
    }
    const uint8_t payload[] = {
        static_cast<uint8_t>(frequency),
        static_cast<uint8_t>(frequency >> 8),
        static_cast<uint8_t>(duration),
        static_cast<uint8_t>(duration >> 8),
    };
    sendToAvr(kBuzzer, sequence++, payload, sizeof(payload));
    result = F("buzzer started");
    return true;
  }
  if (command == F("audio")) {
    uint8_t cue = 0;
    if (!parseAudioCue(PakarCommand::stringValue(commandRequest, "cue"), cue)) {
      result = F("unknown audio cue");
      return false;
    }
    if (!sendToAvr(kAudioCue, sequence++, &cue, 1)) {
      result = F("AVR link unavailable"); return false;
    }
    result = F("audio cue queued");
    return true;
  }
  if (command == F("snake-direction")) {
    const String value = PakarCommand::stringValue(commandRequest, "value");
    const int8_t direction =
        value == F("up") ? 0 : value == F("right") ? 1 :
        value == F("down") ? 2 : value == F("left") ? 3 : -1;
    if (mode != kModeSnake || direction < 0 ||
        !setSnakeDirection(static_cast<uint8_t>(direction))) {
      result = F("invalid or opposite snake direction");
      return false;
    }
    result = F("snake direction updated");
    return true;
  }
  if (command == F("reset")) {
    if(!grid.resetWhite()){result=F("red_cells_locked");return false;}
    winner = 0;
    sendToAvr(kGameReset, sequence++);
    result = F("game reset");
    return true;
  }
  result = F("unknown command");
  return false;
}

void onAvrFrame(const BoardCore::Protocol::Frame &frame, void *) {
  if(avrRecoveryHelloPending) {
    if(frame.opcode!=kHelloResponse || frame.sequence!=avrRecoveryHelloSequence)return;
  }
  // A random/unsupported CRC-valid packet cannot promote an unidentified AVR
  // or keep a stale session alive. Reconnect must start with an actual HELLO.
  if (!BoardCore::validAvrObservation(frame, avrOnline && avrVersionKnown)) return;
  avrRecoveryHelloPending=false;
  if (frame.opcode == kEspPanel && avrOnline && (avrCapabilities & kCapEspPanel)) {
    espFrontPanel.handle(frame, millis(), espPanelSettings, link); return;
  }
  avrOnline = true;
  lastAvrFrameAt = millis();
  if(frame.opcode==kTelemetryResponse) {
    if(frame.payloadLength==sizeof(avrTelemetry) && frame.payload[0]==0) {
      memcpy(avrTelemetry,frame.payload,sizeof(avrTelemetry));
      avrTelemetryKnown=true;avrTelemetryAt=millis();
    }
    return;
  }
  if (frame.opcode == kResetTelemetryResponse) {
    avrResetCause = frame.payload[0];
    memcpy(&avrBootCount, frame.payload + 1, sizeof(avrBootCount));
    avrBootCountPersisted = frame.payload[5] != 0;
    avrResetTelemetryKnown = true;
    avrResetTelemetryAt = millis();
    return;
  }
  if (frame.opcode == kPcContractResponse) {
    auto *receipt = pcTransport.accept(frame, millis());
    if (receipt) {
      if (receipt->usb) {
        hostLink.send(kPcContractResponse, receipt->hostSequence, frame.payload, frame.payloadLength);
        pcTransport.releaseUsb(*receipt);
      } else publishPcEvent(pcReceiptJson(*receipt));
    } else if (frame.sequence == 0 || frame.sequence == 0xFE) {
      String event = F("{\"type\":\"pc-contract-event\",\"board_id\":\"");
      event += pcBoardId; event += F("\",\"boot_id\":\""); event += pcBootId;
      event += F("\",\"avr_session\":"); event += avrSession;
      event += F(",\"event_id\":"); event += ++mirrorSequence;
      event += F(",\"response_opcode\":");
      event += frame.payloadLength ? frame.payload[0] : 0;
      event += F(",\"payload_hex\":\"");
      constexpr char digits[] = "0123456789abcdef";
      for (uint8_t i = 1; i < frame.payloadLength; ++i) {
        event += digits[frame.payload[i] >> 4]; event += digits[frame.payload[i] & 15];
      }
      event += F("\"}"); publishPcEvent(event,
        frame.payloadLength==6 && frame.payload[0]==ControllerProtocol::SegmentChanged);
      hostLink.send(kPcContractResponse, frame.sequence, frame.payload, frame.payloadLength);
    }
    if (frame.payloadLength == 6 && frame.payload[0] == ControllerProtocol::SegmentChanged) {
      memcpy(tm1637Segments, frame.payload + 1, 4);
      tm1637Brightness = frame.payload[5] & 7U;
      tm1637Ack = (frame.payload[5] & 0xC0U) == 0xC0U;
      avrDisplayStatusKnown = true; avrDisplayStatusAt = millis();
    }
    return;
  }
  if ((frame.opcode == kAck || frame.opcode == kErrorResponse) &&
      frame.sequence == awaitedSequence && frame.payloadLength >= 2 &&
      frame.payload[0] == awaitedOpcode) {
    awaitedReplySeen = true;
    awaitedReplyOk = frame.opcode == kAck && frame.payload[1] == kNoError;
  }
  if (frame.opcode == kHelloResponse) {
    avrTelemetryKnown = false;
    avrResetTelemetryKnown = false;
    avrVersionKnown = frame.payloadLength >= 9 &&
                      frame.payload[0] == 0 &&
                      frame.payload[1] == kRoleAvr;
    if (avrVersionKnown) memcpy(avrVersion, frame.payload + 6, sizeof(avrVersion));
    if(!avrVersionKnown || !BoardCore::PeerCompatibility::boardAllowed(avrVersionText().c_str())) {
      avrOnline=false;avrCapabilities=0;avrPcSupported=false;avrAudioCueCount=0;return;
    }
    avrCapabilities = avrVersionKnown ? frame.payload[2] | static_cast<uint16_t>(frame.payload[3]) << 8 : 0;
    avrRoleKnown=frame.payloadLength>=16;avrGameRole=avrRoleKnown?frame.payload[15]:0;
    avrRoleCompatible=!avrRoleKnown||BoardCore::GameRole::accepts(avrGameRole);
    avrAudioCueCount = avrVersionKnown && frame.payloadLength >= 13 ? frame.payload[12] : 0;
    avrToneTimestamps = avrVersionKnown && frame.payloadLength >= 14 && frame.payload[13] == 1;
    avrMaintenancePanel = avrVersionKnown && frame.payloadLength >= 15 && frame.payload[14] == 1;
    ++avrSession; avrPanelKnown = false;
    // A rebooted AVR's volatile defaults cannot erase the ESP-owned scene.
    // Synchronize only the paired mode; do not replay host commands or pixels.
    avrSceneSync.hello(); gameStateInitialized = false;
    avrDisplayStatusKnown = false; tm1637Ack = false;
    avrPcSupported = avrVersionKnown && frame.payloadLength >= 12 && frame.payload[9] == 0x50;
    avrPcFeatures = avrPcSupported ? frame.payload[10] | static_cast<uint16_t>(frame.payload[11]) << 8 : 0;
    sendToAvr(kGetStatus, sequence++);
  } else if (frame.opcode == kGameState && frame.payloadLength >= 4) {
    const bool gameRoleMatches=!avrRoleKnown||avrRoleCompatible;
    const uint8_t previousWinner = winner;
    const uint8_t previousInputs = activeInputs;
    const bool previousInput9 = activeInput9;
    if(gameRoleMatches) {
    const auto sceneSync = avrSceneSync.observe(frame.payload[0], static_cast<uint8_t>(mode));
    if (sceneSync.baseline) {
      gameStateInitialized = false;
      if (sceneSync.sendMode) {
        const uint8_t desiredMode[] = {static_cast<uint8_t>(mode)};
        sendToAvr(kSetMode, sequence++, desiredMode, sizeof(desiredMode));
      }
    } else if (BoardCore::GameRole::accepts(frame.payload[0])) {
      mode = static_cast<GameMode>(frame.payload[0]);
      // A boot/default status is not a new first-wins input.
      if (mode == kModeFirstWins && frame.payload[1]) winner = frame.payload[1];
    }
    }
    if (frame.payloadLength >= 7) {
      activeInputs = frame.payload[4];
      activeInput9 = frame.payload[5] != 0;
      activeOutputs = frame.payload[6];
      if (mode == kModeSnake) {
        const uint8_t pressed =
            static_cast<uint8_t>(activeInputs & ~previousInputs);
        for (uint8_t direction = 0; direction < 4; ++direction) {
          if ((pressed & _BV(direction)) != 0) {
            setSnakeDirection(direction);
            break;
          }
        }
        if (activeInput9 && !previousInput9) startSnake();
      }
    }
    if (frame.payloadLength >= 9) {
      avrUartErrors = static_cast<uint16_t>(frame.payload[7]) |
                      static_cast<uint16_t>(frame.payload[8]) << 8;
    }
    if (frame.payloadLength >= 10) {
      tm1637Ack = frame.payload[9] != 0;
      avrDisplayStatusKnown = true; avrDisplayStatusAt = millis();
    }
    if (frame.payloadLength >= 13) {
      const uint8_t cellRevision = frame.payload[10];
      const Effect actionEffect = static_cast<Effect>(frame.payload[11]);
      const uint8_t currentResetRevision = frame.payload[12];
      if (gameStateInitialized && gameRoleMatches) {
        if (currentResetRevision != lastResetRevision) {
          winner = frame.payload[1];
          if (mode == kModeSnake) startSnake(); else grid.resetWhite();
        }
        if (PAKAR_TABLE_SELECTION && cellRevision != lastCellActionRevision &&
            actionEffect <= kEffectPulse && frame.payload[2] >= 1 &&
            frame.payload[2] <= LedGrid::kCells && frame.payload[3] <= kCellOff) {
          if(grid.setCell(frame.payload[2],static_cast<CellState>(frame.payload[3]),actionEffect)) {
            selectedCell=frame.payload[2];selectedState=frame.payload[3];
          }
        }
      }
      lastCellActionRevision = cellRevision;
      lastResetRevision = currentResetRevision;
      gameStateInitialized = gameRoleMatches;
    }
    if (frame.payloadLength >= 14) {
      programmingLatched = frame.payload[13] != 0;
    }
    if (frame.payloadLength >= 20) {
      memcpy(tm1637Segments, frame.payload + 14, sizeof(tm1637Segments));
      tm1637Brightness = frame.payload[18];
      tm1637HostActive = frame.payload[19] != 0;
    }
    avrPanelKnown = frame.payloadLength >= 26;
    if (avrPanelKnown) {
      avrMenuPage = frame.payload[20]; avrMenuActive = frame.payload[21] != 0;
      avrRunning = frame.payload[22] != 0; avrCaptured = frame.payload[23] != 0;
      avrPanelMeta = frame.payload[24] | static_cast<uint16_t>(frame.payload[25]) << 8;
    }
#if PAKAR_FIRST_WINS_ENABLED
    if (gameRoleMatches && mode == kModeFirstWins && winner != 0 && winner != previousWinner) {
      grid.showWinner(winner);
    }
#endif
  } else if (PAKAR_TABLE_SELECTION && (!avrRoleKnown||avrRoleCompatible) && frame.opcode == kCellEvent && frame.payloadLength >= 3) {
    if(grid.setCell(frame.payload[0],static_cast<CellState>(frame.payload[1]),static_cast<Effect>(frame.payload[2]))) {
      selectedCell=frame.payload[0];selectedState=frame.payload[1];
    }
  }
}

void registerAsyncOta() {
  RegisterFirmwareTransferRoutes(&server, [](AsyncWebServerRequest *request) {
    return webAccess.allowed(request) && WebAccess::sameOrigin(request) && webAccess.peerAllowed(request);
  }, [](){return FirmwareTransferIdentity{runningImageSize(),runningImageMd5(),runningImageIntegrity()};
  }, []()->std::shared_ptr<void>{
    // Admission runs on the same loop owner as programming/configuration;
    // async uploads also reject server.processing() and this live read lease.
    if(urbootBusy.get()||remoteFirmware.busy()||otaActive||otaRebootAt||avrUploadOwner||
        programmingLatched||networkApplyPending||cloudApplyPending||artifactReaders>=4)return {};
    return std::make_shared<FirmwareReaderLease>();
  });
  auto info = [](AsyncWebServerRequest *request) {
    String body = F("{\"product\":\"PakarController\",\"product_id\":\"PakarController\",\"name\":\"");
    appendJsonEscaped(body, APP_NAME);
    body += F("\",\"application\":"); body += applicationIdentityJson();
    body += F(",\"model\":\""); body += FirmwarePlatform::model(); body += F(" + ATmega328P\",\"version\":\"");
    body += BoardCore::BuildInfo::kVersion;
    body += F("\",\"compatibility\":");body+=PakarPeer::policyJson();
    body += F(",\"sketch_size\":"); body += runningImageSize();
    body += F(",\"size\":"); body += runningImageSize();
    body += F(",\"free\":"); body += ESP.getFreeSketchSpace();
    body += F(",\"hash\":\""); body += runningImageMd5(); body += '"';
    body += F(",\"framework\":\"ESPAsyncWebServer\",\"framework_version\":\"3.12.1\"");
    body += F(",\"sketch_md5\":\""); body += runningImageMd5();
    const auto progress=otaProgress.snapshot();
    body += F("\",\"phase\":\""); body += PakarUpdate::name(progress.phase);
    body += F("\",\"uploaded_bytes\":"); body += progress.received;
    body += F(",\"ota_capacity_bytes\":"); body += FirmwarePlatform::otaCapacity();
    body += F(",\"exact_upload_size\":true,\"update\":");body+=localUpdateJson();
    body += F(",\"lifecycle\":\"/api/update/lifecycle\"");
    body += F(",\"free_heap\":"); body += ESP.getFreeHeap();
    body += F(",\"upload\":\"/update\",\"download\":\"/firmware/download\",\"remote_fetch\":\"/api/firmware/fetch\",\"remote_apply\":\"/api/firmware/apply\",\"remote_status\":\"/api/firmware/remote\",\"telemetry\":\"/api/telemetry\",\"avr_full_flash_bytes\":32768,\"avr_eeprom_bytes\":1024,\"filesystem_ota\":false}");
    auto response = request->beginResponse(200, "application/json", body);
    response->addHeader("Cache-Control", "no-store"); request->send(response);
  };
  server.native().on("/update/info", HTTP_GET, info);
  server.on("/info",HTTP_GET,[]() {
    if(!authenticate()) return;
    const String json=buildTelemetryJson();
    if(server.arg("format")=="text") {
      server.send(200,"text/plain",String(APP_NAME)+" "+BoardCore::BuildInfo::kVersion+
        "\nhostname: "+hostname+".local\n"+json+"\n");
    } else {
      auto *response=server.beginResponse(200,"application/json",json);
      response->addHeader("Cache-Control","no-store");server.send(response);
    }
  });
  server.native().on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", F("<!doctype html><meta name='viewport' content='width=device-width'><title>PakarController OTA</title><style>body{max-width:600px;margin:3rem auto;padding:20px;background:#11161e;color:#eef2f7;font:16px system-ui}input,button{margin:12px 0;padding:10px;display:block}a{color:#68a7ff}</style><h1>ESP firmware update</h1><p>Upload the PakarController ESP .bin or .bin.gz. The AVR and stored settings are preserved. Use the MD5 of the uploaded file when available; it checks transfer integrity, not authenticity.</p><form method='post' enctype='multipart/form-data'><label>File MD5 (optional)<input name='MD5' minlength='32' maxlength='32'></label><input type='file' name='firmware' accept='.bin,.gz' required><button>Upload and restart ESP</button></form><a href='/'>Back to controller</a>"));
  });
  server.native().on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    const bool ownsUpdate = otaOwner && request->getAttribute("ota_id") == String(otaOwner);
    if (!ownsUpdate || request->hasAttribute("ota_error") || !request->hasAttribute("ota_uploaded")) {
      if (ownsUpdate) {
        if (Update.isRunning()) FirmwarePlatform::abortUpdate();
        otaOwner = 0; otaActive = false; otaProgress.phase(PakarUpdate::Phase::Failed);
        uploadAttention.finish(false);
      }
      request->send(400, "text/plain", request->getAttribute("ota_error", String(F("No complete firmware uploaded")))); return;
    }
    const uint32_t completedOwner=otaOwner;
    const bool complete = otaUploadedBytes &&
        (!otaExpectedBytes || otaUploadedBytes==otaExpectedBytes) && Update.end(true);
    if(!complete && Update.isRunning()) FirmwarePlatform::abortUpdate();
    if (!complete) { otaOwner=0;otaActive=false;uploadAttention.finish(false); otaProgress.phase(PakarUpdate::Phase::Failed); request->send(400, "text/plain", FirmwarePlatform::updateError()); return; }
    otaRebootAt = millis() + 2000;
    otaCompletion.publish(completedOwner,otaUploadedBytes,Update.md5String().c_str());
    otaProgress.phase(PakarUpdate::Phase::Rebooting);
    otaOwner=0;otaActive=false;
    uploadAttention.finish(true);
    request->send(200, "application/json", F("{\"updated\":true,\"rebooting\":true,\"receipt_pending\":true,\"completion_verified\":false}"));
  }, [](AsyncWebServerRequest *request, String, size_t index, uint8_t *data, size_t length, bool final) {
    // Upload callbacks run before the normal middleware pipeline: authorize before writing anything.
    if (!webAccess.allowed(request) || !WebAccess::sameOrigin(request) || !webAccess.peerAllowed(request)) return;
    if (index == 0) {
      if (urbootBusy.get() || passiveCaptureActive() || artifactReaders || remoteFirmware.busy() || remoteFirmware.ownsEspStage() || otaOwner || otaRebootAt || avrUploadOwner || server.processing() || programmingLatched || networkApplyPending || cloudApplyPending || request->hasAttribute("ota_id")) {
        request->setAttribute("ota_error", "Another operation is active or multiple files were supplied"); return;
      }
      if (++otaNextOwner == 0) ++otaNextOwner;
      otaOwner = otaNextOwner; const uint32_t id = otaOwner;
      request->setAttribute("ota_id", String(id).c_str());
      request->onDisconnect([id]() {
        if (otaOwner != id) return;
        // Uploads are capped below Update.size(), so incomplete end cannot commit boot metadata.
        if (Update.isRunning()) FirmwarePlatform::abortUpdate();
        otaOwner = 0; otaActive = false; otaProgress.phase(PakarUpdate::Phase::Aborted); uploadAttention.finish(false);
      });
      otaUploadedBytes = 0; otaExpectedBytes=0; otaActive = true;
      otaProgress.begin(0,id);
      const uint32_t maximum = FirmwarePlatform::otaCapacity();
      if(request->hasHeader("X-Pakar-Firmware-Size") &&
          !PakarUpdate::imageSize(request->header("X-Pakar-Firmware-Size").c_str(),maximum,otaExpectedBytes)) {
        request->setAttribute("ota_error","Invalid raw firmware size");
        otaOwner=0;otaActive=false;otaProgress.phase(PakarUpdate::Phase::Failed);return;
      }
      otaProgress.begin(otaExpectedBytes,id);
#if !defined(ARDUINO_ARCH_ESP32)
      Update.runAsync(true);
#endif
      if (!Update.begin(maximum, U_FLASH)) {
        request->setAttribute("ota_error", FirmwarePlatform::updateError().c_str());
        otaOwner = 0; otaActive = false; otaProgress.phase(PakarUpdate::Phase::Failed); return;
      }
      String md5=request->header("X-Pakar-Firmware-MD5");
      if(md5.isEmpty())md5=request->arg("MD5"); if (md5.isEmpty()) md5 = request->arg("md5");
      if (!md5.isEmpty()) {
        bool valid = md5.length() == 32;
        for (size_t i = 0; i < md5.length(); ++i) valid = valid && isxdigit(static_cast<unsigned char>(md5[i]));
        if (!valid || !Update.setMD5(md5.c_str())) {
          request->setAttribute("ota_error", "Invalid MD5"); FirmwarePlatform::abortUpdate();
          otaOwner = 0; otaActive = false; otaProgress.phase(PakarUpdate::Phase::Failed); return;
        }
      }
      uploadAttention.begin(); // Shared S3/8266 validated upload; no UART in TCP callback.
    }
    if (request->hasAttribute("ota_error") || request->getAttribute("ota_id") != String(otaOwner)) return;
    if (!Update.isRunning() || index != otaUploadedBytes || index + length >= Update.size() || (otaExpectedBytes &&
        (index>otaExpectedBytes || length>otaExpectedBytes-index)) || Update.write(data, length) != length) {
      request->setAttribute("ota_error", (Update.hasError() ? FirmwarePlatform::updateError() : String(F("Invalid upload sequence or insufficient OTA space"))).c_str());
      if (Update.isRunning()) FirmwarePlatform::abortUpdate();
      otaOwner = 0; otaActive = false; otaProgress.phase(PakarUpdate::Phase::Failed); uploadAttention.finish(false); return;
    }
    otaUploadedBytes += length;
    otaProgress.advance(otaUploadedBytes);
    if (final) {
      if(otaExpectedBytes && otaUploadedBytes!=otaExpectedBytes) {
        request->setAttribute("ota_error","Firmware size differs from declared size");
        FirmwarePlatform::abortUpdate();otaOwner=0;otaActive=false;
        otaProgress.phase(PakarUpdate::Phase::Failed);uploadAttention.finish(false);return;
      }
      request->setAttribute("ota_uploaded", "true");
      // Commit only after the complete HTTP body has arrived: a second file or
      // an interrupted multipart trailer must not accidentally schedule a boot.
      otaProgress.phase(PakarUpdate::Phase::Verifying);
    }
  });
}

#if defined(ARDUINO_ARCH_ESP32)
String statusRgbConfigurationJson() {
  const auto &config=statusRgbConfig.configuration();JsonDocument d;
  d["board_id"]=pcBoardId;d["boot_id"]=pcBootId;
  d["enabled"]=config.enabled;d["pin"]=config.pin;d["pin_confirmed"]=config.pinConfirmed;
  d["brightness"]=config.brightness;d["persisted"]=statusRgbConfig.persisted();d["error"]=statusRgbConfig.lastError();
  String body;serializeJson(d,body);return body;
}
bool updateStatusRgbConfiguration(JsonObjectConst fields,String &body,int &code) {
  code=400;
  if(remoteFirmware.busy()||otaActive||otaRebootAt||avrUploadOwner||programmingLatched||urbootBusy.get()||artifactReaders) {
    code=409;body="Maintenance active";return false;
  }
  if(!fields.size()||fields.size()>4){body="Expected RGB configuration fields";return false;}
  auto config=statusRgbConfig.configuration();
  for(JsonPairConst field:fields) {
    const String key=field.key().c_str();
    if((key!="enabled"&&key!="pin_confirmed"&&key!="pin"&&key!="brightness")||!field.value().is<const char*>()) {
      body="Unknown or invalid RGB configuration field";return false;
    }
  }
  for(const char *key:{"enabled","pin_confirmed"})if(!fields[key].isUnbound()) {
    const String value=fields[key].as<String>();
    if(value!="true"&&value!="false"&&value!="0"&&value!="1") {body="Boolean fields require true/false or 0/1";return false;}
  }
  if(!fields["pin"].isUnbound()) {
    const String pin=fields["pin"].as<String>();
    if(pin!="38"&&pin!="48"&&pin!="-1"){body="Reviewed onboard RGB GPIO must be 38 or 48, or -1 disabled";return false;}
    if(config.pin!=pin.toInt())config.pinConfirmed=false;config.pin=pin.toInt();
  }
  if(!fields["enabled"].isUnbound())config.enabled=fields["enabled"]=="true"||fields["enabled"]=="1";
  if(!fields["pin_confirmed"].isUnbound())config.pinConfirmed=fields["pin_confirmed"]=="true"||fields["pin_confirmed"]=="1";
  if(!fields["brightness"].isUnbound()) {
    uint32_t value;if(!strictU32(fields["brightness"].as<String>(),value)||value>255){body="Brightness must be 0..255";return false;}
    config.brightness=value;
  }
  if(!PakarS3::StatusRgbConfig::validate(config)){body="Confirm a reviewed RGB pin before enabling";return false;}
  if(!statusRgbConfig.save(config)){code=503;body=statusRgbConfig.lastError();return false;}
  if(!statusRgb.configure(config,millis(),writeStatusRgb)){code=503;body="Settings saved; local RGB driver unavailable";return false;}
  JsonDocument result;result["saved"]=true;result["board_id"]=pcBoardId;result["boot_id"]=pcBootId;
  result["driver_submitted"]=statusRgb.snapshot().haveOutput;result["optical_verified"]=false;
  body=String();serializeJson(result,body);code=200;return true;
}
#endif
String avrRecoveryDirectory(const String &id) {
  if(!id.length() || id.length()>32)return String();
  for(size_t i=0;i<id.length();++i) {
    const char c=id[i];
    if(!((c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='_' || c=='-'))return String();
  }
  return String(F("/avr/recovery-"))+id;
}
String avrBackupReadDirectory() {
  return server.hasArg("recovery_id")?avrRecoveryDirectory(server.arg("recovery_id")):String(kBackupDirectory);
}
void registerApi() {
  server.on("/api/panel",HTTP_GET,[](){
    if(!authenticate())return;
    server.send(200,"application/json",panelStatusJson());
  });
  server.on("/api/command",HTTP_POST,[](){
    if(!authenticate())return;
    // Match the existing form transport: the async SDK does not retain an
    // arbitrary JSON body as arg("plain"). URL decoding preserves the exact
    // original WS packet bytes, so its receipt digest is identical on recovery.
    server.send(200,"application/json",commandReply(server.arg("packet")));
    webSocket.textAll(buildStatusJson());
  });
#if defined(ARDUINO_ARCH_ESP32)
  server.on("/api/status-rgb", HTTP_GET, []() {
    if (!authenticate()) return;
    server.send(200,"application/json",statusRgbConfigurationJson());
  });
  server.on("/api/status-rgb", HTTP_POST, []() {
    if (!authenticate()) return;
    JsonDocument form;
    if(auto *request=server.request())for(size_t index=0;index<request->args();++index)
      form[request->argName(index)]=request->arg(index);
    String body;int code=400;const bool ok=updateStatusRgbConfiguration(form.as<JsonObjectConst>(),body,code);
    server.send(code,ok?"application/json":"text/plain",body);
  });
#endif
  server.native().on("/assets/fonts/Vazirmatn-OFL.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200,"text/plain; charset=utf-8",kVazirmatnLicense);
  });
  // Immutable public UI asset. Async response reads PROGMEM, not a heap copy.
  server.native().on("/assets/fonts/Vazirmatn-Regular.ttf", HTTP_GET, [](AsyncWebServerRequest *request) {
    auto *response = request->beginResponse_P(200, "font/ttf", kVazirmatnGzip, sizeof(kVazirmatnGzip));
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "public,max-age=86400");
    response->addHeader("X-Content-Type-Options", "nosniff");
    request->send(response);
  });
  // Immutable static asset: serve on the SDK actor just like the font above.
  // No hardware snapshot or loop-thread state belongs in this handler.
  server.native().on(AsyncURIMatcher::exact("/basic"), HTTP_GET, [](AsyncWebServerRequest *request) {
    // ESP32's deprecated char* send_P selects a heap-backed BasicResponse.
    // Explicit bytes select the SDK's bounded flash response on both targets.
    request->send(200, "text/html; charset=utf-8",
                  reinterpret_cast<const uint8_t *>(kBasicUi), sizeof(kBasicUi) - 1);
  });
  server.on("/", HTTP_GET, []() {
#if defined(ARDUINO_ARCH_ESP8266)
    server.send_P(200,"text/html; charset=utf-8",kWebUiGzip,sizeof(kWebUiGzip),"gzip");
#else
    server.send_P(200, "text/html; charset=utf-8", kWebUi);
#endif
  });
  server.on("/api/status", HTTP_GET, []() {
    if (!authenticate()) return;
    if (server.arg("pixels") == F("1")) {
      String body = buildStatusJson();
      if (!body.reserve(body.length()+LedGrid::kPixels*6U+LedGrid::kCells*4U+180U)) {
        server.send(503,"application/json",F("{\"error\":\"Pixel snapshot memory unavailable\"}")); return;
      }
      body.remove(body.length()-1);
      body += F(",\"pixel_preview_requested\":true,\"pixel_frame\":");
      body += grid.pixelFrameJson(); body += '}';
      server.send(200,"application/json",body);
    } else sendStatusJson();
  });
  server.on("/api/avr/contracts", HTTP_GET, []() {
    if (!authenticate()) return;
    String body = F("{\"dialect\":\"pakar-pccontroller\",\"profile\":80,\"tunnel_opcode\":86,\"chunk_opcode\":87,\"maximum_payload\":48,\"supported\":");
    body += avrOnline && avrPcSupported ? F("true") : F("false");
    body += F(",\"enabled_features\":");
    if (avrOnline && avrPcSupported) body += avrPcFeatures; else body += F("null");
    body += F(",\"esp_addressable_led\":"); body += PAKAR_PC_ESP_LED ? F("true") : F("false");
    body += F(",\"boot_id\":\""); body += pcBootId;
    body += F("\",\"future_hardware_flags\":[\"PAKAR_PC_I2C\",\"PAKAR_PC_SENSORS\",\"PAKAR_PC_PWM\",\"PAKAR_PC_RF\",\"PAKAR_PC_RELAYS\",\"PAKAR_PC_STATUS_RGB\",\"PAKAR_PC_AVR_LED\"]}");
    server.send(200, "application/json", body);
  });
  server.on("/api/avr/contracts", HTTP_POST, []() {
    if (!authenticate()) return;
    uint8_t p[48], n = 0;
    const String opcode = server.arg("opcode");
    bool valid = opcode.length() && opcode.length() <= 3;
    for (size_t i = 0; i < opcode.length(); ++i) valid &= opcode[i] >= '0' && opcode[i] <= '9';
    const long value = opcode.toInt();
    if (!valid || value < 0 || value > 255 ||
        !BoardCore::PcContractTransport::decodeHex(server.arg("payload_hex").c_str(), p, n)) {
      server.send(400, "application/json", F("{\"error\":\"bad_contract_request\"}")); return;
    }
    if (value != ControllerProtocol::AddressableLed && (!avrOnline || !avrPcSupported)) {
      server.send(503, "application/json", F("{\"error\":\"avr_contract_unavailable\"}")); return;
    }
    const uint32_t id = pcTransport.enqueue(value, p, n, millis());
    if (!id) { server.send(409, "application/json", F("{\"error\":\"contract_queue_full\"}")); return; }
    server.send(202, "application/json", pcReceiptJson(*pcTransport.get(id)));
  });
  server.on("/api/avr/contracts/result", HTTP_GET, []() {
    if (!authenticate()) return;
    const String idText = server.arg("id");
    bool valid = idText.length() && idText.length() <= 10;
    uint32_t id = 0;
    for (size_t i = 0; i < idText.length(); ++i) {
      const char c = idText[i];
      if (c < '0' || c > '9' || id > (UINT32_MAX - static_cast<uint32_t>(c - '0')) / 10) { valid = false; break; }
      id = id * 10 + c - '0';
    }
    const auto *receipt = valid && id ? pcTransport.get(id) : nullptr;
    if (!receipt || receipt->usb) { server.send(404, "application/json", F("{\"error\":\"unknown_contract_request\"}")); return; }
    server.send(200, "application/json", pcReceiptJson(*receipt));
  });
  server.on("/api/avr/contracts/result", HTTP_DELETE, []() {
    if (!authenticate()) return;
    const String idText = server.arg("id"), opText = server.arg("opcode");
    uint32_t id = 0;
    bool valid = idText.length() && idText.length() <= 10 &&
                 opText.length() && opText.length() <= 3 &&
                 server.arg("boot_id") == pcBootId;
    for (size_t i = 0; i < idText.length(); ++i) {
      const char c = idText[i];
      if (c < '0' || c > '9' || id > (UINT32_MAX - static_cast<uint32_t>(c - '0')) / 10) { valid = false; break; }
      id = id * 10 + c - '0';
    }
    for (size_t i = 0; i < opText.length(); ++i) valid &= opText[i] >= '0' && opText[i] <= '9';
    const long op = opText.toInt();
    if (!valid || !id || op < 0 || op > 255 ||
        !pcTransport.consume(id, static_cast<uint8_t>(op))) {
      server.send(409, "application/json", F("{\"error\":\"receipt_not_consumable\"}")); return;
    }
    server.send(200, "application/json", F("{\"consumed\":true}"));
  });
  server.on("/api/cells/mapping", HTTP_GET, []() {
    if (!authenticate()) return;
    String body;
    // Names may each contain15 bytes requiring two-byte JSON escaping. Reserve
    // the complete bounded response once; never return truncated JSON as200.
    if(!body.reserve(sizeof(cellMapping.names)*2U+LedGrid::kCells*8U+256U)){
      server.send(503,"application/json",F("{\"error\":\"mapping_memory_unavailable\"}"));return;
    }
    // Bind the mapping itself to its producer. Clients must not need a second
    // status response to establish which board/boot supplied these values.
    body += F("{\"board_id\":\""); body += pcBoardId;
    body += F("\",\"boot_id\":\""); body += pcBootId;
    body += F("\",\"logical_order\":\"row-major\",\"odd_column_offset\":0.5,\"physical\":[");
    for (uint8_t logical = 1; logical <= LedGrid::kCells; ++logical) {
      if (logical != 1) body += ',';
      body += cellMapping.physical[logical - 1];
    }
    body += F("],\"names\":[");
    for (uint8_t logical = 1; logical <= LedGrid::kCells; ++logical) {
      if (logical != 1) body += ',';
      body += '"';
      if (cellMapping.names[logical - 1][0] == '\0') body += logical;
      else appendJsonEscaped(body, cellMapping.names[logical - 1]);
      body += '"';
    }
    body += F("]}");
    server.send(200, "application/json", body);
  });
  server.on("/api/cells/mapping", HTTP_POST, []() {
    if (!authenticate()) return;
    if(server.hasArg("preset")){
      if(server.arg("preset")!=F("table-cable") || server.hasArg("logical_id") ||
         server.hasArg("physical_cell") || server.hasArg("name")){
        server.send(400,"text/plain",F("preset must be table-cable without per-cell fields"));return;
      }
      const CellMappingRecord previous=cellMapping;
      for(uint8_t logical=1;logical<=LedGrid::kCells;++logical)
        cellMapping.physical[logical-1]=defaultPhysicalCell(logical);
      if(!saveCellMapping()){
        cellMapping=previous;
        server.send(500,"text/plain",F("cell mapping persistence failed"));return;
      }
      grid.setPhysicalMapping(cellMapping.physical,LedGrid::kCells);
      server.send(200,"application/json",F("{\"updated\":true,\"preset\":\"table-cable\"}"));return;
    }
    const int logical = server.arg("logical_id").toInt();
    const int physical = server.arg("physical_cell").toInt();
    if (logical < 1 || logical > LedGrid::kCells ||
        physical < 1 || physical > LedGrid::kCells) {
      server.send(400, "text/plain", F("logical_id and physical_cell must be 1..80"));
      return;
    }
    String name = server.arg("name");
    name.trim();
    if (name.length() > 15) {
      server.send(400, "text/plain", F("name must be at most 15 UTF-8 bytes"));
      return;
    }
    const uint8_t logicalIndex = static_cast<uint8_t>(logical - 1);
    const uint8_t oldPhysical = cellMapping.physical[logicalIndex];
    for (uint8_t i = 0; i < LedGrid::kCells; ++i) {
      if (cellMapping.physical[i] == physical) {
        cellMapping.physical[i] = oldPhysical;
        break;
      }
    }
    cellMapping.physical[logicalIndex] = static_cast<uint8_t>(physical);
    memset(cellMapping.names[logicalIndex], 0,
           sizeof(cellMapping.names[logicalIndex]));
    name.toCharArray(cellMapping.names[logicalIndex],
                     sizeof(cellMapping.names[logicalIndex]));
    if (!saveCellMapping()) {
      server.send(500, "text/plain", F("cell mapping persistence failed"));
      return;
    }
    grid.setPhysicalMapping(cellMapping.physical, LedGrid::kCells);
    server.send(200, "application/json", F("{\"updated\":true}"));
  });
  server.on("/description.xml", HTTP_GET, []() {
#if defined(ARDUINO_ARCH_ESP32)
    server.send(200, "text/xml", s3Discovery.xmlDescription()); return;
#else
    String xml = F("<?xml version=\"1.0\"?><root xmlns=\"urn:schemas-upnp-org:device-1-0\"><specVersion><major>1</major><minor>0</minor></specVersion><device><deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType><friendlyName>");
    auto escaped = [](const char *input) { String s(input); s.replace("&","&amp;");s.replace("<","&lt;");s.replace(">","&gt;");s.replace("\"","&quot;");s.replace("'","&apos;");return s; };
    xml += escaped(deviceIdentity.displayName());
    xml += F("</friendlyName><manufacturer>AtomicDeploy</manufacturer><modelName>"); xml += escaped(APP_NAME);
    xml += F("</modelName><UDN>uuid:");
    char uuid[37]; snprintf(uuid, sizeof(uuid), "38323636-4558-4dda-9188-cda0e6%06x", ESP.getChipId()); xml += uuid;
    xml += F("</UDN><presentationURL>http://"); xml += hostname;
    xml += F(".local/</presentationURL></device></root>");
    server.send(200, "text/xml", xml);
#endif
  });
  server.on("/api/time", HTTP_GET, []() {
    if (!authenticate()) return;
    const time_t currentTime = time(nullptr);
    String body = F("{\"synced\":");
    body += currentTime > 1700000000 ? F("true") : F("false");
    body += F(",\"utc_epoch\":");
    body += currentTime > 1700000000 ? static_cast<uint32_t>(currentTime) : 0U;
    body += '}';
    server.send(200, "application/json", body);
  });
  server.on("/api/wifi/scan", HTTP_GET, []() {
    if (!authenticate()) return;
    const int count = WiFi.scanNetworks(false, true);
    String body = F("{\"networks\":[");
    for (int index = 0; index < count; ++index) {
      if (index != 0) body += ',';
      String ssid = WiFi.SSID(index);
      ssid.replace("\\", "\\\\");
      ssid.replace("\"", "\\\"");
      body += F("{\"ssid\":\""); body += ssid;
      body += F("\",\"rssi\":"); body += WiFi.RSSI(index);
      body += F(",\"encrypted\":");
#if defined(ARDUINO_ARCH_ESP32)
      body += WiFi.encryptionType(index) == WIFI_AUTH_OPEN ? F("false") : F("true");
#else
      body += WiFi.encryptionType(index) == ENC_TYPE_NONE ? F("false") : F("true");
#endif
      body += '}';
    }
    body += F("]}");
    WiFi.scanDelete();
    server.send(200, "application/json", body);
  });
  server.on("/api/mode", HTTP_POST, []() {
    if (!authenticate()) return;
    const String value = server.arg("value");
    if (value != F("cell") &&
        value != F("first-wins") && value != F("snake")) {
      server.send(400, "text/plain",
                  "mode must be cell, first-wins, or snake");
      return;
    }
    const GameMode requested = parseMode(value);
    if(!BoardCore::GameRole::accepts(requested)){server.send(409,"application/json",F("{\"accepted\":false,\"error\":\"mode_baked_at_build\"}"));return;}
    if(!grid.resetWhite()){server.send(409,"application/json",F("{\"accepted\":false,\"error\":\"red_cells_locked\"}"));return;}
    mode = static_cast<GameMode>(BoardCore::GameRole::mode);
    winner = 0;
    saveLedPreferences();
    const uint8_t payload[] = {static_cast<uint8_t>(mode)};
    sendToAvr(kSetMode, sequence++, payload, sizeof(payload));
    server.send(202, "application/json", F("{\"accepted\":true}"));
  });
  server.on("/api/scene/save", HTTP_POST, []() {
    if (!authenticate()) return;
    serviceScenePersistence(true);
    const bool saved = sceneStore.saved();
    server.send(saved ? 200 : 503, "application/json",
        saved ? F("{\"saved\":true}") : F("{\"saved\":false,\"pending\":true}"));
  });
  server.on("/api/led/output",HTTP_POST,[](){
    if(!authenticate())return;
    if(server.hasArg("capture")) {
      if((server.arg("capture")!="true" && server.arg("capture")!="1") || server.hasArg("enabled") || server.hasArg("mode")) {
        server.send(400,"text/plain",F("Capture must be requested separately; it does not change LED mode/output"));return;
      }
      const bool accepted=grid.requestSignalCapture();
      server.send(accepted?202:409,"application/json",accepted?F("{\"accepted\":true,\"optical_verified\":false}"):F("{\"accepted\":false,\"error\":\"capture_unavailable_or_busy\"}"));return;
    }
    const bool enabledGiven=server.hasArg("enabled"),modeGiven=server.hasArg("mode");
    const String value=server.arg("enabled"),outputMode=server.arg("mode");
    if((!enabledGiven&&!modeGiven)||(enabledGiven&&value!="true"&&value!="false"&&value!="1"&&value!="0")||
       (modeGiven&&outputMode!="static"&&outputMode!="effects")){
      server.send(400,"text/plain",F("LED output requires enabled true/false and/or mode static/effects"));return;}
    if(modeGiven)grid.setStaticMode(outputMode=="static");
    if(enabledGiven)grid.setOutputEnabled(value=="true"||value=="1");
    server.send(200,"application/json",grid.outputJson());
  });
  server.on("/api/cell", HTTP_POST, []() {
    if (!authenticate()) return;
    const int id = server.arg("id").toInt();
    if (id < 1 || id > LedGrid::kCells) {
      server.send(400, "text/plain", F("cell id must be 1..80"));
      return;
    }
    const CellState state = parseCellState(server.arg("state"));
    const Effect effect = parseEffect(server.arg("effect"));
    if(!grid.setCell(id,state,effect)){server.send(409,"application/json",F("{\"accepted\":false,\"error\":\"red_cells_locked\"}"));return;}
    selectedCell = id;
    selectedState = state;
    const uint8_t payload[] = {static_cast<uint8_t>(id), static_cast<uint8_t>(state),
                               static_cast<uint8_t>(effect)};
    sendToAvr(kCellAction, sequence++, payload, sizeof(payload));
    server.send(202, "application/json", F("{\"accepted\":true}"));
  });
  server.on("/api/cells",HTTP_POST,[](){
    if(!authenticate())return;
    BoardCore::CellBatch selection;CellState state;Effect effect;
    if(!parseCellsAction(server.arg("ids"),server.arg("state"),server.arg("effect"),selection,state,effect)){
      server.send(400,"application/json",F("{\"accepted\":false,\"error\":\"invalid_cell_set\"}"));return;
    }
    // Hardware handlers execute on the loop owner. Stage the whole selection
    // before the next render; persistence is coalesced by the loop, not per cell.
    if(!grid.setCells(selection,state,effect)){
      server.send(409,"application/json",F("{\"accepted\":false,\"error\":\"red_cells_locked\",\"message\":\"Red cells cannot become white while the lock is enabled.\"}"));return;
    }
    selectedCell=selection.first();selectedState=state;
    String reply=F("{\"accepted\":true,\"count\":");reply+=selection.count();reply+='}';
    server.send(202,"application/json",reply);
  });
  server.on("/api/cells/assignments",HTTP_GET,[](){
    if(!authenticate())return;
    server.send(200,"application/json",cellAssignmentsJson());
  });
  server.on("/api/cells/assignments",HTTP_POST,[](){
    if(!authenticate())return;
    JsonDocument document;String error;
    const String json=server.jsonBody();const bool jsonRequest=server.request()->contentType()==F("application/json");
    const bool parsed=jsonRequest? !deserializeJson(document,json) : !deserializeJson(document,server.arg("assignments"));
    const JsonVariantConst assignments=jsonRequest?document["assignments"].as<JsonVariantConst>():document.as<JsonVariantConst>();
    if(!parsed || !applyCellAssignments(assignments,error)){
      if(!error.length())error=F("invalid_cell_assignments");
      const char *message=error==F("red_cells_locked")?BoardCore::CommandReceipts::failureMessage(BoardCore::CommandReceipts::RedCellsLocked):BoardCore::CommandReceipts::failureMessage(BoardCore::CommandReceipts::InvalidCellAssignments);
      server.send(error==F("red_cells_locked")?409:400,"application/json",String(F("{\"accepted\":false,\"error\":\""))+error+F("\",\"message\":\"")+message+F("\"}"));return;
    }
    server.send(200,"application/json",cellAssignmentsJson(true));webSocket.textAll(buildStatusJson());
  });
  server.on("/api/cells/lock",HTTP_POST,[](){
    if(!authenticate())return;
    JsonDocument document;bool enabled=false;const String json=server.jsonBody();
    if(server.request()->contentType()==F("application/json")){
      if(deserializeJson(document,json)||!document["enabled"].is<bool>()){server.send(400,"application/json",F("{\"accepted\":false,\"error\":\"invalid_lock\"}"));return;}
      enabled=document["enabled"].as<bool>();
    }else{
      const String value=server.arg("enabled");if(value!="true"&&value!="false"&&value!="1"&&value!="0"){server.send(400,"application/json",F("{\"accepted\":false,\"error\":\"invalid_lock\"}"));return;}
      enabled=value=="true"||value=="1";
    }
    grid.setRedLock(enabled);server.send(200,"application/json",cellAssignmentsJson(true));webSocket.textAll(buildStatusJson());
  });
  server.on("/api/game/reset", HTTP_POST, []() {
    if (!authenticate()) return;
    if(!grid.resetWhite()){server.send(409,"application/json",F("{\"accepted\":false,\"error\":\"red_cells_locked\"}"));return;}
    winner = 0;
    saveLedPreferences();
    sendToAvr(kGameReset, sequence++);
    server.send(202, "application/json", F("{\"accepted\":true}"));
  });
  server.on("/api/snake/direction", HTTP_POST, []() {
    if (!authenticate()) return;
    const String value = server.arg("value");
    const int8_t direction =
        value == F("up") ? 0 : value == F("right") ? 1 :
        value == F("down") ? 2 : value == F("left") ? 3 : -1;
    if (mode != kModeSnake || direction < 0 ||
        !setSnakeDirection(static_cast<uint8_t>(direction))) {
      server.send(400, "text/plain",
                  "snake must be active and direction cannot reverse");
      return;
    }
    server.send(202, "application/json", F("{\"accepted\":true}"));
  });
  server.on("/api/brightness", HTTP_POST, []() {
    if (!authenticate()) return;
    const int value = server.arg("value").toInt();
    if (value < 1 || value > 255) {
      server.send(400, "text/plain", F("brightness must be 1..255"));
      return;
    }
    grid.setBrightness(value);
    saveLedPreferences();
    server.send(200, "application/json", F("{\"updated\":true}"));
  });
  server.on("/api/effect", HTTP_POST, []() {
    if (!authenticate()) return;
    const String value = server.arg("value");
    const LedGrid::Pattern pattern = parsePattern(value);
    if (pattern == LedGrid::kPatternNone && value != F("none")) {
      server.send(400, "text/plain", F("unknown effect"));
      return;
    }
    if(!grid.setPattern(pattern)){server.send(409,"application/json",F("{\"accepted\":false,\"error\":\"red_cells_locked\",\"message\":\"Cell chase changes red assignments; disable red-cell lock first.\"}"));return;}
    saveLedPreferences();
    server.send(200, "application/json", F("{\"updated\":true}"));
  });
  server.on("/api/buzzer", HTTP_POST, []() {
    if (!authenticate()) return;
    const int frequency = server.arg("frequency").toInt();
    const int duration = server.arg("duration_ms").toInt();
    if (frequency < 20 || frequency > 12000 || duration < 1 || duration > 5000) {
      server.send(400, "text/plain", F("frequency must be 20..12000 and duration_ms 1..5000"));
      return;
    }
    const uint8_t payload[] = {
        static_cast<uint8_t>(frequency), static_cast<uint8_t>(frequency >> 8),
        static_cast<uint8_t>(duration), static_cast<uint8_t>(duration >> 8),
    };
    sendToAvr(kBuzzer, sequence++, payload, sizeof(payload));
    server.send(202, "application/json", F("{\"accepted\":true}"));
  });
  server.on("/api/audio", HTTP_POST, []() {
    if (!authenticate()) return;
    uint8_t cue = 0;
    if (!parseAudioCue(server.arg("cue"), cue)) {
      server.send(400, "text/plain", F("unknown audio cue"));
      return;
    }
    if (!sendToAvr(kAudioCue, sequence++, &cue, 1)) {
      server.send(503, "text/plain", F("AVR link unavailable")); return;
    }
    server.send(202, "application/json", F("{\"accepted\":true,\"state\":\"queued\"}"));
  });
  server.on("/api/display", HTTP_POST, []() {
    if (!authenticate()) return;
    if (server.arg("release") == F("true") || server.arg("release") == F("1")) {
      const uint8_t payload[] = {4, 0, 0, 0};
      sendToAvr(kDisplayText, sequence++, payload, sizeof(payload));
      server.send(202, "application/json", F("{\"accepted\":true}"));
      return;
    }
    if (server.hasArg("text")) {
      const String text = server.arg("text");
      const int speed = server.hasArg("speed_ms")
                            ? server.arg("speed_ms").toInt()
                            : 260;
      const int hold = server.hasArg("hold_ms")
                           ? server.arg("hold_ms").toInt()
                           : 1500;
      const String repeat = server.arg("repeat");
      const bool forceScroll = server.arg("force_scroll") == F("true") ||
                               server.arg("force_scroll") == F("1");
      const int interval = server.hasArg("interval_seconds")
                               ? server.arg("interval_seconds").toInt()
                               : 5;
      String result;
      if (!submitDisplayText(text, speed, hold, repeat, forceScroll, interval, result)) {
        server.send(400, "text/plain", result);
        return;
      }
      server.send(202, "application/json", F("{\"accepted\":true}"));
      return;
    }
    if (!server.hasArg("value")) {
      server.send(400, "text/plain", F("provide text, value, or release=true"));
      return;
    }
    const int value = server.arg("value").toInt();
    if (value < -999 || value > 9999) {
      server.send(400, "text/plain", F("display value must be -999..9999"));
      return;
    }
    const int16_t signedValue = static_cast<int16_t>(value);
    const uint8_t payload[] = {
        static_cast<uint8_t>(signedValue), static_cast<uint8_t>(signedValue >> 8),
    };
    sendToAvr(kDisplayText, sequence++, payload, sizeof(payload));
    server.send(202, "application/json", F("{\"accepted\":true}"));
  });
  server.on("/api/outputs", HTTP_POST, []() {
    if (!authenticate()) return;
    const int mask = server.arg("mask").toInt();
    if (mask < 0 || mask > 255) {
      server.send(400, "text/plain", F("mask must be 0..255"));
      return;
    }
    const uint8_t payload[] = {static_cast<uint8_t>(mask)};
    sendToAvr(kOutputMask, sequence++, payload, sizeof(payload));
    server.send(202, "application/json", F("{\"accepted\":true}"));
  });
  server.on("/api/device/name", HTTP_GET, []() {
    if (!authenticate()) return;
    server.send(200, "application/json", deviceNameJson());
  });
  server.on("/api/device/name", HTTP_POST, []() {
    if (!authenticate()) return;
    if (remoteFirmware.busy() || otaActive || otaRebootAt || avrUploadOwner || programmingLatched || networkApplyPending || cloudApplyPending) {
      server.send(409, "application/json", F("{\"error\":\"device_busy\"}")); return;
    }
    char normalized[64];
    const String input = server.arg("hostname");
    if (input.length() != strlen(input.c_str()) || !BoardCore::Identity::normalizeName(input.c_str(), normalized)) {
      server.send(400, "application/json", F("{\"error\":\"invalid_hostname\"}")); return;
    }
    const String label = server.hasArg("display_name") ? server.arg("display_name") : String(deviceIdentity.displayName());
    if (label.length() != strlen(label.c_str()) || !BoardCore::Identity::validDisplayName(label.c_str())) {
      server.send(400, "application/json", F("{\"error\":\"invalid_display_name\"}")); return;
    }
    if (!scheduleDeviceIdentity(normalized, label)) {
      server.send(503, "application/json", deviceNameJson()); return;
    }
    server.send(202, "application/json", deviceNameJson(true));
  });
  server.on("/api/network", HTTP_GET, []() {
    if (authenticate()) server.send(200, "application/json", networkConfigurationJson());
  });
  server.on("/api/network/ethernet", HTTP_GET, []() {
    if(authenticate())server.send(200,"application/json",ethernetConfigurationJson());
  });
  server.on("/api/network/ethernet", HTTP_POST, []() {
    if(!authenticate())return;
    JsonDocument document;String result;
    if(!configurationForm(document,false,result) ||
        !updateEthernetConfiguration(document.as<JsonObjectConst>(),result)) {
      server.send(400,"text/plain",result);return;
    }
    server.send(202,"application/json",F("{\"accepted\":true,\"apply_after_ms\":1000}"));
  });
  server.on("/api/network", HTTP_POST, []() {
    if (!authenticate()) return;
    JsonDocument document; String result;
    if (!configurationForm(document, false, result) ||
        !updateNetworkConfiguration(document.as<JsonObjectConst>(), result)) {
      server.send(400, "text/plain", result); return;
    }
    server.send(202, "application/json", F("{\"accepted\":true,\"apply_after_ms\":1000}"));
  });
  server.on("/api/cloud", HTTP_GET, []() {
    if (authenticate()) server.send(200, "application/json", cloudConfigurationJson());
  });
  server.on("/api/cloud", HTTP_POST, []() {
    if (!authenticate()) return;
    JsonDocument document; String result;
    if (!configurationForm(document, true, result) ||
        !updateCloudConfiguration(document.as<JsonObjectConst>(), result)) {
      server.send(400, "text/plain", result); return;
    }
    server.send(202, "application/json", F("{\"updated\":true,\"accepted\":true,\"apply_after_ms\":1000}"));
  });
  server.on("/api/cloud/reconnect", HTTP_POST, []() {
    if (!authenticate()) return;
    if (remoteFirmware.busy() || otaActive || otaRebootAt || cloudApplyPending) {
      server.send(409, "text/plain", F("Maintenance/configuration active")); return;
    }
    cloudApplyPending = true; cloudApplyAt = millis() + 1000U;
    server.send(202, "application/json", F("{\"accepted\":true,\"apply_after_ms\":1000}"));
  });

  server.on("/api/avr/probe", HTTP_GET, []() {
    if (!authenticate()) return;
    uint8_t signature[3] = {};
    const bool ok = avrIsp.probe(signature);
    if (!ok) {
      server.send(503, "text/plain", avrIsp.lastError());
      return;
    }
    server.send(200, "application/json", F("{\"signature\":\"1E950F\"}"));
  });
  server.on("/api/avr/backup", HTTP_POST, []() {
    if (!authenticate()) return;
    LittleFS.mkdir("/avr");
    if (!avrIsp.backup(LittleFS, kBackupDirectory)) {
      server.send(500, "text/plain", avrIsp.lastError());
      return;
    }
    server.send(200, "application/json", F("{\"backed_up\":true}"));
  });
  server.on("/api/avr/backup/flash", HTTP_GET, []() {
    if (!authenticate()) return;
    const String directory=avrBackupReadDirectory();
    if(!directory.length()){server.send(400,"text/plain",F("Invalid recovery_id"));return;}
    File file = LittleFS.open(directory + F("/flash.bin"), "r");
    if (!file) { server.send(404, "text/plain", F("no flash backup")); return; }
    server.streamFile(file, "application/octet-stream");
    // Response owns a File copy; explicit close would close its shared handle.
  });
  server.on("/api/avr/backup/eeprom", HTTP_GET, []() {
    if (!authenticate()) return;
    const String directory=avrBackupReadDirectory();
    if(!directory.length()){server.send(400,"text/plain",F("Invalid recovery_id"));return;}
    File file = LittleFS.open(directory + F("/eeprom.bin"), "r");
    if (!file) { server.send(404, "text/plain", F("no EEPROM backup")); return; }
    server.streamFile(file, "application/octet-stream");
    // Let the local wrapper release its reference, not close the response's file.
  });
  server.on("/api/avr/backup/metadata", HTTP_GET, []() {
    if (!authenticate()) return;
    const String directory=avrBackupReadDirectory();
    if(!directory.length()){server.send(400,"text/plain",F("Invalid recovery_id"));return;}
    File file=LittleFS.open(directory+F("/metadata.txt"),"r");
    if(!file){server.send(404,"text/plain",F("no AVR backup metadata"));return;}
    server.streamFile(file,"text/plain");
  });
  server.on("/api/avr/program", HTTP_POST, []() {
    if(!authenticate()) return;
    File image=LittleFS.open(kAvrImagePath,"r");
    if(!image || image.size()!=32768) {if(image)image.close();server.send(400,"text/plain",F("Complete AVR image required"));return;}
    image.close();
    const String recovery=server.arg("recovery");
    if(recovery.length() && recovery!="isp") {
      server.send(400,"text/plain",F("Unknown recovery operation"));return;
    }
    if(recovery=="isp") {
      // Explicit repair for a non-speaking application. Ordinary updates still
      // require UART Prog admission; this branch binds all three operator-held
      // artifacts and proves target bytes before any erase, without fuse writes.
      if(avrOnline || urbootBusy.get() || passiveCaptureActive() || artifactReaders || remoteFirmware.busy() || otaActive ||
          otaRebootAt || avrUploadOwner || networkApplyPending || cloudApplyPending) {
        server.send(409,"text/plain",F("Recovery requires an offline AVR and exclusive maintenance"));return;
      }
      auto matches=[](File &file,const String &expected,uint32_t size)->bool {
        if(!file || file.size()!=size || expected.length()!=64 || !file.seek(0,SeekSet))return false;
        FirmwarePlatform::Sha256 hash;uint8_t buffer[128];
        for(uint32_t at=0;at<size;at+=sizeof(buffer)) {
          const size_t count=min<size_t>(sizeof(buffer),size-at);
          if(file.read(buffer,count)!=count)return false;
          hash.add(buffer,count);yield();
        }
        return hash.hex()==expected && file.seek(0,SeekSet);
      };
      File source=LittleFS.open(kAvrImagePath,"r");
      File flash=LittleFS.open(String(kBackupDirectory)+F("/flash.bin"),"r");
      File eeprom=LittleFS.open(String(kBackupDirectory)+F("/eeprom.bin"),"r");
      if(!matches(source,server.arg("sha256"),32768) ||
          !matches(flash,server.arg("flash_sha256"),32768) ||
          !matches(eeprom,server.arg("eeprom_sha256"),1024)) {
        server.send(409,"text/plain",F("Candidate or original backup hash mismatch; no erase"));return;
      }
      const bool fresh=server.arg("fresh_snapshot")=="1";
      const String recoveryId=server.arg("recovery_id");
      const String directory=fresh?avrRecoveryDirectory(recoveryId):String();
      if(fresh && (!directory.length() || LittleFS.exists(directory))) {
        server.send(409,"text/plain",F("A new nonexisting recovery_id is required; no erase"));return;
      }
      // Fence all competing producers for the bounded, loop-owned transaction.
      // No reset lease is held across an HTTP round trip or caller timeout.
      server.setMaintenanceFence(true);hostFrameIngress.discard();closeDeviceShells();stream.stop();urbootBusy.set(true);
      struct RecoveryFence {
#if defined(ARDUINO_ARCH_ESP32)
        bool ledHeld=false;
#endif
        ~RecoveryFence(){
          urbootRawOwned=false;
#if defined(ARDUINO_ARCH_ESP32)
          if(ledHeld)PakarS3::LedOutput::releaseFlashPause();
#endif
          urbootBusy.set(false);server.setMaintenanceFence(false);hostFrameIngress.discard();hostLink.discardInput();stream.begin(kStreamPort);
        }
      } fence;
#if defined(ARDUINO_ARCH_ESP32)
      fence.ledHeld=PakarS3::LedOutput::acquireFlashPause();
      if(!fence.ledHeld){server.send(503,"text/plain",F("LED output cannot quiesce; no reset or erase"));return;}
#endif
      urbootRawOwned=true;
      // This is a distinct durable file: never rewrite the original snapshot.
      File armed;
      if(!fresh)armed=LittleFS.open("/avr/recovery-eeprom.bin","w+");
      const bool programmed=fresh?avrIsp.recoverFlash(LittleFS,directory.c_str(),source,flash):
          avrIsp.programFlash(source,eeprom,&flash,&armed);
      source.close();flash.close();eeprom.close();armed.close();
      urbootRawOwned=false;
      auto sendFreshResult=[&](int code,const String &error,bool recovered) {
        JsonDocument result;result["programmed"]=programmed;result["verified"]=programmed;
        result["fresh_snapshot"]=true;result["recovery_id"]=recoveryId;result["backup_directory"]=directory;
        result["settings_eeprom_preserved"]=programmed;result["programming_latch_updated"]=programmed;
        result["uart_recovered"]=recovered;result["fuse_write"]=false;result["error"]=error;
        auto addHash=[&](const char *name,const char *fileName) {
          File saved=LittleFS.open(directory+fileName,"r");FirmwarePlatform::Sha256 hash;uint8_t block[128];
          if(!saved)return;
          const size_t size=saved.size();
          for(size_t at=0;at<size;at+=sizeof(block)) {
            const size_t count=min<size_t>(sizeof(block),size-at);
            if(saved.read(block,count)!=count)return;
            hash.add(block,count);yield();
          }
          result[name]=hash.hex();
        };
        addHash("flash_sha256","/flash.bin");addHash("eeprom_sha256","/eeprom.bin");
        addHash("armed_eeprom_sha256","/eeprom-armed.bin");addHash("metadata_sha256","/metadata.txt");
        String body;serializeJson(result,body);server.send(code,"application/json",body);
      };
      if(!programmed){if(fresh)sendFreshResult(500,avrIsp.lastError(),false);else server.send(500,"text/plain",avrIsp.lastError());return;}
      programmingLatched=true;
      if(!waitForAvr() || !avrVersionKnown || !avrPcSupported ||
          avrVersion[0]!=BoardCore::BuildInfo::kVersionMajor ||
          avrVersion[1]!=BoardCore::BuildInfo::kVersionMinor ||
          avrVersion[2]!=BoardCore::BuildInfo::kVersionPatch) {
        if(fresh)sendFreshResult(503,F("ISP flash verified; UART not recovered, Prog remains armed"),false);
        else server.send(503,"text/plain",F("ISP flash verified; UART not recovered, Prog remains armed"));
        return;
      }
      const uint8_t released[]={0};
      if(!requestAndWaitForAck(kProgrammingState,released,1)) {
        if(fresh)sendFreshResult(503,F("ISP flash verified; Prog release outcome unknown, inspect status"),false);
        else server.send(503,"text/plain",F("ISP flash verified; Prog release outcome unknown, inspect status"));
        return;
      }
      programmingLatched=false;
      sendToAvr(kGetStatus,sequence++);
      sendToAvr(kGetTelemetry,sequence++);
      if(fresh){sendFreshResult(200,String(),true);return;}
      server.send(200,"application/json",F("{\"programmed\":true,\"verified\":true,\"settings_eeprom_preserved\":true,\"programming_latch_updated\":true,\"uart_recovered\":true}"));return;
    }
    // Manual uploads retain compatibility, but no longer bypass durable Prog
    // or EEPROM preservation. Exact remote provenance uses the manifest path.
    RemoteFirmware::Plan plan;plan.targets=2;plan.avr.version=BoardCore::BuildInfo::kVersion;
    String reason;
    if(!prepareRemotePair(plan,reason) ||
       !programRemoteAvr(kAvrImagePath,plan.avr.version,reason) ||
       finishRemotePair(plan,true,reason)!=1) {
      server.send(500,"text/plain",reason);return;
    }
    server.send(200,"application/json",F("{\"programmed\":true,\"verified\":true,\"eeprom_preserved\":true}"));
  });

  server.native().on("/api/avr/image", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (!webAccess.allowed(request)) { request->send(401); return; }
      const bool ownsImage = avrUploadOwner && request->getAttribute("upload_id") == String(avrUploadOwner);
      bool valid = ownsImage && !request->hasAttribute("upload_error") && request->hasAttribute("upload_complete");
      if (valid) {
        File pending = LittleFS.open("/avr/upload.pending", "r");
        valid = pending && pending.size() == AvrIsp::kFlashBytes;
        if (pending) pending.close();
        valid = valid && LittleFS.rename("/avr/upload.pending", kAvrImagePath);
      }
      if (ownsImage) { if (uploadFile) uploadFile.close(); avrUploadOwner = 0; }
      request->send(valid ? 201 : 400, "text/plain", valid ? String(F("AVR image stored")) :
          request->getAttribute("upload_error", String(F("A complete 32768-byte image could not be committed"))));
    },
    [](AsyncWebServerRequest *request, String, size_t index, uint8_t *data, size_t length, bool final) {
      if (!webAccess.allowed(request) || !WebAccess::sameOrigin(request) || !webAccess.peerAllowed(request)) return;
      static uint32_t nextUpload = 0;
      if (index == 0) {
        if (urbootBusy.get() || passiveCaptureActive() || artifactReaders || remoteFirmware.busy() || avrUploadOwner || otaActive || otaRebootAt || server.processing() || networkApplyPending || cloudApplyPending || request->hasAttribute("upload_id")) {
          request->setAttribute("upload_error", "Another operation is active or multiple files were supplied"); return;
        }
        if (++nextUpload == 0) ++nextUpload;
        avrUploadOwner = nextUpload;
        const uint32_t id = avrUploadOwner;
        request->setAttribute("upload_id", String(id).c_str());
        request->onDisconnect([id]() { if (avrUploadOwner == id) { if (uploadFile) uploadFile.close(); avrUploadOwner = 0; } });
        LittleFS.mkdir("/avr");
        uploadFile = LittleFS.open("/avr/upload.pending", "w");
      }
      if (request->hasAttribute("upload_error")) return;
      if (request->getAttribute("upload_id") != String(avrUploadOwner)) return;
      if (!uploadFile || index + length > AvrIsp::kFlashBytes || uploadFile.write(data, length) != length) {
        request->setAttribute("upload_error", "write failed or image too large");
        if (uploadFile) uploadFile.close();
        avrUploadOwner = 0;
        return;
      }
      if (final) {
        uploadFile.close();
        if (index + length != AvrIsp::kFlashBytes)
          request->setAttribute("upload_error", "image must be exactly 32768 bytes");
        else request->setAttribute("upload_complete", "true");
      }
    });
}

// The 1300-byte packet exists only during stream processing, not under cloud
// snapshots/TLS after GCC inlining into the loop owner's fixed 8KiB stack.
__attribute__((noinline)) void serviceStream() {
  const int packetSize = stream.parsePacket();
  if (packetSize < 10 || packetSize > 1300) return;
  uint8_t packet[1300];
  const int length = stream.read(packet, sizeof(packet));
  if (length > 0) applyStreamPacket(packet, static_cast<size_t>(length),
      (1ULL<<62)|(uint64_t(static_cast<uint32_t>(stream.remoteIP()))<<16)|stream.remotePort());
}


void serviceConsole() {
  while (Serial.available() != 0) {
    const char command = static_cast<char>(Serial.read());
    if (consoleLineMode) {
      if (command == '\r') continue;
      if (command != '\n' && consoleLine.length() < 110) {
        consoleLine += command;
        continue;
      }
      consoleLineMode = false;
      const int separator = consoleLine.indexOf('\t');
      const String ssid = separator < 0 ? consoleLine : consoleLine.substring(0, separator);
      const String password = separator < 0 ? String() : consoleLine.substring(separator + 1);
      const bool ok = networkConfig.setStation(ssid, password, true) &&
                      networkConfig.save();
      consoleLine = String();
      if (ok) applyNetworkConfig();
      Serial.printf("{\"diagnostic\":\"station_provision\",\"ok\":%s}\r\n",
                    ok ? "true" : "false");
      continue;
    }
    if (command == '@') {
      consoleLine = String();
      consoleLineMode = true;
      continue;
    }
    if (command == 'P') {
      uint8_t signature[3] = {};
      const bool ok = avrIsp.probe(signature);
      Serial.printf(
          "{\"diagnostic\":\"isp_probe\",\"ok\":%s,"
          "\"signature\":\"%02X%02X%02X\",\"error\":\"%s\"}\r\n",
          ok ? "true" : "false", signature[0], signature[1], signature[2],
          ok ? "" : avrIsp.lastError().c_str());
    } else if (command == 'H') {
      const bool sent = sendToAvr(kHello, sequence++);
      Serial.printf("{\"diagnostic\":\"uart_hello\",\"sent\":%s}\r\n",
                    sent ? "true" : "false");
    } else if (command == 'B') {
      const uint8_t payload[] = {0xE8, 0x03, 0x96, 0x00};
      sendToAvr(kBuzzer, sequence++, payload, sizeof(payload));
      Serial.println(F("{\"diagnostic\":\"buzzer_1000hz_150ms\",\"sent\":true}"));
    } else if (command == 'U') {
      const uint8_t payload[] = {kCueUpdateStart};
      const bool ok = requestAndWaitForAck(kAudioCue, payload, sizeof(payload));
      Serial.printf("{\"diagnostic\":\"update_cue\",\"ok\":%s}\r\n",
                    ok ? "true" : "false");
    } else if (command == 'D') {
      const int16_t value = 1234;
      const uint8_t payload[] = {static_cast<uint8_t>(value),
                                 static_cast<uint8_t>(value >> 8)};
      sendToAvr(kDisplayText, sequence++, payload, sizeof(payload));
      Serial.println(F("{\"diagnostic\":\"display_1234\",\"sent\":true}"));
    } else if (command == 'O') {
      const uint8_t payload[] = {0x01};
      sendToAvr(kOutputMask, sequence++, payload, sizeof(payload));
      Serial.println(F("{\"diagnostic\":\"output_1\",\"sent\":true}"));
    } else if (command == 'R') {
      sendToAvr(kGameReset, sequence++);
      Serial.println(F("{\"diagnostic\":\"game_reset\",\"sent\":true}"));
    } else if (command == 'L') {
      grid.setPattern(LedGrid::kPatternRainbow);
      Serial.printf(
          "{\"diagnostic\":\"led_rainbow\",\"brightness\":%u,\"enabled\":true}\r\n",
          grid.brightness());
    } else if (command == 'N') {
      grid.setPattern(LedGrid::kPatternNone);
      grid.resetWhite();
      Serial.println(F("{\"diagnostic\":\"led_rainbow\",\"enabled\":false}"));
    } else if (command == 'W') {
#if defined(ARDUINO_ARCH_ESP32)
      Serial.println(F("{\"diagnostic\":\"avr_tx_wire\",\"error\":\"Use gated ISP reset; direct reset diagnostic disabled on S3\"}"));
      continue;
#else
      pinMode(kAvrResetPin, OUTPUT_OPEN_DRAIN);
      digitalWrite(kAvrResetPin, LOW);
      delay(10);
      digitalWrite(kAvrResetPin, HIGH);
      pinMode(kAvrResetPin, INPUT);
#endif

      const uint32_t startedAt = millis();
      uint32_t nextYieldAt = startedAt + 10;
      uint32_t samples = 0;
      uint32_t lowSamples = 0;
      uint32_t transitions = 0;
      uint8_t previous = digitalRead(kAvrRxPin);
      while (static_cast<uint32_t>(millis() - startedAt) < 1500U) {
        const uint8_t current = digitalRead(kAvrRxPin);
        ++samples;
        if (current == LOW) ++lowSamples;
        if (current != previous) {
          ++transitions;
          previous = current;
        }
        if (static_cast<int32_t>(millis() - nextYieldAt) >= 0) {
          yield();
          nextYieldAt += 10;
        }
      }
      Serial.printf(
          "{\"diagnostic\":\"avr_tx_wire\",\"samples\":%lu,"
          "\"low_samples\":%lu,\"transitions\":%lu,\"final_level\":%u}\r\n",
          static_cast<unsigned long>(samples),
          static_cast<unsigned long>(lowSamples),
          static_cast<unsigned long>(transitions), previous);
    }
  }
}

#include "src/MaintenanceRoutes.h"
#include "src/ShellService.h"
#include "src/ShellTuiBinding.h"
#include "src/UrbootRoutes.h"
uint32_t runningImageSize() { return bootTelemetryImage.size; }
String runningImageMd5() { return bootTelemetryImage.md5; }
bool runningImageIntegrity() { return bootIntegrity; }
#include "src/ArtifactDownload.h"
#if defined(ARDUINO_ARCH_ESP32)
#include "src/BleRoutes.h"
#endif
}  // namespace

void setup() {
  uint8_t bootBytes[16];char bootHex[33];
  if(SystemPlatform::randomBytes(bootBytes,sizeof(bootBytes))==0) {
    for(unsigned i=0;i<16;++i)snprintf(bootHex+i*2,3,"%02x",bootBytes[i]);
    pcBootId=bootHex;
  } // No random identity means remote apply is unavailable, never a reused boot ID.
#if defined(ARDUINO_ARCH_ESP32)
  sntp_set_time_sync_notification_cb([](struct timeval *) {
    clockSource="ntp"; ntpObserved=true; lastClockSetAt=millis();
  });
#else
  settimeofday_cb([](bool sntp) {
    clockSource=sntp?"ntp":"manual";ntpObserved=sntp;lastClockSetAt=millis();
  });
#endif
#if PAKAR_USE_BUSY_WIRE
  // GPIO0 is pulled high by the NodeMCU boot circuit. Explicitly keep the AVR
  // blocked until UART, Wi-Fi, Web, and the LED renderer are ready.
  busyLine.begin();
#endif
  pinMode(kAvrResetPin, INPUT);
  // Never erase damaged storage and silently restore public credentials.
  const bool filesystemMounted = SystemPlatform::mountFilesystem();
  captureBootFilesystemTelemetry(filesystemMounted);
  loopMessages.begin(); // PSRAM-backed S3 queue, before any network callback.
  accessConfig.begin(LittleFS, filesystemMounted);
  networkConfig.begin(LittleFS);
#if defined(ARDUINO_ARCH_ESP32) && PAKAR_S3_ETHERNET_ENABLED
  ethernetConfig.begin(LittleFS);
#endif
  deviceIdentity.begin(LittleFS, filesystemMounted);
  cloudConfig.begin(LittleFS);
  // Initialize the driver without sending a white frame over a restored scene.
  grid.begin(false);
  loadLedPreferences();
  loadCellMapping();
  SceneState fallback;
  fallback.mode = BoardCore::GameRole::mode;
  fallback.brightness = grid.brightness();
  fallback.flags = (grid.outputEnabled() ? 1 : 0) | (grid.staticMode() ? 2 : 0);
  if (sceneStore.begin(LittleFS, filesystemMounted, fallback)) {
    const auto &saved = sceneStore.state();
    mode = static_cast<GameMode>(BoardCore::GameRole::mode); winner = saved.mode==BoardCore::GameRole::mode?saved.winner:0;
    selectedCell = saved.selectedCell; selectedState = saved.selectedState;
    restoreLedScene(grid, saved);
    if (mode == kModeSnake) startSnake();
  }
#if PAKAR_LED_START_CELL_CHASE
  grid.setStaticMode(true);
  grid.setPattern(LedGrid::kPatternCellChase);
#endif
  upgradeLifecycle.boot(millis());
  if (grid.outputEnabled()) grid.presentMaintenance(upgradeLifecycle,millis());
  else grid.setOutputEnabled(false); // Explicit complete black frame.
  hostFrameIngress.begin(onHostFrame,rejectHostFrame,revokeHostAdmission);
  hostLink.begin(115200, onHostFrameIngress);
#if defined(ARDUINO_ARCH_ESP32)
#if PAKAR_AVR_LINK_ENABLED
  avrUart.begin(onAvrFrame);
#else
  // Quiet commissioning: no UART driver, TX frame or pin drive while tracing
  // the observed AVR reset loop. Reset/ISP pins remain untouched too.
  pinMode(PAKAR_S3_AVR_RX, INPUT);
  pinMode(PAKAR_S3_AVR_TX, INPUT);
#endif
  PakarS3::StatusRgb::Configuration statusConfig;
#ifdef PAKAR_S3_STATUS_RGB_PIN
  statusConfig.pin = PAKAR_S3_STATUS_RGB_PIN;
  statusConfig.pinConfirmed = true; statusConfig.enabled = true;
#endif
  statusRgbConfig.begin(LittleFS, statusConfig);
  statusRgb.configure(statusRgbConfig.configuration(), millis(), writeStatusRgb);
  statusRgb.setState(PakarS3::StatusRgb::State::Boot, millis());
  lcdPort.home(deviceIdentity.hostname(),"PakarController",millis());
  lcdPort.begin(millis());
#else
  avrSerial.begin(kBaud);
#endif
#if !defined(ARDUINO_ARCH_ESP32)
  if (PAKAR_AVR_LINK_ENABLED) avrLink.begin(kBaud, onAvrFrame);
#endif

  // This is deliberately stable. Chip identity belongs in discovery metadata,
  // not in the human-facing host or recovery AP name.
  hostname = deviceIdentity.hostname();
  apStarted = networkConfig.configure(hostname.c_str(), kDevelopmentPassword);
  applyEthernetConfig();
  NetworkPath::service();
  uint8_t stationMac[6] = {};
#if defined(ARDUINO_ARCH_ESP32)
  // WiFi.macAddress() can be all-zero on the S3's first AP-only boot. Hardware
  // identity must exist before STA is enabled, not change when mode changes.
  esp_efuse_mac_get_default(stationMac);
#else
  WiFi.macAddress(stationMac);
#endif
  bool macNonzero = false, macBroadcast = true;
  for (uint8_t byte : stationMac) { macNonzero |= byte != 0; macBroadcast &= byte == 255; }
  if (macNonzero && !macBroadcast && !(stationMac[0] & 1)) {
    char board[21];
    snprintf(board, sizeof(board), "%s-%02x%02x%02x%02x%02x%02x", SystemPlatform::architecture(),
        stationMac[0], stationMac[1], stationMac[2], stationMac[3], stationMac[4], stationMac[5]);
    pcBoardId = board;
  }
  webAccess.configurePeerFence(pcBoardId, pcBootId,hostname);
#if defined(ARDUINO_ARCH_ESP32)
  cloudServices.begin(hostname, handleCloudCommand, buildCloudStatusJson,buildTelemetryJson,pcBoardId,pcBootId);
#else
  cloudServices.begin(hostname, handleCloudCommand, buildStatusJson,buildTelemetryJson,pcBoardId,pcBootId);
#endif
  cloudServices.setProgressService([](){
    serviceHostLink(); // Capture/queue only while cloud IO owns its stack.
#if defined(ARDUINO_ARCH_ESP32)
    // Same loop owner; advance animation between cloud writes, without nested
    // network service, UART commands, storage scans or a duplicate FX engine.
    const uint32_t at=millis();
    PakarLed::serviceDuringCloud(grid,at,otaActive||otaRebootAt||remoteFirmware.busy(),
        upgradeLifecycle.visual(at)!=PakarUpdate::Visual::None,
        PakarLed::uartWindowReady(PAKAR_UART_BUFFERED,at,ledResumeAt));
#endif
  });
  stream.begin(kStreamPort);
  beginDeviceShells();

  registerApi();
  registerLcdApi();
  registerMaintenanceRoutes();
  registerShellApi();
  registerUrbootRoutes();
  registerArtifactDownload();
#if defined(ARDUINO_ARCH_ESP32)
  bleService.begin(LittleFS,filesystemMounted,hostname,bleRequest);
  serviceBle();
  registerBleApi();
#endif
  webAccess.registerRoutes(server.native(), []() {
    webSocket.closeAll(1008, "Sign in again");
    closeDeviceShells();
  }, []() { return urbootBusy.get() || remoteFirmware.busy() || otaActive || otaRebootAt || avrUploadOwner || server.processing() || programmingLatched; });
  webSocket.onEvent(onWebSocketEvent);
  server.native().addHandler(&webSocket);
  registerAsyncOta();
  server.begin();
  grid.setPanelObserver(observePanelAction);
  serviceDiscovery();
  sendToAvr(kHello, sequence++);
  upgradeLifecycle.boot(millis());
#if PAKAR_USE_BUSY_WIRE
  setAvrTransmitBlocked(false);
#endif
}

namespace {
// Keep the bounded 1300-byte message workspace out of the loop owner's stack
// while cloud telemetry builds its own nested snapshots. This separate call
// completes before cloud service; no message or command is moved to a new task.
__attribute__((noinline)) void serviceQueuedWebSocketMessage() {
#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
  loopMessages.consume([](const LoopMessages::Message &message){
    if(message.opcode==0xfe)grid.discardStaged((1ULL<<63)|message.client);
    else if(message.epoch==server.ingressEpoch()&&webAccess.socketAllowed(message.client))
      handleWebSocketMessage(message.client,message.opcode,message.data,message.length);
  });
#else
  static LoopMessages::Message message; // Never add a 4KB command to the loop stack.
  if(loopMessages.pop(message)){
    if(message.opcode==0xfe) grid.discardStaged((1ULL << 63) | message.client);
    else if(message.epoch==server.ingressEpoch() && webAccess.socketAllowed(message.client))
      handleWebSocketMessage(message.client,message.opcode,message.data,message.length);
  }
#endif
}
}

void loop() {
#if defined(ARDUINO_ARCH_ESP32)
  serviceBle();
#endif
  // A queued job executes outside any request handler. Its bounded background
  // callback services reads, but never framed AVR bytes during a raw session.
  if(urbootQueued) {serviceUpgrade();serviceUrbootJob();serviceUpgrade();return;}
#if defined(ARDUINO_ARCH_ESP32) && !PAKAR_AVR_LINK_ENABLED
  passiveAvrRx.service(millis());
  passiveCaptureGuard.store(passiveAvrRx.snapshot().active,std::memory_order_release);
#endif
  servicePcContracts();
  const uint32_t now = millis();
  if (NetworkPath::service()) {
    cloudServices.suspend();
    ntpConfigured = false;
  }
  avrLink.service();
  serviceHostLink();
  serviceUpgrade();
  // Render before issuing UART requests. The AVR is request/response only, so
  // its reply arrives while FastLED is not masking ESP8266 interrupts.
  const uint32_t ledNow=millis(); // AVR observation/cloud callbacks may have taken time.
  const bool ledFrameDue = upgradeLifecycle.visual(ledNow)==PakarUpdate::Visual::None && !otaActive && !remoteFirmware.busy() &&
      PakarLed::uartWindowReady(PAKAR_UART_BUFFERED,ledNow,ledResumeAt) &&
      grid.needsRender(ledNow);
  if (ledFrameDue) {
    setAvrTransmitBlocked(true);
    // The busy edge prevents a new unsolicited AVR frame, but one may have
    // passed the AVR's LOW check just before this edge and still be draining
    // from its hardware UART. Consume that bounded frame before FastLED masks
    // ESP8266 interrupts for the 480-pixel transfer.
#if !defined(ARDUINO_ARCH_ESP32)
    serviceAvrFor(8U);
#endif
    grid.service(ledNow);
    setAvrTransmitBlocked(false);
  }
  server.service();
  serviceQueuedWebSocketMessage();
  serviceAttentionCues();
  serviceUpgrade();
  if (remoteFirmware.busy()) {
    if(remoteFirmware.needsDownloadConnection())
      NetworkPath::serviceDns(millis(),true);
    // Retain existing Internet control/monitoring while staging, but never
    // initiate another TLS handshake during a flash/ISP transaction.
    cloudServices.service(NetworkPath::ready(),time(nullptr)>1700000000,now,false,false);
    setAvrTransmitBlocked(true);serviceAvrFor(18);
    if(!remoteCueLead || strcmp(remoteFirmware.phase(),"prepare") ||
       (static_cast<int32_t>(millis()-remoteCueUntil)>=0 && upgradeRebootReady())) {
      remoteCueLead=false;remoteFirmware.service();
    }
    setAvrTransmitBlocked(false);
  }
  webSocket.cleanupClients(8);
  webAccess.expireSockets(webSocket);
  serviceAttentionCues();
  if (otaRebootAt && static_cast<int32_t>(millis() - otaRebootAt) >= 0 && upgradeRebootReady()) ESP.restart();
  if (!uploadFile) serviceDeviceShells();
  if (!remoteFirmware.busy() && !otaActive && !otaRebootAt && networkApplyPending &&
      static_cast<int32_t>(millis() - networkApplyAt) >= 0) {
    networkApplyPending = false;
    if(ethernetApplyOnly) {ethernetApplyOnly=false;applyEthernetConfig();}
    else applyNetworkConfig();
  }
  if (!remoteFirmware.busy() && !otaActive && !otaRebootAt && cloudApplyPending &&
      static_cast<int32_t>(millis() - cloudApplyAt) >= 0) {
    cloudApplyPending = false;
    cloudServices.applyConfiguration();
  }
  serviceDiscovery();
  configureNtp();
  if (wifiScanQueued && !remoteFirmware.busy() && !otaActive && !networkApplyPending) {
    wifiScanQueued = false;
    if (WiFi.scanComplete() != -1) WiFi.scanNetworks(true, true);
  }
  if (avrOnline && (avrCapabilities & kCapEspPanel) && !programmingLatched &&
      !remoteFirmware.busy() && !otaActive && static_cast<uint32_t>(now-lastEspPanelOfferAt)>=2000U) {
    lastEspPanelOfferAt = now;
    espFrontPanel.offer(espPanelSettings, avrLink, sequence++);
  }
  // TLS reconnects can take seconds on the ESP8266. Keep them out of the
  // timing-critical ISP/update transaction. After a failed transaction, cloud
  // diagnostics can reconnect even with Prog armed; mutations remain guarded.
  if (!remoteFirmware.busy() && !otaActive && !otaRebootAt
#if defined(ARDUINO_ARCH_ESP32) && !PAKAR_AVR_LINK_ENABLED
      && !passiveAvrRx.snapshot().active
#endif
      ) {
    setAvrTransmitBlocked(true);
#if !defined(ARDUINO_ARCH_ESP32)
    serviceAvrFor(18);
#endif
    cloudServices.service(NetworkPath::ready(),
                          time(nullptr) > 1700000000, now);
    setAvrTransmitBlocked(false);
  }
  if (!remoteFirmware.busy() && !otaActive && !otaRebootAt && upgradeLifecycle.visual(millis())==PakarUpdate::Visual::None) { serviceSnake(now);serviceStream(); }
  serviceScenePersistence();
  ntfyService.service(NetworkPath::ready(),time(nullptr)>1700000000,
      remoteFirmware.busy() || otaActive || otaRebootAt || urbootBusy.get() || avrUploadOwner || urbootRawOwned,
      deviceIdentity.hostname(),millis());
  // Cloud socket calls may have waited while the buffered AVR UART received
  // valid replies. Consume those actual frames before evaluating presence;
  // otherwise the loop reports a false peer loss, then recovery next pass.
  // This is AFTER cloud IO, never reentrant from its progress callback.
  avrLink.service();
  if (!remoteFirmware.busy() && avrOnline && avrVersionKnown &&
      (avrVersion[0]>0 || avrVersion[1]>2 || (avrVersion[1]==2 && avrVersion[2]>=7)) &&
      static_cast<uint32_t>(now-lastTelemetryRequestAt)>=5000U) {
    lastTelemetryRequestAt=now;sendToAvr(kGetTelemetry,sequence++);
  }
  const uint32_t afterServices = millis();
  if (avrOnline &&
      static_cast<uint32_t>(afterServices - lastAvrFrameAt) > 3000U) {
    avrOnline = false;
    avrVersionKnown = false;
    avrPcSupported = false;
  }
  if (!avrOnline && static_cast<uint32_t>(now - lastHelloAt) > 1000) {
    lastHelloAt = now;
    sendToAvr(kHello, sequence++);
  }
  if ((PAKAR_USE_BUSY_WIRE
#if defined(PAKAR_UART_BUFFERED)
      || PAKAR_UART_BUFFERED
#endif
      ) && avrOnline &&
      static_cast<uint32_t>(now - lastAvrPingAt) >= 1000U) {
    lastAvrPingAt = now;
    sendToAvr(kPing, sequence++); // Presence heartbeat, not state polling.
  }
  // AVR owns its welcome melody. A first HELLO is not a completed update:
  // sending the success cue here could replace the still-playing welcome.
#if !defined(PAKAR_UART_BUFFERED) || !PAKAR_UART_BUFFERED
  if (!PAKAR_USE_BUSY_WIRE && avrOnline &&
      static_cast<uint32_t>(now - lastStatusPollAt) >= 100U) {
    lastStatusPollAt = now;
    sendToAvr(kGetStatus, sequence++);
  }
#endif
  if (static_cast<uint32_t>(now - lastWebSocketBroadcastAt) >= 500U) {
    lastWebSocketBroadcastAt = now;
    if (webSocket.count() != 0) {
      String status = buildStatusJson();
      webSocket.textAll(status);
    }
  }
  // Coalesced, finite update presentation, never from the async flash writer.
  // An ISP target held in RESET cannot run its own display; Prog keeps priority.
  const auto update=otaProgress.snapshot();
  char updateText[5];PakarUpdate::panelText(update,updateText);
  if(remoteFirmware.busy()) {
    const auto stage=remoteFirmware.progress();
    if(stage.known())snprintf(updateText,sizeof(updateText),"P%03u",stage.percent());
    else snprintf(updateText,sizeof(updateText),"UPdt");
  }
  const bool showUpdate=otaActive || otaRebootAt || remoteFirmware.busy();
  serviceSemanticPanel(millis());
  // Bounded latest-display cloud feed, outside AVR receive/renderer ownership.
  // Local WS/USB retain immediate raw events. Do not flush stale animation
  // windows after a blocked link; one newest snapshot is sufficient.
  if(!urbootRawOwned && !otaActive && !otaRebootAt && !remoteFirmware.busy() &&
      pendingDisplayCloud.length() && uint32_t(millis()-lastDisplayCloudAt)>=1000U) {
    lastDisplayCloudAt=millis();String latest=std::move(pendingDisplayCloud);pendingDisplayCloud=String();
    cloudServices.publishEvent(latest);
  }
  // Single loop-owner I2C writer, independent of AVR TM1637 and LED RMT/DMA.
#if defined(ARDUINO_ARCH_ESP32)
  char lcdLine[17];
  if(showUpdate) {
    const auto stage=remoteFirmware.busy()?remoteFirmware.progress():update;
    if(stage.known())snprintf(lcdLine,sizeof(lcdLine),"Update %3u%%",stage.percent());
    else snprintf(lcdLine,sizeof(lcdLine),"Update %.9s",remoteFirmware.busy()?remoteFirmware.phase():PakarUpdate::name(update.phase));
  }
  else snprintf(lcdLine,sizeof(lcdLine),"Cell %u %s",selectedCell,cellName(grid.state(selectedCell)));
  lcdPort.home(deviceIdentity.hostname(),lcdLine,millis());
  lcdPort.service(millis());
#endif
  yield();
}
