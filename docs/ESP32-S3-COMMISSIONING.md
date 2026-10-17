# ESP32-S3 migration and commissioning

**Current delivery priority:** [LED-first acceptance](LED-FIRST-ACCEPTANCE.md).
AVR integration is explicitly deferred until the480-pixel matrix and LED API
work. Historical AVR commissioning evidence below is not a request to resume it.

The ESP32-S3 target builds the **same integrated application** as the ESP8266:
Web/API/WebSocket, table effects, cloud clients, storage, self-OTA and AVR ISP.
`firmware/esp32s3/esp32s3.ino` includes the shared sketch; its individual source
wrappers retain separate translation units. The ESP8266 target remains available
for a later, separately configured device. Do not attach two controllers to the
same AVR UART/ISP signals at once.

## Identified connected hardware

The actual device reported ESP32-S3 revision 0.2, 16 MB quad flash, 8 MB embedded
OPI PSRAM and MAC `90:70:69:ea:c8:00`. This identifies the chip/memory, **not** the
DevKit PCB revision or its onboard RGB GPIO. David-ASUS exposes the CH343 UART
bridge as COM22. Its native USB application previously appeared as COM13.
Use the current UART/COM USB socket for commissioning; no further switch is
required while COM22 remains present.

The complete original 16 MB flash was read before any upload and copied to both
PCs. Both copies have SHA-256
`cd0f30ac9491da499eaf2167f07b5accf23cf6779c506110431aa766f346273e`.
The backup is private and outside Git because it may contain credentials.
Secure boot and flash encryption reported disabled. No eFuse change is needed.

## Wiring contract

| ESP32-S3 GPIO | Controller Board Mini / function |
|---|---|
| 17, UART1 TX | AVR D0 / PD0 / RXD |
| 18, UART1 RX | AVR D1 / PD1 / TXD |
| 11, ISP MOSI | AVR D11 / PB3 / MOSI |
| 13, ISP MISO | AVR D12 / PB4 / MISO |
| 12, ISP clock | AVR D13 / PB5 / SCK |
| 10, reset control | AVR RESET; user reports direct connection and current AVR VCC 3.3 V |
| 4 | External WS2811 matrix DIN, 480 pixels / 80 cells |
| GND | Common signal ground |

The printed TX/RX bridge pins (commonly GPIO43/44) are the independent PC host
path, not the AVR UART. Both UART wires cross TX to RX. UART1 is 38400, 8N1;
the CH343 PC protocol is 115200, 8N1. The AVR retains hardware Serial. The paired
buffered build does not require a busy wire. Old busy-wire AVR firmware must not
be mistaken for the new buffered image; unsolicited events can remain blocked
until that firmware is safely updated.

Current commissioning firmware deliberately pauses the normal AVR UART. The
38400 setting above is the intended paired application profile, not a claim that
the unknown firmware currently on the AVR has acknowledged this protocol.

ESP32-S3 GPIOs are not 5 V tolerant. Reconfirm AVR VCC, clock and any level
translation before driving the connected circuit. GPIO10 must not be connected
to AVR D10 or ESP EN. The integrated ISP implementation defaults to disabled
until `PAKAR_S3_ISP_ELECTRICAL_VERIFIED=1` is deliberately selected after reviewing
the actual RESET circuit and its assertion polarity. A successful S3 build is
not permission to alter AVR fuses, EEPROM or bootloader.

AVR display pins remain PuzzleBoard's A4/PC4 DIO and A5/PC5 CLK. Its buzzer is
D9. ESP settings navigation calls the AVR's actual existing cue owner, rather
than synthesizing a second melody engine on the ESP.

## Reproducible target

From a whitespace-free checkout, use the guarded build:

```powershell
./tools/build-firmware.ps1 -Target esp32s3 -LinkMode buffered `
  -S3FlashMB 16 -S3Psram opi -DeviceName ledboard -Jobs 4
./tools/build-firmware.ps1 -Target avr -LinkMode buffered -Jobs 2
```

The current quiet commissioning image was built with:

```powershell
./tools/build-firmware.ps1 -Target esp32s3 -LinkMode buffered `
  -S3FlashMB 16 -S3Psram opi -DeviceName ledboard `
  -PauseAvrLink -S3IspElectricalVerified -Jobs 4
