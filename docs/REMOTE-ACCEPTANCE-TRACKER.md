# Remote updater / telemetry / PU850 route acceptance

Current contracts are in [architecture and gaps](ARCHITECTURE.md),
[duplex control](DUPLEX-CONTROL.md) and [remote maintenance](REMOTE-MAINTENANCE.md).
Source is0.2.8/client build23; installed-board/client identities require fresh
inspection. The checklist below records the older0.2.7 feature checkpoint,
not a claim that its host status or all accepted operations are current.

Current blockers include interrupted/unverified GPIO-fallback OTA, current AVR
pair recovery, cold-boot NTP/standalone Internet and full LAN/cloud soak.
ASUS was reached on localhost:8022 with CH343/COM24 in the latest inventory;
ledboard.local was unresolved. The old1022 observation below is historical.
No board action is authorized by this document.

## Historical source/test milestone

- [x] Pinned manifest download, selected-board staging, SHA-256/profile/size checks and exact job/boot identity.
- [x] Durable recovery/Prog gates, EESAVE-independent EEPROM restoration, full flash readback, no automatic mutation replay.
- [x] Post-reboot ESP hash/version/CRC and actual AVR version/profile gate before one Prog release.
- [x] Integrated native Firmware settings, Web, JSON API, local WebSocket, cloud command, CLI/TUI and Telnet paths.
- [x] Full operational board telemetry, eight-client bounded reports, Windows host metrics and checksummed MQTT snapshot fragments.
- [x] All 50 donor route registrations classified with current hashes, anchors and exact omission reasons.
- [x] Locked busy-wire/polling builds, Rust tests, actual ISP emulator, Web/publisher/CLI/reassembly tests.
- [x] Actual RemoteFirmware state-machine host tests: complete pair, stage-only, boot ownership, tampered staging/postboot, interrupted prepare/program/release journals, lost release ACK, storage/HTTP/callback/OTA failures, cancellation and cross-origin bearer isolation. Network/FS/updater doubles, not physical/TLS proof.
- [ ] Visual sign-off: candidate settings window launched against a labelled simulated peer; screenshot helper failed twice with service timeout 0x8007041D. Existing production app/Preferences preserved.
- [ ] Physical HTTPS download and wrong hash/size/profile/clock/CA rejection against the actual publisher.
- [ ] Eight-client heap/TLS/telemetry stress on ESP8266 and real cloud MQTT/WebSocket reception.
- [ ] Actual both-board update, EEPROM/Urboot/postboot/Prog/peripheral acceptance and controlled power-loss recovery.
- [ ] ASUS deployment/visual check on the remote laptop, currently SSH localhost:1022 refuses connections; ledboard.local unresolved locally.

Retain draft PR status and open production acceptance until hardware checks pass. See docs/REMOTE-MAINTENANCE.md and docs/PU850-ROUTE-PARITY.json. Broader donor/UI/mobile parity remains tracked by existing issues, not closed by this feature.
