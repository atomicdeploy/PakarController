#include <Arduino.h>
#include <ArduinoJson.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <Update.h>
#include <firmware/esp32s3/src/BoardPins.h>
#include <firmware/esp32s3/src/Discovery.h>
#include <firmware/esp32s3/src/DnsRecovery.h>
#include <firmware/esp32s3/src/StatusRgb.h>
#include <firmware/esp32s3/src/StatusRgbConfig.h>
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
#include "src/EspFrontPanel.h"

#include <BuildInfo.h>
#include <AudioCues.h>
#include <Protocol.h>
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
#include "src/LedGrid.h"
#include "src/LoopMessages.h"
#include "src/LedPreferences.h"
#include "src/NetworkConfig.h"
#include "src/DeviceIdentity.h"
#include "src/ApplicationIdentity.h"
#include "src/BusyLine.h"
#include "src/TelnetShell.h"
#include "src/WebUi.h"
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
#if !PAKAR_AVR_LINK_ENABLED
PakarS3::PassiveAvrRx passiveAvrRx;
std::atomic<bool> passiveCaptureGuard{false};
#endif
bool writeStatusRgb(int pin, uint8_t red, uint8_t green, uint8_t blue) {
  rgbLedWrite(pin, red, green, blue); return true;
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
WiFiClient telnetClient;
#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial avrSerial(1);
#else
SoftwareSerial avrSerial(D4, D0);
#endif
BoardCore::SerialLink link(static_cast<Stream &>(avrSerial));
BoardCore::SerialLink &avrLink = link; // Unambiguous outside namespace vs POSIX link().
#if defined(ARDUINO_ARCH_ESP32) && !defined(PAKAR_S3_HOST_NATIVE_USB)
// CH343/UART socket remains a real independent PC path on UART0; UART1 is AVR.
BoardCore::SerialLink hostLink(Serial0);
#else
BoardCore::SerialLink hostLink(Serial);
#endif
AvrIsp avrIsp(kAvrResetPin);
RemoteFirmware remoteFirmware;
unsigned artifactReaders=0;
BusyLine<PAKAR_USE_BUSY_WIRE != 0> busyLine(kBusyPin);
LedGrid grid;
LoopMessages loopMessages;
File uploadFile;
uint32_t avrUploadOwner = 0;
String hostname;
bool avrOnline = false;
bool avrVersionKnown = false;
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
GameMode mode = kModeCellSelect;
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
uint32_t lastWebSocketBroadcastAt = 0;
bool ntpConfigured = false;
bool ntpObserved = false;
uint32_t lastClockSetAt = 0;
const char *clockSource = "unset";
uint8_t avrTelemetry[46] = {};
uint32_t avrTelemetryAt = 0, lastTelemetryRequestAt = 0;
bool avrTelemetryKnown = false;
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
uint8_t snakeBody[LedGrid::kCells] = {};
uint8_t snakeLength = 0;
uint8_t snakeFood = 0;
uint8_t snakeDirection = 1;
uint32_t lastSnakeAt = 0;
bool snakeAlive = false;
bool telnetAuthenticated = false;
bool otaActive = false;
bool otaStartCuePending = false;
bool otaSuccessCuePending = false;
bool otaErrorCuePending = false;
uint32_t otaRebootAt = 0;
size_t otaUploadedBytes = 0;
uint32_t otaOwner = 0;
String otaPhase = "idle";
PakarShell::TelnetParser telnetParser;
PakarShell::LineEditor telnetEditor;
uint32_t telnetLastInputAt = 0;
CellMappingRecord cellMapping{};

void saveLedPreferences();
void loadCellMapping();
bool scheduleDeviceRename(const String &name);
bool scheduleDeviceIdentity(const String &name, const String &displayName);
uint32_t identityRevision();
String applicationIdentityJson();
void appendJsonEscaped(String &body, const char *text);
String deviceNameJson(bool accepted = false);
bool saveCellMapping();
String buildStatusJson();
String buildTelemetryJson();
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
void serviceTelnet();
void serviceDiscovery();
void registerAsyncOta();
bool sendToAvr(uint8_t opcode, uint8_t requestSequence,
               const uint8_t *payload = nullptr, uint8_t payloadLength = 0);
String pcReceiptJson(const BoardCore::PcContractTransport::Receipt &receipt);
void publishPcEvent(const String &event) {
  webSocket.textAll(event);
  cloudServices.publishEvent(event);
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
    if (!deviceIdentity.ready() || strlen(deviceIdentity.hostname()) > 32) return false;
    state.mode = (networkConfig.accessPointEnabled() ? 1 : 0) | (networkConfig.stationEnabled() ? 2 : 0);
    state.dhcp = networkConfig.dhcpEnabled();
    state.stationConfigured = strlen(networkConfig.stationSsid()) != 0;
    state.stationConnected = networkConfig.stationConnected();
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
  static bool busy() { return remoteFirmware.busy() || otaActive || otaRebootAt || avrUploadOwner || programmingLatched || networkApplyPending; }
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
#if defined(ARDUINO_ARCH_ESP32) && !PAKAR_S3_ISP_ELECTRICAL_VERIFIED
  available &= ~BoardCore::Context::Isp;
#endif
  return available;
}

bool contextAllows(uint8_t opcode, const uint8_t *payload, uint8_t length) {
  using namespace BoardCore::Context;
  const uint16_t available = currentFeatures();
  switch (opcode) {
    case kSetMode: return length == 1 && payload[0] < 4 && (modes(available) & (1U << payload[0]));
    case kCellAction: case kGameReset: return available & Table;
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
      static_cast<uint8_t>(tm1637Ack), static_cast<uint8_t>(programmingLatched),
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
  };
  hostLink.send(kStatusResponse, requestSequence, payload, sizeof(payload));
}

void onHostFrame(const BoardCore::Protocol::Frame &frame, void *) {
  if (!contextAllows(frame.opcode, frame.payload, frame.payloadLength)) {
    const uint8_t error[] = {frame.opcode, kHardwareUnavailable};
    hostLink.send(kErrorResponse, frame.sequence, error, sizeof(error)); return;
  }
  static BoardCore::PcContractAssembly usbPcAssembly;
  if ((remoteFirmware.busy() || otaActive || otaRebootAt || avrUploadOwner) && frame.opcode != kGetStatus && frame.opcode != kGetIdentity && frame.opcode != kHello && frame.opcode != kPing) {
    hostLink.error(frame.sequence, frame.opcode, kBusy); return;
  }
#if defined(ARDUINO_ARCH_ESP32) && !PAKAR_AVR_LINK_ENABLED
  if (passiveAvrRx.snapshot().active && frame.opcode!=kAvrRxDiagnostic &&
      frame.opcode!=kGetStatus && frame.opcode!=kGetIdentity && frame.opcode!=kHello && frame.opcode!=kPing) {
    hostLink.error(frame.sequence,frame.opcode,kBusy);return;
  }
#endif
  switch (frame.opcode) {
    case kGetIdentity: {
      if (frame.payloadLength != 3) { hostLink.error(frame.sequence, frame.opcode, kBadPayload); break; }
      String json = F("{\"revision\":"); json += identityRevision();
      json += F(",\"hostname\":\""); json += deviceIdentity.hostname();
      json += F("\",\"display_name\":\""); appendJsonEscaped(json, deviceIdentity.displayName());
      json += F("\",\"application\":"); json += applicationIdentityJson(); json += '}';
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
        if (telnetClient) telnetClient.stop();
        telnetAuthenticated = false;
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
      const uint8_t payload[] = {
          0, kRoleEsp, // Reserved zero, not a protocol generation.
          static_cast<uint8_t>(capabilities),
          static_cast<uint8_t>(capabilities >> 8), LedGrid::kCells, 9,
          BoardCore::BuildInfo::kVersionMajor,
          BoardCore::BuildInfo::kVersionMinor,
          BoardCore::BuildInfo::kVersionPatch,
      };
      hostLink.send(kHelloResponse, frame.sequence, payload, sizeof(payload));
      break;
    }
    case kGetStatus:
      sendHostStatus(frame.sequence);
      break;
    case kSetMode:
      if (frame.payloadLength != 1 || frame.payload[0] > kModeSnake) {
        hostLink.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      mode = static_cast<GameMode>(frame.payload[0]);
      winner = 0;
      if (mode == kModeSnake) startSnake();
      else grid.resetWhite();
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
      selectedCell = frame.payload[0];
      selectedState = frame.payload[1];
      grid.setCell(selectedCell, static_cast<CellState>(selectedState),
                   frame.payloadLength == 3
                       ? static_cast<Effect>(frame.payload[2])
                       : kEffectFade);
      sendToAvr(kCellAction, sequence++, frame.payload, frame.payloadLength);
      hostLink.ack(frame.sequence, frame.opcode);
      break;
    case kGameReset:
      winner = 0;
      if (mode == kModeSnake) startSnake();
      else grid.resetWhite();
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
    case kSetLedOutput:
      if(frame.payloadLength!=1 || frame.payload[0]>1){hostLink.error(frame.sequence,frame.opcode,kBadPayload);break;}
      grid.setOutputEnabled(frame.payload[0]);hostLink.ack(frame.sequence,frame.opcode);break;
    case kSetLedPattern:
      if (frame.payloadLength != 1 || frame.payload[0] > LedGrid::kPatternFire) {
        hostLink.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      grid.setPattern(static_cast<LedGrid::Pattern>(frame.payload[0]));
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
  const uint8_t index = logicalCell - 1;
  const uint8_t row = index / 10;
  const uint8_t fromLeft = index % 10;
  const uint8_t fromRight = 9 - fromLeft;
  return static_cast<uint8_t>(fromRight * 8 +
                              ((fromRight & 1U) == 0 ? 8 - row : row + 1));
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
  const LedPreferences value = whiteBootPreferences(grid.brightness());
  // Like PCController SettingsStore's changed-byte writes, avoid touching flash
  // when only a transient cell/effect changed. LittleFS replaces EEPROM here.
  File existing = LittleFS.open(kLedPreferencesPath, "r");
  if (existing) {
    LedPreferences saved{};
    const bool same = existing.size() == sizeof(saved) &&
        existing.read(reinterpret_cast<uint8_t *>(&saved), sizeof(saved)) == sizeof(saved) &&
        memcmp(&saved, &value, sizeof(value)) == 0;
    existing.close();
    if (same) return;
  }
  File file = LittleFS.open(kLedPreferencesPath, "w");
  if (file) {
    file.write(reinterpret_cast<const uint8_t *>(&value), sizeof(value));
    file.close();
  }
}

void loadLedPreferences() {
  File file = LittleFS.open(kLedPreferencesPath, "r");
  if (!file) return;
  LedPreferences value{};
  const bool complete = file.read(reinterpret_cast<uint8_t *>(&value),
                                  sizeof(value)) == sizeof(value);
  file.close();
  if (complete) applyLedBootPreferences(grid, value);
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
  if (!networkConfig.stationConnected() && !apStarted) return;
  discoveryAddress = networkConfig.stationConnected() ? WiFi.localIP() : WiFi.softAPIP();
  s3Discovery.begin(hostname.c_str(), deviceIdentity.displayName(), SystemPlatform::chipId(), kStreamPort);
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
    MDNS.addService("pakar-stream", "udp", kStreamPort);
    MDNS.addService("telnet", "tcp", 23);
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
  const bool connected = networkConfig.stationConnected() || apStarted;
  const IPAddress address = networkConfig.stationConnected() ? WiFi.localIP() : apStarted ? WiFi.softAPIP() : IPAddress();
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
#else
  if (mdnsStarted) MDNS.update();
  mndpService.service(hostname.c_str(),pcBoardId.c_str());
#endif
}

void applyNetworkConfig() {
  if (hostname != deviceIdentity.hostname()) {
    cloudServices.renameDevice(deviceIdentity.hostname());
    hostname = deviceIdentity.hostname();
    webSocket.closeAll(1012, "Device name changed");
    if (telnetClient) telnetClient.stop();
    telnetAuthenticated = false;
  }
  stopDiscovery();
  apStarted = networkConfig.configure(hostname.c_str(), kDevelopmentPassword);
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
  body += F(",\"accepted\":"); body += accepted ? F("true") : F("false");
  body += F(",\"apply_pending\":"); body += networkApplyPending ? F("true") : F("false");
  body += F(",\"apply_after_ms\":"); body += networkApplyPending ? 1000 : 0;
  body += F(",\"error\":\""); appendJsonEscaped(body, deviceIdentity.error().c_str()); body += F("\"}");
  return body;
}

bool sendToAvr(uint8_t opcode, uint8_t requestSequence,
               const uint8_t *payload, uint8_t payloadLength) {
  if (!PAKAR_AVR_LINK_ENABLED) return false;
  const bool sent = link.send(opcode, requestSequence, payload, payloadLength);
  if (sent) {
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
  if (remoteFirmware.busy() || otaActive || otaRebootAt || avrUploadOwner) return;
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
  if (!sent && !request->usb) publishPcEvent(pcReceiptJson(*request));
}

void serviceAvrFor(uint32_t durationMs) {
  const uint32_t startedAt = millis();
  do {
    link.service();
    delay(1);
    yield();
  } while (static_cast<uint32_t>(millis() - startedAt) < durationMs);
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

bool playUpdateStartCue() {
  const uint8_t cue[] = {kCueUpdateStart};
  if (requestAndWaitForAck(kAudioCue, cue, sizeof(cue))) {
    serviceAvrFor(520);
    return true;
  }
  const uint8_t legacyCue[] = {0x10, 0x03, 0x2C, 0x01};  // 784 Hz, 300 ms
  const bool ok = requestAndWaitForAck(kBuzzer, legacyCue, sizeof(legacyCue));
  if (ok) serviceAvrFor(350);
  return ok;
}

void playUpdateSuccessCue() {
  const uint8_t cue[] = {kCueUpdateSuccess};
  if (requestAndWaitForAck(kAudioCue, cue, sizeof(cue))) {
    serviceAvrFor(350);
    return;
  }
  const uint8_t legacyCue[] = {0x20, 0x06, 0xDC, 0x00};  // 1568 Hz, 220 ms
  if (requestAndWaitForAck(kBuzzer, legacyCue, sizeof(legacyCue))) {
    serviceAvrFor(250);
  }
}

bool authenticate() {
  if ((remoteFirmware.busy() || artifactReaders) && server.request() &&
      !server.request()->url().startsWith("/api/firmware/") &&
      server.request()->url()!=F("/file/cancel") &&
      (server.request()->method()!=HTTP_GET || server.request()->url()==F("/api/avr/probe"))) {
    server.send(409,"text/plain","Remote firmware operation owns board maintenance"); return false;
  }
  if (otaActive || otaRebootAt) { server.send(503, "text/plain", "ESP update in progress"); return false; }
  if (avrUploadOwner && server.request() && server.request()->method() != HTTP_GET) {
    server.send(409, "text/plain", "AVR image upload in progress"); return false;
  }
  if (server.request() && webAccess.allowed(server.request())) {
    // Middleware admits async requests before loop dispatch. Recheck ownership
    // at the hardware boundary as well, not only when entering the queue.
    if (!webAccess.peerAllowed(server.request())) {
      server.send(409, "application/json", "{\"error\":\"peer_changed\",\"accepted\":false}"); return false;
    }
    const String path = server.request()->url();
#if defined(ARDUINO_ARCH_ESP32) && !PAKAR_AVR_LINK_ENABLED
    if (passiveAvrRx.snapshot().active && (server.request()->method()!=HTTP_GET ||
        path==F("/api/wifi/scan") || path==F("/hostname/check") || path==F("/api/avr/probe"))) {
      server.send(409,"text/plain","Bounded passive UART capture owns diagnostics");return false;
    }
#endif
    using namespace BoardCore::Context;
    uint16_t required = 0;
    if (path == F("/api/cell") || path == F("/api/cells/mapping") ||
        path == F("/api/game/reset") || path == F("/api/snake/direction")) required = Table;
    if (path == F("/api/display")) required = server.hasArg("text") ? Text : Display;
    if (path == F("/api/audio") || path == F("/api/buzzer")) required = Buzzer;
    if (path == F("/api/outputs")) required = Outputs;
    bool allowed = !required || (currentFeatures() & required);
    if (path == F("/api/mode")) {
      allowed = false;
      for (uint8_t code = 0; code < 4; ++code)
        if (server.arg("value") == BoardCore::Context::modeName(code) && (modes(currentFeatures()) & (1U << code))) allowed = true;
    }
    if (!allowed) { server.send(409, "application/json", "{\"error\":\"capability_not_acknowledged\"}"); return false; }
    return true;
  }
  server.send(401, "application/json", "{\"error\":\"login_required\"}");
  return false;
}

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

String buildStatusJson() {
  String body;
  // Reserve the complete document before TLS/WebSocket activity fragments the
  // ESP8266 heap. Arduino String otherwise fails mid-JSON without an error.
  body.reserve(2600);
  body += F("{\"version\":\""); body += BoardCore::BuildInfo::kVersion;
  body += F("\",\"board_id\":\""); body += pcBoardId;
  body += F("\",\"boot_id\":\""); body += pcBootId;
  body += F("\",\"http_peer_fencing\":"); body += webAccess.peerFenceReady() ? F("true") : F("false");
  body += F(",\"hostname\":\""); body += hostname;
  body += F("\",\"ip\":\""); body += (networkConfig.stationConnected() ? WiFi.localIP() : WiFi.softAPIP()).toString();
  body += F("\",\"mode\":\""); body += modeName();
  body += F("\",\"led_output\":");body+=grid.outputJson();
  body += F(",\"ws_queue_rejected\":");body+=loopMessages.rejected();
  body += F(",\"avr_online\":"); body += avrOnline ? F("true") : F("false");
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
  const auto &rgb = statusRgb.snapshot();
  body += F(",\"status_rgb\":{\"available\":"); body += rgb.available ? F("true") : F("false");
  body += F(",\"enabled\":"); body += rgb.enabled ? F("true") : F("false");
  body += F(",\"have_output\":"); body += rgb.haveOutput ? F("true") : F("false");
  body += F(",\"pin\":"); body += rgb.pin;
  body += F(",\"rgb\":["); body += rgb.red; body += ','; body += rgb.green; body += ','; body += rgb.blue;
  body += F("],\"brightness\":"); body += rgb.brightness;
  body += F(",\"state\":\""); body += PakarS3::StatusRgb::name(rgb.condition);
  body += F("\",\"revision\":"); body += rgb.revision;
  body += F(",\"updated_uptime_ms\":"); body += rgb.updatedAt; body += '}';
  body += F(",\"isp_available\":"); body += PAKAR_S3_ISP_ELECTRICAL_VERIFIED ? F("true") : F("false");
#else
  body += mndpService.running() ? F("true") : F("false");
#endif
  body += F(",\"netbios_started\":"); body += netbiosStarted ? F("true") : F("false");
  body += F(",\"canonical_url\":\"http://"); body += hostname; body += F(".local\"");
  body += F(",\"mdns_url\":\"http://"); body += hostname; body += F(".local\"");
  body += F(",\"winner\":"); body += winner;
  body += F(",\"selected_cell\":"); body += selectedCell;
  body += F(",\"snake_alive\":"); body += snakeAlive ? F("true") : F("false");
  body += F(",\"snake_length\":"); body += snakeLength;
  body += F(",\"brightness\":"); body += grid.brightness();
  body += F(",\"effect\":\""); body += grid.patternName(); body += '"';
  body += F(",\"esp_uart_errors\":"); body += link.errors();
  body += F(",\"avr_uart_errors\":"); body += avrUartErrors;
  body += F(",\"tm1637_ack\":"); body += tm1637Ack ? F("true") : F("false");
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
  body += (telnetClient && telnetClient.connected()) ? F("true") : F("false");
  body += F(",\"mqtt_enabled\":");
  body += cloudConfig.mqttEnabled() ? F("true") : F("false");
  body += F(",\"mqtt_connected\":");
  body += cloudServices.mqttConnected() ? F("true") : F("false");
  body += F(",\"mqtt_state\":"); body += cloudServices.mqttState();
  body += F(",\"mqtt_messages_received\":");
  body += cloudServices.mqttMessagesReceived();
  body += F(",\"cloud_websocket_enabled\":");
  body += cloudConfig.webSocketEnabled() ? F("true") : F("false");
  body += F(",\"cloud_websocket_connected\":");
  body += cloudServices.webSocketConnected() ? F("true") : F("false");
  body += F(",\"cloud_websocket_messages_received\":");
  body += cloudServices.webSocketMessagesReceived();
  String cloudError = cloudServices.lastError();
  cloudError.replace("\\", "\\\\");
  cloudError.replace("\"", "\\\"");
  body += F(",\"cloud_error\":\""); body += cloudError; body += '"';
  body += F(",\"uart_bytes_received\":"); body += link.bytesReceived();
  body += F(",\"uart_frames_received\":"); body += link.framesReceived();
  body += F(",\"usb_serial_bytes_received\":"); body += hostLink.bytesReceived();
  body += F(",\"usb_serial_frames_received\":"); body += hostLink.framesReceived();
  body += F(",\"usb_serial_errors\":"); body += hostLink.errors();
  body += F(",\"pc_contract_supported\":");
  body += avrOnline && avrPcSupported ? F("true") : F("false");
  body += F(",\"pc_contract_features\":");
  if (avrOnline && avrPcSupported) body += avrPcFeatures;
  else body += F("null");
  body += F(",\"capabilities\":{\"board_kind\":\"");
  body += PAKAR_TABLE_ENABLED ? F("table") : F("controller");
  body += F("\",\"name\":\""); appendJsonEscaped(body, deviceIdentity.displayName());
  body += F("\",\"hostname\":\""); body += deviceIdentity.hostname();
  body += F("\",\"application\":"); body += applicationIdentityJson();
  body += F(",\"acknowledged_features\":[");
  const uint16_t available = currentFeatures();
  bool separator = false;
  for (uint8_t bit = 0; bit < 14; ++bit) if (available & (1U << bit)) {
    if (separator) body += ',';
    body += '"'; body += BoardCore::Context::name(bit); body += '"'; separator = true;
  }
  body += F("],\"modes\":["); separator = false;
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
  body += F("]}");
  return body;
}

void sendStatusJson() {
  server.send(200, "application/json", buildStatusJson());
}

void configureNtp() {
#if defined(ARDUINO_ARCH_ESP32)
  PakarS3::serviceDhcpDns(millis(),networkConfig.dhcpEnabled(),!ntpConfigured);
#endif
  if (!networkConfig.stationConnected() || ntpConfigured) return;
  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
  ntpConfigured = true;
}

bool applyStreamPacket(const uint8_t *packet, size_t length,uint64_t owner=0) {
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

void handleWebSocketMessage(uint32_t clientId, uint8_t opcode,
                           const uint8_t *payload, size_t length) {
  // Only an immutable ID crosses the async -> loop boundary. SDK lookup/use
  // must remain under its own client-list lock; client(id) returns no lease.
  if (!webAccess.socketAllowed(clientId)) { webSocket.close(clientId, 1008, "Sign in again"); return; }
  if(passiveCaptureActive()) {
    webSocket.text(clientId, "{\"accepted\":false,\"error\":\"passive_uart_capture_active\"}");return;
  }
  if (otaActive || otaRebootAt) return;
  if (opcode == WS_BINARY) { if(!remoteFirmware.busy())applyStreamPacket(payload, length, (1ULL << 63) | clientId); return; }
  if (opcode != WS_TEXT || length > 1024U) return;
  String message;
  message.reserve(length);
  message.concat(reinterpret_cast<const char *>(payload), length);
  JsonDocument parsed;
  if(deserializeJson(parsed,message) || !parsed.is<JsonObject>()) return;
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

struct WsAssembly { size_t used; size_t length; uint8_t opcode; uint8_t data[1]; };
void onWebSocketEvent(AsyncWebSocket *, AsyncWebSocketClient *client,
                      AwsEventType type, void *argument, uint8_t *payload, size_t length) {
  const auto closeSocket = [&](uint16_t code, const char *reason) {
    // A close handshake need not disconnect immediately. Revoke queued-command
    // admission before marking the callback-owned client as closing.
    webAccess.forgetSocket(client->id()); client->close(code, reason);
  };
  if (type == WS_EVT_DISCONNECT || type == WS_EVT_ERROR) {
    webAccess.forgetSocket(client->id());
    const uint8_t unused = 0;
    // A full queue may drop this advisory release; the stage-owner lease still
    // expires, and immediately revoked auth rejects already queued data.
    loopMessages.push(client->id(), 0xfe, &unused, 0);
    if (client->_tempObject) { free(client->_tempObject); client->_tempObject = nullptr; }
    return;
  }
  if (type == WS_EVT_CONNECT) {
    if (webSocket.count() > 8) { closeSocket(1013, "Client limit reached"); return; }
    if (!webAccess.bindSocket(client->id(), static_cast<AsyncWebServerRequest *>(argument))) {
      closeSocket(1008, "Sign in required"); return;
    }
    const uint8_t statusRequest[]="{\"type\":\"status\"}";
    if(!loopMessages.push(client->id(),WS_TEXT,statusRequest,sizeof(statusRequest)-1))closeSocket(1013,"Command queue full");
    return;
  }
  if (type != WS_EVT_DATA) return;
  if (!webAccess.socketAllowed(client->id())) { closeSocket(1008, "Sign in again"); return; }
  AwsFrameInfo *info = static_cast<AwsFrameInfo *>(argument);
  if (!info || !info->final || info->num != 0 || info->len > 1300 ||
      info->index + length > info->len) { closeSocket(1009, "Message exceeds stream bounds"); return; }
  if (info->index == 0 && info->len == length) {
    if(!loopMessages.push(client->id(),info->opcode,payload,length))closeSocket(1013,"Command queue full");return;
  }
  if (info->index == 0) {
    if (client->_tempObject) free(client->_tempObject);
    auto *assembly = static_cast<WsAssembly *>(malloc(sizeof(WsAssembly) + info->len));
    client->_tempObject = assembly;
    if (!assembly) { closeSocket(1013, "Message buffer unavailable"); return; }
    assembly->used = 0; assembly->length = info->len; assembly->opcode = info->opcode;
  }
  auto *assembly = static_cast<WsAssembly *>(client->_tempObject);
  if (!assembly || assembly->used != info->index || assembly->length != info->len ||
      assembly->used + length > assembly->length) { closeSocket(1002, "Invalid message sequence"); return; }
  memcpy(assembly->data + assembly->used, payload, length); assembly->used += length;
  if (assembly->used == assembly->length) {
    const bool queued=loopMessages.push(client->id(),assembly->opcode,assembly->data,assembly->length);
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

LedGrid::Pattern parsePattern(const String &value) {
  if (value == F("rainbow")) return LedGrid::kPatternRainbow;
  if (value == F("fire")) return LedGrid::kPatternFire;
  return LedGrid::kPatternNone;
}

bool parseAudioCue(const String &value, uint8_t &cue) {
  return BoardCore::AudioCues::parse(value.c_str(), cue) &&
         cue < BoardCore::AudioCues::supportedCount(avrOnline, (avrCapabilities & kCapBuzzer) != 0, avrAudioCueCount);
}

String jsonStringValue(const String &json, const char *key) {
  String needle = String('"') + key + '"';
  int at = json.indexOf(needle);
  if (at < 0) return String();
  at = json.indexOf(':', at + needle.length());
  if (at < 0) return String();
  while (++at < static_cast<int>(json.length()) &&
         (json[at] == ' ' || json[at] == '\t')) {}
  if (at >= static_cast<int>(json.length()) || json[at] != '"') return String();
  String value;
  bool escaped = false;
  for (++at; at < static_cast<int>(json.length()); ++at) {
    const char current = json[at];
    if (escaped) {
      if (current == 'n') value += '\n';
      else if (current == 'r') value += '\r';
      else if (current == 't') value += '\t';
      else value += current;
      escaped = false;
    } else if (current == '\\') {
      escaped = true;
    } else if (current == '"') {
      return value;
    } else {
      value += current;
    }
  }
  return String();
}

long jsonIntegerValue(const String &json, const char *key, long fallback) {
  String needle = String('"') + key + '"';
  int at = json.indexOf(needle);
  if (at < 0) return fallback;
  at = json.indexOf(':', at + needle.length());
  if (at < 0) return fallback;
  while (++at < static_cast<int>(json.length()) &&
         (json[at] == ' ' || json[at] == '\t')) {}
  if (at >= static_cast<int>(json.length())) return fallback;
  const int end = json.indexOf(',', at);
  return json.substring(at, end < 0 ? json.length() : end).toInt();
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
  sendToAvr(kDisplayText, sequence++, payload, static_cast<uint8_t>(8 + text.length()));
  result = F("display text submitted");
  return true;
}

bool jsonBooleanValue(const String &json, const char *key) {
  const String needle = String('"') + key + '"';
  int at = json.indexOf(needle);
  if (at < 0) return false;
  at = json.indexOf(':', at + needle.length());
  if (at < 0) return false;
  const int end = json.indexOf(',', at);
  String value = json.substring(at + 1, end < 0 ? json.length() - 1 : end);
  value.trim();
  return value == F("true") || value == F("1");
}

bool handleCloudCommand(const String &message, String &result) {
  if(passiveCaptureActive()) {result=F("Bounded passive UART capture owns diagnostics");return false;}
  const String command = jsonStringValue(message, "command");
  using namespace BoardCore::Context;
  uint16_t required = 0;
  if (command == F("cell") || command == F("reset") || command == F("snake-direction") || command == F("mode")) required = Table;
  if (command == F("display")) required = message.indexOf(F("\"text\"")) >= 0 ? Text : Display;
  if (command == F("audio") || command == F("buzzer")) required = Buzzer;
  if (command == F("outputs")) required = Outputs;
  if (required && !(currentFeatures() & required)) { result = F("Capability not acknowledged by this board"); return false; }
  if (command == F("mode")) {
    const String value = jsonStringValue(message, "value");
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
  if (command == F("status") ||
      jsonStringValue(message, "type") == F("status")) {
    result = F("status will be published");
    return true;
  }
  if (command == F("mode")) {
    const String value = jsonStringValue(message, "value");
    if (value != F("cell") &&
        value != F("first-wins") && value != F("snake")) {
      result = F("mode must be cell, first-wins, or snake");
      return false;
    }
    mode = parseMode(value);
    winner = 0;
    if (mode == kModeSnake) startSnake();
    else grid.resetWhite();
    const uint8_t payload[] = {static_cast<uint8_t>(mode)};
    sendToAvr(kSetMode, sequence++, payload, sizeof(payload));
    result = F("mode updated");
    return true;
  }
  if (command == F("cell")) {
    const long id = jsonIntegerValue(message, "id", 0);
    const String stateName = jsonStringValue(message, "state");
    const String effectName = jsonStringValue(message, "effect");
    if (id < 1 || id > LedGrid::kCells ||
        (stateName != F("white") && stateName != F("red") &&
         stateName != F("off"))) {
      result = F("cell needs id 1..80 and state white, red, or off");
      return false;
    }
    selectedCell = static_cast<uint8_t>(id);
    selectedState = parseCellState(stateName);
    const Effect effect = parseEffect(effectName);
    grid.setCell(selectedCell, static_cast<CellState>(selectedState), effect);
    const uint8_t payload[] = {selectedCell, selectedState,
                               static_cast<uint8_t>(effect)};
    sendToAvr(kCellAction, sequence++, payload, sizeof(payload));
    result = F("cell updated");
    return true;
  }
  if (command == F("brightness")) {
    const long value = jsonIntegerValue(message, "value", 0);
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
    JsonDocument request;if(deserializeJson(request,message)||!request["enabled"].is<bool>()){
      result=F("enabled must be boolean");return false;}
    grid.setOutputEnabled(request["enabled"].as<bool>());result=grid.outputJson();return true;
  }
  if (command == F("effect")) {
    const String value = jsonStringValue(message, "value");
    const LedGrid::Pattern pattern = parsePattern(value);
    if (pattern == LedGrid::kPatternNone && value != F("none")) {
      result = F("effect must be none, rainbow, or fire");
      return false;
    }
    grid.setPattern(pattern);
    saveLedPreferences();
    result = F("effect updated");
    return true;
  }
  if (command == F("outputs")) {
    const long mask = jsonIntegerValue(message, "mask", -1);
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
    if (jsonBooleanValue(message, "release")) {
      const uint8_t payload[] = {4, 0, 0, 0};
      sendToAvr(kDisplayText, sequence++, payload, sizeof(payload));
      result = F("display released");
      return true;
    }
    if (message.indexOf(F("\"text\"")) >= 0) {
      return submitDisplayText(jsonStringValue(message, "text"),
          jsonIntegerValue(message, "speed_ms", 260), jsonIntegerValue(message, "hold_ms", 1500),
          jsonStringValue(message, "repeat"), jsonBooleanValue(message, "force_scroll"),
          jsonIntegerValue(message, "interval_seconds", 5), result);
    }
    const long value = jsonIntegerValue(message, "value", 10000);
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
    const long frequency = jsonIntegerValue(message, "frequency", 0);
    const long duration = jsonIntegerValue(message, "duration_ms", 0);
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
    if (!parseAudioCue(jsonStringValue(message, "cue"), cue)) {
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
    const String value = jsonStringValue(message, "value");
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
    winner = 0;
    if (mode == kModeSnake) startSnake(); else grid.resetWhite();
    sendToAvr(kGameReset, sequence++);
    result = F("game reset");
    return true;
  }
  result = F("unknown command");
  return false;
}

void onAvrFrame(const BoardCore::Protocol::Frame &frame, void *) {
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
      event += F("\"}"); publishPcEvent(event);
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
    avrVersionKnown = frame.payloadLength >= 9 &&
                      frame.payload[0] == 0 &&
                      frame.payload[1] == kRoleAvr;
    if (avrVersionKnown) memcpy(avrVersion, frame.payload + 6, sizeof(avrVersion));
    avrCapabilities = avrVersionKnown ? frame.payload[2] | static_cast<uint16_t>(frame.payload[3]) << 8 : 0;
    avrAudioCueCount = avrVersionKnown && frame.payloadLength >= 13 ? frame.payload[12] : 0;
    avrToneTimestamps = avrVersionKnown && frame.payloadLength >= 14 && frame.payload[13] == 1;
    ++avrSession; avrPanelKnown = false;
    avrDisplayStatusKnown = false; tm1637Ack = false;
    avrPcSupported = avrVersionKnown && frame.payloadLength >= 12 && frame.payload[9] == 0x50;
    avrPcFeatures = avrPcSupported ? frame.payload[10] | static_cast<uint16_t>(frame.payload[11]) << 8 : 0;
    sendToAvr(kGetStatus, sequence++);
  } else if (frame.opcode == kGameState && frame.payloadLength >= 4) {
    const uint8_t previousWinner = winner;
    const uint8_t previousInputs = activeInputs;
    const bool previousInput9 = activeInput9;
    mode = static_cast<GameMode>(frame.payload[0]);
    winner = frame.payload[1];
    selectedCell = frame.payload[2];
    selectedState = frame.payload[3];
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
      if (gameStateInitialized) {
        if (currentResetRevision != lastResetRevision) {
          if (mode == kModeSnake) startSnake(); else grid.resetWhite();
        }
        if (cellRevision != lastCellActionRevision &&
            actionEffect <= kEffectPulse) {
          grid.setCell(selectedCell, static_cast<CellState>(selectedState),
                       actionEffect);
        }
      }
      lastCellActionRevision = cellRevision;
      lastResetRevision = currentResetRevision;
      gameStateInitialized = true;
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
    if (mode == kModeFirstWins && winner != 0 && winner != previousWinner) {
      grid.showWinner(winner);
    }
  } else if (frame.opcode == kCellEvent && frame.payloadLength >= 3) {
    selectedCell = frame.payload[0];
    selectedState = frame.payload[1];
    grid.setCell(selectedCell, static_cast<CellState>(selectedState),
                 static_cast<Effect>(frame.payload[2]));
  }
}

void registerAsyncOta() {
  RegisterFirmwareTransferRoutes(&server.native(), [](AsyncWebServerRequest *request) {
    return webAccess.allowed(request) && WebAccess::sameOrigin(request) && webAccess.peerAllowed(request);
  });
  auto info = [](AsyncWebServerRequest *request) {
    String body = F("{\"product\":\"PakarController\",\"product_id\":\"PakarController\",\"name\":\"");
    appendJsonEscaped(body, APP_NAME);
    body += F("\",\"application\":"); body += applicationIdentityJson();
    body += F(",\"model\":\""); body += FirmwarePlatform::model(); body += F(" + ATmega328P\",\"version\":\"");
    body += BoardCore::BuildInfo::kVersion;
    body += F("\",\"sketch_size\":"); body += FirmwarePlatform::sketchSize();
    body += F(",\"size\":"); body += FirmwarePlatform::sketchSize();
    body += F(",\"free\":"); body += ESP.getFreeSketchSpace();
    body += F(",\"hash\":\""); body += FirmwarePlatform::sketchMd5(); body += '"';
    body += F(",\"framework\":\"ESPAsyncWebServer\",\"framework_version\":\"3.12.1\"");
    body += F(",\"sketch_md5\":\""); body += FirmwarePlatform::sketchMd5();
    body += F("\",\"phase\":\""); body += otaPhase;
    body += F("\",\"uploaded_bytes\":"); body += otaUploadedBytes;
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
      auto *response=server.request()->beginResponse(200,"application/json",json);
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
        otaOwner = 0; otaActive = false; otaPhase = F("failed");
      }
      request->send(400, "text/plain", request->getAttribute("ota_error", String(F("No complete firmware uploaded")))); return;
    }
    const bool complete = otaUploadedBytes && Update.end(true);
    otaOwner = 0; otaActive = false;
    if (!complete) { otaPhase = F("failed"); request->send(400, "text/plain", FirmwarePlatform::updateError()); return; }
    otaPhase = F("ready-to-reboot");
    request->send(200, "application/json", "{\"updated\":true,\"rebooting\":true}");
    otaRebootAt = millis() + 2000;
  }, [](AsyncWebServerRequest *request, String, size_t index, uint8_t *data, size_t length, bool final) {
    // Upload callbacks run before the normal middleware pipeline: authorize before writing anything.
    if (!webAccess.allowed(request) || !WebAccess::sameOrigin(request) || !webAccess.peerAllowed(request)) return;
    static uint32_t nextOwner = 0;
    if (index == 0) {
      if (passiveCaptureActive() || artifactReaders || remoteFirmware.busy() || remoteFirmware.ownsEspStage() || otaOwner || otaRebootAt || avrUploadOwner || server.processing() || programmingLatched || request->hasAttribute("ota_id")) {
        request->setAttribute("ota_error", "Another operation is active or multiple files were supplied"); return;
      }
      if (++nextOwner == 0) ++nextOwner;
      otaOwner = nextOwner; const uint32_t id = otaOwner;
      request->setAttribute("ota_id", String(id).c_str());
      request->onDisconnect([id]() {
        if (otaOwner != id) return;
        // Uploads are capped below Update.size(), so incomplete end cannot commit boot metadata.
        if (Update.isRunning()) FirmwarePlatform::abortUpdate();
        otaOwner = 0; otaActive = false; otaPhase = F("aborted"); otaErrorCuePending = true;
      });
      otaUploadedBytes = 0; otaPhase = F("receiving"); otaActive = true;
      const uint32_t maximum = FirmwarePlatform::otaCapacity();
#if !defined(ARDUINO_ARCH_ESP32)
      Update.runAsync(true);
#endif
      if (!Update.begin(maximum, U_FLASH)) {
        request->setAttribute("ota_error", FirmwarePlatform::updateError().c_str());
        otaOwner = 0; otaActive = false; otaPhase = F("failed"); return;
      }
      String md5 = request->arg("MD5"); if (md5.isEmpty()) md5 = request->arg("md5");
      if (!md5.isEmpty()) {
        bool valid = md5.length() == 32;
        for (size_t i = 0; i < md5.length(); ++i) valid = valid && isxdigit(static_cast<unsigned char>(md5[i]));
        if (!valid || !Update.setMD5(md5.c_str())) {
          request->setAttribute("ota_error", "Invalid MD5"); FirmwarePlatform::abortUpdate();
          otaOwner = 0; otaActive = false; otaPhase = F("failed"); return;
        }
      }
    }
    if (request->hasAttribute("ota_error") || request->getAttribute("ota_id") != String(otaOwner)) return;
    if (!Update.isRunning() || index != otaUploadedBytes || index + length >= Update.size() || Update.write(data, length) != length) {
      request->setAttribute("ota_error", (Update.hasError() ? FirmwarePlatform::updateError() : String(F("Invalid upload sequence or insufficient OTA space"))).c_str());
      if (Update.isRunning()) FirmwarePlatform::abortUpdate();
      otaOwner = 0; otaActive = false; otaPhase = F("failed"); otaErrorCuePending = true; return;
    }
    otaUploadedBytes += length;
    if (final) {
      request->setAttribute("ota_uploaded", "true");
      // Commit only after the complete HTTP body has arrived: a second file or
      // an interrupted multipart trailer must not accidentally schedule a boot.
      otaPhase = F("verifying");
    }
  });
}

void registerApi() {
#if defined(ARDUINO_ARCH_ESP32)
  server.on("/api/status-rgb", HTTP_GET, []() {
    if (!authenticate()) return;
    const auto &config = statusRgbConfig.configuration();
    JsonDocument d;
    d["enabled"] = config.enabled; d["pin"] = config.pin;
    d["pin_confirmed"] = config.pinConfirmed; d["brightness"] = config.brightness;
    d["persisted"] = statusRgbConfig.persisted(); d["error"] = statusRgbConfig.lastError();
    String body; serializeJson(d,body); server.send(200,"application/json",body);
  });
  server.on("/api/status-rgb", HTTP_POST, []() {
    if (!authenticate()) return;
    if (remoteFirmware.busy() || otaActive || otaRebootAt || avrUploadOwner || programmingLatched) {
      server.send(409,"text/plain","Maintenance active"); return;
    }
    auto config = statusRgbConfig.configuration();
    for (const char *field : {"enabled","pin_confirmed"}) if (server.hasArg(field)) {
      const String value = server.arg(field);
      if (value!="true" && value!="false" && value!="0" && value!="1") {
        server.send(400,"text/plain","Boolean fields require true/false or 0/1"); return;
      }
    }
    if (server.hasArg("enabled")) config.enabled = server.arg("enabled")=="true" || server.arg("enabled")=="1";
    if (server.hasArg("pin")) {
      const String pin=server.arg("pin");
      if(pin!="38" && pin!="48" && pin!="-1") {server.send(400,"text/plain","Reviewed onboard RGB GPIO must be 38 or 48, or -1 disabled");return;}
      if(config.pin!=pin.toInt()) config.pinConfirmed=false;
      config.pin=pin.toInt();
    }
    if (server.hasArg("pin_confirmed")) config.pinConfirmed = server.arg("pin_confirmed")=="true" || server.arg("pin_confirmed")=="1";
    if (server.hasArg("brightness")) {
      uint32_t value;
      if (!strictU32(server.arg("brightness"),value) || value>255) {server.send(400,"text/plain","Brightness must be 0..255");return;}
      config.brightness=value;
    }
    if (!PakarS3::StatusRgbConfig::validate(config) || !statusRgbConfig.save(config)) {
      server.send(400,"text/plain",statusRgbConfig.lastError()); return;
    }
    if (!statusRgb.configure(config,millis(),writeStatusRgb)) {
      server.send(503,"text/plain","Settings saved; local RGB driver unavailable"); return;
    }
    server.send(200,"application/json","{\"saved\":true,\"driver_submitted\":true,\"optical_verified\":false}");
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
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html; charset=utf-8", kWebUi);
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
      server.send(400, "application/json", "{\"error\":\"bad_contract_request\"}"); return;
    }
    if (value != ControllerProtocol::AddressableLed && (!avrOnline || !avrPcSupported)) {
      server.send(503, "application/json", "{\"error\":\"avr_contract_unavailable\"}"); return;
    }
    const uint32_t id = pcTransport.enqueue(value, p, n, millis());
    if (!id) { server.send(409, "application/json", "{\"error\":\"contract_queue_full\"}"); return; }
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
    if (!receipt || receipt->usb) { server.send(404, "application/json", "{\"error\":\"unknown_contract_request\"}"); return; }
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
      server.send(409, "application/json", "{\"error\":\"receipt_not_consumable\"}"); return;
    }
    server.send(200, "application/json", "{\"consumed\":true}");
  });
  server.on("/api/cells/mapping", HTTP_GET, []() {
    if (!authenticate()) return;
    String body;
    // Names may each contain15 bytes requiring two-byte JSON escaping. Reserve
    // the complete bounded response once; never return truncated JSON as200.
    if(!body.reserve(sizeof(cellMapping.names)*2U+LedGrid::kCells*8U+256U)){
      server.send(503,"application/json","{\"error\":\"mapping_memory_unavailable\"}");return;
    }
    body += F("{\"logical_order\":\"row-major\",\"odd_column_offset\":0.5,\"physical\":[");
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
    const int logical = server.arg("logical_id").toInt();
    const int physical = server.arg("physical_cell").toInt();
    if (logical < 1 || logical > LedGrid::kCells ||
        physical < 1 || physical > LedGrid::kCells) {
      server.send(400, "text/plain", "logical_id and physical_cell must be 1..80");
      return;
    }
    String name = server.arg("name");
    name.trim();
    if (name.length() > 15) {
      server.send(400, "text/plain", "name must be at most 15 UTF-8 bytes");
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
      server.send(500, "text/plain", "cell mapping persistence failed");
      return;
    }
    grid.setPhysicalMapping(cellMapping.physical, LedGrid::kCells);
    server.send(200, "application/json", "{\"updated\":true}");
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
    mode = requested;
    winner = 0;
    if (mode == kModeSnake) startSnake();
    else grid.resetWhite();
    saveLedPreferences();
    const uint8_t payload[] = {static_cast<uint8_t>(mode)};
    sendToAvr(kSetMode, sequence++, payload, sizeof(payload));
    server.send(202, "application/json", "{\"accepted\":true}");
  });
  server.on("/api/led/output",HTTP_POST,[](){
    if(!authenticate())return;
    const String value=server.arg("enabled");
    if(value!="true"&&value!="false"&&value!="1"&&value!="0"){
      server.send(400,"text/plain","enabled must be true or false");return;}
    grid.setOutputEnabled(value=="true"||value=="1");
    server.send(200,"application/json",grid.outputJson());
  });
  server.on("/api/cell", HTTP_POST, []() {
    if (!authenticate()) return;
    const int id = server.arg("id").toInt();
    if (id < 1 || id > LedGrid::kCells) {
      server.send(400, "text/plain", "cell id must be 1..80");
      return;
    }
    const CellState state = parseCellState(server.arg("state"));
    const Effect effect = parseEffect(server.arg("effect"));
    grid.setCell(id, state, effect);
    saveLedPreferences();
    selectedCell = id;
    selectedState = state;
    const uint8_t payload[] = {static_cast<uint8_t>(id), static_cast<uint8_t>(state),
                               static_cast<uint8_t>(effect)};
    sendToAvr(kCellAction, sequence++, payload, sizeof(payload));
    server.send(202, "application/json", "{\"accepted\":true}");
  });
  server.on("/api/game/reset", HTTP_POST, []() {
    if (!authenticate()) return;
    winner = 0;
    if (mode == kModeSnake) startSnake(); else grid.resetWhite();
    saveLedPreferences();
    sendToAvr(kGameReset, sequence++);
    server.send(202, "application/json", "{\"accepted\":true}");
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
    server.send(202, "application/json", "{\"accepted\":true}");
  });
  server.on("/api/brightness", HTTP_POST, []() {
    if (!authenticate()) return;
    const int value = server.arg("value").toInt();
    if (value < 1 || value > 255) {
      server.send(400, "text/plain", "brightness must be 1..255");
      return;
    }
    grid.setBrightness(value);
    saveLedPreferences();
    server.send(200, "application/json", "{\"updated\":true}");
  });
  server.on("/api/effect", HTTP_POST, []() {
    if (!authenticate()) return;
    const String value = server.arg("value");
    const LedGrid::Pattern pattern = parsePattern(value);
    if (pattern == LedGrid::kPatternNone && value != F("none")) {
      server.send(400, "text/plain", "unknown effect");
      return;
    }
    grid.setPattern(pattern);
    saveLedPreferences();
    server.send(200, "application/json", "{\"updated\":true}");
  });
  server.on("/api/buzzer", HTTP_POST, []() {
    if (!authenticate()) return;
    const int frequency = server.arg("frequency").toInt();
    const int duration = server.arg("duration_ms").toInt();
    if (frequency < 20 || frequency > 12000 || duration < 1 || duration > 5000) {
      server.send(400, "text/plain", "frequency must be 20..12000 and duration_ms 1..5000");
      return;
    }
    const uint8_t payload[] = {
        static_cast<uint8_t>(frequency), static_cast<uint8_t>(frequency >> 8),
        static_cast<uint8_t>(duration), static_cast<uint8_t>(duration >> 8),
    };
    sendToAvr(kBuzzer, sequence++, payload, sizeof(payload));
    server.send(202, "application/json", "{\"accepted\":true}");
  });
  server.on("/api/audio", HTTP_POST, []() {
    if (!authenticate()) return;
    uint8_t cue = 0;
    if (!parseAudioCue(server.arg("cue"), cue)) {
      server.send(400, "text/plain", "unknown audio cue");
      return;
    }
    if (!sendToAvr(kAudioCue, sequence++, &cue, 1)) {
      server.send(503, "text/plain", "AVR link unavailable"); return;
    }
    server.send(202, "application/json", "{\"accepted\":true,\"state\":\"queued\"}");
  });
  server.on("/api/display", HTTP_POST, []() {
    if (!authenticate()) return;
    if (server.arg("release") == F("true") || server.arg("release") == F("1")) {
      const uint8_t payload[] = {4, 0, 0, 0};
      sendToAvr(kDisplayText, sequence++, payload, sizeof(payload));
      server.send(202, "application/json", "{\"accepted\":true}");
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
      server.send(202, "application/json", "{\"accepted\":true}");
      return;
    }
    if (!server.hasArg("value")) {
      server.send(400, "text/plain", "provide text, value, or release=true");
      return;
    }
    const int value = server.arg("value").toInt();
    if (value < -999 || value > 9999) {
      server.send(400, "text/plain", "display value must be -999..9999");
      return;
    }
    const int16_t signedValue = static_cast<int16_t>(value);
    const uint8_t payload[] = {
        static_cast<uint8_t>(signedValue), static_cast<uint8_t>(signedValue >> 8),
    };
    sendToAvr(kDisplayText, sequence++, payload, sizeof(payload));
    server.send(202, "application/json", "{\"accepted\":true}");
  });
  server.on("/api/outputs", HTTP_POST, []() {
    if (!authenticate()) return;
    const int mask = server.arg("mask").toInt();
    if (mask < 0 || mask > 255) {
      server.send(400, "text/plain", "mask must be 0..255");
      return;
    }
    const uint8_t payload[] = {static_cast<uint8_t>(mask)};
    sendToAvr(kOutputMask, sequence++, payload, sizeof(payload));
    server.send(202, "application/json", "{\"accepted\":true}");
  });
  server.on("/api/device/name", HTTP_GET, []() {
    if (!authenticate()) return;
    server.send(200, "application/json", deviceNameJson());
  });
  server.on("/api/device/name", HTTP_POST, []() {
    if (!authenticate()) return;
    if (otaActive || otaRebootAt || avrUploadOwner || programmingLatched || networkApplyPending) {
      server.send(409, "application/json", "{\"error\":\"device_busy\"}"); return;
    }
    char normalized[64];
    const String input = server.arg("hostname");
    if (input.length() != strlen(input.c_str()) || !BoardCore::Identity::normalizeName(input.c_str(), normalized)) {
      server.send(400, "application/json", "{\"error\":\"invalid_hostname\"}"); return;
    }
    const String label = server.hasArg("display_name") ? server.arg("display_name") : String(deviceIdentity.displayName());
    if (label.length() != strlen(label.c_str()) || !BoardCore::Identity::validDisplayName(label.c_str())) {
      server.send(400, "application/json", "{\"error\":\"invalid_display_name\"}"); return;
    }
    if (!scheduleDeviceIdentity(normalized, label)) {
      server.send(503, "application/json", deviceNameJson()); return;
    }
    server.send(202, "application/json", deviceNameJson(true));
  });
  server.on("/api/network", HTTP_GET, []() {
    if (!authenticate()) return;
    String body = F("{\"station_enabled\":");
    body += networkConfig.stationEnabled() ? F("true") : F("false");
    body += F(",\"station_connected\":");
    body += networkConfig.stationConnected() ? F("true") : F("false");
    body += F(",\"ap_enabled\":");
    body += networkConfig.accessPointEnabled() ? F("true") : F("false");
    body += F(",\"station_ssid\":\""); appendJsonEscaped(body,networkConfig.stationSsid());
    body += F("\",\"station_ip\":\""); body += WiFi.localIP().toString();
    body += F("\",\"ap_ip\":\""); body += WiFi.softAPIP().toString();
    body += F("\",\"dhcp\":"); body += networkConfig.dhcpEnabled() ? F("true") : F("false");
    body += F(",\"address\":\""); body += networkConfig.stationAddress().toString();
    body += F("\",\"gateway\":\""); body += networkConfig.stationGateway().toString();
    body += F("\",\"subnet\":\""); body += networkConfig.stationSubnet().toString();
    body += F("\",\"dns1\":\""); body += networkConfig.stationDns1().toString();
    body += F("\",\"dns2\":\""); body += networkConfig.stationDns2().toString(); body += F("\"}");
    server.send(200, "application/json", body);
  });
  server.on("/api/network", HTTP_POST, []() {
    if (!authenticate()) return;
    if (remoteFirmware.busy() || otaActive || otaRebootAt || networkApplyPending || programmingLatched) {
      server.send(409, "text/plain", "Device maintenance/network change active"); return;
    }
    NetworkConfigStore candidate = networkConfig;
    const bool enabled = server.hasArg("station_enabled") ?
        server.arg("station_enabled") != F("false") && server.arg("station_enabled") != F("0") : candidate.stationEnabled();
    const bool apEnabled = server.hasArg("ap_enabled") ?
        server.arg("ap_enabled") != F("false") && server.arg("ap_enabled") != F("0") : candidate.accessPointEnabled();
    const bool dhcp = server.hasArg("dhcp") ? server.arg("dhcp") != F("false") && server.arg("dhcp") != F("0") : candidate.dhcpEnabled();
    // Omitted addressing fields retain the stored values. Empty password retains
    // the existing station secret unless explicitly changing SSID/open network.
    if (!candidate.setStation(server.hasArg("ssid") ? server.arg("ssid") : String(candidate.stationSsid()), server.arg("password"), enabled, server.hasArg("password")) ||
        !candidate.setAccessPointEnabled(apEnabled) ||
        !candidate.setAddressing(dhcp,
            server.hasArg("address") ? server.arg("address") : candidate.stationAddress().toString(),
            server.hasArg("gateway") ? server.arg("gateway") : candidate.stationGateway().toString(),
            server.hasArg("subnet") ? server.arg("subnet") : candidate.stationSubnet().toString(),
            server.hasArg("dns1") ? server.arg("dns1") : candidate.stationDns1().toString(),
            server.hasArg("dns2") ? server.arg("dns2") : candidate.stationDns2().toString()) || !candidate.save()) {
      server.send(400, "text/plain", candidate.lastError());
      return;
    }
    networkConfig = candidate;
    server.send(202, "application/json", "{\"accepted\":true}");
    // Flush the response through the old interface before changing station
    // association. The recovery AP remains active throughout the reconfigure.
    networkApplyAt = millis() + 250U;
    networkApplyPending = true;
  });

  server.on("/api/cloud", HTTP_GET, []() {
    if (!authenticate()) return;
    String body;
    body.reserve(700);
    body += F("{\"mqtt_enabled\":");
    body += cloudConfig.mqttEnabled() ? F("true") : F("false");
    body += F(",\"mqtt_tls\":");
    body += cloudConfig.mqttTls() ? F("true") : F("false");
    body += F(",\"mqtt_host\":\""); body += cloudConfig.mqttHost();
    body += F("\",\"mqtt_port\":"); body += cloudConfig.mqttPort();
    body += F(",\"mqtt_user\":\""); body += cloudConfig.mqttUser();
    body += F("\",\"mqtt_password_set\":");
    body += cloudConfig.mqttPasswordSet() ? F("true") : F("false");
    body += F(",\"mqtt_base_topic\":\""); body += cloudConfig.mqttBaseTopic();
    body += F("\",\"mqtt_connected\":");
    body += cloudServices.mqttConnected() ? F("true") : F("false");
    body += F(",\"mqtt_state\":"); body += cloudServices.mqttState();
    body += F(",\"websocket_enabled\":");
    body += cloudConfig.webSocketEnabled() ? F("true") : F("false");
    body += F(",\"websocket_tls\":");
    body += cloudConfig.webSocketTls() ? F("true") : F("false");
    body += F(",\"websocket_host\":\""); body += cloudConfig.webSocketHost();
    body += F("\",\"websocket_port\":"); body += cloudConfig.webSocketPort();
    body += F(",\"websocket_path\":\""); body += cloudConfig.webSocketPath();
    body += F("\",\"websocket_token_set\":");
    body += cloudConfig.webSocketTokenSet() ? F("true") : F("false");
    body += F(",\"websocket_connected\":");
    body += cloudServices.webSocketConnected() ? F("true") : F("false");
    body += F(",\"telemetry_seconds\":");
    body += cloudConfig.telemetrySeconds();
    String error = cloudServices.lastError();
    error.replace("\\", "\\\\");
    error.replace("\"", "\\\"");
    body += F(",\"last_error\":\""); body += error; body += F("\"}");
    server.send(200, "application/json", body);
  });
  server.on("/api/cloud", HTTP_POST, []() {
    if (!authenticate()) return;
    const bool mqttEnabled =
        server.arg("mqtt_enabled") == F("true") ||
        server.arg("mqtt_enabled") == F("1");
    const bool mqttTls =
        server.arg("mqtt_tls") == F("true") ||
        server.arg("mqtt_tls") == F("1");
    const bool webSocketEnabled =
        server.arg("websocket_enabled") == F("true") ||
        server.arg("websocket_enabled") == F("1");
    const bool webSocketTls =
        server.arg("websocket_tls") == F("true") ||
        server.arg("websocket_tls") == F("1");
    const int mqttPort = server.arg("mqtt_port").toInt();
    const int webSocketPort = server.arg("websocket_port").toInt();
    const int telemetrySeconds = server.arg("telemetry_seconds").toInt();
    if (mqttPort < 1 || mqttPort > 65535 ||
        webSocketPort < 1 || webSocketPort > 65535 ||
        telemetrySeconds < 1 || telemetrySeconds > 3600 ||
        !cloudConfig.setMqtt(
            mqttEnabled, mqttTls, server.arg("mqtt_host"),
            static_cast<uint16_t>(mqttPort), server.arg("mqtt_user"),
            server.arg("mqtt_password"), server.arg("mqtt_base_topic"),
            server.hasArg("mqtt_password")) ||
        !cloudConfig.setWebSocket(
            webSocketEnabled, webSocketTls, server.arg("websocket_host"),
            static_cast<uint16_t>(webSocketPort), server.arg("websocket_path"),
            server.arg("websocket_token"), server.hasArg("websocket_token")) ||
        !cloudConfig.setTelemetrySeconds(
            static_cast<uint16_t>(telemetrySeconds)) ||
        !cloudConfig.save()) {
      server.send(400, "text/plain", cloudConfig.lastError());
      return;
    }
    cloudServices.applyConfiguration();
    server.send(200, "application/json", "{\"updated\":true}");
  });
  server.on("/api/cloud/reconnect", HTTP_POST, []() {
    if (!authenticate()) return;
    cloudServices.applyConfiguration();
    server.send(202, "application/json", "{\"accepted\":true}");
  });

  server.on("/api/avr/probe", HTTP_GET, []() {
    if (!authenticate()) return;
    uint8_t signature[3] = {};
    const bool ok = avrIsp.probe(signature);
    if (!ok) {
      server.send(503, "text/plain", avrIsp.lastError());
      return;
    }
    server.send(200, "application/json", "{\"signature\":\"1E950F\"}");
  });
  server.on("/api/avr/backup", HTTP_POST, []() {
    if (!authenticate()) return;
    if (!avrIsp.backup(LittleFS, kBackupDirectory)) {
      server.send(500, "text/plain", avrIsp.lastError());
      return;
    }
    server.send(200, "application/json", "{\"backed_up\":true}");
  });
  server.on("/api/avr/backup/flash", HTTP_GET, []() {
    if (!authenticate()) return;
    File file = LittleFS.open(String(kBackupDirectory) + F("/flash.bin"), "r");
    if (!file) { server.send(404, "text/plain", "no flash backup"); return; }
    server.streamFile(file, "application/octet-stream");
    // Response owns a File copy; explicit close would close its shared handle.
  });
  server.on("/api/avr/backup/eeprom", HTTP_GET, []() {
    if (!authenticate()) return;
    File file = LittleFS.open(String(kBackupDirectory) + F("/eeprom.bin"), "r");
    if (!file) { server.send(404, "text/plain", "no EEPROM backup"); return; }
    server.streamFile(file, "application/octet-stream");
    // Let the local wrapper release its reference, not close the response's file.
  });
  server.on("/api/avr/program", HTTP_POST, []() {
    if(!authenticate()) return;
    File image=LittleFS.open(kAvrImagePath,"r");
    if(!image || image.size()!=32768) {if(image)image.close();server.send(400,"text/plain","Complete AVR image required");return;}
    image.close();
    // Manual uploads retain compatibility, but no longer bypass durable Prog
    // or EEPROM preservation. Exact remote provenance uses the manifest path.
    RemoteFirmware::Plan plan;plan.targets=2;plan.avr.version=BoardCore::BuildInfo::kVersion;
    String reason;
    if(!prepareRemotePair(plan,reason) ||
       !programRemoteAvr(kAvrImagePath,plan.avr.version,reason) ||
       finishRemotePair(plan,true,reason)!=1) {
      server.send(500,"text/plain",reason);return;
    }
    server.send(200,"application/json","{\"programmed\":true,\"verified\":true,\"eeprom_preserved\":true}");
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
        if (passiveCaptureActive() || artifactReaders || remoteFirmware.busy() || avrUploadOwner || otaActive || otaRebootAt || server.processing() || request->hasAttribute("upload_id")) {
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

void serviceStream() {
  const int packetSize = stream.parsePacket();
  if (packetSize < 10 || packetSize > 1300) return;
  uint8_t packet[1300];
  const int length = stream.read(packet, sizeof(packet));
  if (length > 0) applyStreamPacket(packet, static_cast<size_t>(length),
      (1ULL<<62)|(uint64_t(static_cast<uint32_t>(stream.remoteIP()))<<16)|stream.remotePort());
}

void telnetPrompt() {
  if (telnetClient && telnetClient.connected()) telnetClient.print(F("pakar> "));
}

void executeTelnetCommand(String line) {
  line.trim();
  if (line.length() == 0) {
    telnetPrompt();
    return;
  }
  if (line == F("quit") || line == F("exit")) {
    telnetClient.println(F("Goodbye."));
    telnetClient.stop();
    return;
  }
  if (line == F("help") || line == F("?")) {
    telnetClient.println(F("info | telemetry | firmware status | firmware fetch HTTPS_URL SHA256 [all|esp|avr] | firmware apply JOB BOOT SHA256 | firmware cancel JOB BOOT"));
    telnetClient.println(
        F("help | status | led-output <on|off> | mode <cell|first-wins|snake> | "
          "cell <1..80> <white|red|off> [immediate|fade|pulse]"));
    telnetClient.println(
        F("brightness <1..255> | effect <none|rainbow|fire> | "
          "display <-999..9999> | display stop | audio <cue> | reset | quit"));
    telnetClient.println(F("display text <once|loop|interval> <speed_ms> <hold_ms> <pause_seconds> <scroll 0|1> <TEXT>"));
    telnetClient.println(F("device identity | device rename <HOST[.local]> | device label <DISPLAY NAME>"));
    telnetPrompt();
    return;
  }
  if (line == F("status")) {
    telnetClient.println(buildStatusJson());
    telnetPrompt();
    return;
  }
  if(line=="led-output on" || line=="led-output off"){
    if(remoteFirmware.busy()||otaActive||programmingLatched)telnetClient.println(F("ERR device busy"));
    else{grid.setOutputEnabled(line=="led-output on");telnetClient.println(grid.outputJson());}
    telnetPrompt();return;
  }
  if (line == F("device name") || line == F("device identity")) { telnetClient.println(deviceNameJson()); telnetPrompt(); return; }
  if (line.startsWith(F("device label "))) {
    if (networkApplyPending || programmingLatched || remoteFirmware.busy()) telnetClient.println(F("ERR device busy"));
    else if (scheduleDeviceIdentity(deviceIdentity.hostname(), line.substring(13))) telnetClient.println(deviceNameJson(true));
    else telnetClient.println(deviceIdentity.error());
    telnetPrompt(); return;
  }
  if(line=="info" || line=="telemetry" || line.startsWith("firmware ")) {
    String result;handleMaintenanceShell(line,result);telnetClient.println(result);telnetPrompt();return;
  }
  if(remoteFirmware.busy()) {telnetClient.println(F("ERR remote firmware owns maintenance"));telnetPrompt();return;}
  if (line.startsWith(F("device rename "))) {
    if (networkApplyPending || programmingLatched) telnetClient.println(F("ERR device busy"));
    else if (scheduleDeviceRename(line.substring(14))) telnetClient.println(deviceNameJson(true));
    else telnetClient.println(deviceIdentity.error());
    telnetPrompt(); return;
  }
  if (line == F("display stop")) {
    const uint8_t payload[] = {4, 0, 0, 0};
    sendToAvr(kDisplayText, sequence++, payload, sizeof(payload));
    telnetClient.println(F("OK display released"));
    telnetPrompt();
    return;
  }
  if (line.startsWith(F("display text "))) {
    String remainder = line.substring(13);
    String values[5];
    bool complete = true;
    for (uint8_t i = 0; i < 5; ++i) {
      const int space = remainder.indexOf(' ');
      if (space < 0) { complete = false; break; }
      values[i] = remainder.substring(0, space);
      remainder = remainder.substring(space + 1);
    }
    String result;
    bool numeric = true;
    for (uint8_t i = 1; i < 5; ++i) {
      if (values[i].length() == 0) numeric = false;
      for (size_t j = 0; j < values[i].length(); ++j)
        if (values[i][j] < '0' || values[i][j] > '9') numeric = false;
    }
    const bool ok = complete && numeric && (values[4] == F("0") || values[4] == F("1")) &&
        submitDisplayText(remainder, values[1].toInt(), values[2].toInt(), values[0],
                          values[4] == F("1"), values[3].toInt(), result);
    telnetClient.print(ok ? F("OK ") : F("ERR "));
    telnetClient.println(result.isEmpty() ? F("invalid display text arguments; use help") : result);
    telnetPrompt();
    return;
  }

  const int firstSpace = line.indexOf(' ');
  const String command = firstSpace < 0 ? line : line.substring(0, firstSpace);
  String arguments = firstSpace < 0 ? String() : line.substring(firstSpace + 1);
  arguments.trim();
  String json;
  if (command == F("mode") || command == F("effect") ||
      command == F("audio")) {
    json = String(F("{\"command\":\"")) + command +
           F("\",\"value\":\"") + arguments + F("\"}");
  } else if (command == F("brightness") || command == F("display")) {
    json = String(F("{\"command\":\"")) + command +
           F("\",\"value\":") + arguments + F("}");
  } else if (command == F("reset")) {
    json = F("{\"command\":\"reset\"}");
  } else if (command == F("cell")) {
    const int a = arguments.indexOf(' ');
    const int b = a < 0 ? -1 : arguments.indexOf(' ', a + 1);
    const String id = a < 0 ? String() : arguments.substring(0, a);
    const String state = a < 0 ? String()
                                      : (b < 0 ? arguments.substring(a + 1)
                                               : arguments.substring(a + 1, b));
    const String effect = b < 0 ? F("immediate") : arguments.substring(b + 1);
    json = String(F("{\"command\":\"cell\",\"id\":")) + id +
           F(",\"state\":\"") + state + F("\",\"effect\":\"") +
           effect + F("\"}");
  } else {
    telnetClient.println(F("ERR unknown command; use help"));
    telnetPrompt();
    return;
  }

  String result;
  const bool ok = handleCloudCommand(json, result);
  telnetClient.print(ok ? F("OK ") : F("ERR "));
  telnetClient.println(result);
  telnetPrompt();
}

void serviceTelnet() {
  auto write = [](const uint8_t* bytes, size_t length) { telnetClient.write(bytes, length); };
  if (telnetServer.hasClient()) {
    WiFiClient candidate = telnetServer.accept();
    if (telnetClient && telnetClient.connected()) {
      candidate.println(F("PakarController shell already has a client."));
      candidate.stop();
    } else {
      telnetClient = candidate;
      telnetClient.setNoDelay(true);
      telnetAuthenticated = false;
      telnetEditor.reset();
      telnetParser.greet(write);
      telnetLastInputAt = millis();
      telnetClient.println(F("PakarController authenticated shell"));
      telnetClient.print(F("Password: "));
    }
  }
  if (!telnetClient || !telnetClient.connected()) { telnetEditor.reset(); telnetParser.reset(); return; }
  const uint32_t idleLimit = telnetAuthenticated ? 1800000UL : 60000UL;
  if (static_cast<uint32_t>(millis() - telnetLastInputAt) >= idleLimit) {
    telnetClient.println(F("Session expired.")); telnetClient.stop(); telnetEditor.reset(); return;
  }
  uint8_t budget = 96;
  while (budget-- != 0 && telnetClient.available() != 0) {
    telnetLastInputAt = millis();
    telnetParser.feed(static_cast<uint8_t>(telnetClient.read()), write, [&](uint8_t ch) {
      const auto event = telnetEditor.feed(ch, write);
      if (event == PakarShell::LineEditor::None) return;
      if (event == PakarShell::LineEditor::Cancel || event == PakarShell::LineEditor::TooLong) {
        if (event == PakarShell::LineEditor::TooLong) telnetClient.println(F("ERR line too long; discarded"));
        telnetEditor.clear();
        if (telnetAuthenticated) telnetPrompt(); else telnetClient.print(F("Password: "));
        return;
      }
      if (!telnetAuthenticated) {
        const bool valid = accessConfig.ready() && BoardCore::Access::equal(telnetEditor.line(), accessConfig.password());
        telnetEditor.clear();
        if (valid) {
          telnetAuthenticated = true;
          telnetEditor.setEcho(true);
          telnetClient.println(F("Authenticated. Type help for commands."));
          telnetPrompt();
        } else {
          telnetClient.println(F("Authentication failed."));
          telnetClient.stop();
        }
        return;
      }
      const String line(telnetEditor.line());
      telnetEditor.clear();
      executeTelnetCommand(line);
    });
    if (!telnetClient.connected()) { telnetEditor.reset(); break; }
  }
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
#include "src/ArtifactDownload.h"
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
  accessConfig.begin(LittleFS, filesystemMounted);
  networkConfig.begin(LittleFS);
  deviceIdentity.begin(LittleFS, filesystemMounted);
  cloudConfig.begin(LittleFS);
  grid.begin();
  loadLedPreferences();
  loadCellMapping();
  hostLink.begin(115200, onHostFrame);
#if defined(ARDUINO_ARCH_ESP32)
#if PAKAR_AVR_LINK_ENABLED
  avrSerial.setRxBufferSize(PakarS3::kAvrRxBuffer);
  avrSerial.setTxBufferSize(PakarS3::kAvrTxBuffer);
  avrSerial.begin(kBaud, SERIAL_8N1, PAKAR_S3_AVR_RX, PAKAR_S3_AVR_TX);
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
#else
  avrSerial.begin(kBaud);
#endif
  if (PAKAR_AVR_LINK_ENABLED) avrLink.begin(kBaud, onAvrFrame);

  // This is deliberately stable. Chip identity belongs in discovery metadata,
  // not in the human-facing host or recovery AP name.
  hostname = deviceIdentity.hostname();
  apStarted = networkConfig.configure(hostname.c_str(), kDevelopmentPassword);
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
  webAccess.configurePeerFence(pcBoardId, pcBootId);
  cloudServices.begin(hostname, handleCloudCommand, buildStatusJson,buildTelemetryJson);
  stream.begin(kStreamPort);
  telnetServer.begin();
  telnetServer.setNoDelay(true);

  registerApi();
  registerMaintenanceRoutes();
  registerArtifactDownload();
  webAccess.registerRoutes(server.native(), []() {
    webSocket.closeAll(1008, "Sign in again");
    if (telnetClient) telnetClient.stop();
    telnetAuthenticated = false;
  }, []() { return remoteFirmware.busy() || otaActive || otaRebootAt || avrUploadOwner || server.processing() || programmingLatched; });
  webSocket.onEvent(onWebSocketEvent);
  server.native().addHandler(&webSocket);
#if !defined(ARDUINO_ARCH_ESP32)
  Update.onStart([]() {
    if(!remoteFirmware.applying())otaStartCuePending = true;
  });
  Update.onEnd([]() {
    if(!remoteFirmware.applying())otaSuccessCuePending = true;
  });
  Update.onError([](uint8_t) {
    otaErrorCuePending = true;
  });
#endif
  registerAsyncOta();
  server.begin();
  serviceDiscovery();
  sendToAvr(kHello, sequence++);
#if PAKAR_USE_BUSY_WIRE
  setAvrTransmitBlocked(false);
#endif
}

void loop() {
#if defined(ARDUINO_ARCH_ESP32) && !PAKAR_AVR_LINK_ENABLED
  passiveAvrRx.service(millis());
  passiveCaptureGuard.store(passiveAvrRx.snapshot().active,std::memory_order_release);
#endif
  servicePcContracts();
  const uint32_t now = millis();
#if defined(ARDUINO_ARCH_ESP32)
  using RgbState = PakarS3::StatusRgb::State;
  const RgbState rgbState = otaActive || remoteFirmware.busy() ? RgbState::Updating :
      !accessConfig.ready() ? RgbState::Fault :
      networkConfig.stationEnabled() && !networkConfig.stationConnected() ? RgbState::Connecting :
      !avrOnline ? RgbState::PeerLost :
      networkConfig.stationConnected() ? RgbState::Ready : RgbState::AccessPoint;
  statusRgb.setState(rgbState, now);
  statusRgb.service(now, writeStatusRgb);
#endif
  avrLink.service();
  hostLink.service();
  // Render before issuing UART requests. The AVR is request/response only, so
  // its reply arrives while FastLED is not masking ESP8266 interrupts.
  const bool ledFrameDue = !otaActive && !remoteFirmware.busy() &&
      static_cast<int32_t>(now - ledResumeAt) >= 0 &&
      grid.needsRender(now);
  if (ledFrameDue) {
    setAvrTransmitBlocked(true);
    // The busy edge prevents a new unsolicited AVR frame, but one may have
    // passed the AVR's LOW check just before this edge and still be draining
    // from its hardware UART. Consume that bounded frame before FastLED masks
    // ESP8266 interrupts for the 480-pixel transfer.
#if !defined(ARDUINO_ARCH_ESP32)
    serviceAvrFor(8U);
#endif
    grid.service(now);
    setAvrTransmitBlocked(false);
  }
  server.service();
  LoopMessages::Message message;
  if(loopMessages.pop(message)){
    if(message.opcode==0xfe) grid.discardStaged((1ULL << 63) | message.client);
    else if(webAccess.socketAllowed(message.client))
      handleWebSocketMessage(message.client,message.opcode,message.data,message.length);
  }
  if (remoteFirmware.busy()) {
#if defined(ARDUINO_ARCH_ESP32)
    if(remoteFirmware.needsDownloadConnection())
      PakarS3::serviceDhcpDns(millis(),networkConfig.dhcpEnabled(),true);
#endif
    if(cloudServices.mqttConnected() || cloudServices.webSocketConnected())cloudServices.suspend();
    setAvrTransmitBlocked(true);serviceAvrFor(18);
    remoteFirmware.service();setAvrTransmitBlocked(false);
  }
  webSocket.cleanupClients(8);
  webAccess.expireSockets(webSocket);
  if (otaStartCuePending) { otaStartCuePending = false; playUpdateStartCue(); }
  if (otaSuccessCuePending) { otaSuccessCuePending = false; playUpdateSuccessCue(); }
  if (otaErrorCuePending) {
    otaErrorCuePending = false; const uint8_t cue[] = {kCueError}; sendToAvr(kAudioCue, sequence++, cue, sizeof(cue));
  }
  if (otaRebootAt && static_cast<int32_t>(millis() - otaRebootAt) >= 0) ESP.restart();
  if (!otaActive && !otaRebootAt && !uploadFile) serviceTelnet();
  if (!remoteFirmware.busy() && !otaActive && !otaRebootAt && networkApplyPending &&
      static_cast<int32_t>(millis() - networkApplyAt) >= 0) {
    networkApplyPending = false;
    applyNetworkConfig();
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
    cloudServices.service(networkConfig.stationConnected(),
                          time(nullptr) > 1700000000, now);
    setAvrTransmitBlocked(false);
  }
  if (!remoteFirmware.busy()) { serviceSnake(now);serviceStream(); }
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
  yield();
}
