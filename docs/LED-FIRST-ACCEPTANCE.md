# LED-first delivery

## Current priority

The user explicitly deferred AVR integration until the ESP32-S3 WS2811 matrix
and Pakar LED API are operational. Do not let AVR diagnosis, ISP, buzzer,
TM1637, donor-wide audits, unrelated UI redesign or optional hardware block
this milestone. Retain existing code; defer rather than delete those features.
Do not resume AVR commands, resets, flashing or audible/PBX tests in this phase.

Target: GPIO4, WS2811/RGB, 480 addressable pixels, 80 logical cells with six
pixels per cell. The existing image has normal AVR UART disabled. The ESP owns
the renderer and does not need a working AVR. There is no arbitrary 20 FPS cap;
physical LED refresh/throughput still needs measurement.

- [x] Actual ASUS-to-ESP authenticated Wi-Fi API requests, pinned to one
  acknowledged hardware/boot identity, without AVR or USB serial operations.
- [x] All 480 committed RGB pixels verified off, then white.
- [x] Rainbow snapshots differ and frame revision advances, not just an ACK.
- [x] Logical cell1 maps to six red pixels and cell80 to six off pixels;
  every other cell stays white, using the returned physical mapping.
- [x] Final rainbow request acknowledged and read back, brightness255.
- [ ] Human/physical check: rainbow visible across the connected matrix.
- [ ] Confirm all 480 addresses and six-pixel cell geometry on actual hardware;
  count WS2811 controller addresses rather than physical emitters alone.
- [ ] Complete actual full-frame WebSocket pixel streaming/readback, including
  boundary rejection. Source support and previous read-only WS proof are not
  a successful streaming test.
- [ ] Confirm reboot starts all80 cells white with external power and Wi-Fi
  access. Rainbow/fire are explicit runtime requests, not restored boot effects.
- [ ] Confirm authenticated Wi-Fi OTA and same-board/new-boot/image/settings
  readback before declaring USB recovery unnecessary.

## Observed API evidence

Actual board `esp32s3-907069eac800`, firmware0.2.7, boot
`176746de7a93c4ebd78c22a7e73d2c9f`. The completed one-boot API sequence verified
480 pixels/80 cells/six pixels per cell, all-off, all-white, rainbow revisions5
and27 with different RGB data, cell1 red, cell80 off and all78 other cells
unchanged. Final effect rainbow, brightness255, AVR link false.

The pixel snapshot is the renderer's **committed command buffer**, not a light
sensor or GPIO waveform measurement. It does not prove optical output, electrical
timing, sustained frame rate or recovery. No firmware rebuild/flash was needed
to exercise these existing LED functions.

The initial long encoded SSH test command was truncated and failed parsing before
execution; it sent no requests. The successful test used a locally parsed small
script transferred to ASUS, with process-scoped credentials and no mutation
replay. This follows the existing PowerShell SSH-size recovery guidance rather
than retrying an uncertain LED command. Private verification scripts are retained
under outputs/PakarController/verification, not presented as shipping API code.

A later streaming check failed at its first hostname lookup, before a socket
opened or any pixel/HTTP mutation was sent. Rainbow was therefore not replaced
by that check. The user needs to unplug/reboot/move the ESP to connect the strip;
all further board requests were stopped to let that happen. Moving off PC USB for
an externally powered test is allowed; this is NOT a claim that OTA/recovery is
verified or that no USB cable will be needed again.

## LED API quick reference

Use `http://ledboard.local`. Authenticate, obtain a fresh `/api/status`, and pin
`X-Pakar-Board` plus `X-Pakar-Boot` for writes and their readbacks. Stop if the
identity changes or a request outcome is uncertain; do not automatically replay.

| Method/path | Form fields | Result |
|---|---|---|
| GET `/api/status?pixels=1` | none | Status plus committed480-pixel RGB frame and mapping |
| POST `/api/effect` | `value=rainbow` | Continuous rainbow |
| POST `/api/effect` | `value=fire` | Continuous fire |
| POST `/api/effect` | `value=none` | Stop pattern; does not mean all-off |
| POST `/api/mode` | `value=idle` | All pixels off |
| POST `/api/mode` | `value=cell` | Reset all cells white |
| POST `/api/cell` | `id=1&state=red&effect=immediate` | Six pixels of logical cell1 red |
| POST `/api/cell` | `id=80&state=off&effect=fade` | Fade logical cell80 off |
| POST `/api/brightness` | `value=255` | Global brightness; current range1..255 |
| GET `/api/cells/mapping` | none | Logical/physical mapping, labels and stagger |

