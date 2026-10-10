# PakarController ESP8266 + ATmega328P

First-install login: `admin` / `pakarcontroller`; change it in **Device access**.
See [Access and recovery](docs/ACCESS.md) and the explicit
[PU850 parity audit](docs/PU850-PARITY.md). ESPAsyncWebServer now hosts the
Web UI/API/WebSocket, ESP OTA and AVR ISP in the same firmware.

PakarController is a two-controller game platform built from the reusable parts of
AtomicDeploy's earlier PuzzleBoard, 40LED, Puzzles, pu850-esp, PCController,
and Pealayer work.

- The ATmega328P owns deterministic keys, shift-register outputs, the ninth
  ADC6 button, TM1637 display, buzzer, melodies, and winner arbitration.
- The ESP8266 owns the MT16703/WS2811-compatible LED field, Wi-Fi/API/UI,
  streaming, self-OTA, AVR ISP recovery, and LittleFS firmware backups.
- Both use the PCController COBS + CRC-8/ATM UART envelope at 38400 baud. The
  AVR uses its hardware UART; the ESP uses SoftwareSerial, while its USB UART
  is a second framed 115200-baud PC transport.
- The Windows controller uses Pealayer's eframe/egui design approach without
  linking or loading mpv.
- Optional outbound cloud links provide TLS MQTT telemetry/commands and a
  configurable WebSocket client. MQTT and WebSocket secrets are write-only and
  stored in checksum-backed LittleFS configuration.
- The PU850-derived network toolbox also includes mDNS discovery and a
  password-authenticated Telnet command shell without taking over either UART.

No I2C peripheral is enabled or required.

See `docs/HARDWARE.md`, `docs/HARDWARE-VERIFICATION.md`,
`docs/ARCHITECTURE.md`, `docs/API.md`, `docs/TOOLCHAIN.md`, and
`docs/LEGACY-CODE-AUDIT.md` before wiring or flashing.

The native Windows app is in `desktop/`. It deliberately has no mpv or
`libmpv2` dependency. Its eframe, reqwest, serde versions and system-aware
visual styling are derived from Pealayer's MIT-licensed desktop foundation.
It can stream a generated rainbow or sample the primary screen into the 10 by
8 staggered table through the authenticated WebSocket.
The Streaming panel exposes a shared target rate (60 FPS by default; 0 is
uncapped) and measured completed host frame writes. This is not measured LED
refresh. Monitor/region selection and live frame-rate acceptance remain open.

## Build

On the Windows hardware host with Arduino CLI and the locked libraries
installed (FastLED 3.10.6, WebSockets 2.7.2, PubSubClient 2.8.0,
ESP Async WebServer 3.12.1, ESP Async TCP 2.0.0):

```powershell
.\tools\build-firmware.ps1 -LinkMode polling
.\tools\build-firmware.ps1 -LinkMode busy-wire
# PU850-style repository entrypoint; also runs the protocol tests
.\build.cmd
```

Linux/CI uses `./build.sh polling` and `./build.sh busy-wire`. CI builds both
variants; the deployed pair must use the same link mode. Every merged
AVR+Urboot artifact must be exactly 32 KiB.

The script compiles the ESP8266 image and the exact MiniCore ATmega328P target,
then verifies that `build/avr/avr.ino.with_bootloader.bin` is a complete 32 KiB
application plus Urboot image suitable for the ESP ISP programmer. Run
`.\tools\test-protocol.ps1` for the native COBS/CRC bounds and corruption test.

Build the Windows application with:

```powershell
cargo test --manifest-path .\desktop\Cargo.toml
cargo build --release --manifest-path .\desktop\Cargo.toml
```

The same executable provides scriptable CLI and interactive TUI surfaces. Set
`PAKAR_ENDPOINT`, and optionally `PAKAR_USER` / `PAKAR_PASSWORD`, then use:

```powershell
PakarController.exe --status
PakarController.exe --command cell 17 red fade
PakarController.exe --command mode snake
PakarController.exe --tui
```

Logical IDs are row-major: 1 is upper-left, 2 is next to it, and 11 begins the
second row. Graphical tables offset one-based odd columns by half a cell in Y;
each cell contains six LEDs. Physical strip order is a separate editable map.
The current text surface prints a flat status table and a command prompt;
the requested fully interactive staggered TUI remains open in issue #2.
