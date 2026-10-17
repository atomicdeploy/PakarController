# S3 status RGB and front-panel integration

This is source work and a wiring-safe integration plan, not deployed or optical
acceptance. The common S3 DevKit onboard RGB pin is board-dependent. Neither
GPIO38 nor GPIO48 is automatically selected; GPIO4 remains matrix-only.

## Actual code reuse and mirror contract

`firmware/esp32s3/src/StatusRgb.h` adapts the actual PCController
`Project/StatusLedMath.h` byte scaling and unsigned interpolation arithmetic,
and `StatusLedController.cpp` absolute elapsed/period evaluation, repeat-state
phase preservation, output-byte reporting, and nonblocking service. It does not
copy the donor PWM-channel driver, AVR EEPROM profile table, or sixteen-bit
clock restriction: this S3 has a separate addressable onboard pixel, its own
persistent configuration owner, and a 32-bit clock. General custom effect/profile
editing remains useful pending work, not full donor parity.

Pealayer `src/ui/status_bar.rs::status_led_display_color` and its
`physical_status_led_uses_authoritative_rgb_without_double_dimming` regression
test establish the mirror rule: publish the actual already-scaled driver output
bytes, not a color palette guess, and never apply brightness twice. Only display
the physical-pixel mirror when the selected live board reports available output.
PCController `Tools/Controller/web/src/status-led-event.ts` further protects
transport-source epoch, instance identity, revision ordering and duplicate
suppression. That TypeScript source was read, but its complete source/session
ordering adapter is NOT implemented by this firmware class; clients must retain
their existing peer freshness gate and clear output availability on disconnect.

The canonical integrated sketch now hooks one board-owned instance from
setup/loop. These are source integrations, not completed hardware acceptance:

1. After board identification, validate the confirmed onboard GPIO against
   `supportedPin` (currently only 38/48) and persisted explicit enable/brightness.
2. Supply a writer `(pin,r,g,b)->bool`, wrapping the platform RGB driver; return
   false if it cannot accept the output. Local driver acceptance is not light
   seen by a person. Never use `RGB_BUILTIN` or a pin guess as identification.
3. Select the semantic state from actual lifecycle observations: boot;
   connecting; ready only with network and required AVR session ready; AP-only;
   AVR peer lost; update; warning; fault. AP+STA policy and error priorities
   belong to that lifecycle, not an independent timer in the LED class.
4. Call `service(millis(), writer)` in the cooperative loop. Its 16ms output
   interval applies ONLY to one status pixel, never matrix animation/stream FPS.
5. The current `/api/status` serializes `status_rgb`: actual output `rgb`,
   brightness, state, pin, available/enabled/have_output, revision and
   updated_uptime_ms. Persisted configuration has its own authenticated
   `/api/status-rgb` GET/POST owner. Driver submission is not optical acceptance.
   Publishing effect/progress metadata and revision-driven event delivery remain
   pending; the class snapshot alone does not establish those API fields.
6. Native/Web source mirrors the authoritative bytes with live-peer and
   have-output guards, without double brightness or stale-board fallback.
   Mobile parity and rendered/physical acceptance remain pending.

## ESP settings through the AVR front panel

The current AVR is authoritative for its local six-page menu:
`PcControllerRuntime.cpp::key` handles physical page/action keys; page0 cell,
page1 outputs, page2 key monitor, page3 mute, page4 display brightness, page5 run.
`MenuList`, `MenuLayoutGet/Set`, `MenuSetPage`, `FrontPanelGet`, captured panel
ownership, settings persistence and safety latch are real contracts. Do not
rename these pages or fabricate a seventh-page ACK from the S3 alone.

The current `HostMenuDirectory`, `HostMenuContent`, `HostMenuStateGet` opcodes
are explicitly returned unsupported by the AVR, as in the donor registry. Full
ESP setting pages therefore require a paired AVR implementation and truthful
capability advertisement, not just a new Web card.

The paired source extension now lives in `PcControllerRuntime::{observeEspPanel,
espKey,requestEspAction}` and `FrontPanel::{setEspMenu,showEspMenu}`, with ESP
owner adapter `firmware/esp8266/src/EspFrontPanel.h`. It does not pretend to
implement the unrelated generic donor HostMenu opcodes. The living contract
adds `kEspPanel`, `kEspPanelResponse` and explicit `kCapEspPanel` instead.

Current paired source hooks and their acceptance requirements:

1. AVR HELLO advertises `kCapEspPanel` only when `PAKAR_PC_MENU` is compiled.
   Route actual `kEspPanel`/`kEspPanelResponse` into
   `pcController.observeEspPanel(frame, now)`; handle these even in the polling
   receive path. Do not forward or invent an ACK before this method accepts.
2. The AVR hardware adapter overrides `showEspMenu(...)` to call
   `frontPanel.setEspMenu(...)`, and `menuNavigationCue(forward)` to call its
   existing `playAudioCue(kCueIncrease/kCueDecrease)` owner. The base methods
   deliberately do nothing so an unintegrated adapter cannot claim playback.
3. ESP implements `EspFrontPanel::Settings` against the existing authoritative
   network service. `read` must return persisted/applied mode, DHCP and hostname,
   never a pending UI draft. `setMode/setDhcp` must validate, persist and apply,
   then readback; helper rejects a successful callback whose readback disagrees.
   Restore the previous in-memory configuration after save failure too.
4. Only after AVR HELLO individually ACKs `kCapEspPanel`, send an actual
   `offer` periodically within five seconds. Route physical menu requests to
   `handle(frame, now, settings, sender)`. Sequence+action and a 1500ms AVR
   deadline correlate results; exact retransmissions within two seconds reuse
   the result, preventing duplicate saves/reconnect/scan work.
