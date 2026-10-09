# PakarController ESP8266 + ATmega328P

PakarController is a two-controller game platform built from the reusable parts of
AtomicDeploy's earlier PuzzleBoard, 40LED, Puzzles, pu850-esp, PCController,
and Pealayer work.

- The ATmega328P owns deterministic keys, shift-register outputs, the ninth
  ADC6 button, TM1637 display, buzzer, melodies, and winner arbitration.
- The ESP8266 owns the MT16703/WS2811-compatible LED field, Wi-Fi/API/UI,
  streaming, self-OTA, AVR ISP recovery, and LittleFS firmware backups.
- Both use the PCController COBS + CRC-8/ATM UART envelope over a dedicated
  38400-baud SoftwareSerial link. Each board's hardware UART remains available
  for USB diagnostics and bootloader recovery.
- The Windows controller uses Pealayer's eframe/egui design approach without
  linking or loading mpv.

No I2C peripheral is enabled or required.

See `docs/HARDWARE.md`, `docs/ARCHITECTURE.md`, `docs/API.md`, `docs/TOOLCHAIN.md`, and
`docs/LEGACY-CODE-AUDIT.md` before wiring or flashing.

The native Windows app is in `desktop/`. It deliberately has no mpv or
`libmpv2` dependency. Its eframe, reqwest, serde versions and system-aware
visual styling are derived from Pealayer's MIT-licensed desktop foundation.

## Build

On the Windows hardware host with Arduino CLI, MiniCore, FastLED, and TM1637
installed:

```powershell
.\tools\build-firmware.ps1
```

The script compiles the ESP8266 image and the exact MiniCore ATmega328P target,
then verifies that `build/avr/avr.ino.with_bootloader.bin` is a complete 32 KiB
application plus Urboot image suitable for the ESP ISP programmer. Run
`.\tools\test-protocol.ps1` for the native COBS/CRC bounds and corruption test.

Build the Windows application with:

```powershell
cargo test --manifest-path .\desktop\Cargo.toml
cargo build --release --manifest-path .\desktop\Cargo.toml
```
