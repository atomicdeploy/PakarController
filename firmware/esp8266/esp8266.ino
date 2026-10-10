#include <Arduino.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <SoftwareSerial.h>
#include <Updater.h>
#include <WebSockets4WebServer.h>
#include <WiFiUdp.h>
#include <time.h>

#include <BuildInfo.h>
#include <Protocol.h>
#include <SerialLink.h>

#include "src/AvrIsp.h"
#include "src/CloudConfig.h"
#include "src/CloudServices.h"
#include "src/LedGrid.h"
#include "src/NetworkConfig.h"
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
constexpr char kAdmin[] = "admin";
constexpr char kDevelopmentPassword[] = "pakarcontroller";
constexpr char kAvrImagePath[] = "/avr/staged.bin";
constexpr char kBackupDirectory[] = "/avr/backup";
constexpr char kLedPreferencesPath[] = "/led.cfg";

struct LedPreferences {
  uint8_t magic0;
  uint8_t magic1;
  uint8_t schema;
  uint8_t pattern;
  uint8_t brightness;
  uint8_t check;
};

ESP8266WebServer server(80);
WebSockets4WebServer webSocket;
ESP8266HTTPUpdateServer updater;
WiFiUDP stream;
WiFiServer telnetServer(23);
WiFiClient telnetClient;
SoftwareSerial avrSerial(D4, D0);
BoardCore::SerialLink link(static_cast<Stream &>(avrSerial));
BoardCore::SerialLink hostLink(Serial);
AvrIsp avrIsp(D1);
LedGrid grid;
File uploadFile;
String hostname;
bool avrOnline = false;
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
String stagedStationSsid;
String stagedStationPassword;
uint8_t snakeBody[LedGrid::kCells] = {};
uint8_t snakeLength = 0;
uint8_t snakeFood = 0;
uint8_t snakeDirection = 1;
uint32_t lastSnakeAt = 0;
bool snakeAlive = false;
bool telnetAuthenticated = false;
String telnetLine;

void saveLedPreferences();
String buildStatusJson();
bool handleCloudCommand(const String &message, String &result);
void startSnake();
bool setSnakeDirection(uint8_t direction);
void serviceSnake(uint32_t now);
void serviceTelnet();
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
  };
  hostLink.send(kStatusResponse, requestSequence, payload, sizeof(payload));
}