```

`-PauseAvrLink` leaves normal AVR TX/RX as inputs and disables normal AVR requests;
only explicit physical-USB diagnostic requests can start the separate bounded
RX-only capture. `-S3IspElectricalVerified` is an explicit, S3-only operator
acknowledgment for the reported 3.3 V/direct GPIO10 RESET path; it defaults off.
Its name is not an electrical measurement or successful programmer probe.
Enabling it does not automatically touch RESET, flash, fuses or EEPROM. Never
use an ISP probe to silence the AVR: its scoped operation releases RESET again.

The actual installed SDK is Arduino-ESP32 3.3.12. Dependencies are locked in
`firmware/esp32s3/sketch.yaml`; Async TCP 3.5.0 is the maintained ESP32Async
implementation. The selected known partition layout uses the first 8 MB:
two 0x330000-byte OTA slots and a 0x180000-byte filesystem. The remaining flash
is intentionally unallocated, not claimed as usable filesystem capacity.

Only first commissioning after the full backup may add `-InitializeFilesystem`.
That build attempts filesystem creation at most once, recording the attempt in
NVS **before** formatting. Ordinary builds never auto-format damaged settings.
Do not erase the whole flash to resolve a mount error.

The normal host protocol uses UART0/CH343. Defining
`PAKAR_S3_HOST_NATIVE_USB` selects native USB instead; do not expect the same
physical COM port to carry both implementations. The AVR UART remains separate.

## Network setup and persistent configuration

Use physical USB commissioning with the checked-in helper on David-ASUS:

```powershell
$wifiPassword = Read-Host 'Station password' -AsSecureString
./tools/set-device-network.ps1 -Port COM22 -Ssid 'DRS Tether' `
  -Password $wifiPassword -Mode both
```

The helper confirms ESP identity, correlates every COBS/CRC ACK and never replays
an uncertain mutation. It verifies configured station/AP mode separately from
actual association/AP startup. Credentials are never printed. Prefer
`http://ledboard.local/` after live DNS-SD/mDNS verification; use a current
observed address only for diagnosing discovery failures.

Fresh filesystem Web authentication is `admin` / `pakarcontroller`; change it
through Device settings before untrusted network access. The recovery AP uses
the configured hostname as its SSID. Its current commissioning password is
`pakarcontroller`. Neither value is the private station password.

Web/native Device settings expose persistent station/AP/both and DHCP/static
configuration. Changing mode without providing a password preserves the saved
station credential. DHCP preserves the last validated static profile.
The paired AVR ESP submenu currently supports mode, reconnect, scan request,
DHCP/static-profile selection and a once-scrolling hostname summary. Full text
editing and scan-result selection remain pending; see
[front-panel details](S3-STATUS-FRONTPANEL-INTEGRATION.md).

## Status pixel and discovery

### Standalone WS2811 wiring and USB-free handoff

The ESP owns the matrix renderer; the AVR is not required for rainbow, fire,
cell colors or streamed pixels. The current image uses WS2811/RGB on **GPIO4**,
with 480 addressable RGB pixels arranged as 80 logical cells of six pixels.
Count addressable controller groups, not merely physical LED emitters: some
WS2811 strips drive several emitters from one address. Current per-cell effects
are immediate/fade/pulse with white/red/off; arbitrary named semantic groups,
independent group effect instances and full WLED feature/API parity are not yet
implemented or accepted.

With all power disconnected, connect GPIO4 to the strip's **DIN/input** end,
ESP GND to strip GND and supply negative, and the strip's V+ directly to its
correctly rated external supply. Follow its printed voltage; WS2811 products can
use different supply voltages. Never feed strip voltage into a GPIO or 3V3 pin,
and never power 480 pixels through the DevKit regulator/USB/header traces. Use
appropriate fused distribution/injection and wire ratings. A 3.3-to-5 V AHCT
data buffer and a short data lead are preferred for reliable final wiring.

Removing PC USB still requires an independent ESP power source. For the assumed
common DevKit, use a USB wall supply or regulated 5 V at 5V/GND, not strip 12 V.
Use one selected power-input method; do not parallel external 5 V with PC USB
unless the actual board's isolation is established. AVR stays on its reported
3.3 V supply. Matrix GPIO4 is independent of the onboard status RGB, which remains
disabled until its configured GPIO is enabled and optically checked.

**Current stored LED state was observed as cell mode, all cells white,
brightness255.** Do not treat startup as dark or current-limited. Arrange an
adequate supply before connecting the full matrix, or deliberately select off
before wiring and verify its persisted state.

