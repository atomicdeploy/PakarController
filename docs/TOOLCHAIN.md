# Firmware toolchain

The primary target is ESP32-S3; ESP8266 is retained shared-source reuse, and
ATmega328P is the paired peripheral controller. See
[architecture and current gaps](ARCHITECTURE.md).

## Reproducible inputs

`tools/firmware-dependencies.json` and each `sketch.yaml` declare reviewed
pins: Arduino-ESP32 3.3.12, ESP8266 core3.1.2, MiniCore3.1.3, FastLED3.10.6,
WebSockets2.7.2, PubSubClient2.8.0, ArduinoJson7.4.3, ESP Async WebServer3.12.1,
Async TCP3.5.0 and ESP Async TCP2.0.0. Builds do not silently update these.
Use the explicit dependency maintenance command when reviewing newer releases;
“latest” is not a reproducible installed-host claim.

A project-local `tools/arduino-cli.exe` takes precedence; otherwise the wrapper
resolves Arduino CLI from PATH. SDK installation and dependency upgrades are
explicit, not side effects of a firmware compile.

## Profiles and ownership

Use `tools/build-firmware.ps1` or the shared `pakar.cmd` / `pakar.sh`
dispatcher. Relevant build decisions:

- `-Target avr|esp8266|esp32s3|both`; `both` is the legacy ESP8266+AVR pair.
- `-GameMode table-selection|first-wins`: exactly one program on each paired
  processor; source macro `PAKAR_GAME_MODE=1|2`.
- `-LinkMode busy-wire|polling|buffered`: both peers must match.
  Buffered is required for S3; ESP8266 uses busy-wire or polling.
- S3 physical flash/PSRAM, GPIO versus RMT-DMA, UART0/HWCDC/OTG host connector,
  BLE build exclusion, optional Ethernet and explicit ISP electrical flags
  are separate from runtime device settings.

The UART envelope is one **living unversioned** COBS/CRC-8 contract, with bounded
payloads, identity, capabilities and receipts. Its reserved byte is not an API
generation selector. Build/software releases remain descriptive identities and
supported-release admission is a separate policy.

## Low-write build policy

Use existing board/profile caches and the guarded wrappers. Backend selection
uses an application-only generated profile, not SDK-wide macro changes.
Avoid `-Clean`, duplicate worktrees/caches and debug symbols for normal iteration.
Build only affected targets; an AVR-only or native UI change does not require
recompiling all firmware. A successful compile is not an upload, a running image,
clock/fuse safety or physical LED/peripheral acceptance.
