# ESP32-S3 integrated firmware

S3 is the primary PakarController target. Its entrypoint includes the shared
application at `../esp/application.ino`; S3 `Shared*.cpp` units compile common
modules with S3 hardware adapters. It is not an ESP8266 binary or a separate
programmer-only image. Source support/builds do not certify a production board.

Read [architecture and gaps](../../docs/ARCHITECTURE.md),
[migration/wiring](../../docs/ESP32-S3-MIGRATION.md) and
[historical commissioning receipts](../../docs/ESP32-S3-COMMISSIONING.md).

## Independent hardware paths

| Purpose | Common DevKit source mapping |
|---|---|
| AVR UART1 | S3 GPIO17 TX → AVR D0/RXD; AVR D1/TXD → S3 GPIO18 RX;38400baud |
| Matrix / box LEDs | GPIO4 → DIN, common GND; strip supply/logic margin must be appropriate |
| First-wins game reset | Normally-open momentary button GPIO21 → GND; internal pull-up, 20ms debounce, one reset per press |
| AVR ISP | MOSI11, MISO13, SCK12, reset-control10 through a voltage-safe ownership arrangement |
| Optional W5500 | Separate SPI3/HSPI: SCK5, MISO6, MOSI7, CS8, reset9, interrupt14 |
| Optional S3 I2C LCD | SDA15, SCL16; not the AVR's TM1637 connector |
| Host USB | Build-selected UART0 bridge, native HWCDC/JTAG or OTG CDC; native USB usesGPIO19/20 |
| Onboard RGB | Independent configurable status output; module-dependent GPIO38/48, not the matrix DIN |

`src/BoardPins.h` owns overridable defaults and collision checks. Do not connect
5V signals directly to S3 pins. AVR keeps hardware serial and its TM1637 mapping
A4/DIO + A5/CLK. Native USB VBUS/external-power isolation needs physical checking.

The first-wins reset button clears the winner and restores enabled boxes to the
configured idle color (white by default), after the AVR acknowledges reset.
It does not reboot either MCU. It is ignored during programming/update or when
the AVR is not ready; held buttons do not repeat and presses are not replayed
after recovery. `PAKAR_S3_GAME_RESET_PIN=-1` disables it; table builds disable it
by default. EN/RST only reboots, GPIO10 belongs to AVR ISP, and BOOT is unchanged.

## Build profiles

Use the canonical `tools/build-firmware.ps1` wrapper from the repository root:

- `-Target esp32s3` selects buffered hardware UART; no busy wire.
- `-GameMode table-selection|first-wins` bakes one program, respectively480
  table pixels or216 Keys pixels. Paired AVR must use the same role.
- `-S3LedGpio -S3LedRmtDma` includes both wire drivers, selectable at runtime.
  Either flag can be used alone; neither means no LED driver and no LED-pin
  ownership. The corresponding macros are `PAKAR_S3_LED_GPIO_ENABLED` and
  `PAKAR_S3_LED_RMT_ENABLED`, each defaulting to zero.
- `-S3LedDefault auto|gpio-static|rmt-dma` chooses the initial preference only
  when no valid board-saved choice exists. Auto prefers GPIO if included.
  This is independent of runtime `static|effects` rendering; GPIO can animate.
- `-S3HostTransport uart0|hwcdc|otg` selects the actual framed host connector.
  Default isUART0. OTG/HWCDC have explicit output variants but share the
  canonical compile cache; see [USB/OTG](../../docs/USB-OTG.md).
- `-S3Bluetooth disabled` excludes BLE initialization. Runtime-capable builds
  preserve opt-in configuration; missing/corrupt BLE configuration defaults Off.
- `-S3Ethernet` compiles optional W5500 support; new profiles leave it disabled
  until commissioned. Do not disable a previously active profile by assumption.
- `-S3FlashMB`, `-S3Psram` must describe the actual module. The historical
  ledboard profile uses16MB/OPI; generic defaults are not that board's identity.
- `-S3IspElectricalVerified` is an instance-specific acknowledgement, not proof
  about any newly connected DevKit.

## Execution and services

The common `LedGrid` owns mapping/effects/streaming. FastLED supplies color
helpers; the S3 wire is the custom `src/LedOutput.cpp` worker, not automatic
FastLED fallback. It consumes immutable complete frames on core1 through a
capacity-one latest-frame mailbox. RMT explicitly requires GDMA; the same worker
finishes the current frame, quiesces and releases the old driver, initializes
the requested one, and republishes the current complete frame. Failed RMT setup
falls back to GPIO only after safe resource release. Uncertain DMA ownership
fails closed, retaining its buffers until explicit recovery. Flash-pause, bounded refill and digital-capture
guards are implemented, but do not establish optical or electrical success.

Runtime selection uses `POST /api/led/output` with `backend=gpio-static` or
`backend=rmt-dma`, WebSocket `led-output`, or USB opcode `0x5c` payload `[3,1]`
or `[3,2]`. Request the backend separately from renderer/output settings.
The board saves the preference with stage/readback/rename before acknowledging
intent. `led_output.driver` reports compiled choices, actual/requested backend,
switch pending/error and revision; `led_output.backend_saved` and
`backend_settings_error` report persistence separately. An ACK is not completion.

Async HTTP, inbound WebSocket, USB host framing, MQTT/outbound WebSocket,
LittleFS, semantic front-panel events and ISP/Urboot maintenance coexist in one
application. Binary shell framing remains separate from raw serial text.
`src/MenuAudio.h` adapts AVR cue IDs with ACK/deadline/cadence guards;
physical playback and menu parity remain acceptance gates.

W5500 uses native ETH/lwIP so existing HTTP/WS/OTA/TLS/cloud APIs can use Ethernet
without a second WIZnet socket implementation. Independent enable/DHCP/static/DNS
settings live at `/api/network/ethernet`. Disabling Wi-Fi or Ethernet requires an
actual surviving recovery path, not merely a saved profile. Cable/link,
Ethernet-only/dual-interface serving and Internet fetching remain unaccepted.

S3 discovery includes mDNS/DNS-SD, SSDP, MNDP and bounded NBNS. NBNS is not SMB,
WINS or LLMNR. LLDP TX is scheduled on the TCPIP owner; receiving is an explicit
promiscuous-mode opt-in because of W5500 filtering, retaining at most four
sanitized neighbors. No raw L2 bridge is implemented. Router IGD/port mapping is
separate from SSDP advertisements and is not proven by a compile or WAN address.

## Open gates

Partial/glitchy matrix output, periodic stalls, interrupted OTA and current
AVR recovery must be resolved with fresh runtime and physical evidence.
Cold-boot NTP, BLE/TLS/OTA memory coexistence, independent external-power
operation, W5500/LLDP, optional LCD/TFT and full client parity remain separate
requirements. Preserve saved scenes, images and journals; do not replay an
uncertain update or infer current firmware from these build options.
