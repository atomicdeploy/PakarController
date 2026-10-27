# System architecture and current gaps

This is the current source-level overview, checked against commit
`8ce8f29eee01a979bbd42e7d3573a6e3ace9eba2` plus the existing local working
changes. The local ESP sketch and native shell already had concurrent edits;
this documentation pass does not validate, alter or deploy those edits.

**Source support is not deployed support.** A build, ACK, framebuffer or driver
counter is not proof of physical light, sound, Ethernet, radio stability or an
installed client. The master acceptance gate remains [issue #2](https://github.com/atomicdeploy/PakarController/issues/2).

## 1. Topology and ownership

```text
Windows Rust app / CLI / TUI     Android Flutter app     Embedded browser UI
            |                         |                         |
            +---- selected USB or HTTP + WebSocket / BLE -------+
                                      |
                               integrated ESP firmware
                          commands, state, renderer, updates
                           /          |                  \
             hardware UART        GPIO4 LEDs          IP network stack
                   |                  |              Wi-Fi / optional W5500
              ATmega328P       table or key boxes             |
          keys, shift registers,                   MQTT / outbound WS
          TM1637 and buzzer                       Internet control relay
                   ^
                   +----- separate ISP/reset path from ESP
```

There is one integrated ESP application, not a normal image plus a separate
ISP-programmer image. ESP32-S3 is the primary hardware target. Its entrypoint
`firmware/esp32s3/esp32s3.ino` includes the shared application under
`firmware/esp/application.ino`. S3 `Shared*.cpp` translation units compile
the same reusable application modules with S3 hardware adapters.

| Owner | Authoritative responsibilities |
|---|---|
| ESP | Board/application identity, network settings, LED scene and mapping, renderer, cloud services, client admission, persisted configuration, firmware transactions |
| AVR | Physical key sampling/debounce and first-winner arbitration, shift registers, actual TM1637 masks/ACK, exact-tone playback, local menu and programming safety |
| Client | User selection, Find text, panel arrangement, scroll positions, drafts and transport preferences; never invented hardware state |
| Bridge | Forward through the selected existing board owner; preserve downstream identity and distinguish its own instance/hop |
| Client-management server | App packages, presentation bundles, enrolled client telemetry and limited management; not the ESP renderer or AVR authority |

An ESP-only table can render and accept LED commands without a healthy AVR.
AVR-dependent display/text/buzzer/input/output controls need their own current
acknowledgements. A buzzer capability acknowledges the firmware adapter, not an
electrical sensor proving a buzzer is fitted or heard.

## 2. Repository structure

| Path | Role / important entrypoints |
|---|---|
| `firmware/esp32s3/` | S3 entrypoint, hardware UART, native USB selection, explicit LED worker, BLE, ETH/W5500, discovery, RGB status and LCD adapters |
| `firmware/esp/` | Shared integrated sketch; `LedGrid`, `AsyncHttpService`, `CloudServices`, `RemoteFirmware`, `AvrIsp`, `UrbootRoutes`, shell, storage, identity and management routes |
| `firmware/esp8266/` | Thin compatibility sketch and translation units; no separate application implementation |
| `firmware/avr/` | ATmega328P program, local hardware drivers, menus, persistent safety/Prog state and PCController-derived contracts |
| `libraries/BoardCore/src/` | COBS/CRC protocol, sequence/receipt and identity guards, role/capability definitions, task scheduler, cell mapping and shared bounded policies |
| `desktop/` | Rust binary: egui workspace, CLI/TUI, USB owner, HTTP/WS workers, selected-owner bridge, firmware/update and host-management modules |
| `mobile/` | Flutter workspace and Android Kotlin platform channels for USB/BLE, document access, tone mirroring, discovery and installer integration |
| `assets/contracts/`, `interfaces/` | Shared settings catalog, public release/support policy and generated-interface inputs |
| `assets/web/` | Reusable embedded Web modules; generated assets are embedded into `WebUi.h`, `BasicUi.h` and compressed headers |
| `services/remote-panel/` | Node MQTT-to-HTTP/WS remote board-control relay |
| `services/client-management/` | Separate Node/SQLite app distribution, data-only presentation updates and enrolled client-management service |
| `deployment/` | Scoped broker/service deployment configuration |
| `tools/`, `pakar.cmd`, `pakar.sh` | Guarded builds, dependency pins, staging, publication, update/recovery and evidence checks |
| `tests/`, component test directories | Host doubles, actual transport loopbacks, protocol/renderer/ownership and UI regressions; not physical certification |
| `docs/` | Contracts, hardware plans, historical execution receipts, donor ledger and open acceptance |

The shared ESP sketch and client orchestration entrypoints are still substantial
monoliths. The surrounding modules improve ownership but this is not yet a fully
split independent library/service architecture. Refactoring must preserve the
one-owner and receipt boundaries rather than duplicate handlers.

## 3. Exactly one board role per firmware

`BoardCore::GameRole` selects `PAKAR_GAME_MODE=1|2`; any other value fails
compilation. The canonical wrapper exposes `-GameMode table-selection|first-wins`.
Both processors in a pair must use the same role. This is **not** a runtime
chooser between several baked games. Effects and diagnostics are presentation
services. Snake is currently compiled out by `PAKAR_SNAKE_ENABLED=0`.

| Role | LED capacity | Behavior |
|---|---|---|
| Table selection / Pakar Table | 80 groups × 6 pixels = 480 | Persistent white/red/off assignments, atomic batches/imports, red-to-white lock, mapping/names, effects/streaming/diagnostics |
| First wins / Pakar Keys | 9 boxes × 2 groups × 12 pixels = 216 | AVR selects a winner; each box has illumination and indicator groups; persistent enabled-box mask and idle/win/lose RGB palette |

First-wins settings are in `KeyGameSettings.h` and `/keys.json`.
`POST /api/game/settings` saves the enabled mask/palette and reports AVR
application separately: `saved:true` is not `avr_applied:true`. The AVR must
ACK the enabled-key mask before a fresh input may start a round. Disabling a
middle box does not compact the physical cable; active pixel count and required
wire span are different. Physical nine-key/output/palette acceptance is open.

The two roles share transport/maintenance components but must not borrow each
other's table-specific UI. Some clients/routes/diagnostics still have
80-cell/480-pixel assumptions; contextual generalization for the newer
216-pixel role is incomplete and must not be claimed as universal support.

### Table identities and geometry

Three separate concepts must stay separate:

1. Stable logical IDs 1–80, retaining their logical 10×8 indexing for API/files.
2. Display positions: **20 interleaved columns × 4 rows**, not a simple 10×8
   on-screen grid. The first visual row is `1,11,2,12,...,10,20`.
   The first column is `1,21,41,61`; the second is `11,31,51,71`.
   The first column is not shifted down; alternate columns are +0.5 cell in Y.
   Current Flutter row pitch is 1.0 with no alternating horizontal row shift.
3. Physical cable groups: the board's independent editable 80-entry permutation,
   not UI order or names.

The default cable function starts
`80,60,40,20,10,30,50,70,79,59,39,19,9,29,49,69,...`.
Each entry denotes six consecutive physical pixels. Logical 80 is pixels 1–6;
logical 1 is physical group 77/pixels 457–462, not the last group. Valid saved
custom maps are not silently replaced. See `TableCellMapping.h`,
`mobile/lib/table_layout.dart` and [LED acceptance](LED-FIRST-ACCEPTANCE.md).

Red lock rejects a direct red-to-white assignment atomically, including an
affected batch. Off is still allowed; the lock is not an electrical interlock.
Rust/Flutter have JSON/CSV/XLSX exchange; embedded Web has JSON/CSV.
See [assignment contract](CELL-ASSIGNMENTS.md).

## 4. Execution, rendering and LED signal

Network callbacks do not own the LED scene. HTTP work crosses the
`AsyncHttpService` mailbox; complete WebSocket messages cross `LoopMessages`
to the application loop. Callback-owned buffers must not outlive callbacks.
The S3 WS queue has eight bounded 4096-byte message slots allocated explicitly
in PSRAM; allocation failure is a real unavailable service, not permission to
consume the internal OTA/network heap silently.

`LedGrid` composes a complete frame, then `PakarS3::LedOutput` sends it through
a capacity-one latest-frame mailbox. The worker is pinned to core 1, while Wi-Fi
normally runs on core 0. Complete obsolete frames may be coalesced; in-flight
wire buffers must remain immutable. Flash activity acquires the LED flash-pause
guard before cache-sensitive work. Periodic refresh is distinct from animation.

Two **build-time** wire backends exist:

- `-S3LedBackend static`: custom IRAM/cycle-timed GPIO transmission; no RMT/DMA.
  It masks local core 1 interrupts for one bounded payload and requires the
  checked CPU timing profile. The name describes the wire backend, **not** a
  prohibition on animated rainbow. LED output mode `static|effects` is a
  separate runtime setting.
- `-S3LedBackend rmt-dma`: explicit ESP-IDF RMT TX + GDMA, bounded symbol ring,
  refill/flash guards and optional digital signal capture. It requires DMA
  rather than silently falling back to another backend.

FastLED supplies colors/effect helpers; the current S3 data wire is the project's
explicit worker, **not** FastLED automatically deciding whether DMA is available.
Runtime capabilities/status expose backend, RMT/DMA use, readiness, failures and
transmission/capture counters. The separate onboard status RGB has its own pin,
configuration and semantic state engine.

At 800 kbit/s, 480 RGB pixels plus a 300 µs latch need at least 14.7 ms: the
theoretical wire ceiling is about 68 FPS. This is not measured application/optical
FPS. Host streaming requests default to 60 FPS and can be uncapped; physical
serialization, CPU/network/flash stalls and backpressure still impose limits.
A 216-pixel role has a different wire budget. No arbitrary 20 FPS cap is intended.

**Open incident:** partial/glitchy output after roughly 22–23 cells, periodic
pauses, and earlier all-Off/readback mismatches were reported. Digital pad
capture and counters do not establish DIN analogue margin or optical behavior.
The GPIO fallback and RMT/DMA comparison still need controlled physical
acceptance. Do not mark the incident solved from a build or successful ACK.

## 5. ESP–AVR and USB ownership

| Path | Current source contract |
|---|---|
| ESP–AVR | Separate hardware UART1 on S3; AVR hardware UART0; 38400 baud |
| Legacy ESP8266–AVR | SoftwareSerial on ESP only; AVR remains hardware UART |
| PC/Android–ESP | Framed host serial at 115200; selected S3 UART0 bridge, hardware CDC/JTAG or native OTG CDC |
| AVR ISP | Separate SCK/MOSI/MISO/reset path; exclusive maintenance ownership |

Shared framing is COBS with CRC-8/ATM, bounded 48-byte payloads, sequence IDs and
explicit ACK/event operations. Larger snapshots/transfers use bounded chunks.
This is not CORBA, nor a multidrop UART bus: AVR and host serial stay physically
and logically separate. Raw shell/debug text must not corrupt framed channels.

Link policies are matched build flags on both processors:

- S3 `buffered`: hardware buffers and complete-frame admission, no busy wire.
- ESP8266 `busy-wire`: HIGH blocks unsolicited AVR TX; input pull-up retained.
- ESP8266 `polling`: explicit status requests, no busy GPIO.

Removing a busy wire from an old busy-wire AVR image is not a migration.

`-S3HostTransport uart0|hwcdc|otg` selects the actual host connector.
UART0 is the default bridge path; OTG uses native GPIO19/20 and TinyUSB CDC.
Android USB currently targets explicitly identified Espressif CDC, **not every
CH343/CP210x/CH340 adapter**. Its table control/mirroring subset does not yet
expose every firmware, storage or configuration operation.
See [USB/OTG limits](USB-OTG.md).

A client selects one physical board transport at a time. HTTP and WS can coexist
inside the one IP method; that does not authorize simultaneous USB and network
mutation owners. Multiple authorized clients may independently connect to the
board. A native bridge shares its existing selected owner instead of opening a
second serial handle.

## 6. IP transports, cloud and discovery

- One ESPAsyncWebServer service hosts HTTP API, embedded Web, inbound `/ws`,
  firmware transfer and maintenance. S3 uses AsyncTCP; ESP8266 uses ESPAsyncTCP.
- Wi-Fi supports persisted AP, station, or both, DHCP/static/DNS and configured
  hostname. AP SSID/password, device authentication and broker credentials are
  separate settings. No random hostname suffix is required.
- Optional W5500 uses the ESP32 native ETH/lwIP path: the existing API, WS, TLS,
  MQTT and updater use the same stack, not a separate limited Ethernet API.
  It is build-gated and runtime-configurable; cable/link and Ethernet-only
  acceptance remain open.
- Board discovery includes mDNS/DNS-SD, SSDP description/lifecycle, MNDP and S3
  NBNS. Clients/bridges have their own discovery owners and supported subsets.
  W5500 LLDP is optional; receive requires explicit promiscuous mode and retains
  bounded neighbors. Discovery is neither authentication nor proof of control.
- SSDP advertisement is not router UPnP IGD port mapping. Router recognition,
  IGD discovery/mapping and public-WAN reachability are separate open gates.
- NTP supplies UTC for TLS/cloud deadlines; cold-boot/autonomous recovery is
  not established by manually setting the clock during an older test.
- BLE is S3-only GATT, not Classic/SPP/audio. It is a separately selected offline
  transport, runtime opt-in or excluded by build. BLE/TLS/OTA memory coexistence
  remains open; the recovery build must not silently re-enable BLE.
- Telnet/serial shell uses shared validated handlers, VT100/line editing,
  history and TUI properties. Source default is two Telnet sessions, bounded
  by build to 1–4, not the old single-client-only description.

### Bidirectional flow

LAN HTTP/WS commands use request ID, board ID, boot ID and an advertised
monotonic command-clock deadline. Same-intent HTTP recovery retrieves a receipt;
it does not invent a new mutation after a lost WS reply.

MQTT and configured outbound WS use identity/request IDs plus UTC expiry,
maximum 60 seconds. Commands, replies, board events, status and fragmented
checksummed telemetry flow both ways. MCU/bridge queues and deduplication are
bounded; uncertainty never grants permission to resend with a new ID.

The Internet `remote-panel` service translates authenticated HTTPS/WS control
to scoped broker commands. It forwards only supported routes/capabilities, not
every binary/ISP/storage API. Native uses that existing HTTP/WS interface rather
than another direct MQTT client. Android's separate advanced bridge has MQTT,
outbound WS and authenticated local support surfaces, but remains
foreground-only. HTTP event journals use bounded cursor reads; HTTP itself is
not a full-duplex socket. See [duplex details and exclusions](DUPLEX-CONTROL.md).

The pinned PubSubClient callback exposes no RETAIN bit. Firmware cannot claim
retained-command rejection at that layer; broker policy must prohibit retained
command publication. TLS/ACLs, boot/deadline/replay admission and SUBACK/PUBACK
limitations remain separate concerns.

n8n, Telegram and Bale are future consumers of these contracts. No completed
bot/webhook integration is claimed.

## 7. Clients and persistence

**Desktop:** one Rust/eframe/egui executable supplies the docked workspace,
preferences, actions/dialogs, tray, notifications, single-instance IPC, CLI/TUI,
screen sampling, USB/network owners, bridge and firmware/app updates. Pealayer
assets and foundation code are actually imported/adapted, but complete donor
parity is not established. There is no mpv dependency. Windows is the principal
delivered target; cross-platform scripts do not prove macOS/Linux app packaging.

**Flutter:** one phone/tablet workspace uses shared page/capability catalogs,
stable board-keyed selection/Find/scroll drafts, explicit action/readback,
secure credentials and native Android channels. Firmware, diagnostics, bridge,
updates/support and mirroring are separate contextual pages. Android background
execution, physical USB/BLE behavior and full localization/settings/visual parity
are not established by loopback/widget tests.

**Embedded Web:** compiled, local-first HTML/CSS/JS shares artwork, fonts,
settings catalog, capability policy, guarded HTTP/WS and renderer metadata.
It is not a copy of all Rust/Flutter widgets; parity needs explicit mapping/tests.

Persistence has separate owners:

- ESP LittleFS: identity/access/network/cloud/Ethernet/shell/radio configuration,
  scene, physical mapping/names, first-wins settings and firmware journals/images.
- `SceneStore`: semantic cells/mode/effect/brightness/flags, debounced saves,
  unchanged-write suppression, pending-file verification and atomic rename.
  Animation frames and live peer authorization are not written per frame.
- AVR EEPROM: hardware configuration and durable Prog/reset safety records.
- Client storage: layout/selection/drafts/policies; platform secret stores keep
  credentials separate. Retained presentation is not fresh command authority.
- Server SQLite/content storage: enrolled clients, single-use claims, catalogs
  and immutable packages, separate from board flash state.

Unknown, stale, pending, applied and physically verified must remain distinct.
No client can promise never to show Unknown when it genuinely has no valid
observation; it should retain layout and disable writes instead of inventing
On/Off or saying “synchronized” after rejection.

## 8. Firmware and app updates are different systems

Board updates include local Async HTTP OTA, SHA-pinned Internet stage/apply,
running-image export and boot/slot/digest verification. AVR paths include ISP
full-image backup/program/readback and Urboot application programming through
reset/UART. Prog ownership, EEPROM preservation and recovered identity gate
release. Compilation does not prove current electrical/reset/fuse correctness.

A two-board transaction has a journal and recovery policy; it is not magic
atomic rollback of two independent processors. Interrupted/failed jobs require
inspection. Lost upload/apply responses are not automatically replayed. Preserve
the previous image/settings and verify the running digest, not just a version.

The separate `client-management` service handles Windows/Android packages,
release notifications, enrollment, bounded telemetry and allowlisted
check-update/cleanup operations. Presentation updates currently carry validated
palette data, not executable Dart/HTML or arbitrary patches. APK behavior changes
still need a newer same-signer package and Android installer consent. Android
automatic processing is foreground-only; a silent installer/background daemon
is not implemented. Windows replacement needs a managed install, verified helper,
closed drafts and normal lifecycle shutdown. Remote arbitrary shell/uninstall
is not an available management feature.
See [client management](CLIENT-MANAGEMENT.md).

## 9. Build flags, dependencies and delivery

Source release is `0.2.8`; shared client build is currently 23 in
`interfaces/client-release.json` and `mobile/pubspec.yaml`. Installed versions
must be read from those installations; these source values do not certify them.

| Layer | Checked-in dependency declarations |
|---|---|
| Firmware platforms | Arduino-ESP32 3.3.12; ESP8266 3.1.2; MiniCore 3.1.3 |
| Firmware libraries | FastLED 3.10.6; WebSockets 2.7.2; PubSubClient 2.8.0; ArduinoJson 7.4.3; ESP Async WebServer 3.12.1; Async TCP 3.5.0; ESP Async TCP 2.0.0 |
| Rust UI | eframe 0.36.2; egui_dock 0.21.1; egui-phosphor 0.14.0; egui-elegance 0.16.2 |
| Rust I/O/data | reqwest/rustls, tungstenite, serialport, serde, mdns-sd, xcap, calamine and rust_xlsxwriter; exact resolution is in Cargo.lock |
| Flutter | Dart constraint ^3.13.3; http, web_socket_channel, mqtt_client, secure storage/preferences, crypto, archive/XML; pubspec.lock owns resolution |
| Node services | Node >=24, ws 8.22.0; remote relay additionally mqtt 5.16.1; management uses node:sqlite |

Firmware pins are in `tools/firmware-dependencies.json` and sketch profiles;
builds must not silently upgrade them. These are repository declarations, not
a claim of globally newest libraries or the currently installed toolchain.

Use canonical firmware/native/mobile wrappers and the existing warm caches,
not clean/debug builds by default. Role, UART policy, LED backend, host USB,
BLE exclusion, optional Ethernet, ISP electrical acknowledgement and physical
flash/PSRAM layout are build decisions. Network mode, hostname, palettes and
other supported settings are runtime/persistent decisions; do not present build
flags as freely editable runtime hardware settings.

CI performs contract/donor/generated-asset gates, focused tests, firmware builds,
native/Android packaging and release provenance. Deployment is a separate opt-in
self-hosted workflow/environment. A push, artifact or green CodeQL run is not a
flash or complete green build.

## 10. Gap register and release gates

| Priority / owner | Current gap | Required next evidence |
|---|---|---|
| Critical LED / #2 | Partial 480-pixel output, flicker/pauses and all-Off failures have no blanket optical resolution | Explicit backend comparison, known frame/readback, adequate electrical margin and user observation across the complete matrix |
| Critical recovery / #7 | Latest side-chat GPIO fallback OTA reset its TCP connection; new image/boot was not verified | Fresh identity/slot inspection, authorized safe recovery, exact running-image digest and actual output test; do not replay an uncertain upload |
| High CI / #11 | Main `8ce8f29` run 39469143527 failed quality: LedWire.h could not include GameRole.h in the message-queue host fixture; firmware/native/Android jobs were skipped | Repair the fixture include path, rerun affected checks and exact-head CI; do not weaken production headers or label skipped builds passing |
| High AVR / #6 | Historical ISP/Urboot successes are not current paired-board recovery, role, clock/fuse or peripheral acceptance | Fresh signature/backups/reset ownership, matched role firmware, EEPROM verification, UART/peripheral tests |
| High newer role / #2/#10 | 216-pixel first-wins enabled-mask/palette work is newer than several table-centric clients, relays and diagnostics | Nine physical inputs, configured masks/ACK, both LED groups per box, reset/winner behavior and appropriate UI across transports |
| High client reliability / #8/#10 | Timeout/reconnect, mapping, background/resume, selection and panel stability require installed-client acceptance | Actual Rust/Android round trips, observed events/readback, lifecycle/scroll retention and bounded soak |
| High autonomous IP / #2/#7 | Cold-boot NTP, Internet recovery/update and USB-disconnected operation not generally accepted | External-power restart, fresh TLS/cloud exchange, safe changed-image OTA and reconnect/AP recovery |
| Hardware expansion / #19/#20 | W5500 Ethernet-only/dual-interface/LLDP, optional LCD/TFT and BLE/TLS/OTA coexistence remain incomplete or unverified | Actual wiring/module/power, interface/radio memory observations, served/fetched endpoints and peripheral acceptance |
| Security/release / #7/#11 | Android preview signing; opt-in auth and plain LAN HTTP/Telnet; broker retained-command limitation | Durable signing/update consent, scoped TLS/ACL policy and controlled deployment; no unauthenticated public port forwarding |
| Donor/UX / #3/#4/#5 | Broad Pealayer/PCController/RayanLamp/PuzzleBoard parity remains incomplete | Actual complete-file comparison, concrete omission reasons, feature tests and real visual/physical acceptance |
| Future integrations | n8n/Telegram/Bale; background Android agent; richer sampler/TUI and generic PCController RPC parity | Explicit integration/lifecycle/security work, not relabeling existing REST as a finished bot |

Current inventory, complete-read, pending and gap counts are maintained in
[the generated authoritative ledger](FILE-COMPARISON.md), not duplicated here
as quickly stale totals. Complete-read coverage does not mean feature parity;
the PCController convergence review adds explicit design-level gap records, not
new firmware imports. Unread PuzzleBoard files remain unreviewed, not exclusions.

Fresh GitHub inspection found no open PRs, but the main build is red as described
above; CodeQL passed and deployment was skipped. These are separate results.

Latest direct hardware inventory in this chat reached David-ASUS through trusted
SSH localhost:8022 and saw one present CH343 USB–UART adapter on COM24
(VID 1A86/PID 55D3, serial 5CE5200177). The adapter alone does not identify its MCU.
`ledboard.local` did not resolve in that inventory. This documentation pass did
not reopen serial ports, reset, flash, issue LED commands or test current boards.

## 11. Documentation map and freshness

- [API](API.md), [living API policy](API-POLICY.md),
  [peer identity](PEER-IDENTITY.md), [duplex control](DUPLEX-CONTROL.md).
- [S3 hardware/migration](ESP32-S3-MIGRATION.md),
  [S3 commissioning receipts](ESP32-S3-COMMISSIONING.md),
  [USB/OTG](USB-OTG.md), [BLE](BLUETOOTH.md).
- [Device context](DEVICE-CONTEXT.md), [shared UI catalog](INTERFACE-CONTRACT.md),
  [assignment exchange](CELL-ASSIGNMENTS.md), [client management](CLIENT-MANAGEMENT.md).
- [Production acceptance](PRODUCTION-ACCEPTANCE.md),
  [donor comparison](FILE-COMPARISON.md), [reuse procedure](REUSE-PREFLIGHT.md).
- [PCController gaps and future shared AVR core](PC-CONTROLLER-MERGE-READINESS.md):
  exact transport, timing, pins and EEPROM incompatibilities; not a stock-image
  replacement or completed firmware merge.

Older documents retain exact version/hash/boot/test checkpoints as historical
evidence. They do not override newer role, geometry, selected-transport or wire
backend source. In particular, old “current” USB/hostname/AVR/OTA observations
must be refreshed before operation. This overview is not an assertion that every
specialist acceptance item or historical documentation sentence is complete.
