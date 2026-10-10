#include <Arduino.h>
#include <ESP8266SSDP.h>
#include <ESP8266NetBIOS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <SoftwareSerial.h>
#include <Updater.h>
#include <WiFiUdp.h>
#include <time.h>

#include <BuildInfo.h>
#include <AudioCues.h>
#include <Protocol.h>
#include <SerialLink.h>

#include "src/AvrIsp.h"
#include "src/AsyncHttpService.h"
#include "src/WebAccess.h"
#include "src/FirmwareTransfer.h"
#include "src/CloudConfig.h"
#include "src/CloudServices.h"
#include "src/LedGrid.h"
#include "src/NetworkConfig.h"
#include "src/DeviceIdentity.h"
#include "src/BusyLine.h"
#include "src/TelnetShell.h"
#include "src/WebUi.h"

using namespace BoardCore::Protocol;

#ifndef PAKAR_USE_BUSY_WIRE
#define PAKAR_USE_BUSY_WIRE 1
#endif

#if PAKAR_USE_BUSY_WIRE != 0 && PAKAR_USE_BUSY_WIRE != 1
#error "PAKAR_USE_BUSY_WIRE must be 0 or 1"
#endif

namespace {

constexpr uint32_t kBaud = 38400;
constexpr uint16_t kStreamPort = 7890;
constexpr uint8_t kBusyPin = D3;  // GPIO0 -> AVR D10/PB2, HIGH blocks AVR TX.
constexpr char kDevelopmentPassword[] = "pakarcontroller";
constexpr char kAvrImagePath[] = "/avr/staged.bin";
constexpr char kBackupDirectory[] = "/avr/backup";
constexpr char kLedPreferencesPath[] = "/led.cfg";
constexpr char kCellMappingPath[] = "/cells.map";

struct LedPreferences {
  uint8_t magic0;
  uint8_t magic1;
  uint8_t schema;
  uint8_t pattern;
  uint8_t brightness;
  uint8_t check;
};

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
SoftwareSerial avrSerial(D4, D0);
BoardCore::SerialLink link(static_cast<Stream &>(avrSerial));
BoardCore::SerialLink hostLink(Serial);
AvrIsp avrIsp(D1);
BusyLine<PAKAR_USE_BUSY_WIRE != 0> busyLine(kBusyPin);
LedGrid grid;
File uploadFile;
uint32_t avrUploadOwner = 0;
String hostname;
bool avrOnline = false;
bool avrVersionKnown = false;
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
bool gameStateInitialized = false;
bool streamFrameReady = false;
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
bool mdnsStarted = false;
bool ssdpStarted = false;
bool netbiosStarted = false;
IPAddress discoveryAddress;
String stagedStationSsid;
String stagedStationPassword;
String stagedAccessUser;
String stagedAccessPassword;
String stagedDeviceName;
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
String deviceNameJson(bool accepted = false);
bool saveCellMapping();
String buildStatusJson();
bool handleCloudCommand(const String &message, String &result);
void startSnake();
bool setSnakeDirection(uint8_t direction);
void serviceSnake(uint32_t now);
void serviceTelnet();
void serviceDiscovery();
void registerAsyncOta();
bool sendToAvr(uint8_t opcode, uint8_t requestSequence,
               const uint8_t *payload = nullptr, uint8_t payloadLength = 0);

void sendHostStatus(uint8_t requestSequence) {
  const time_t currentTime = time(nullptr);
  const uint16_t errors = link.errors();
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
      BoardCore::AudioCues::supportedCount(avrOnline, avrVersionKnown, avrVersion),
  };
  hostLink.send(kStatusResponse, requestSequence, payload, sizeof(payload));
}

