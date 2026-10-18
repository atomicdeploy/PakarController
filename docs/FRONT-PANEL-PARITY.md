# Front-panel reuse and acceptance

## Current semantic presentation

Version0.2.7 / clientbuild14 supersedes the historical checkpoint below.
Current donor comparison uses b0111ae0 and its1379-line FrontPanelRuntime fragment;
exact source/target/test hashes are in file-comparison-evidence.json.

| Context | Display and lifetime |
|---|---|
| Cell selection | C001..C080 resting selected logical cell; no brand-logo filler |
| Physical pixel diagnostic | P001..P480 stays until another LED operation supersedes diagnostics |
| Pattern / output | rAIn, FIrE, CHAS or OFF remain the resting context |
| First wins | rEAd, then U001..U009 (seven-segment W approximation) |
| Action / mode / network / scan | Normally2s; long text scrolls once through a terminal blank, with the actual extended duration in WS/USB metadata |
| Connectivity warning | Slow500ms on/500ms off, then return to resting context |
| Failure / completion | Double-fast100ms on/100off/100on/700off; failure6s, completion2s |
| Update | Actual P000..P100 while progress is known, UPdt otherwise; no synthetic percentage or unverified completion |
| ESP settings menu | Contextual labels/values including APCL, not unexplained both; abandoned physical menu closes after15s, offers cannot renew it |

Programming remains highest priority, then explicit capture. An individually
acknowledged maintenance-panel option permits finite update messages over menus.
Ordinary notices do not steal a manual message/capture/menu. Lower-priority
events remain in the client stream even when not selected for physical display.
No notice writes flash/EEPROM or triggers a buzzer. The ESP owns bounded policy;
AVR continues the confirmed A4/A5 TM1637 transport and raw-segment observation.

GET `/api/panel` returns requested text, bounded event history/drops and separate
fresh AVR masks/ACK/session. WS `panel-event` and USB opcode0xB8 share kind, ID,
uptime, value, actual lifetime, blink and full text. Their hardware_confirmed
is false; only AVR observation is physical bus readback, not optical acceptance.
Native and Flutter retain separate owner-bound semantic and actual mirror state.

The common receipted WS/HTTP command supports `panel-notice` with printable ASCII
text<=32, optional blink steady/slow/double-fast and duration_ms100..10000.
Use the same required board_id/boot_id/request_id/expires_at_ms envelope as other
commands. A long one-pass message extends its advertised lifetime enough to
finish; an accepted request is not an AVR ACK or a physical-display assertion.

Focused verification: `tools/test-panel.ps1`, `tools/test-display.ps1`,
`tools/test-update.ps1`, native panel_event/peripheral_events and Flutter
panel_event/mirroring_transport fixtures. Actual delivery/readback evidence is
recorded in the commissioning checkpoint; fixture success is not human approval.

Source comparison: canonical PCController commit
`20c6115bb462fe5111b66cd7c21f7328a1f78892`. This is a focused front-panel
comparison, not a claim that every PCController subsystem has been imported.
The file inventory in `legacy-source-ledger.csv` distinguishes screening from
manual integration in `reuse-decisions.csv`.

| Actual PCController source | Pakar implementation | Scope / evidence |
|---|---|---|
| `Project/Runtime/FrontPanelRuntime.inc.h::showHostSegmentText` | `firmware/avr/FrontPanel.cpp` | terminal empty frame; once, loop, interval; local display during interval pause; native tests execute production AVR code |
| `Project/Runtime/ProtocolRuntime.inc.h::DisplayText` | `FrontPanel::acceptDisplayPayload` | scheduled target 5, speed/hold/repeat/interval/forced scroll, raw masks, explicit release target 4; legacy long text deliberately changed from implicit loop to once |
| `Tools/Controller/internal/native/commands.go::ScheduledSegmentPayload` | `desktop/src/display.rs::Settings::payload` | same 8-byte header, printable ASCII <=40 bytes and force-scroll flag; Rust tests |
| `Tools/Controller/internal/native/commands.go::HostPanelReleasePayload` | `display::RELEASE`, HTTP `release=true` | relinquishes text ownership without sending a placeholder |
| `Tools/Controller/internal/tui/display_editor.go` | native front-panel controls plus shared CLI/TUI parser | repeat selector, speed, static hold, interval pause, forced short-text scrolling; no LCD targets advertised on this board |
| `Project/Hardware/SevenSegments.*` | `firmware/avr/SevenSegments.*` | existing copied transport, fixed-point rendering, cached masks and brightness ramp; PuzzleBoard A4/DIO and A5/CLK retained |
| PCController front-panel readback | status and native/Web seven-segment preview | actual peer masks, brightness, ACK, and host-active state; no simulated display state |
| PCController menu / sensors / LCD pages | not declared equivalent | hardware-specific pages and host menu editing require their own capability and parity work; no LCD/I2C modules installed |

The default text request is **Once**. Text longer than four cells scrolls
automatically. Short text remains static unless forced scrolling is selected.
Static once text uses its hold duration (1500 ms by default); zero means a
persistent static presentation until replaced or released. Explicit static Loop
also remains visible until released. Scrolling ends with a fully empty frame,
then returns to the local page. Its cadence, not static hold, controls duration.
Interval waits expose the local page and restart after the selected pause.

Numbers now cancel older text ownership; otherwise a loop could hide a new
number. `Prog` remains higher priority than host text. Text settings persist in
desktop preferences but opening the app never automatically presents text.

Native and Web expose repeat choices and Stop while the live peer reports
host-owned text. Native uses both the authenticated API and framed USB payload
according to the selected transport.

The USB status extension appends four masks, brightness and host-active state
after the unchanged 19-byte prefix. Older firmware remains parsable, but does
not supply segment preview or host-ownership readback. Firmware and native client
share version `0.2.1`. HTTP and cloud JSON use the same text scheduler; Telnet
also exposes the timing/repeat parameters and explicit release.

CLI/TUI examples:

```
PakarController --command display segments --speed-ms 260 --repeat once -- HELLO WORLD
PakarController --command display segments --repeat interval --interval-ms 30000 --scroll -- HI
PakarController --command display stop
```

Run `tools/test-protocol.ps1` or `tools/test-protocol.sh` for wire and actual AVR
presentation tests, and `cargo test --locked --manifest-path desktop/Cargo.toml`
for client validation. Native tests cover once/legacy termination, the terminal
blank frame, explicit looping, interval local ownership, release, numeric
override, raw masks, programming priority, bounds, and millis rollover.

Physical acceptance still requires deploying both firmware images through the
integrated ISP/OTA path and observing live masks/ACK and `tm1637_host_active`
transition true -> false -> remain false after a one-shot message. Never treat
host tests or a client screenshot as physical board proof.