USB-free update readiness is still OPEN. A fresh ASUS hostname-first read returned
the expected board ID, station/AP active, AVR link disabled and 480 configured
pixels. `/update/info` initially exceeded a seven-second request deadline; a
subsequent independent bounded curl read returned HTTP200 in2.868267 seconds,
OTA phase idle, slot capacity3342336 and MD5
`9ec4ce6a0f2a0d4ad982468acfe46f2e`, matching the exact local quiet application.
An intended same-image Wi-Fi OTA then stopped at the initial hostname preflight:
`ledboard.local` did not resolve. **No upload started and no firmware was written.**
This does not distinguish power-off from a discovery/network fault. Do not
declare OTA broken or successful from these observations.

Before a Wi-Fi-only handoff, verify one authenticated/pinned OTA upload, reconnect
to the same hardware/new boot with the expected image and preserved network
settings, then perform external-power startup and actual WS2811 effects tests.
Retain USB recovery until this milestone is explicitly reported passed. Keep
AVR reset/ISP, buzzer and UART actuation out of that standalone LED test.

Electrical references: [Espressif common DevKit power and headers](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html),
[WLED strip voltage variants](https://kno.wled.ge/basics/compatible-led-strips/) and
[WLED power distribution guidance](https://kno.wled.ge/advanced/wiring/).

The onboard pixel is independent of matrix GPIO4. Its actual GPIO must first
be confirmed (supported DevKit variants: 38 or 48). It then transitions smoothly
between semantic boot/connecting/ready/AP/peer-lost/update/warning/fault states.
Unidentified pins stay disabled. Native/Web previews consume actual reported
driver RGB bytes without applying brightness twice. Driver submission is not
optical acceptance.

The S3 discovery implementation contains mDNS/DNS-SD, the adapted PU850 SSDP
device/search queue, and MNDP on active station/AP interfaces. NetBIOS is
explicitly unavailable on this target rather than falsely advertised. Live
multicast reachability must be tested from each relevant PC and phone; hotspot
client isolation can prevent it even when ordinary HTTP works.

## Acceptance boundaries

### Observed quiet commissioning checkpoint

The exact shared main is
`e19a7c10fe7e0bcd2ed523a9a011123748ba1640637827bfe5fa72411930b5f2`.
The real S3 SDK build passed: sketch 1,843,223 / 3,342,336 bytes, globals 74,916
bytes. The application binary is 1,843,376 bytes, SHA-256
`6c65e309173c91284605476662c2aad1f26349e5c6fa68bc2431c4482c3789f5`.
Only the application at `0x10000` was subsequently flashed on the same COM22/MAC
and hash-verified by esptool. The original full-flash backup remains unchanged;
bootloader/partition data were not rewritten in this quiet-image update.

Actual USB identified ESP 0.2.7. Its valid 42-byte status acknowledged normal
AVR link disabled, station connected, AP started and NTP synchronized, with
reported protocol errors zero. The two explicit RX-only captures returned:

| Selected baud | Elapsed ms | Received | Stored | Dropped | Hardware RX errors |
|---|---:|---:|---:|---:|---:|
| 38400 | 3059 | 140 | 128 | 12 | 96 |
| 115200 | 3030 | 172 | 128 | 44 | 92 |

Both confirmed pin-ownership checks and stop. GPIO10 was observed high with
output disabled; RX18 was high at stopped snapshots and was also observed low
during the 115200 capture. Stored bytes were mostly `00`, `FF`, `FE`, without
plausible framing. These are raw observations, not valid protocol communication,
the correct baud, electrical safety, or a diagnosis of power/wiring/firmware.
Neither capture transmitted AVR data or drove RESET.

The user reports the annoying welcome sound still repeats with this quiet image.
Its cause is **unknown**; a repeating melody alone does not prove MCU resets.
All further board tests have stopped. The immediate requested action is to
disconnect AVR power, or the whole assembly if it shares the supply, before
continuing diagnosis. No ISP probe, AVR reset-control write, AVR flash/fuse/EEPROM
operation, PBX call or test tone was performed. There is no existing reset-hold
API: the programmer's probe/backup/program operations release RESET on exit.

### Remaining acceptance

Track actual flash, startup, Wi-Fi scan/association/reboot persistence,
API/WebSocket, UART ACK/error counters, keypad/display/audio and matrix output
individually in GitHub issue #2. SDK compilation and host fixtures are not these
physical tests. FastLED uses the S3 backend; actual DMA use and refresh timing
are not inferred from a target macro. Firmware transfer abort/partition checks
do not establish hardware rollback or interrupted-power recovery.

Optional TFT LCD and W5500 work is separate from this migration's working
hardware capability; see the [hardware backlog](S3-STATUS-FRONTPANEL-INTEGRATION.md#additional-hardware-backlog).
