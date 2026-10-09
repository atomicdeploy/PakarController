#include <Arduino.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <SoftwareSerial.h>
#include <Updater.h>
#include <WiFiUdp.h>

#include <BuildInfo.h>
#include <Protocol.h>
#include <SerialLink.h>

#include "src/AvrIsp.h"
#include "src/LedGrid.h"
#include "src/NetworkConfig.h"
#include "src/WebUi.h"

using namespace BoardCore::Protocol;

namespace {

constexpr uint32_t kBaud = 38400;
constexpr uint16_t kStreamPort = 7890;
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
ESP8266HTTPUpdateServer updater;
WiFiUDP stream;
SoftwareSerial avrSerial(D4, D0);
BoardCore::SerialLink link(static_cast<Stream &>(avrSerial));
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
bool awaitedReplySeen = false;
bool awaitedReplyOk = false;
uint8_t awaitedSequence = 0;
uint8_t awaitedOpcode = 0;
String consoleLine;
bool consoleLineMode = false;
bool networkApplyPending = false;
uint32_t networkApplyAt = 0;

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
}

bool sendToAvr(uint8_t opcode, uint8_t requestSequence,
               const uint8_t *payload = nullptr, uint8_t payloadLength = 0) {
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

const char *modeName() {
  switch (mode) {
    case kModeCellSelect: return "cell";
    case kModeFirstWins: return "first-wins";
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

void sendStatusJson() {
  String body;
  body.reserve(900);
  body += F("{\"version\":\""); body += BoardCore::BuildInfo::kVersion;
  body += F("\",\"protocol_version\":"); body += BoardCore::BuildInfo::kProtocolVersion;
  body += F(",\"hostname\":\""); body += hostname;
  body += F("\",\"ip\":\""); body += WiFi.softAPIP().toString();
  body += F("\",\"mode\":\""); body += modeName();
  body += F("\",\"avr_online\":"); body += avrOnline ? F("true") : F("false");
  body += F(",\"ap_started\":"); body += apStarted ? F("true") : F("false");
  body += F(",\"ap_ip\":\""); body += WiFi.softAPIP().toString(); body += '"';
  body += F(",\"station_enabled\":"); body += networkConfig.stationEnabled() ? F("true") : F("false");
  body += F(",\"station_connected\":"); body += networkConfig.stationConnected() ? F("true") : F("false");
  body += F(",\"station_ssid\":\""); body += networkConfig.stationSsid(); body += '"';
  body += F(",\"station_ip\":\""); body += WiFi.localIP().toString(); body += '"';
  body += F(",\"station_rssi\":"); body += networkConfig.stationConnected() ? WiFi.RSSI() : 0;
  body += F(",\"winner\":"); body += winner;
  body += F(",\"selected_cell\":"); body += selectedCell;
  body += F(",\"brightness\":"); body += grid.brightness();
  body += F(",\"effect\":\""); body += grid.patternName(); body += '"';
  body += F(",\"esp_uart_errors\":"); body += link.errors();
  body += F(",\"avr_uart_errors\":"); body += avrUartErrors;
  body += F(",\"tm1637_ack\":"); body += tm1637Ack ? F("true") : F("false");
  body += F(",\"programming_latch\":"); body += programmingLatched ? F("true") : F("false");
  body += F(",\"uart_bytes_received\":"); body += link.bytesReceived();
  body += F(",\"uart_frames_received\":"); body += link.framesReceived();
  body += F(",\"pixels\":"); body += LedGrid::kPixels;
  body += F(",\"cells\":[");
  for (uint8_t cell = 1; cell <= LedGrid::kCells; ++cell) {
    if (cell != 1) body += ',';
    body += '\"'; body += cellName(grid.state(cell)); body += '\"';
  }
  body += F("]}");
  server.send(200, "application/json", body);
}

GameMode parseMode(const String &value) {
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
    mode = static_cast<GameMode>(frame.payload[0]);
    winner = frame.payload[1];
    selectedCell = frame.payload[2];
    selectedState = frame.payload[3];
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
          grid.resetWhite();
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
  server.on("/api/v1/mode", HTTP_POST, []() {
    if (!authenticate()) return;
    const GameMode requested = parseMode(server.arg("value"));
    if (requested == kModeIdle) {
      server.send(400, "text/plain", "mode must be cell or first-wins");
      return;
    }
    mode = requested;
    winner = 0;
    grid.resetWhite();
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
    grid.resetWhite();
    saveLedPreferences();
    sendToAvr(kGameReset, sequence++);
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
    if (!networkConfig.setStation(server.arg("ssid"), server.arg("password"),
                                  enabled) || !networkConfig.save()) {
      server.send(400, "text/plain", networkConfig.lastError());
      return;
    }
    server.send(202, "application/json", "{\"accepted\":true}");
    // Flush the response through the old interface before changing station
    // association. The recovery AP remains active throughout the reconfigure.
    networkApplyAt = millis() + 250U;
    networkApplyPending = true;
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
    const LedGrid::Pattern patternBeforeUpdate = grid.pattern();
    if (!avrIsp.backup(LittleFS, kBackupDirectory)) {
      image.close();
      server.send(500, "text/plain", String("backup failed: ") + avrIsp.lastError());
      return;
    }
    if (!waitForAvr()) {
      image.close();
      server.send(500, "text/plain", "backup succeeded but AVR UART did not return");
      return;
    }
    // One-time migration accepts the original tone opcode; current images use
    // the richer cue command. Both paths finish before ISP reset asserts.
    if (!playUpdateStartCue()) {
      image.close();
      server.send(500, "text/plain", "AVR rejected both update cue formats");
      return;
    }
    const uint8_t armed[] = {1};
    const bool latchArmed =
        requestAndWaitForAck(kProgrammingState, armed, sizeof(armed));
    // The first migration image has no latch opcode. A complete ISP backup is
    // already durable, so allow that one transition; every resulting and
    // subsequent PakarController AVR image must prove the latch below.
    programmingLatched = latchArmed;
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
    server.send(200, "application/json", "{\"programmed\":true,\"verified\":true}");
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
  if (length < 10 || memcmp(packet, "PKR1", 4) != 0) return;
  const uint16_t offset = packet[6] | static_cast<uint16_t>(packet[7]) << 8;
  const uint8_t count = packet[8];
  const uint8_t flags = packet[9];
  if (length != 10 + count * 3) return;
  if (grid.stagePixels(offset, packet + 10, count) &&
      (flags & 0x01U) != 0) {
    streamFrameReady = true;
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
  pinMode(D1, INPUT);
  LittleFS.begin();
  networkConfig.begin(LittleFS);
  grid.begin();
  loadLedPreferences();
  Serial.begin(115200);
  avrSerial.begin(kBaud);
  link.begin(kBaud, onAvrFrame);

  hostname = String(F("pakarcontroller-")) + String(ESP.getChipId(), HEX);
  apStarted = networkConfig.configure(hostname.c_str(), kDevelopmentPassword);
  MDNS.begin(hostname.c_str());
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("pakarcontroller", "udp", kStreamPort);
  stream.begin(kStreamPort);

  registerApi();
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
  Serial.printf("{\"device\":\"PakarController\",\"version\":\"%s\","
                "\"event\":\"boot\",\"hostname\":\"%s\"}\r\n",
                BoardCore::BuildInfo::kVersion, hostname.c_str());
  sendToAvr(kHello, sequence++);
}

void loop() {
  const uint32_t now = millis();
  link.service();
  // Render before issuing UART requests. The AVR is request/response only, so
  // its reply arrives while FastLED is not masking ESP8266 interrupts.
  if (static_cast<int32_t>(now - ledResumeAt) >= 0) {
    if (streamFrameReady) {
      grid.showStaged();
      streamFrameReady = false;
    } else {
      grid.service(now);
    }
  }
  serviceConsole();
  server.handleClient();
  if (networkApplyPending &&
      static_cast<int32_t>(millis() - networkApplyAt) >= 0) {
    networkApplyPending = false;
    applyNetworkConfig();
  }
  MDNS.update();
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
  if (avrOnline && static_cast<uint32_t>(now - lastStatusPollAt) >= 100U) {
    lastStatusPollAt = now;
    sendToAvr(kGetStatus, sequence++);
  }
  if (static_cast<uint32_t>(now - lastDiagnosticAt) >= 1000) {
    lastDiagnosticAt = now;
    Serial.printf(
        "{\"device\":\"PakarController\",\"version\":\"%s\","
        "\"avr_online\":%s,\"mode\":\"%s\","
        "\"winner\":%u,\"effect\":\"%s\",\"ap_started\":%s,"
        "\"station_connected\":%s,\"station_ip\":\"%s\","
        "\"uart_rx_level\":%u,\"uart_bytes_received\":%lu,"
        "\"uart_frames_received\":%lu,\"uart_bytes_transmitted\":%lu,"
        "\"uart_frames_transmitted\":%lu,\"esp_uart_errors\":%u,"
        "\"avr_uart_errors\":%u,\"tm1637_ack\":%s}\r\n",
        BoardCore::BuildInfo::kVersion, avrOnline ? "true" : "false",
        modeName(), winner, grid.patternName(), apStarted ? "true" : "false",
        networkConfig.stationConnected() ? "true" : "false",
        WiFi.localIP().toString().c_str(),
        digitalRead(D4), static_cast<unsigned long>(link.bytesReceived()),
        static_cast<unsigned long>(link.framesReceived()),
        static_cast<unsigned long>(link.bytesTransmitted()),
        static_cast<unsigned long>(link.framesTransmitted()), link.errors(),
        avrUartErrors, tm1637Ack ? "true" : "false");
  }
  yield();
}