void onHostFrame(const BoardCore::Protocol::Frame &frame, void *) {
  switch (frame.opcode) {
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
#if PAKAR_USE_BUSY_WIRE
  digitalWrite(kBusyPin, blocked ? HIGH : LOW);
#else
  (void)blocked;
#endif
}

uint8_t ledPreferencesCheck(const LedPreferences &value) {
  return static_cast<uint8_t>(value.magic0 ^ value.magic1 ^ value.schema ^
                              value.pattern ^ value.brightness ^ 0xA5U);
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

void applyNetworkConfig() {
  MDNS.close();
  apStarted = networkConfig.configure(hostname.c_str(), kDevelopmentPassword);
  MDNS.begin(hostname.c_str());
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("pakarcontroller", "udp", kStreamPort);
  MDNS.addService("telnet", "tcp", 23);
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
  if (server.authenticate(kAdmin, kDevelopmentPassword)) return true;
  server.requestAuthentication();
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
  body.reserve(1200);
  body += F("{\"version\":\""); body += BoardCore::BuildInfo::kVersion;
  body += F("\",\"protocol_version\":"); body += BoardCore::BuildInfo::kProtocolVersion;
  body += F(",\"hostname\":\""); body += hostname;
  body += F("\",\"ip\":\""); body += WiFi.softAPIP().toString();
  body += F("\",\"mode\":\""); body += modeName();
  body += F("\",\"avr_online\":"); body += avrOnline ? F("true") : F("false");
  body += F(",\"ap_started\":"); body += apStarted ? F("true") : F("false");
  body += F(",\"ap_enabled\":"); body += networkConfig.accessPointEnabled() ? F("true") : F("false");
  body += F(",\"ap_ip\":\""); body += WiFi.softAPIP().toString(); body += '"';
  body += F(",\"station_enabled\":"); body += networkConfig.stationEnabled() ? F("true") : F("false");
  body += F(",\"station_connected\":"); body += networkConfig.stationConnected() ? F("true") : F("false");
  body += F(",\"station_ssid\":\""); body += networkConfig.stationSsid(); body += '"';
  body += F(",\"station_ip\":\""); body += WiFi.localIP().toString(); body += '"';
  body += F(",\"station_rssi\":"); body += networkConfig.stationConnected() ? WiFi.RSSI() : 0;
  body += F(",\"winner\":"); body += winner;
  body += F(",\"selected_cell\":"); body += selectedCell;
  body += F(",\"snake_alive\":"); body += snakeAlive ? F("true") : F("false");
  body += F(",\"snake_length\":"); body += snakeLength;
  body += F(",\"brightness\":"); body += grid.brightness();
  body += F(",\"effect\":\""); body += grid.patternName(); body += '"';
  body += F(",\"esp_uart_errors\":"); body += link.errors();
  body += F(",\"avr_uart_errors\":"); body += avrUartErrors;
  body += F(",\"tm1637_ack\":"); body += tm1637Ack ? F("true") : F("false");
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
  body += webSocket.connectedClients(false);
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

void onWebSocketEvent(uint8_t client, WStype_t type, uint8_t *payload,
                      size_t length) {
  if (type == WStype_CONNECTED) {
    String status = buildStatusJson();
    webSocket.sendTXT(client, status);
    return;
  }
  if (type == WStype_BIN) {
    applyStreamPacket(payload, length);
    return;
  }
  if (type != WStype_TEXT || length > 256U) return;
  String message;
  message.reserve(length);
  message.concat(reinterpret_cast<const char *>(payload), length);
  if (message.indexOf(F("\"type\":\"status\"")) >= 0) {
    String status = buildStatusJson();
    webSocket.sendTXT(client, status);
  } else if (message.indexOf(F("\"type\":\"ping\"")) >= 0) {
    String pong = F("{\"type\":\"pong\"}");
    webSocket.sendTXT(client, pong);
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
  const char *names[] = {
      "welcome", "update-start", "update-success", "error", "winner", "finish",
      "lost", "incorrect", "rapid-error", "fault", "increase", "decrease",
  };
  for (uint8_t index = 0; index <= kCueDecrease; ++index) {
    if (value == names[index]) {
      cue = index;
      return true;
    }
  }
  return false;
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
    sendToAvr(kAudioCue, sequence++, &cue, 1);
    result = F("audio cue started");
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

void registerApi() {
  server.on("/", HTTP_GET, []() {
    if (!authenticate()) return;
    server.send_P(200, "text/html; charset=utf-8", kWebUi);
  });
  server.on("/api/v1/status", HTTP_GET, []() {
    if (!authenticate()) return;
    sendStatusJson();
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
    sendToAvr(kAudioCue, sequence++, &cue, 1);
    server.send(202, "application/json", "{\"accepted\":true}");
  });
  server.on("/api/v1/display", HTTP_POST, []() {
    if (!authenticate()) return;
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

  server.on("/api/v1/avr/image", HTTP_POST,
    []() {
      if (!authenticate()) return;
      if (uploadFile) uploadFile.close();
      File image = LittleFS.open(kAvrImagePath, "r");
      const bool valid = image && image.size() == AvrIsp::kFlashBytes;
      if (image) image.close();
      server.send(valid ? 201 : 400, "text/plain",
                  valid ? "AVR image stored" : "image must be exactly 32768 bytes");
    },
    []() {
      if (!server.authenticate(kAdmin, kDevelopmentPassword)) return;
      HTTPUpload &upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        LittleFS.mkdir("/avr");
        uploadFile = LittleFS.open(kAvrImagePath, "w");
      } else if (upload.status == UPLOAD_FILE_WRITE && uploadFile) {
        uploadFile.write(upload.buf, upload.currentSize);
      } else if (upload.status == UPLOAD_FILE_END || upload.status == UPLOAD_FILE_ABORTED) {
        if (uploadFile) uploadFile.close();
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
          "display <-999..9999> | audio <cue> | reset | quit"));
    telnetPrompt();
    return;
  }
  if (line == F("status")) {
    telnetClient.println(buildStatusJson());
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
  if (telnetServer.hasClient()) {
    WiFiClient candidate = telnetServer.accept();
    if (telnetClient && telnetClient.connected()) {
      candidate.println(F("PakarController shell already has a client."));
      candidate.stop();
    } else {
      telnetClient = candidate;
      telnetClient.setNoDelay(true);
      telnetAuthenticated = false;
      telnetLine = String();
      telnetClient.println(F("PakarController authenticated shell"));
      telnetClient.print(F("Password: "));
    }
  }
  if (!telnetClient || !telnetClient.connected()) return;
  uint8_t budget = 96;
  while (budget-- != 0 && telnetClient.available() != 0) {
    const char ch = static_cast<char>(telnetClient.read());
    if (ch == '\r') continue;
    if (ch != '\n') {
      if (telnetLine.length() < 120 && ch >= 0x20 && ch < 0x7f) telnetLine += ch;
      continue;
    }
    String line = telnetLine;
    telnetLine = String();
    if (!telnetAuthenticated) {
      if (line == kDevelopmentPassword) {
        telnetAuthenticated = true;
        telnetClient.println(F("Authenticated. Type help for commands."));
        telnetPrompt();
      } else {
        telnetClient.println(F("Authentication failed."));
        telnetClient.stop();
      }
      continue;
    }
    executeTelnetCommand(line);
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
  pinMode(kBusyPin, OUTPUT);
  setAvrTransmitBlocked(true);
#endif
  pinMode(D1, INPUT);
  LittleFS.begin();
  networkConfig.begin(LittleFS);
  cloudConfig.begin(LittleFS);
  grid.begin();
  loadLedPreferences();
  hostLink.begin(115200, onHostFrame);
  avrSerial.begin(kBaud);
  link.begin(kBaud, onAvrFrame);

  hostname = String(F("pakarcontroller-")) + String(ESP.getChipId(), HEX);
  apStarted = networkConfig.configure(hostname.c_str(), kDevelopmentPassword);
  cloudServices.begin(hostname, handleCloudCommand, buildStatusJson);
  MDNS.begin(hostname.c_str());
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("pakarcontroller", "udp", kStreamPort);
  MDNS.addService("telnet", "tcp", 23);
  stream.begin(kStreamPort);
  telnetServer.begin();
  telnetServer.setNoDelay(true);

  registerApi();
  webSocket.setAuthorization(kAdmin, kDevelopmentPassword);
  server.addHook(webSocket.hookForWebserver("/ws", onWebSocketEvent));
  Update.onStart([]() {
    playUpdateStartCue();
  });
  Update.onEnd([]() {
    playUpdateSuccessCue();
  });
  Update.onError([](uint8_t) {
    const uint8_t cue[] = {kCueError};
    requestAndWaitForAck(kAudioCue, cue, sizeof(cue));
  });
  updater.setup(&server, "/update", kAdmin, kDevelopmentPassword);
  server.begin();
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
  if (static_cast<int32_t>(now - ledResumeAt) >= 0) {
    setAvrTransmitBlocked(true);
    if (streamFrameReady) {
      grid.showStaged();
      streamFrameReady = false;
    } else {
      grid.service(now);
    }
    setAvrTransmitBlocked(false);
  }
  server.handleClient();
  webSocket.loop();
  serviceTelnet();
  if (networkApplyPending &&
      static_cast<int32_t>(millis() - networkApplyAt) >= 0) {
    networkApplyPending = false;
    applyNetworkConfig();
  }
  MDNS.update();
  configureNtp();
  // TLS reconnects can take seconds on the ESP8266. Keep them out of the
  // timing-critical ISP recovery path while the AVR's Prog latch is armed.
  if (!programmingLatched) {
    cloudServices.service(networkConfig.stationConnected(),
                          time(nullptr) > 1700000000, now);
  }
  serviceSnake(now);
  serviceStream();
  const uint32_t afterServices = millis();
  if (avrOnline &&
      static_cast<uint32_t>(afterServices - lastAvrFrameAt) > 3000U) {
    avrOnline = false;
  }
  if (!avrOnline && static_cast<uint32_t>(now - lastHelloAt) > 1000) {
    lastHelloAt = now;
    sendToAvr(kHello, sequence++);
  }
  if (!PAKAR_USE_BUSY_WIRE && avrOnline &&
      static_cast<uint32_t>(now - lastStatusPollAt) >= 100U) {
    lastStatusPollAt = now;
    sendToAvr(kGetStatus, sequence++);
  }
  if (static_cast<uint32_t>(now - lastWebSocketBroadcastAt) >= 500U) {
    lastWebSocketBroadcastAt = now;
    if (webSocket.connectedClients(false) != 0) {
      String status = buildStatusJson();
      webSocket.broadcastTXT(status);
    }
  }
  yield();
}
