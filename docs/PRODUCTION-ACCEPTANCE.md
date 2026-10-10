# PakarController production acceptance

This checklist is the full user-requested release scope. A checked source/test
checkpoint is **not** a checked physical acceptance item. Keep the GitHub master
issue open until every applicable acceptance item has linked evidence. Any
exclusion needs a concrete donor path, reason and hardware/business scope;
"equivalent soul" and content-hash screening are not integration proof.

Master tracking issue: https://github.com/atomicdeploy/PakarController/issues/2

Current source checkpoint: version `0.2.4`, PR #1; commit/CI receipts are posted
to the issue. ASUS localhost:1022 currently refuses
connections; the previous board address times out and ledboard.local does not
resolve here. Continue offline source/UI work while this live gate is unavailable.
No Wi-Fi password or other deployment secret belongs in GitHub.

## Existing source/test checkpoints (not release acceptance)

- [x] One integrated ESP firmware contains API/WebSocket, LED engine, cloud,
  storage, self-OTA and AVR ISP; no separate programmer firmware is required.
- [x] Busy-wire and polling build flags exist on both boards; AVR busy input uses
  INPUT_PULLUP; busy-wire is the current source default.
- [x] Pealayer-derived native shell, Phosphor assets, dock panels, preferences,
  dialogs, notifications, tray, native icons and single-instance IPC exist with
  explicit source mappings in PEALAYER-DESKTOP-FOUNDATION.md.
- [x] Half-cell odd-column stagger, alternating native column backgrounds,
  logical-number search, row-major IDs and physical/name mapping exist.
- [x] PCController-compatible one-shot/loop/interval display text and actual
  segment readback are implemented; production AVR presentation tests pass.
- [x] Actual PU850 firmware transfer source and real async web transport are
  integrated; persistent auth editing and USB recovery have source tests.
- [x] Persistent hostname editing via API/CLI/TUI/Web/native/Telnet/USB exists;
  no suffix is added; 0.2.3 builds and source tests pass locally.
- [x] Actual PU850 Telnet negotiation and bounded line editor are adapted;
  hidden password input, cursor editing and no truncated-command execution tested.
- [x] ESP busy-high explicitly uses INPUT_PULLUP; receive-ready preloads LOW;
  polling builds avoid that GPIO. Production source tested; live acceptance open.
- [x] Production AVR tone player preserves 530 ms donor welcome deadlines under
  delayed service and rollover; register-double tests pass, audible gate open.
- [x] Expanded 1,868-file donor inventory exposes missing sparse paths; manual
  reviews are hash-bound. Strict release gate rejects unresolved rows.
- [x] Existing Flutter source preserved and hardened: secure credential migration,
  no mutation replay, exact logical search and Once text default; source tests
  and analyzer pass. Phone/tablet and production-signing acceptance remain open.

## Donor completeness and provenance

- [ ] Refresh exact refs and dirty state for Pealayer, canonical and legacy
  PCController, ControllerBoardMini/Puzzles, PuzzleBoard, 40LED and PU850/ASA0002E.
- [ ] Inventory every source/tool/asset/test file, including sparse-checkout and
  missing files; bind manual decisions to source hashes/revisions.
- [ ] Manually read every applicable donor source file; assign imported/adapted,
  verified superior replacement, business-only exclusion or hardware exclusion.
- [ ] Import useful Puzzles code/features, including input/menu/display/audio/
  configuration behavior; retain PuzzleBoard's A4/DIO, A5/CLK pin mapping.
- [ ] Reconcile every applicable PCController feature and protocol operation;
  no hardware-mismatched LCD/I2C page is falsely advertised.
- [ ] Complete all useful PU850 features, including actual shell/editor,
  Telnet negotiation, discovery metadata/MNDP, diagnostics, file tooling,
  uploaders and CI/dependency/deployment scripts; document each exclusion.
- [ ] Give each custom Pealayer UI element a donor file, target implementation,
  invocation path, tests and real rendered screenshot evidence.
- [ ] Add an automated release gate for unresolved applicable ledger rows;
  distinguish screening from manual read and from runtime acceptance.

## Electrical, timing and integrated firmware

- [ ] Confirm busy-wire pull-ups and boot-safe GPIO0 behavior on actual hardware;
  retain configurable polling mode and select default using measured loss/latency.
- [ ] Verify AVR hardware UART and ESP SoftwareSerial bidirectionally with CRC,
  error counters, reboot, lost-link recovery and LED/stream/update contention.
- [ ] Verify TM1637 ACK, brightness, actual segments and correct connector pins.
- [ ] Compare/import every donor melody, expose cue selection, test exact welcome
  timing and transitions; physically hear correct melody without clicks/jitter.
- [ ] Verify all 74HC165 inputs, 74HC595 outputs and ADC6 ninth input/debounce.
- [ ] Verify front-panel menus for ESP/AVR/device settings, actual parameters,
  game modes and logic; save/readback with truthful validation.
- [ ] Verify meaningful seven-segment cell/winner/config/update/error values,
  imported perimeter animation and progress; no permanent nuisance marquee.
