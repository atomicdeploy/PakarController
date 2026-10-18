# LED-first delivery

## Cell-selection flicker comparison (prepared, not physically accepted)

Latest optical report: rainbow is smooth but cell selection flickers. RMT-DMA
formerly retransmitted unchanged frames continuously. Both workers now consume
only submitted complete frames; the renderer retains periodic repair refresh
and uncapped rainbow submission. This removes redundant static transmissions,
not a claim that DMA refill/electrical timing has been measured or fixed.

The confirmed physical-group sequence is
`80,60,40,20,10,30,50,70,79,59,39,19,9,29,49,69,...`, subtracting one from
each member for each successive eight-group lane. Every group is six pixels.
Thus logical80 is pixels1–6,60 is7–12,40 is13–18,20 is19–24.
The final lane is`71,51,31,11,1,21,41,61`; logical1 is group77/pixels457–462,
not the last cable group. A clarification was requested about that distinction.

`POST /api/cells/mapping` with`preset=table-cable` applies the whole permutation
in one saved mapping operation and preserves custom names. Ordinary valid saved
custom mappings are not silently migrated. An explicit `-CellChaseOnBoot` S3
test build selects the requested cable order in RAM, without overwriting names
or flash, then boots static cell selection. Normal builds still boot white.

`POST /api/effect` with`value=cell-chase` starts a board-owned diagnostic: one
red logical cell at a time,80 down to1,750ms per cell, all others white. Both
the departing white and arriving red group are composed before one full frame.
It visits every cell once and stops at1; no fade, catch-up skips, loop, per-cell
network requests, AVR action or per-tick persistence. Manual cell/effect/stream
commands supersede it. `led_output.cell_chase` reports active, visited, current
logical/physical group, interval and completion. These are renderer state,
not an optical sensor or proof that the final frame reached the LEDs.

Build either driver with the canonical wrapper and`-CellChaseOnBoot`:
`-S3LedBackend rmt-dma` or`-S3LedBackend static` (non-RMT GPIO). Keep exact-image
identities, readback and PBX/DTMF feedback separate for each driver. SDK doubles
pass both RGB orders, immutable full frames, startup/Off black, coalescing and
flash-boundary guards. Actual all80-cell renderer tests pass in both render
modes, including rollover, delay/no-skip and final hold. Hardware deployment,
visual flicker acceptance and DTMF optical responses remain pending ASUS access.

## Current priority

Critical optical regression: the user observed rapid flashing on deployed
c48201f0...; Off stopped flashing but left some latched pixels lit. The user has
now requested deployment and operation of a static non-DMA fallback. Keep the old
image disabled until replacement. Driver failed0/error0 did NOT establish a correct waveform. The staged
actor-only bc624443... image was explicitly cancelled as job9, not applied.
Read/read_raw/full image-verification guards and guarded metadata helpers are
now required alongside write/erase guards. Quiet diagnostic builds use
`-PauseLedOutput` (`PAKAR_LED_START_ENABLED=0`) to send black once and remain Off;
ordinary builds retain default On with static white. Do not mark the regression
fixed without controlled physical confirmation after fallback deployment.

The user explicitly deferred AVR integration until the ESP32-S3 WS2811 matrix
and Pakar LED API are operational. Do not let AVR diagnosis, ISP, buzzer,
TM1637, donor-wide audits, unrelated UI redesign or optional hardware block
this milestone. Retain existing code; defer rather than delete those features.
Do not resume AVR commands, resets, flashing or audible/PBX tests in this phase.

Target: GPIO4, WS2811, logical RGB / default S3 BGR wire order, 480 addressable pixels, 80 logical cells with six
pixels per cell. The existing image has normal AVR UART disabled. The ESP owns
the renderer and does not need a working AVR. There is no arbitrary 20 FPS cap;
physical LED refresh/throughput still needs measurement.

## Output service and static fallback

The new source replaces the Idle presentation with an independent output service
(On by default each boot, runtime Off is not a saved boot-black policy). Off sends
black before stopping, while retaining logical cells/mode/effect settings. Use
authenticated, peer-fenced POST `/api/led/output` with form `enabled=true|false`;
CLI/TUI/Telnet `led-output on|off`, ESP USB opcode0x5C with a boolean byte, or cloud
JSON `{"command":"led-output","enabled":true}`. Status `led_output.enabled` is
the requested setting; `led_output.driver.active/ready/transmitted/failed/error_code`
report actual S3 driver observations, not optical acceptance. No wire refresh cap
of20fps is imposed on the optional effects path. The new default static mode sends
changes immediately, without transitions; each physical group of six receives
one color. Unchanged enabled frames are also resent once per second so delayed
strip power or a missed frame can recover. This maintenance resend neither
advances fire/fades nor imposes an animation FPS ceiling. The fallback worker
still sleeps between submitted complete frames; Off remains quiet after black.

