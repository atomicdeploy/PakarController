# PCController current-hardware contracts and future enablement

The checked milestones below describe the original source/test checkpoint,
not current deployment or complete donor parity. Current source release is
0.2.8; current comparison and future AVR convergence gates are in
[PCController gaps and shared-core readiness](PC-CONTROLLER-MERGE-READINESS.md).
In particular, current tone mirroring has timestamp fields and S3 uses buffered
hardware UART; the older five-byte/ESP8266-only wording is historical.

Focused issue: https://github.com/atomicdeploy/PakarController/issues/6.
Parent production scope: #2. Source parity: #3. Device settings GUI: #4. Native Windows: #5.

## Delivered source and host acceptance

- [x] Actual PCController registry/generated constants imported; isolate donor PWM 0x11 from legacy Pakar audio.
- [x] Full 48-byte request/chunk transport; no mutation from incomplete/expired assembly.
- [x] Explicit current-hardware profile; observed capabilities, not fictitious sensor fields.
- [x] Settings persistence, CRC dual banks, readback, deferred coalescing, failed-write pending-edit preservation.
- [x] Silent/welcome, display brightness/off, stream period, ADC6 threshold, board name, page defaults/visibility/order/save-last-page.
- [x] Actual TM1637 ACK, keepalive, cache/brightness restoration on correct A4/A5 pins.
- [x] Text/raw/capture/release, metadata, full 40-character scheduling and non-repeating default.
- [x] Donor key classifier on all nine inputs; immediate Down, hold/repeat/click/up telemetry.
- [x] Menu listing/layout/action/navigation and observed front-panel schema-2 mirror.
- [x] Actual donor macro ring, guarded execution, buffering/query/cancel, recording/download/replay.
- [x] Actual donor automation store/executor, all nine key IDs, budgets, CRC/readback, no boot provisioning.
- [x] ESP-owned addressable LED adapter; one existing effects engine; retain full 480-pixel PKAR streaming.
- [x] JSON API, WebSocket/cloud, USB contract ingress and correlated receipts; no uncertain mutation retries.
- [x] Prog/host-loss/reset safety; separate application state from ISP latch.
- [x] Busy-wire and polling builds; polling mirror/event drains; AVR internal pull-up retained.
- [x] Current features can be disabled. Seven future hardware guards default off and fail closed.
- [x] 23 current-donor mappings with actual source/target hashes, line anchors and exclusions.
- [x] Actual typed donor event encoder; five-byte key/buzzer payloads, without incompatible UART timestamp extensions.
- [x] Native CLI/TUI configuration, ACK plus persistent readback; authenticated built-CLI fixture.
- [x] Explicit terminal receipt consumption; repeated settings edits free queue capacity while preserving late-response quarantine. No cleanup of pending or reboot-mismatched operations.
- [x] Request-selector-bound macro query/download versus mutation response validation; read-only queries cannot be completed by an ACK, zero-code ERROR rejected.
- [x] Source versions consistent at 0.2.6; local firmware/host/Rust checks.

## Acceptance still open

- [x] Baseline f2e38a7 GitHub CI green: firmware, Windows desktop and Android preview in run 38044317586. Later receipt-consumption changes require a new run.
- [ ] Exact final response-validation commit GitHub CI green.
- [ ] Physical AVR/ESP firmware flash and version readback when ASUS/device returns.
- [ ] Live UART/busy grant, all nine key/output identities and electrical acceptance.
- [ ] Physical TM1637 unplug/replug, brightness/off/scroll/capture and welcome timing.
- [ ] Live hostname-based API/WebSocket/USB and multi-client routing.
- [ ] Live MQTT/outbound WebSocket endpoint/authentication acceptance.
- [ ] Dedicated professional GUI/Web/mobile editing for every newly exposed AVR contract (track #4).
- [ ] AVR-stored macro steps targeting ESP-owned LEDs: correlated cross-owner execution/completion without treating queue acceptance as LED success.
- [ ] Remaining donor-wide file reviews/UI work (track #3/#5, not silently closed here).
- [ ] Future sensors/PWM/RF/relay/status-RGB/I2C/AVR LED driver adapters, pins, memory and hardware tests before changing guards to 1.
- [ ] Optional host-directory/menu adapter for donor registry-only 0x42..0x44.
- [ ] Production release and two-machine rollout after real acceptance, not just builds.

No physical firmware or EEPROM writes occurred in this checkpoint. ASUS SSH localhost:1022 refuses connection. Queue acceptance and loopback fixtures are not board ACK proof.
