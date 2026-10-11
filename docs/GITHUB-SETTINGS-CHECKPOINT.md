# Native device settings and remaining acceptance

Settings tracking: [native device settings #4](https://github.com/atomicdeploy/PakarController/issues/4).
Parent tracking: [production acceptance #2](https://github.com/atomicdeploy/PakarController/issues/2),
[exact donor comparisons #3](https://github.com/atomicdeploy/PakarController/issues/3).

## Delivered source and local acceptance

- [x] Pealayer-derived native/embedded device settings center, grouped navigation, search, cards, Phosphor icons and theme application.
- [x] Ten actual domains: overview, identity, Wi-Fi, MQTT, WebSocket, access, LEDs/cells, front panel, AVR/inputs, firmware/storage.
- [x] Existing real identity/access/front-panel/hardware widgets invoked; workspace links preserve pending edits and return to the actual editor.
- [x] Full current network/cloud public parameter forms, required-field validation, SSID scan/selection and explicit open-network handling.
- [x] Secret keep/replace/clear, write-only values and secret-free persisted preferences/IPC.
- [x] One settings POST, positive acknowledgment, exact public GET readback; no automatic replay after ambiguous outcome.
- [x] Dirty draft/conflict/endpoint guards; native/embedded close protects in-flight operations.
- [x] Preserve actual USB network staging/chunking as explicitly unconfirmed provisioning, not fabricated saved state.
- [x] Real ISP probe/backup/download/stage/program, latch recovery and uncompressed ESP OTA with explicit confirmation and positive acknowledgments.
- [x] Export size guards and no PC-file overwrite; header/length checks never called authenticity proof.
- [x] Single-instance settings/deep-link CLI/IPC navigation and secret-free runtime settings snapshot.
- [x] 60 native tests, locked release build, existing Web/access/audio/CLI and comparison/ledger/version checks pass locally.
- [x] Inspect actual rendered Overview/Wi-Fi/MQTT/Firmware screenshots against a read-only SIMULATED loopback peer using an isolated profile.
- [x] Preserve user preference file; reopen updated app at `http://ledboard.local` with honest offline state.

## Not certified / remaining work

- [ ] Real ASUS/device reachability, discovery and both-PC deployment/visual acceptance.
- [ ] Real save + reconnect/readback for Wi-Fi/cloud/access/identity; MQTT/WebSocket connectivity, not just request acknowledgment.
- [ ] Physical display, melodies, inputs/outputs/LEDs and USB provisioning acknowledgment/readback acceptance.
- [ ] Physical ISP backup/export/stage/program/recovery and ESP OTA/reboot acceptance with verified electrical readiness.
- [ ] Staged AVR image hash/ownership and configuration revision/CAS in the firmware API for stronger multi-client guarantees.
- [ ] Public JSON escaping and additional meaningful runtime parameter contracts where feasible.
- [ ] Web/mobile settings contract and visual parity, and the full existing donor-file backlog; this native checkpoint does not resolve it.
- [ ] Narrow-window/light-theme/native/embedded interaction and mutation tests beyond current static render/model/HTTP fixture coverage.
- [ ] Complete CI for this exact settings commit and full production-release acceptance before a final release claim.

ASUS `localhost:1022` refused SSH and `ledboard.local` did not resolve during
this checkpoint. No real board write or audible/electrical claim was made.
The Windows capture service failed twice with 0x8007041D; static screenshots
were taken with the existing repository exporter, while navigation used the
production CLI/IPC. Model/HTTP/GUI fixtures are labeled and are not hardware.

Exact controls, source mapping, intentional build-only limits and tests:
[DEVICE-SETTINGS.md](https://github.com/atomicdeploy/PakarController/blob/feature/pealayer-desktop-foundation/docs/DEVICE-SETTINGS.md).
