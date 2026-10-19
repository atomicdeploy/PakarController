# PakarController ESP32-S3 migration

Status: integrated S3 firmware and native build13 are delivered to ASUS; actual
Wi-Fi OTA/full-image readback and discovery passed. Physical and broader parity
acceptance remain open, **not a completed production release**.
David-ASUS is currently reachable through SSH8022. Fresh inventory confirms the
same CH343 bridge on COM22; live telemetry confirms16MB flash/8MB PSRAM and the
identified board MAC. The complete private flash backup remains preserved on
both PCs. See current commissioning evidence below before changing firmware.
Existing ESP8266 firmware remains a separate target.
See [commissioning and recovery](ESP32-S3-COMMISSIONING.md) for exact hardware,
backup hash, guarded build and setup commands. Do not interpret old source-only
checkpoint statements below as current deployment proof.

## Provisional common DevKitC-1 wiring

Provisional target: a common ESP32-S3 DevKitC-1, as requested. Exact module,
flash/PSRAM variant and clone routing still need confirmation before connecting
power. GPIO numbers below are not NodeMCU D labels. UART remains hardware Serial
on the AVR. This assumption does not select a flash size or partition layout.

| ESP32-S3 | Connect to | Direction |
|---|---|---|
| GPIO17 / UART1 TX | AVR D0 / PD0 / RXD | S3 to AVR |
| GPIO18 / UART1 RX | AVR D1 / PD1 / TXD | AVR to S3 |
| GPIO4 | WS2811 DIN | S3 to strip |
| GPIO12 / SPI SCK | AVR D13 / PB5 / SCK | ISP clock |
| GPIO11 / SPI MOSI | AVR D11 / PB3 / MOSI | ISP write |
| GPIO13 / SPI MISO | AVR D12 / PB4 / MISO | ISP read |
| GPIO10 | AVR RESET, not AVR D10 | ISP reset |
| GND | AVR GND and LED-supply GND | Common reference |

This is the canonical mapping and supersedes the earlier provisional RX17/TX18
plan. The S3 GPIO matrix permits either assignment, but the firmware must match
the wires. BoardPins now defaults to TX17/RX18, agreeing with the PCController
chat's conventional UART1 plan. AvrUart passes RX then TX to HardwareSerial.begin.
Neither plan is evidence of a deployed S3 programmer or working physical link.

### Optional W5500 Ethernet: separate SPI bus

| W5500 signal | ESP32-S3 | Direction |
|---|---|---|
| SCK | GPIO5 | S3 to W5500 |
| MISO | GPIO6 | W5500 to S3 |
| MOSI | GPIO7 | S3 to W5500 |
| CS | GPIO8 | Active-low chip select |
| RSTn | GPIO9 | Active-low W5500 reset, not AVR RESET |
| INTn | GPIO14 | Active-low interrupt from W5500 |
| GND | GND | Common reference |
| Supply | Module-specific input | Confirm carrier schematic; W5500 silicon uses 3.3 V |

Reserve SPI3/HSPI for W5500 and SPI2/FSPI for AVR ISP. The installed S3 Arduino
core maps HSPI to SPI3 and FSPI to SPI2; initialize both with their explicit pins,
not the core's default board pin choices. W5500 must not borrow the global AVR
ISP SPI object. The actual old AvrIsp driver calls SPI.end() on exit; a direct
shared-object port would stop Ethernet. AVR has no conventional programming CS:
while RESET is held low in ISP, Ethernet clocks/data could reach the AVR and its
MISO can conflict. A second bus avoids that electrical/ownership problem and
leaves a path for future network-driven AVR updates, without claiming they work.

All thirteen assigned signals are disjoint and limited to GPIO4..18; UART17/18,
ISP10..13 and LED4 stay unchanged. GPIO15/16 are reserved for the separate LCD
bus described below. The optional
native driver is enabled explicitly with `tools/build-firmware.ps1 -S3Ethernet`
or `PAKAR_S3_ETHERNET_ENABLED=1`; ordinary builds do not allocate this SPI bus.

