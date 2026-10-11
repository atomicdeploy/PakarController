# PakarController ESP32-S3 migration

Status: migration adapters added; **not an S3 production build or physical acceptance**.
ASUS/board acceptance is deliberately deferred while hardware is being replaced.
Existing ESP8266 firmware is preserved. No flash/EEPROM writes were attempted.

## Provisional common DevKitC-1 wiring

Provisional target: a common ESP32-S3 DevKitC-1, as requested. Exact module,
flash/PSRAM variant and clone routing still need confirmation before connecting
power. GPIO numbers below are not NodeMCU D labels. UART remains hardware Serial
on the AVR. This assumption does not select a flash size or partition layout.

| ESP32-S3 | Connect to | Direction |
|---|---|---|
| GPIO17 / UART1 RX | AVR D1 / TXD | AVR to S3 |
| GPIO18 / UART1 TX | AVR D0 / RXD | S3 to AVR |
| GPIO4 | WS2811 DIN | S3 to strip |
| GPIO12 / SPI SCK | AVR D13 / PB5 / SCK | ISP clock |
| GPIO11 / SPI MOSI | AVR D11 / PB3 / MOSI | ISP write |
| GPIO13 / SPI MISO | AVR D12 / PB4 / MISO | ISP read |
| GPIO10 | AVR RESET, not AVR D10 | ISP reset |
| GND | AVR GND and LED-supply GND | Common reference |

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
All three AVR profiles compile; this is not a deployed S3/AVR pair.
Buffered UART does not guarantee zero loss. Bounded frames,
ACK/deadlines, reconnect recovery and overflow stress are still necessary.

## Current structure and libraries

| Area | Current source / stack |
|---|---|
| ESP application | firmware/esp8266; Arduino ESP8266 3.1.2, FastLED 3.10.6, ESP Async WebServer 3.12.1 / ESP Async TCP 2.0.0, ArduinoJson 7.4.3, WebSockets 2.7.2, PubSubClient 2.8.0, LittleFS |
| S3 migration | firmware/esp32s3/src: BoardPins, AvrUart, MenuAudio; Arduino-ESP32 3.3.12 installed, integration/compile pending |
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
They do not currently implement a full browsable DNS-SD discovery catalog.
Physical multicast/reconnect/AP/rename acceptance remains unverified while offline.
S3 discovery still needs its platform adapters; none is claimed operating.

## LED and protocol decisions

Reuse LedGrid rather than add another effects engine. FastLED RMT5 attempts S3
DMA for its first nonempty TX channel, with possible non-DMA fallback. Actual DMA
status and Wi-Fi/UART stress remain release gates; do not infer DMA from the chip
name. Existing application has no default 20 FPS cap. A 480-RGB-pixel 800-kbit/s
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

Next gates: shared ESP platform extraction; partition-safe OTA and recovery;
S3 TLS/telemetry/filesystem/USB/ISP/discovery adapters; paired buffered-link integration;
shared LED pin/RMT transport; desktop/mobile feedback and discovery UI; real S3
build; simulated UI matrix; finally physical UART/ISP/LED/display/audio and OTA.

Primary references: [Espressif GPIO restrictions](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/gpio.html),
[Arduino-ESP32 UART buffer/pin API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/serial.html),
[S3 RMT DMA resource/transaction API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/rmt.html).
The exact installed source, not a generic chip claim, informs driver adaptation.