void onHostFrame(const BoardCore::Protocol::Frame &frame, void *) {
  if ((otaActive || otaRebootAt || avrUploadOwner) && frame.opcode != kGetStatus && frame.opcode != kHello && frame.opcode != kPing) {
    hostLink.error(frame.sequence, frame.opcode, kBusy); return;
  }
  switch (frame.opcode) {
    case kDeviceName: {
      if (programmingLatched || networkApplyPending) { hostLink.error(frame.sequence, frame.opcode, kBusy); break; }
      if (frame.payloadLength == 1 && frame.payload[0] == 0) stagedDeviceName = String();
      else if (frame.payloadLength >= 2 && frame.payload[0] == 1) {
        if (stagedDeviceName.length() + frame.payloadLength - 1 > 69) { hostLink.error(frame.sequence, frame.opcode, kBadPayload); break; }
        for (uint8_t i = 1; i < frame.payloadLength; ++i) {
          if (!frame.payload[i]) { stagedDeviceName = String(); hostLink.error(frame.sequence, frame.opcode, kBadPayload); return; }
          stagedDeviceName += static_cast<char>(frame.payload[i]);
        }
      } else if (frame.payloadLength == 1 && frame.payload[0] == 2) {
        const bool ok = scheduleDeviceRename(stagedDeviceName); stagedDeviceName = String();
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
      const uint16_t capabilities =
          kCapLedGrid | kCapIsp | kCapOta | kCapLittleFs | kCapApi |
          kCapWebSocket | kCapNtp | kCapHostSerial |
          (PAKAR_USE_BUSY_WIRE ? kCapBusyWire : 0U);
      const uint8_t payload[] = {
          BoardCore::BuildInfo::kProtocolVersion, kRoleEsp,
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
      else if (mode == kModeIdle) grid.fill(kCellOff);
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
      else if (mode == kModeIdle) grid.fill(kCellOff);
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
        const bool stationEnabled = frame.payload[1] != 0;
        const bool apEnabled = frame.payload[2] != 0;
        if (!networkConfig.setStation(stagedStationSsid,
                                      stagedStationPassword,
                                      stationEnabled) ||
            !networkConfig.setAccessPointEnabled(apEnabled) ||
            !networkConfig.save()) {
          hostLink.error(frame.sequence, frame.opcode, kBadPayload);
          break;
        }
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
           BoardCore::AudioCues::supportedCount(avrOnline, avrVersionKnown, avrVersion))) {
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

uint8_t ledPreferencesCheck(const LedPreferences &value) {
  return static_cast<uint8_t>(value.magic0 ^ value.magic1 ^ value.schema ^
                              value.pattern ^ value.brightness ^ 0xA5U);
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
  LedPreferences value = {0x50, 0x4C, 1, static_cast<uint8_t>(grid.pattern()),
                          grid.brightness(), 0};
  value.check = ledPreferencesCheck(value);
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
  if (!complete || value.magic0 != 0x50 || value.magic1 != 0x4C ||
      value.schema != 1 || value.pattern > LedGrid::kPatternFire ||
      value.brightness == 0 || value.check != ledPreferencesCheck(value)) {
    return;
  }
  grid.setBrightness(value.brightness);
  grid.setPattern(static_cast<LedGrid::Pattern>(value.pattern));
}

void stopDiscovery() {
  MDNS.close();
  SSDP.end();
  NBNS.end();
  mdnsStarted = false;
  ssdpStarted = false;
  netbiosStarted = false;
  discoveryAddress = IPAddress();
}

void startDiscovery() {
  stopDiscovery();
  if (!networkConfig.stationConnected()) return;

  discoveryAddress = WiFi.localIP();
  mdnsStarted = MDNS.begin(hostname.c_str());
  if (mdnsStarted) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "path", "/");
    MDNS.addServiceTxt("http", "tcp", "version",
                       BoardCore::BuildInfo::kVersion);
    MDNS.addService("pakarcontroller", "tcp", 80);
    MDNS.addServiceTxt("pakarcontroller", "tcp", "api", "/api/v1/status");
    MDNS.addServiceTxt("pakarcontroller", "tcp", "websocket", "/ws");
    MDNS.addService("pakar-stream", "udp", kStreamPort);
    MDNS.addService("telnet", "tcp", 23);
  }

  SSDP.setSchemaURL("description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName(hostname);
  SSDP.setSerialNumber(ESP.getChipId());
  SSDP.setURL("/");
  SSDP.setModelName("PakarController ESP8266 + ATmega328P");
  SSDP.setModelNumber(BoardCore::BuildInfo::kVersion);
  SSDP.setManufacturer("AtomicDeploy");
  char uuid[37];
  snprintf(uuid, sizeof(uuid), "38323636-4558-4dda-9188-cda0e6%06x", ESP.getChipId());
  SSDP.setUUID(uuid);
  ssdpStarted = SSDP.begin();
  // NetBIOS labels are limited to 15; never advertise a silently truncated alias.
  netbiosStarted = hostname.length() <= 15 && NBNS.begin(hostname.c_str());
}

void serviceDiscovery() {
  const bool connected = networkConfig.stationConnected();
  const IPAddress address = connected ? WiFi.localIP() : IPAddress();
  if ((connected && (!mdnsStarted || address != discoveryAddress)) ||
      (!connected && (mdnsStarted || ssdpStarted))) {
    startDiscovery();
  }
  if (mdnsStarted) MDNS.update();
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
  if (!deviceIdentity.rename(name)) return false;
  networkApplyAt = millis() + 1000U; networkApplyPending = true;
  return true;
}

String deviceNameJson(bool accepted) {
  String body = F("{\"hostname\":\""); body += deviceIdentity.hostname();
  body += F("\",\"fqdn\":\""); body += deviceIdentity.hostname();
  body += F(".local\",\"canonical_url\":\"http://"); body += deviceIdentity.hostname();
  body += F(".local\",\"active_hostname\":\""); body += hostname;
  body += F("\",\"persisted\":"); body += deviceIdentity.ready() ? F("true") : F("false");
  body += F(",\"accepted\":"); body += accepted ? F("true") : F("false");
  body += F(",\"apply_pending\":"); body += networkApplyPending ? F("true") : F("false");
  body += F(",\"apply_after_ms\":"); body += accepted ? 1000 : 0;
  body += F(",\"error\":\""); appendJsonEscaped(body, deviceIdentity.error().c_str()); body += F("\"}");
  return body;
}

bool sendToAvr(uint8_t opcode, uint8_t requestSequence,
               const uint8_t *payload, uint8_t payloadLength) {
  const bool sent = link.send(opcode, requestSequence, payload, payloadLength);
  if (sent) {
    // The longest routine response is below 6 ms at 38400 baud. Keep a full
    // margin so FastLED cannot mask interrupts after only part of it arrived.
    ledResumeAt = millis() + 12U;
  }
  return sent;
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
  if (otaActive || otaRebootAt) { server.send(503, "text/plain", "ESP update in progress"); return false; }
  if (avrUploadOwner && server.request() && server.request()->method() != HTTP_GET) {
    server.send(409, "text/plain", "AVR image upload in progress"); return false;
  }
  if (server.request() && webAccess.allowed(server.request())) return true;
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
    default: return "idle";
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
  body += F("\",\"protocol_version\":"); body += BoardCore::BuildInfo::kProtocolVersion;
  body += F(",\"hostname\":\""); body += hostname;
  body += F("\",\"ip\":\""); body += WiFi.softAPIP().toString();
  body += F("\",\"mode\":\""); body += modeName();
  body += F("\",\"avr_online\":"); body += avrOnline ? F("true") : F("false");
  body += F(",\"avr_version\":");
  if (avrOnline && avrVersionKnown) {
    body += '"'; body += avrVersion[0]; body += '.'; body += avrVersion[1];
    body += '.'; body += avrVersion[2]; body += '"';
  } else body += F("null");
  body += F(",\"audio_cues\":[");
  const uint8_t cueCount = BoardCore::AudioCues::supportedCount(avrOnline, avrVersionKnown, avrVersion);
  for (uint8_t i = 0; i < cueCount; ++i) {
    if (i) body += ',';
    body += '"'; body += BoardCore::AudioCues::name(i); body += '"';
  }
  body += ']';
  body += F(",\"ap_started\":"); body += apStarted ? F("true") : F("false");
  body += F(",\"ap_enabled\":"); body += networkConfig.accessPointEnabled() ? F("true") : F("false");
  body += F(",\"ap_ip\":\""); body += WiFi.softAPIP().toString(); body += '"';
  body += F(",\"station_enabled\":"); body += networkConfig.stationEnabled() ? F("true") : F("false");
  body += F(",\"station_connected\":"); body += networkConfig.stationConnected() ? F("true") : F("false");
  body += F(",\"station_ssid\":\""); body += networkConfig.stationSsid(); body += '"';
  body += F(",\"station_ip\":\""); body += WiFi.localIP().toString(); body += '"';
  body += F(",\"station_rssi\":"); body += networkConfig.stationConnected() ? WiFi.RSSI() : 0;
  body += F(",\"mdns_started\":"); body += mdnsStarted ? F("true") : F("false");
  body += F(",\"ssdp_started\":"); body += ssdpStarted ? F("true") : F("false");
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
  body += F(",\"programming_latch\":"); body += programmingLatched ? F("true") : F("false");
  body += F(",\"link_mode\":\"");
  body += PAKAR_USE_BUSY_WIRE ? F("busy-wire") : F("polling");
  body += '"';
  body += F(",\"active_inputs\":"); body += activeInputs;
  body += F(",\"input_9\":"); body += activeInput9 ? F("true") : F("false");
  body += F(",\"active_outputs\":"); body += activeOutputs;
  const time_t currentTime = time(nullptr);
  body += F(",\"ntp_synced\":");
  body += currentTime > 1700000000 ? F("true") : F("false");
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
  if (!networkConfig.stationConnected() || ntpConfigured) return;
  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
  ntpConfigured = true;
}

bool applyStreamPacket(const uint8_t *packet, size_t length) {
  if (packet == nullptr || length < 10U || length > 1300U ||
      memcmp(packet, "PKR1", 4) != 0) return false;
  const uint16_t offset = packet[6] | static_cast<uint16_t>(packet[7]) << 8;
  const uint8_t count = packet[8];
  const uint8_t flags = packet[9];
  if (length != static_cast<size_t>(10U + count * 3U)) return false;
  if (!grid.stagePixels(offset, packet + 10, count)) return false;
  if ((flags & 0x01U) != 0) streamFrameReady = true;
  return true;
}

void handleWebSocketMessage(AsyncWebSocketClient *client, uint8_t opcode,
                           const uint8_t *payload, size_t length) {
  if (!webAccess.socketAllowed(client->id())) { client->close(1008, "Sign in again"); return; }
  if (otaActive || otaRebootAt) return;
  if (opcode == WS_BINARY) { applyStreamPacket(payload, length); return; }
  if (opcode != WS_TEXT || length > 256U) return;
  String message;
  message.reserve(length);
  message.concat(reinterpret_cast<const char *>(payload), length);
  if (message.indexOf(F("\"type\":\"status\"")) >= 0) {
    String status = buildStatusJson();
    client->text(status);
  } else if (message.indexOf(F("\"type\":\"ping\"")) >= 0) {
    client->text("{\"type\":\"pong\"}");
  }
}

struct WsAssembly { size_t used; size_t length; uint8_t opcode; uint8_t data[1]; };
void onWebSocketEvent(AsyncWebSocket *, AsyncWebSocketClient *client,
                      AwsEventType type, void *argument, uint8_t *payload, size_t length) {
  if (type == WS_EVT_DISCONNECT || type == WS_EVT_ERROR) {
    webAccess.forgetSocket(client->id());
    if (client->_tempObject) { free(client->_tempObject); client->_tempObject = nullptr; }
    return;
  }
  if (type == WS_EVT_CONNECT) {
    if (webSocket.count() > 8) { client->close(1013, "Client limit reached"); return; }
    if (!webAccess.bindSocket(client->id(), static_cast<AsyncWebServerRequest *>(argument))) {
      client->close(1008, "Sign in required"); return;
    }
    client->text(buildStatusJson()); return;
  }
  if (type != WS_EVT_DATA) return;
  AwsFrameInfo *info = static_cast<AwsFrameInfo *>(argument);
  if (!info || !info->final || info->num != 0 || info->len > 1300 ||
      info->index + length > info->len) { client->close(1009, "Message exceeds stream bounds"); return; }
  if (info->index == 0 && info->len == length) {
    handleWebSocketMessage(client, info->opcode, payload, length); return;
  }
  if (info->index == 0) {
    if (client->_tempObject) free(client->_tempObject);
    auto *assembly = static_cast<WsAssembly *>(malloc(sizeof(WsAssembly) + info->len));
    client->_tempObject = assembly;
    if (!assembly) { client->close(1013, "Message buffer unavailable"); return; }
    assembly->used = 0; assembly->length = info->len; assembly->opcode = info->opcode;
  }
  auto *assembly = static_cast<WsAssembly *>(client->_tempObject);
  if (!assembly || assembly->used != info->index || assembly->length != info->len ||
      assembly->used + length > assembly->length) { client->close(1002, "Invalid message sequence"); return; }
  memcpy(assembly->data + assembly->used, payload, length); assembly->used += length;
  if (assembly->used == assembly->length) {
    handleWebSocketMessage(client, assembly->opcode, assembly->data, assembly->length);
    free(assembly); client->_tempObject = nullptr;
  }
}

GameMode parseMode(const String &value) {
  if (value == F("snake")) return kModeSnake;
  if (value == F("first-wins")) return kModeFirstWins;
  if (value == F("cell")) return kModeCellSelect;
  return kModeIdle;
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
         cue < BoardCore::AudioCues::supportedCount(avrOnline, avrVersionKnown, avrVersion);
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
  const String command = jsonStringValue(message, "command");
  if (command == F("status") ||
      jsonStringValue(message, "type") == F("status")) {
    result = F("status will be published");
    return true;
  }
  if (command == F("mode")) {
    const String value = jsonStringValue(message, "value");
    if (value != F("idle") && value != F("cell") &&
        value != F("first-wins") && value != F("snake")) {
      result = F("mode must be idle, cell, first-wins, or snake");
      return false;
    }
    mode = parseMode(value);
    winner = 0;
    if (mode == kModeSnake) startSnake();
    else if (mode == kModeIdle) grid.fill(kCellOff);
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
  avrOnline = true;
  lastAvrFrameAt = millis();
  if ((frame.opcode == kAck || frame.opcode == kErrorResponse) &&
      frame.sequence == awaitedSequence && frame.payloadLength >= 2 &&
      frame.payload[0] == awaitedOpcode) {
    awaitedReplySeen = true;
    awaitedReplyOk = frame.opcode == kAck && frame.payload[1] == kNoError;
  }
  if (frame.opcode == kHelloResponse) {
    avrVersionKnown = frame.payloadLength >= 9 &&
                      frame.payload[0] == BoardCore::BuildInfo::kProtocolVersion &&
                      frame.payload[1] == kRoleAvr;
    if (avrVersionKnown) memcpy(avrVersion, frame.payload + 6, sizeof(avrVersion));
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
    return webAccess.allowed(request) && WebAccess::sameOrigin(request);
  });
  auto info = [](AsyncWebServerRequest *request) {
    String body = F("{\"product\":\"PakarController\",\"product_id\":\"PakarController\",\"name\":\"PakarController\",\"model\":\"ESP8266 + ATmega328P\",\"version\":\"");
    body += BoardCore::BuildInfo::kVersion;
    body += F("\",\"sketch_size\":"); body += ESP.getSketchSize();
    body += F(",\"size\":"); body += ESP.getSketchSize();
    body += F(",\"free\":"); body += ESP.getFreeSketchSpace();
    body += F(",\"hash\":\""); body += ESP.getSketchMD5(); body += '"';
    body += F(",\"framework\":\"ESPAsyncWebServer\",\"framework_version\":\"3.12.1\"");
    body += F(",\"sketch_md5\":\""); body += ESP.getSketchMD5();
    body += F("\",\"phase\":\""); body += otaPhase;
    body += F("\",\"uploaded_bytes\":"); body += otaUploadedBytes;
    body += F(",\"free_heap\":"); body += ESP.getFreeHeap();
    body += F(",\"upload\":\"/update\",\"download\":\"/firmware/download\",\"filesystem_ota\":false}");
    auto response = request->beginResponse(200, "application/json", body);
    response->addHeader("Cache-Control", "no-store"); request->send(response);
  };
  server.native().on("/update/info", HTTP_GET, info);
  server.native().on("/info", HTTP_GET, [](AsyncWebServerRequest *request) {
    String text = F("product: PakarController\nmodel: ESP8266 + ATmega328P\nhostname: "); text += hostname;
    text += F("\nversion: "); text += BoardCore::BuildInfo::kVersion;
    text += F("\nweb server: ESPAsyncWebServer 3.12.1\nfirmware hash: "); text += ESP.getSketchMD5();
    text += F("\nflash size: "); text += ESP.getFlashChipSize();
    text += F("\nheap memory: "); text += ESP.getFreeHeap();
    text += F("\nuptime seconds: "); text += millis() / 1000;
    text += F("\nreset reason: "); text += ESP.getResetReason();
    text += F("\nprotocol revision: "); text += BoardCore::BuildInfo::kProtocolVersion;
    request->send(200, "text/plain", text);
  });
  server.native().on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", F("<!doctype html><meta name='viewport' content='width=device-width'><title>PakarController OTA</title><style>body{max-width:600px;margin:3rem auto;padding:20px;background:#11161e;color:#eef2f7;font:16px system-ui}input,button{margin:12px 0;padding:10px;display:block}a{color:#68a7ff}</style><h1>ESP firmware update</h1><p>Upload the PakarController ESP .bin or .bin.gz. The AVR and stored settings are preserved. Use the MD5 of the uploaded file when available; it checks transfer integrity, not authenticity.</p><form method='post' enctype='multipart/form-data'><label>File MD5 (optional)<input name='MD5' minlength='32' maxlength='32'></label><input type='file' name='firmware' accept='.bin,.gz' required><button>Upload and restart ESP</button></form><a href='/'>Back to controller</a>"));
  });
  server.native().on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    const bool ownsUpdate = otaOwner && request->getAttribute("ota_id") == String(otaOwner);
    if (!ownsUpdate || request->hasAttribute("ota_error") || !request->hasAttribute("ota_uploaded")) {
      if (ownsUpdate) {
        if (Update.isRunning()) Update.end(false);
        otaOwner = 0; otaActive = false; otaPhase = F("failed");
      }
      request->send(400, "text/plain", request->getAttribute("ota_error", String(F("No complete firmware uploaded")))); return;
    }
    const bool complete = otaUploadedBytes && Update.end(true);
    otaOwner = 0; otaActive = false;
    if (!complete) { otaPhase = F("failed"); request->send(400, "text/plain", Update.getErrorString()); return; }
    otaPhase = F("ready-to-reboot");
    request->send(200, "application/json", "{\"updated\":true,\"rebooting\":true}");
    otaRebootAt = millis() + 2000;
  }, [](AsyncWebServerRequest *request, String, size_t index, uint8_t *data, size_t length, bool final) {
    // Upload callbacks run before the normal middleware pipeline: authorize before writing anything.
    if (!webAccess.allowed(request) || !WebAccess::sameOrigin(request)) return;
    static uint32_t nextOwner = 0;
    if (index == 0) {
      if (otaOwner || otaRebootAt || avrUploadOwner || server.processing() || programmingLatched || request->hasAttribute("ota_id")) {
        request->setAttribute("ota_error", "Another operation is active or multiple files were supplied"); return;
      }
      if (++nextOwner == 0) ++nextOwner;
      otaOwner = nextOwner; const uint32_t id = otaOwner;
      request->setAttribute("ota_id", String(id).c_str());
      request->onDisconnect([id]() {
        if (otaOwner != id) return;
        // Uploads are capped below Update.size(), so incomplete end cannot commit boot metadata.
        if (Update.isRunning()) Update.end(false);
        otaOwner = 0; otaActive = false; otaPhase = F("aborted"); otaErrorCuePending = true;
      });
      otaUploadedBytes = 0; otaPhase = F("receiving"); otaActive = true;
      const uint32_t maximum = (ESP.getFreeSketchSpace() - 0x1000) & 0xfffff000;
      Update.runAsync(true);
      if (!Update.begin(maximum, U_FLASH)) {
        request->setAttribute("ota_error", Update.getErrorString().c_str());
        otaOwner = 0; otaActive = false; otaPhase = F("failed"); return;
      }
      String md5 = request->arg("MD5"); if (md5.isEmpty()) md5 = request->arg("md5");
      if (!md5.isEmpty()) {
        bool valid = md5.length() == 32;
        for (size_t i = 0; i < md5.length(); ++i) valid = valid && isxdigit(static_cast<unsigned char>(md5[i]));
        if (!valid || !Update.setMD5(md5.c_str())) {
          request->setAttribute("ota_error", "Invalid MD5"); Update.end(false);
          otaOwner = 0; otaActive = false; otaPhase = F("failed"); return;
        }
      }
    }
    if (request->hasAttribute("ota_error") || request->getAttribute("ota_id") != String(otaOwner)) return;
    if (!Update.isRunning() || index != otaUploadedBytes || index + length >= Update.size() || Update.write(data, length) != length) {
      request->setAttribute("ota_error", (Update.hasError() ? Update.getErrorString() : String(F("Invalid upload sequence or insufficient OTA space"))).c_str());
      if (Update.isRunning()) Update.end(false);
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
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html; charset=utf-8", kWebUi);
  });
  server.on("/api/v1/status", HTTP_GET, []() {
    if (!authenticate()) return;
    sendStatusJson();
  });
  server.on("/api/v1/cells/mapping", HTTP_GET, []() {
    if (!authenticate()) return;
    String body;
    body.reserve(1300);
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
  server.on("/api/v1/cells/mapping", HTTP_POST, []() {
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
    String xml = F("<?xml version=\"1.0\"?><root xmlns=\"urn:schemas-upnp-org:device-1-0\"><specVersion><major>1</major><minor>0</minor></specVersion><device><deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType><friendlyName>");
    xml += hostname;
    xml += F("</friendlyName><manufacturer>AtomicDeploy</manufacturer><modelName>PakarController</modelName><UDN>uuid:");
    char uuid[37]; snprintf(uuid, sizeof(uuid), "38323636-4558-4dda-9188-cda0e6%06x", ESP.getChipId()); xml += uuid;
    xml += F("</UDN><presentationURL>http://"); xml += hostname;
    xml += F(".local/</presentationURL></device></root>");
    server.send(200, "text/xml", xml);
  });
  server.on("/api/v1/time", HTTP_GET, []() {
    if (!authenticate()) return;
    const time_t currentTime = time(nullptr);
    String body = F("{\"synced\":");
    body += currentTime > 1700000000 ? F("true") : F("false");
    body += F(",\"utc_epoch\":");
    body += currentTime > 1700000000 ? static_cast<uint32_t>(currentTime) : 0U;
    body += '}';
    server.send(200, "application/json", body);
  });
  server.on("/api/v1/wifi/scan", HTTP_GET, []() {
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
      body += WiFi.encryptionType(index) == ENC_TYPE_NONE ? F("false") : F("true");
      body += '}';
    }
    body += F("]}");
    WiFi.scanDelete();
    server.send(200, "application/json", body);
  });
  server.on("/api/v1/mode", HTTP_POST, []() {
    if (!authenticate()) return;
    const String value = server.arg("value");
    if (value != F("idle") && value != F("cell") &&
        value != F("first-wins") && value != F("snake")) {
      server.send(400, "text/plain",
                  "mode must be idle, cell, first-wins, or snake");
      return;
    }
    const GameMode requested = parseMode(value);
    mode = requested;
    winner = 0;
    if (mode == kModeSnake) startSnake();
    else if (mode == kModeIdle) grid.fill(kCellOff);
    else grid.resetWhite();
    saveLedPreferences();
    const uint8_t payload[] = {static_cast<uint8_t>(mode)};
    sendToAvr(kSetMode, sequence++, payload, sizeof(payload));
    server.send(202, "application/json", "{\"accepted\":true}");
  });
  server.on("/api/v1/cell", HTTP_POST, []() {
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
  server.on("/api/v1/game/reset", HTTP_POST, []() {
    if (!authenticate()) return;
    winner = 0;
    if (mode == kModeSnake) startSnake(); else grid.resetWhite();
    saveLedPreferences();
    sendToAvr(kGameReset, sequence++);
    server.send(202, "application/json", "{\"accepted\":true}");
  });
  server.on("/api/v1/snake/direction", HTTP_POST, []() {
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
  server.on("/api/v1/brightness", HTTP_POST, []() {
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
  server.on("/api/v1/effect", HTTP_POST, []() {
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
  server.on("/api/v1/buzzer", HTTP_POST, []() {
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
  server.on("/api/v1/audio", HTTP_POST, []() {
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
  server.on("/api/v1/display", HTTP_POST, []() {
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
  server.on("/api/v1/outputs", HTTP_POST, []() {
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
  server.on("/api/v1/device/name", HTTP_GET, []() {
    if (!authenticate()) return;
    server.send(200, "application/json", deviceNameJson());
  });
  server.on("/api/v1/device/name", HTTP_POST, []() {
    if (!authenticate()) return;
    if (otaActive || otaRebootAt || avrUploadOwner || programmingLatched || networkApplyPending) {
      server.send(409, "application/json", "{\"error\":\"device_busy\"}"); return;
    }
    char normalized[64];
    const String input = server.arg("hostname");
    if (input.length() != strlen(input.c_str()) || !BoardCore::Identity::normalizeName(input.c_str(), normalized)) {
      server.send(400, "application/json", "{\"error\":\"invalid_hostname\"}"); return;
    }
    if (!scheduleDeviceRename(normalized)) {
      server.send(503, "application/json", deviceNameJson()); return;
    }
    server.send(202, "application/json", deviceNameJson(true));
  });
  server.on("/api/v1/network", HTTP_GET, []() {
    if (!authenticate()) return;
    String body = F("{\"station_enabled\":");
    body += networkConfig.stationEnabled() ? F("true") : F("false");
    body += F(",\"station_connected\":");
    body += networkConfig.stationConnected() ? F("true") : F("false");
    body += F(",\"ap_enabled\":");
    body += networkConfig.accessPointEnabled() ? F("true") : F("false");
    body += F(",\"station_ssid\":\""); body += networkConfig.stationSsid();
    body += F("\",\"station_ip\":\""); body += WiFi.localIP().toString();
    body += F("\",\"ap_ip\":\""); body += WiFi.softAPIP().toString();
    body += F("\"}");
    server.send(200, "application/json", body);
  });
  server.on("/api/v1/network", HTTP_POST, []() {
    if (!authenticate()) return;
    const bool enabled = server.arg("station_enabled") != F("false") &&
                         server.arg("station_enabled") != F("0");
    const bool apEnabled = !server.hasArg("ap_enabled") ||
                           (server.arg("ap_enabled") != F("false") &&
                            server.arg("ap_enabled") != F("0"));
    if (!networkConfig.setStation(server.arg("ssid"), server.arg("password"),
                                  enabled) ||
        !networkConfig.setAccessPointEnabled(apEnabled) ||
        !networkConfig.save()) {
      server.send(400, "text/plain", networkConfig.lastError());
      return;
    }
    server.send(202, "application/json", "{\"accepted\":true}");
    // Flush the response through the old interface before changing station
    // association. The recovery AP remains active throughout the reconfigure.
    networkApplyAt = millis() + 250U;
    networkApplyPending = true;
  });

  server.on("/api/v1/cloud", HTTP_GET, []() {
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
  server.on("/api/v1/cloud", HTTP_POST, []() {
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
  server.on("/api/v1/cloud/reconnect", HTTP_POST, []() {
    if (!authenticate()) return;
    cloudServices.applyConfiguration();
    server.send(202, "application/json", "{\"accepted\":true}");
  });

  server.on("/api/v1/avr/probe", HTTP_GET, []() {
    if (!authenticate()) return;
    uint8_t signature[3] = {};
    const bool ok = avrIsp.probe(signature);
    if (!ok) {
      server.send(503, "text/plain", avrIsp.lastError());
      return;
    }
    server.send(200, "application/json", "{\"signature\":\"1E950F\"}");
  });
  server.on("/api/v1/avr/backup", HTTP_POST, []() {
    if (!authenticate()) return;
    if (!avrIsp.backup(LittleFS, kBackupDirectory)) {
      server.send(500, "text/plain", avrIsp.lastError());
      return;
    }
    server.send(200, "application/json", "{\"backed_up\":true}");
  });
  server.on("/api/v1/avr/backup/flash", HTTP_GET, []() {
    if (!authenticate()) return;
    File file = LittleFS.open(String(kBackupDirectory) + F("/flash.bin"), "r");
    if (!file) { server.send(404, "text/plain", "no flash backup"); return; }
    server.streamFile(file, "application/octet-stream");
    file.close();
  });
  server.on("/api/v1/avr/backup/eeprom", HTTP_GET, []() {
    if (!authenticate()) return;
    File file = LittleFS.open(String(kBackupDirectory) + F("/eeprom.bin"), "r");
    if (!file) { server.send(404, "text/plain", "no EEPROM backup"); return; }
    server.streamFile(file, "application/octet-stream");
    file.close();
  });
  server.on("/api/v1/avr/program", HTTP_POST, []() {
    if (!authenticate()) return;
    File image = LittleFS.open(kAvrImagePath, "r");
    if (!image || image.size() != AvrIsp::kFlashBytes) {
      if (image) image.close();
      server.send(400, "text/plain", "store one complete 32768-byte AVR image first");
      return;
    }
    const bool resume = programmingLatched;
    const LedGrid::Pattern patternBeforeUpdate = grid.pattern();
    const uint8_t armed[] = {1};
    bool latchArmed = resume;
    if (!resume) {
      if (!avrIsp.backup(LittleFS, kBackupDirectory)) {
        image.close();
        server.send(500, "text/plain",
                    String("backup failed: ") + avrIsp.lastError());
        return;
      }
      if (!waitForAvr()) {
        image.close();
        server.send(500, "text/plain",
                    "backup succeeded but AVR UART did not return");
        return;
      }
      // One-time migration accepts the original tone opcode; current images
      // use the richer cue command. Both paths finish before ISP reset asserts.
      if (!playUpdateStartCue()) {
        image.close();
        server.send(500, "text/plain",
                    "AVR rejected both update cue formats");
        return;
      }
      latchArmed =
          requestAndWaitForAck(kProgrammingState, armed, sizeof(armed));
      // The first migration image has no latch opcode. A complete ISP backup
      // is already durable, so allow that one transition; every resulting and
      // subsequent PakarController AVR image must prove the latch below.
      programmingLatched = latchArmed;
    }
    const bool ok = avrIsp.programFlash(image);
    image.close();
    if (!ok) { server.send(500, "text/plain", avrIsp.lastError()); return; }
    if (!waitForAvr()) {
      server.send(500, "text/plain",
                  "flash verified but the AVR did not return; Prog remains latched");
      return;
    }
    if (!latchArmed &&
        !requestAndWaitForAck(kProgrammingState, armed, sizeof(armed))) {
      server.send(500, "text/plain",
                  "flash verified but the new AVR Prog latch did not verify");
      return;
    }
    programmingLatched = true;
    const uint8_t released[] = {0};
    if (!requestAndWaitForAck(kProgrammingState, released, sizeof(released))) {
      server.send(500, "text/plain",
                  "flash verified but Prog could not be cleared");
      return;
    }
    programmingLatched = false;
    playUpdateSuccessCue();
    grid.setPattern(patternBeforeUpdate);
    saveLedPreferences();
    server.send(200, "application/json",
                resume
                    ? "{\"programmed\":true,\"verified\":true,\"resumed\":true}"
                    : "{\"programmed\":true,\"verified\":true}");
  });

  server.native().on("/api/v1/avr/image", HTTP_POST,
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
      if (!webAccess.allowed(request) || !WebAccess::sameOrigin(request)) return;
      static uint32_t nextUpload = 0;
      if (index == 0) {
        if (avrUploadOwner || otaActive || otaRebootAt || server.processing() || request->hasAttribute("upload_id")) {
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
  if (length > 0) applyStreamPacket(packet, static_cast<size_t>(length));
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
    telnetClient.println(
        F("help | status | mode <idle|cell|first-wins|snake> | "
          "cell <1..80> <white|red|off> [immediate|fade|pulse]"));
    telnetClient.println(
        F("brightness <1..255> | effect <none|rainbow|fire> | "
          "display <-999..9999> | display stop | audio <cue> | reset | quit"));
    telnetClient.println(F("display text <once|loop|interval> <speed_ms> <hold_ms> <pause_seconds> <scroll 0|1> <TEXT>"));
    telnetClient.println(F("device name | device rename <NAME[.local]>"));
    telnetPrompt();
    return;
  }
  if (line == F("status")) {
    telnetClient.println(buildStatusJson());
    telnetPrompt();
    return;
  }
  if (line == F("device name")) { telnetClient.println(deviceNameJson()); telnetPrompt(); return; }
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
      pinMode(D1, OUTPUT_OPEN_DRAIN);
      digitalWrite(D1, LOW);
      delay(10);
      digitalWrite(D1, HIGH);
      pinMode(D1, INPUT);

      const uint32_t startedAt = millis();
      uint32_t nextYieldAt = startedAt + 10;
      uint32_t samples = 0;
      uint32_t lowSamples = 0;
      uint32_t transitions = 0;
      uint8_t previous = digitalRead(D4);
      while (static_cast<uint32_t>(millis() - startedAt) < 1500U) {
        const uint8_t current = digitalRead(D4);
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

}  // namespace

void setup() {
#if PAKAR_USE_BUSY_WIRE
  // GPIO0 is pulled high by the NodeMCU boot circuit. Explicitly keep the AVR
  // blocked until UART, Wi-Fi, Web, and the LED renderer are ready.
  busyLine.begin();
#endif
  pinMode(D1, INPUT);
  // Never erase damaged storage and silently restore public credentials.
  LittleFS.setConfig(LittleFSConfig().setAutoFormat(false));
  const bool filesystemMounted = LittleFS.begin();
  accessConfig.begin(LittleFS, filesystemMounted);
  networkConfig.begin(LittleFS);
  deviceIdentity.begin(LittleFS, filesystemMounted);
  cloudConfig.begin(LittleFS);
  grid.begin();
  loadLedPreferences();
  loadCellMapping();
  hostLink.begin(115200, onHostFrame);
  avrSerial.begin(kBaud);
  link.begin(kBaud, onAvrFrame);

  // This is deliberately stable. Chip identity belongs in discovery metadata,
  // not in the human-facing host or recovery AP name.
  hostname = deviceIdentity.hostname();
  apStarted = networkConfig.configure(hostname.c_str(), kDevelopmentPassword);
  cloudServices.begin(hostname, handleCloudCommand, buildStatusJson);
  stream.begin(kStreamPort);
  telnetServer.begin();
  telnetServer.setNoDelay(true);

  registerApi();
  webAccess.registerRoutes(server.native(), []() {
    webSocket.closeAll(1008, "Sign in again");
    if (telnetClient) telnetClient.stop();
    telnetAuthenticated = false;
  }, []() { return otaActive || otaRebootAt || avrUploadOwner || server.processing() || programmingLatched; });
  webSocket.onEvent(onWebSocketEvent);
  server.native().addHandler(&webSocket);
  Update.onStart([]() {
    otaStartCuePending = true;
  });
  Update.onEnd([]() {
    otaSuccessCuePending = true;
  });
  Update.onError([](uint8_t) {
    otaErrorCuePending = true;
  });
  registerAsyncOta();
  server.begin();
  serviceDiscovery();
  sendToAvr(kHello, sequence++);
#if PAKAR_USE_BUSY_WIRE
  setAvrTransmitBlocked(false);
#endif
}

void loop() {
  const uint32_t now = millis();
  link.service();
  hostLink.service();
  // Render before issuing UART requests. The AVR is request/response only, so
  // its reply arrives while FastLED is not masking ESP8266 interrupts.
  const bool ledFrameDue = !otaActive &&
      static_cast<int32_t>(now - ledResumeAt) >= 0 &&
      ((streamFrameReady && grid.readyToShow(now)) || grid.needsRender(now));
  if (ledFrameDue) {
    setAvrTransmitBlocked(true);
    // The busy edge prevents a new unsolicited AVR frame, but one may have
    // passed the AVR's LOW check just before this edge and still be draining
    // from its hardware UART. Consume that bounded frame before FastLED masks
    // ESP8266 interrupts for the 480-pixel transfer.
    serviceAvrFor(8U);
    if (streamFrameReady) {
      grid.showStaged();
      streamFrameReady = false;
    } else {
      grid.service(now);
    }
    setAvrTransmitBlocked(false);
  }
  server.service();
  webSocket.cleanupClients(8);
  webAccess.expireSockets(webSocket);
  if (otaStartCuePending) { otaStartCuePending = false; playUpdateStartCue(); }
  if (otaSuccessCuePending) { otaSuccessCuePending = false; playUpdateSuccessCue(); }
  if (otaErrorCuePending) {
    otaErrorCuePending = false; const uint8_t cue[] = {kCueError}; sendToAvr(kAudioCue, sequence++, cue, sizeof(cue));
  }
  if (otaRebootAt && static_cast<int32_t>(millis() - otaRebootAt) >= 0) ESP.restart();
  if (!otaActive && !otaRebootAt && !uploadFile) serviceTelnet();
  if (!otaActive && !otaRebootAt && networkApplyPending &&
      static_cast<int32_t>(millis() - networkApplyAt) >= 0) {
    networkApplyPending = false;
    applyNetworkConfig();
  }
  serviceDiscovery();
  configureNtp();
  // TLS reconnects can take seconds on the ESP8266. Keep them out of the
  // timing-critical ISP recovery path while the AVR's Prog latch is armed.
  if (!programmingLatched && !otaActive && !otaRebootAt) {
    cloudServices.service(networkConfig.stationConnected(),
                          time(nullptr) > 1700000000, now);
  }
  serviceSnake(now);
  serviceStream();
  const uint32_t afterServices = millis();
  if (avrOnline &&
      static_cast<uint32_t>(afterServices - lastAvrFrameAt) > 3000U) {
    avrOnline = false;
    avrVersionKnown = false;
  }
  if (!avrOnline && static_cast<uint32_t>(now - lastHelloAt) > 1000) {
    lastHelloAt = now;
    sendToAvr(kHello, sequence++);
  }
  // AVR owns its welcome melody. A first HELLO is not a completed update:
  // sending the success cue here could replace the still-playing welcome.
  if (!PAKAR_USE_BUSY_WIRE && avrOnline &&
      static_cast<uint32_t>(now - lastStatusPollAt) >= 100U) {
    lastStatusPollAt = now;
    sendToAvr(kGetStatus, sequence++);
  }
  if (static_cast<uint32_t>(now - lastWebSocketBroadcastAt) >= 500U) {
    lastWebSocketBroadcastAt = now;
    if (webSocket.count() != 0) {
      String status = buildStatusJson();
      webSocket.textAll(status);
    }
  }
  yield();
}