5. A physical ninth-key hold enters/leaves the ESP submenu only with a fresh
   valid offer and no programming/captured-panel conflict. Keys1/2 change page,
   keys3/4 adjust local mode/DHCP drafts. Ninth-key click confirms; double-click
   discards/exits. Existing six AVR pages and their stored layout stay unchanged.
   Page0 is AP/CL/both, page1 reconnect, page2 scan, page3 DHCP/static from valid
   saved addresses, page4 hostname summary, page5 back. Mode/DHCP never change
   until ESP accepts, persists and returns actual state. A scan/reconnect result
   displays `SEnt`, not “saved”, scan completed or link connected.

Bounded source acceptance and remaining work:

- [x] Paired menu source, draft/confirm behavior, real readback correlation,
  stale/deadline/Prog gates and once-only hostname summary implemented.
- [x] Focused native tests execute production runtime/helper/display/status
  engine: persistence failure, dishonest callback readback, no-station safety,
  duplicate requests, wrong/late response, unchanged local AVR pages, draft
  isolation, expiry and programming gates. These are host doubles, not a board.
- [x] AVR production hardware-adapter hooks, feature-conditional HELLO bit and
  actual-frame receive routing, plus source hook guard. Existing PCController
  regression suite passes; hardware UART38400 and display A4/A5 unchanged.
- [x] ESP network-owner callbacks are wired in the canonical integrated sketch:
  persist candidate mode/DHCP settings before replacing active ownership,
  preserve the stored station secret on mode-only changes, defer network apply,
  and route actual frame/offer state. This is source wiring, not a board ACK.
- [x] S3 status-RGB persistence and authenticated configuration owner are wired;
  bounds/checksum/readback, failed-save preservation and no-op writes have host
  tests. Native/Web source uses actual output bytes rather than inferred color.
- [ ] Final integrated AVR/S3 SDK compilation and exact-source package hashes.
- [ ] Paired board deployment and live capability/menu ACKs; physical keys,
  TM1637 display, welcome/navigation buzzer timing and actual RGB transitions.
- [ ] Safe hostname character editor and full static-address entry (first pass
  displays hostname and toggles only an already-valid stored static profile).
- [ ] Station scan results/selection, status-pixel settings, configurable menu
  audio and all remote surface parity. Scan acceptance alone is not result data.

For physical testing, enter the ordinary AVR menu with ninth-key double-click
before holding it to enter ESP settings. The existing immediate game-mode Down
edge is intentionally retained for first-wins latency, so holding ninth-key
directly from a running game can still execute that game's initial Down action.
No business action is executed while the ESP submenu is already active.

## Additional hardware backlog

Tracked durably in [optional TFT LCD and W5500 issue #19](https://github.com/atomicdeploy/PakarController/issues/19),
under production acceptance [#2](https://github.com/atomicdeploy/PakarController/issues/2).
Existing issue titles and bodies were checked before creating this focused
backlog. These are reservations only: no TFT or Ethernet implementation,
physical voltage/pin acceptance or live link is claimed.

- [ ] TFT LCD: identify exact controller, supply, resolution and touch hardware;
  allocate GPIOs without colliding with AVR UART/ISP, LED4, onboard RGB, USB or
  W5500 reservations. Add optional capability/driver, shared scene/settings
  model and front-panel navigation; never advertise a display that did not ACK.
- [ ] W5500: implement independent SPI3/HSPI driver on SCK5/MISO6/MOSI7/CS8,
  RST9/INT14. Keep AVR ISP on SPI2/FSPI, confirm module supply, and integrate
  shared hostname/DHCP/static addressing, AP+STA coexistence, discovery,
  link-loss/reconnect, JSON/WebSocket/MQTT, OTA and network-driven AVR flashing.
  Pin reservation is not a running Ethernet implementation or link proof.
- [ ] Use native ESP32 ETH/lwIP integration and the existing application network
  services, not a second Ethernet socket/client/credential/update stack. Confirm
  the exact module/supply and TFT controller before assigning any TFT GPIOs;
  protect UART17/18, matrix4, isolated ISP11/13/12/reset10, USB/bridge/strap and
  actual module flash/PSRAM/RGB exclusions. Any later shared TFT/W5500 bus needs
  explicit transaction ownership and contention evidence, never AVR ISP sharing.
- [ ] Expose only individually acknowledged optional capabilities, with shared
  configuration/readback across applicable front-panel/API/CLI/TUI/Telnet/Web/
  native/mobile surfaces. Measure coexistence under UART, matrix and OTA/ISP;
  source declarations and simulated UIs do not close physical acceptance.

## Donor bytes read for this bounded slice

Local donor files were read from installed source checkouts, not treated as
fresh ledger snapshots. SHA256 values are original bytes:

| File | SHA256 |
|---|---|
| PCController Project/StatusLedMath.h | 997e15f12ca1d51689567f3a15b2f3b716261173878797cd5e68866289724740 |
| PCController Project/StatusLedController.h | afbc70e3e8674c80d779168c37dc7430ab058e35baf2705b4b45cc11dd4e22ca |
| PCController Project/StatusLedController.cpp | b68da7e21af4396027c0f7ed7267d4c778292da47c19f1b9172bb4547ed1f53d |
| PCController Tools/Controller/web/src/status-led-event.ts | 8331dd092e597dd728e6a18615d64463bfa0e70719f51c22ddc3f72ff9e3cd24 |
| Pealayer src/ui/status_bar.rs | e407025a1ee959230634f8350de20d58432313d6dd0f734ada735b8fc9fec4db |

The Pealayer full file includes many unrelated UI consumers; only the stated
physical RGB mirror/test contract is mapped here. Generic status widgets,
operations, telemetry, menus and frame-rate policy are not claimed complete.
The authoritative reuse evidence ledger still owns their pending work.