- [ ] Verify full-brightness rainbow and deterministic effects with UART active;
  measure frame rate rather than adding an arbitrary 20fps cap.
- [ ] Test live ESP self-OTA (including abort/bad hash/parallel clients) with cues.
- [ ] Test live AVR ISP update/verify/recovery with Prog latch while full ESP
  services remain installed; prove reset/ISP/UART electrical ownership.
- [ ] Store/retrieve verified AVR+Urboot and EEPROM backups; test filesystem-full,
  interrupted writes and restore without erasing configuration.

## Business and streaming

- [ ] Validate 80 logical row-major cells, six LEDs per cell, half-cell stagger,
  alternating backgrounds, number highlighting, rename and physical remap on all UIs.
- [ ] Verify initial white table and per-cell red/off/effect changes via API,
  physical keypad/front panel, CLI/TUI, Web, desktop and mobile.
- [ ] Verify nine-button first-wins logic, ties/reset/debounce and matching
  shift-register and strip outputs.
- [ ] Complete snake example: input/control, reset, collision/game-over and
  consistent table mapping on all requested surfaces.
- [ ] Complete real screen sampler: monitor/region selection, geometry mapping,
  controls and measured Wi-Fi streaming; no mpv dependency.
- [ ] Verify Wi-Fi/WebSocket/UDP streaming framing, bounds, frame commit and
  disconnect recovery; define the AVR strip decision without duplicating engines.
- [ ] Verify multi-client control and simultaneous Wi-Fi plus USB host transport
  without double-applying commands or presenting inferred hardware state.

## Network, authentication and cloud

- [ ] Deploy and verify this board as ledboard.local; API readback and reboot
  persistence; hostname-first clients with current-address recovery only.
- [ ] Scan actual SSIDs; join DRS Tether using the latest privately supplied
  credentials; test reconnect and AP recovery without logging secrets.
- [ ] Verify station/AP/AP+station selection and persistence through all menus.
- [ ] Verify mDNS, SSDP, NetBIOS and all retained donor discovery on both PCs;
  verify advertisements refer to the same hardware UUID and configured name.
- [ ] Verify NTP sync, offline/reconnect behavior and displayed time state.
- [ ] Verify configured Basic/session authentication, clear sign-in UI, credential
  rotation, expiry, restart, USB recovery and access to protected WebSockets.
- [ ] Test mqtt.digitalogic.ir round-trip, TLS/auth/ACLs and disconnect/reconnect;
  coordinate only scoped credentials/service changes on office.digitalogic.ir.
- [ ] Verify configurable outbound WebSocket endpoint and round-trip/reconnect.
- [ ] Expose safe network/cloud/device configuration across applicable API,
  CLI/TUI/Web/native/mobile/front-panel surfaces with secrets write-only.

## Applications and visual acceptance

- [ ] Complete native Pealayer-derived UX parity: modals, preferences, themes,
  palettes, icon vocabulary, actions/events, workspace/tray/single instance,
  shortcuts, seven-segment mirror and hardware monitor; exclude media/timeline/cue.
- [ ] Inspect real Pakar and Pealayer screenshots in light/dark themes and narrow
  layouts; fix alignment, overflow, focus, empty/error/stale/pending states.
- [ ] Build a proper interactive table TUI with per-cell effects, search/mapping,
  business modes and device configuration, not only a command prompt.
- [ ] Complete Web UI parity with contextual panels, table controls, configuration,
  progress and hardware monitor; no fixed architecture narration or fake status.
- [ ] Audit/preserve/finish the existing Flutter Android/tablet work; analyze/test/
  APK build, adaptive UI, secure settings, discovery and live board round-trips.
- [ ] Install/run identical verified desktop packages on both PCs and open Web UI
  on David-ASUS; verify same authoritative board state across clients.

## GitHub and release

- [ ] Preserve every local-only source change and relevant semantic ref in GitHub;
  leave unrelated user work intact, and never treat a draft as a release.
- [ ] Pass all exact-head firmware (both flags), native, Web, mobile and ledger
  checks; fix CI failures before merge.
- [ ] Merge ready verified work to main; use semantic branches and no date/random
  suffixes in product/release names.
- [ ] Publish a version-consistent production release with artifacts/hashes,
  donor licenses, migration/recovery instructions and actual deployment receipts.
- [ ] Close the master issue only after applicable acceptance boxes have evidence;
  leave any blocked/untested requirement visibly open with its next action.

## Immediate continuation order

1. Bind this checklist to the GitHub master issue/PR and fix exact-head CI.
2. Audit Puzzles/audio/busy wiring and full donor inventories; import/test omissions.
3. Complete interactive TUI, richer Web/native menus and retained PU850 services.
4. Audit and preserve Flutter sources; integrate mobile CI and builds.
5. Restore ASUS reachability for live Wi-Fi, both firmware deployments, all
   physical tests, cloud round-trip and multi-client verification.
6. Merge/release only after the evidence gates above are actually satisfied.