`led_output.mode` is `static` by default or explicit `effects`. An explicit
rainbow test animates even in static mode, retaining identical six-pixel groups.
Ordinary cell updates remain immediate and stationary. Effects mode permits the
other animations. Runtime effects selection is not restored as a boot animation.

To build the explicitly requested RMT-DMA test, use the same canonical wrapper
with `-Target esp32s3 -LinkMode buffered -S3FlashMB 16 -S3Psram opi
-S3IspElectricalVerified -S3LedBackend rmt-dma`. Omitting `-S3LedBackend` retains
the non-DMA fallback. Verify actual `led_output.driver.backend`/`dma` at runtime;
`led_output.mode=effects` selects per-pixel rendering, independently of backend.

The default S3 build uses `PAKAR_S3_LED_USE_DMA=0`: `gpio-static`, an internal-RAM
payload and IRAM cycle-timed GPIO transmitter on core1. It clears black once on
startup, then sends only submitted changes. One 480-pixel transfer masks core1
interrupts for approximately14.4ms, followed by a300us low latch with interrupts
enabled. Core0 Wi-Fi remains scheduled. This is a limited static fallback;
simultaneous AVR UART reception and high-rate animation are not accepted with
this transport. The AVR link remains disabled for this delivery. Explicit macro1
retains the experimental RMT-DMA backend; runtime Static/Effects chooses rendering
behavior, not the compiled transport. Diagnostics expose backend/dma/on_change_only.

The prior S3 implementation uses a dedicated core1 RMT-DMA output worker, immutable in-flight encoding and
a one-frame mailbox; old complete frames coalesce rather than building latency.
WebSocket messages are copied into a bounded loop-owned FIFO; staging and complete
output are separate buffers. The reviewed SDK permits at most2046 DMA symbols,
so this is DMA with bounded refill, not a claim that all11521 symbols fit one SDK
DMA descriptor. Wi-Fi/LED interrupt contention is a documented SDK concern:
[Espressif RMT FAQ](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/rmt.html).

- [x] Focused renderer/complete-frame FIFO/encoding/source guard checks.
- [x] Guarded SDK build and final ELF partition-wrapper binding for this worker.
- [ ] Exact-image OTA and new-boot readback for this new worker.
- [ ] Actual transmitted counter advances with static content and during network work.
- [ ] Explicit Off completes a black frame; On resumes current content without reboot.
- [ ] Human optical confirmation: no corrupt/stale frames under Wi-Fi/streaming load.

Earlier compiled/deployed scheduler (not accepted optical fix):1773600bytes,
SHA256 `c48201f02e2e52046d3dc3e0f7c5a5935acb2263291b3ffb24fb067aec917c43`.
This identity is not deployment proof. API/effect RGB remains logical RGB; build
flag `PAKAR_S3_LED_SWAP_RED_BLUE=0` selects unswapped RGB when needed. Status
`led_output.driver.channel_order` reports the built wire order. The pixel preview
now keeps its last published command snapshot during pending renders/stream
assembly/Off and exposes `pending`; it is not an optical feedback sensor.

## Earlier acceptance observations

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
| POST `/api/effect` | `value=rainbow` | Animated rainbow test; six identical pixels per group in static mode |
| POST `/api/effect` | `value=fire` | Single frame in static mode; animation in effects mode |
| POST `/api/effect` | `value=none` | Stop pattern; does not mean all-off |
| POST `/api/led/output` | `enabled=false` | Send complete black even when already Off, then stop; retain scene |
| POST `/api/led/output` | `enabled=true` | Resume latest scene; default On each boot |
| POST `/api/led/output` | `mode=static` | Immediate six-pixel cells, no animation or transitions; default |
| POST `/api/led/output` | `mode=effects` | Explicitly allow effects and individual-pixel streaming |
| POST `/api/mode` | `value=cell` | Reset all cells white |
| POST `/api/cell` | `id=1&state=red&effect=immediate` | Six pixels of logical cell1 red |
| POST `/api/cell` | `id=80&state=off&effect=fade` | Cell80 off immediately in static mode; fade in effects mode |
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

### Earlier source-only hold (superseded by the requested USB delivery)

At that checkpoint the user paused checks, tests, builds, deployments and board
contact. The later explicit request to deliver the basic USB app and diagnose
GPIO4 authorized that scoped work again. The current USB app confirms all80
mappings and actual480-pixel snapshots; neither source nor driver counters alone
establishes the visible table layout or correct wire waveform.

Mapping retrieval is being separated from optional configuration reads and
given correlated, bounded pending requests. The mapping response now carries
its own board/boot identity so clients can validate it without relying on a
second status response; responses without those fields retain the guarded
status-read path. No saved physical mapping is guessed or rewritten.

Physical acceptance remains open: the user reported that logical cell 1 red
appeared across parts of labeled cells 51 and 71, and that some pixels in cell
80 were dark. This does not establish whether mapping, grouping or transmission
is at fault. Manual single-physical-pixel stepping is the next optical diagnostic
when board interaction is authorized again.

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