A cell/mode action can replace the current pattern. Use only documented values;
complete strict validation/typed error parity and transactional multi-client
stream ownership remain unaccepted work, not inferred from this happy-path test.
Named semantic groups, arbitrary independent group effect instances and WLED API
compatibility are not implemented by this existing per-cell contract.

## Physical test wiring

With all power off: GPIO4 to strip DIN/input, common GND between ESP/strip/supply,
strip V+ directly to its correctly rated external supply. Do not power480 pixels
through the DevKit. Power the ESP separately with an appropriate USB wall supply
or the reviewed regulated5V header route; never feed strip12V to it or parallel
power inputs without confirmed isolation. The last verified older-image test
selection was rainbow at255; it is not the new white boot policy below.
The AVR is unnecessary and may remain disconnected. Details and references are
in [commissioning](ESP32-S3-COMMISSIONING.md).

## White startup policy checkpoint

The shared ESP8266/ESP32-S3 firmware now starts white even when an older valid
`/led.cfg` contains rainbow or fire. Brightness still persists; the existing
six-byte private record stays compatible. Explicit `/api/effect` or front-panel
effect choices still run until replaced or rebooted. Saving transient cell/effect
changes no longer rewrites an identical preferences file. This adapts the actual
PCController `Project/SettingsStore.cpp` changed-byte write avoidance to LittleFS;
it does not import its EEPROM layout, journal or imply full settings parity.

`tools/test-protocol.ps1` exercises the actual LED renderer and boot-preferences
helper: all valid older pattern records produce480 white command pixels and80
white cell states, brightness is retained, damaged records are rejected, and
explicit runtime rainbow still changes pixels. These use Arduino/FastLED doubles,
not an optical test. Source and compiled candidate do not change the installed
board; its last known quiet image predates this policy. USB/application access
and stable power are needed before deployment/readback/reboot acceptance.

The guarded S3 build passed with the identified16MB/OPI-PSRAM buffered profile,
hostname `ledboard`, normal AVR UART paused and the existing ISP build gate
unchanged. Application image:1,843,552bytes,
SHA256 `599509ab551e4b372ad1f15b8ac98d3f22dfdea093d0d107e0ddda4e7ba9fac9`.
The identical image is staged on ASUS as
`C:\Users\David\Documents\PakarController\staging\firmware\esp32s3-white.ino.bin`.
This is staged only: no upload/reset/erase/EEPROM/fuse operation took place.

The Flutter0.2.7 preview was updated on the ASUS-connected authorized Samsung
using `install-r`, after package/signature/rollback verification. Installed APK
SHA256 `3ce5304a88caad521238216107da51f789a982a69882c44ce08a3b90e516e1b9`
matches the build; app-data directories/inodes and first-install time were
preserved. The prior APK remains backed up. Runtime launch/screenshot remain
unverified: one permitted unsecured-lock swipe did not dismiss the lock, and
the guard stopped before launch. Manual phone unlock is required; installation
does not prove mobile-to-board control or production signing.

## Deferred queue: resume after the LED milestone

These remain required, not removed or checked complete:

1. Finish any remaining LED handoff gates above, then restore the integrated AVR
   path: silent power/clock/reset diagnosis; read-only ISP signature and verified
   flash/Urboot/EEPROM/fuse backup before programming; paired hardware UART,
   buffering and protocol/ACK/readback/reconnect validation. No blind erase or
   fuse changes. Preserve TM1637 A4/DIO and A5/CLK, shift-register/input wiring.
2. AVR TM1637, key/shift-register/ADC6 input/output, exact melodies and front-panel
   settings/game integration; audible tests only with renewed user agreement.
3. Finish named semantic groups/effects/presets and broader LED API robustness,
   streaming ownership/recovery/throughput, mapping and requested application
   controls; retain one renderer rather than a duplicated AVR effects engine.
4. Finish desktop/Web/Flutter parity, cloud/bridge/telemetry, complete network
   discovery and donor provenance/production acceptance. Existing unresolved
   requirements stay in master issue#2; TFT/W5500 stay in backlog#19.

This is a priority queue, not permission to resume all deferred hardware as soon
as a source test passes. Obtain actual matrix feedback first and respect the
latest user instructions. Never close master production issue#2 from this slice.
