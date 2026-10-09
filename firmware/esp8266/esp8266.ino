#include <Arduino.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <SoftwareSerial.h>
#include <WiFiUdp.h>

#include <BuildInfo.h>
#include <Protocol.h>
#include <SerialLink.h>

#include "src/AvrIsp.h"
#include "src/LedGrid.h"
#include "src/WebUi.h"

using namespace BoardCore::Protocol;

namespace {

constexpr uint32_t kBaud = 38400;
constexpr uint16_t kStreamPort = 7890;
constexpr char kAdmin[] = "admin";
constexpr char kDevelopmentPassword[] = "pakarcontroller";
constexpr char kAvrImagePath[] = "/avr/staged.bin";
constexpr char kBackupDirectory[] = "/avr/backup";

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
GameMode mode = kModeCellSelect;
uint8_t winner = 0;
uint8_t selectedCell = 1;
uint8_t selectedState = kCellWhite;
uint8_t sequence = 1;
uint32_t lastAvrFrameAt = 0;
uint32_t lastHelloAt = 0;
uint32_t lastDiagnosticAt = 0;
uint16_t avrUartErrors = 0;

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
  body += F(",\"winner\":"); body += winner;
  body += F(",\"selected_cell\":"); body += selectedCell;
  body += F(",\"brightness\":"); body += grid.brightness();
  body += F(",\"effect\":\""); body += grid.patternName(); body += '"';
  body += F(",\"esp_uart_errors\":"); body += link.errors();
  body += F(",\"avr_uart_errors\":"); body += avrUartErrors;
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
  if (frame.opcode == kHelloResponse) {
    link.send(kGetStatus, sequence++);
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
    const uint8_t payload[] = {static_cast<uint8_t>(mode)};
    link.send(kSetMode, sequence++, payload, sizeof(payload));
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
    selectedCell = id;
    selectedState = state;
    const uint8_t payload[] = {static_cast<uint8_t>(id), static_cast<uint8_t>(state),
                               static_cast<uint8_t>(effect)};
    link.send(kCellAction, sequence++, payload, sizeof(payload));
    server.send(202, "application/json", "{\"accepted\":true}");
  });
  server.on("/api/v1/game/reset", HTTP_POST, []() {
    if (!authenticate()) return;
    winner = 0;
    grid.resetWhite();
    link.send(kGameReset, sequence++);
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
    link.send(kBuzzer, sequence++, payload, sizeof(payload));
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
    link.send(kDisplayText, sequence++, payload, sizeof(payload));
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
    link.send(kOutputMask, sequence++, payload, sizeof(payload));
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
    if (!avrIsp.backup(LittleFS, kBackupDirectory)) {
      image.close();
      server.send(500, "text/plain", String("backup failed: ") + avrIsp.lastError());
      return;
    }
    const bool ok = avrIsp.programFlash(image);
    image.close();
    if (!ok) { server.send(500, "text/plain", avrIsp.lastError()); return; }
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
  if (grid.stagePixels(offset, packet + 10, count) && (flags & 0x01U) != 0) {
    grid.showStaged();
  }
}

}  // namespace

void setup() {
  pinMode(D1, INPUT);
  LittleFS.begin();
  grid.begin();
  Serial.begin(115200);
  avrSerial.begin(kBaud);
  link.begin(kBaud, onAvrFrame);

  hostname = String(F("pakarcontroller-")) + String(ESP.getChipId(), HEX);
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(hostname.c_str(), kDevelopmentPassword);
  MDNS.begin(hostname.c_str());
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("pakarcontroller", "udp", kStreamPort);
  stream.begin(kStreamPort);

  registerApi();
  updater.setup(&server, "/update", kAdmin, kDevelopmentPassword);
  server.begin();
  Serial.printf("{\"device\":\"PakarController\",\"version\":\"%s\","
                "\"event\":\"boot\",\"hostname\":\"%s\"}\r\n",
                BoardCore::BuildInfo::kVersion, hostname.c_str());
  link.send(kHello, sequence++);
}

void loop() {
  const uint32_t now = millis();
  link.service();
  server.handleClient();
  MDNS.update();
  serviceStream();
  grid.service(now);
  if (avrOnline && static_cast<uint32_t>(now - lastAvrFrameAt) > 3000) avrOnline = false;
  if (!avrOnline && static_cast<uint32_t>(now - lastHelloAt) > 1000) {
    lastHelloAt = now;
    link.send(kHello, sequence++);
  }
  if (static_cast<uint32_t>(now - lastDiagnosticAt) >= 1000) {
    lastDiagnosticAt = now;
    Serial.printf(
        "{\"device\":\"PakarController\",\"version\":\"%s\","
        "\"avr_online\":%s,\"mode\":\"%s\","
        "\"winner\":%u,\"effect\":\"%s\",\"esp_uart_errors\":%u,"
        "\"avr_uart_errors\":%u}\r\n",
        BoardCore::BuildInfo::kVersion, avrOnline ? "true" : "false",
        modeName(), winner, grid.patternName(),
        link.errors(), avrUartErrors);
  }
  yield();
}