Use 3.3 V GPIO signals and short wires. A bare W5500/3.3 V carrier needs a regulated
3.3 V supply; a breakout's VCC/VIN may instead feed its own regulator and must be
confirmed from its input labels/schematic. Allow about 150 mA additional supply budget for
the Ethernet module, plus S3/AVR/peripheral load; no DevKit regulator capacity is
assumed. Do not join independently regulated 3V3 outputs. External 10k pull-ups
on CS/RSTn to 3.3 V are optional startup hardening when the carrier lacks them.
Do not power a W5500 3.3 V input at 5 V or tie W5500 reset to AVR reset.

Fully read the installed Arduino-ESP32 3.3.12 SPI.h, ETH.h and actual
ETH_W5500_Arduino_SPI example. The implementation uses the core ETH/lwIP path,
not a separate Arduino Ethernet socket stack, so the shared API/WebSocket/OTA
application uses the same network abstractions. The example's blocking demo
HTTP loop and unrelated pin defaults are not imported. Independent persisted
DHCP/static settings, Ethernet-preferred IPv4 route selection, Wi-Fi fallback,
AP coexistence and explicit driver recovery are implemented with host fixtures
and an actual W5500-enabled SDK build. Local API/WebSocket serving, discovery,
authentication, Internet firmware fetching, OTA and link-loss behavior still need
physical acceptance. See the [Ethernet commissioning procedure](ESP32-S3-COMMISSIONING.md#w5500-ethernet-commissioning).

References: [Arduino-ESP32 Ethernet](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ethernet.html),
[W5500 electrical/reset specifications](https://docs.wiznet.io/img/products/w5500/w5500_ds_v109e.pdf).

### TM1637 and optional I2C character LCD

| Display | Data | Clock | Bus owner |
|---|---|---|---|
| TM1637 | AVR A4 / PC4 → DIO | AVR A5 / PC5 → CLK | AVR, non-I2C |
| HD44780 + PCF8574 16x2 LCD | S3 GPIO15 → SDA | S3 GPIO16 → SCL | S3 hardware I2C1 |

This is the recommended separate-bus plan; a direct LCD substitution on the
AVR's same A4/A5 connector is NOT enabled. Never attach both protocols to that
connector or enable AVR Wire on those occupied pins. W5500, UART, ISP and LED
pins remain unchanged. The S3 LCD adapter is compiled by default and can be
excluded with PAKAR_S3_LCD_ENABLED=0 for a different hardware profile.

Power off before inserting/removing either module. A3V3-rated LCD/backpack with
bus pull-ups to3V3 may connect directly to the S3; a5V module/backpack with5V
pull-ups needs a bidirectional I2C level shifter and its rated supply. Do not
assume a PCF8574's supply range proves the LCD glass operates at3V3. Confirm
LCD voltage/backpack type before power-on; no picture of the S3 is needed.
See [NXP PCF8574 electrical limits](https://www.nxp.com/docs/en/data-sheet/PCF8574_PCF8574A.pdf)
and [Espressif S3 electrical characteristics](https://documentation.espressif.com/esp32-s3-wroom-2_datasheet_en.html).

The adapted PCController LCD driver supports the common0x27/0x3F backpack
mapping at100kHz,16columns/two rows. It does not infer OLED/TFT/20x4 geometry
or treat an arbitrary I2C ACK as proof of visible characters. Initialization
is paced through the main loop; all expander writes must ACK. Default text
shows hostname and selected cell/state, with update-state text during firmware
maintenance. Bounded unplug/replug discovery reinitializes and replays the
current desired frame without an MCU reset after an observed outage. If only
the LCD loses power too briefly for the health probe to observe it, use the
explicit rescan action; a backpack cannot report the glass's internal reset.
Each ordinary service performs at
most one LCD byte (six short I2C transfers); the SDK adapter uses a5ms per-transfer
timeout, with early stop on the first failure. Measure contention on hardware.

GET `/api/display/lcd` and `status.lcd` expose detection, initialization,
completed-frame availability, actual address, two padded rows and requested/
presented revisions. POST form `action=present&line1=...&line2=...&hold_ms=5000`
queues a finite ASCII presentation (each row≤16,hold1..60000ms); a202 is only
admission. Read `pending=false` and matching presented/requested revisions.
`action=release` restores standalone text; `action=rescan` starts explicit
rediscovery. Normal access/board/boot fencing and maintenance guards apply.
Desktop, Web and Flutter previews require the individual lcd capability plus
complete initialized-frame evidence, and disappear on loss. They mirror the
last completed driver frame, not an optical sensor. Full native/mobile LCD
editors and USB LCD mutation contracts remain pending; no Wi-Fi fallback is
introduced for USB mode. S3 JSON snapshots preserve read-only USB preview data.

TM1637 detection continues to use actual AVR ninth-clock ACK and cached-frame/
brightness replay. Status now distinguishes detection_known, detected and null
Unknown on expired/missing AVR evidence; legacy tm1637_ack also becomes false
when stale. Build/host tests are not a physically verified insertion/recovery
gate. Deploy the candidate and commission both actual displays when ASUS access
returns, then verify segments/characters, brightness/contrast and simultaneous
W5500/LED traffic. No display hardware test was performed in this checkpoint.

TM1637 stays on AVR A4/DIO and A5/CLK; buzzer stays on AVR D9. These are not
I2C modules. Reserve S3 GPIO19/20 for native USB, GPIO43/44 for bridge UART;
avoid boot straps and module flash/PSRAM/built-in RGB pins. Header adapters reject
overlapping or nonconservative pin overrides at compile time.

**Do not feed 5 V signals into the S3.** Confirm AVR supply and oscillator before
wiring; UART TXD and ISP MISO need level conversion if AVR is running at 5 V.
The ISP RESET connection is also voltage-sensitive: an AVR RESET pull-up to 5 V
must not connect directly to GPIO10. Use a suitable isolated/open-drain reset
interface that keeps the S3 pin within its electrical limits. Check all ISP
directions and AVR input thresholds, not only the UART receive wire.
An external transistor reset driver may invert the GPIO control polarity; the
ISP implementation must match that circuit. S3 GPIO open-drain mode alone does
not make its pad safe for a 5 V RESET pull-up.
Power the strip from its correct external supply, not the DevKit 3V3 regulator.
Previous ESP8266 operation without level shifting is not S3 electrical acceptance.
Prefer a suitable 3.3-to-5 V data driver and series resistor for robust LED output.

The S3 UART adapter uses UART1 at the existing 38400 baud, RX buffer 2048 bytes,
TX buffer 256 bytes and hardware error counters. No busy wire is planned by
default. The paired AVR **build** now has an explicit buffered profile:
`./tools/build-firmware.ps1 -Target avr -LinkMode buffered`, defining
`PAKAR_USE_BUSY_WIRE=0` and `PAKAR_UART_BUFFERED=1`. The existing polling build
remains polling-only; changing hardware must not silently reinterpret it.
Buffered sends reserve room for a complete frame and do not access the busy GPIO.
All three AVR profiles compiled at the earlier preparation checkpoint; the new
front-panel extension requires its own paired build/deployment acceptance.
Buffered UART does not guarantee zero loss. Bounded frames,
ACK/deadlines, reconnect recovery and overflow stress are still necessary.

## Current structure and libraries

| Area | Current source / stack |
|---|---|
| ESP application | firmware/esp8266; Arduino ESP8266 3.1.2, FastLED 3.10.6, ESP Async WebServer 3.12.1 / ESP Async TCP 2.0.0, ArduinoJson 7.4.3, WebSockets 2.7.2, PubSubClient 2.8.0, LittleFS |
| S3 migration | firmware/esp32s3 includes the shared integrated application; BoardPins, hardware UART1, semantic StatusRgb, PU-derived SSDP/MNDP/mDNS; Arduino-ESP323.3.12, maintained Async TCP3.5.0; actual SDK acceptance in progress |
| AVR | firmware/avr + shared libraries/BoardCore, MiniCore/Urboot, actual PCController-compatible contracts and board peripherals |
| Desktop | desktop: Rust/eframe+egui 0.36.2, egui_dock 0.21.1, Phosphor 0.14.0, egui-elegance 0.16.1, tray-icon, reqwest, tungstenite, serialport; no mpv |
| Android | mobile: Flutter/Dart, http, web_socket_channel, shared_preferences, secure storage; vendored Vazirmatn/Phosphor fonts |
| Shared UI assets/contracts | assets, generated interface catalog/branding/font/appearance, firmware embedded Web UI; imports and pending omissions in docs file-comparison ledger |

Native/Web/Android source versions currently agree on 0.2.7. This is not deployed
board identity. Actual menus/peripherals remain capability-gated per peer.

## Discovery: implemented is not complete

ESP8266 advertises mDNS HTTP/controller/stream/telnet services, SSDP description,
and NetBIOS (only labels fitting 15 characters). Desktop and Android use stable
hostname-first access plus authenticated, hostname-matched SSDP transport fallback.
Actual S3 Wi-Fi A/PTR/SRV/TXT DNS-SD, multicast/unicast SSDP and MNDP responses
are now captured in the commissioning checkpoint. This does not establish a
complete all-client browsable catalog or AP/rename/reconnect matrix.
S3 platform adapters now exist, including the actual adapted PU SSDP engine and
AP/STA interface handling; live LAN/phone discovery acceptance remains pending.
S3 now has bounded interface-bound NBNS name/node-status replies, including
Ethernet/STA/AP ownership and explicit15-character-name limits. Readiness is
observed, not inferred from compilation; Windows/live-W5500 validation remains
open. WINS registration/conflict arbitration, SMB sessions and LLMNR are not
implemented. See issue#19 and the NetDiscovery source decisions in the existing
file-comparison ledger; donor's experimental blocking ND protocol is not NBNS.

## LED and protocol decisions

Reuse LedGrid rather than add another effects engine. The current S3 driver is
Pakar's own LedOutput worker, with explicit gpio-static or rmt-dma build selection;
it is not FastLED's automatic RMT fallback. FastLED still supplies shared color/
effect helpers and the ESP8266 transmitter. Production currently uses static
GPIO4/BGR with actual rmt=false/dma=false capabilities. DMA optical/Wi-Fi/UART
stress remains a release gate; do not infer DMA from the chip name.
Existing application has no default 20 FPS cap. A 480-RGB-pixel 800-kbit/s
single wire still needs about 14.4 ms of data time; DMA frees CPU but does not
remove that physical limit.

BoardCore already uses **COBS framing + CRC-8**, opcodes, sequences and bounded
payloads. This can be shared on PC–S3 USB and S3–AVR UART as two separate links;
it does not create automatic routing, arbitration, authentication or a multi-drop
bus. PCController envelopes remain adapted explicitly. “CORB” is not an existing
separate implementation here. Network REST/JSON/WebSocket contracts remain one
living unversioned API, not forcibly replaced by serial framing.

MenuAudio queues existing Increase/Decrease cues only after AVR advertises buzzer
and cue support; accepts success only on matching ACK. Menu actions must call it
once per navigation, not per repaint. Physical beeps and all-client integration
remain pending, not claimed complete.

Implemented source now includes the shared sketch/per-TU wrappers, partition-safe
OTA reads/abort, TLS/telemetry/filesystem/USB/ISP/discovery adapters, buffered UART,
shared LED engine and actual-output RGB native/Web mirror. Next gates remain
actual SDK compilation, paired safe deployment, optical/audio/UART/ISP/OTA
acceptance, mobile/TUI parity and full ESP front-panel text/address editors.

Primary references: [Espressif GPIO restrictions](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/gpio.html),
[Arduino-ESP32 UART buffer/pin API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/serial.html),
[S3 RMT DMA resource/transaction API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/rmt.html).
The exact installed source, not a generic chip claim, informs driver adaptation.
