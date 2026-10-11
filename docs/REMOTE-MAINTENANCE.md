# Remote firmware and telemetry

Version 0.2.7 introduces one integrated updater for the ESP8266 and ATmega328P. This is a release candidate: compilation and host/emulator tests are not physical board, TLS endpoint, power-loss or production deployment proof. The current SSH endpoint refuses connections and ledboard.local does not resolve from the build PC.

## Owner workflow

Use Device settings → Firmware in the existing native application, the Web workspace's Verified remote update card, or the CLI/TUI commands below. Download & verify is stage-only. Apply is a separate confirmation tied to the observed job ID, ESP boot identity and trusted manifest SHA-256. Read actual phase/error/byte counters rather than treating an accepted request as completion.

```text
PakarController.exe --command firmware status
PakarController.exe --command firmware fetch https://updates.example.test/PakarController/manifest.json MANIFEST_SHA256 all
PakarController.exe --command firmware apply JOB_ID BOOT_ID MANIFEST_SHA256
PakarController.exe --command firmware cancel JOB_ID BOOT_ID MANIFEST_SHA256
PakarController.exe --command telemetry
```

The CLI/TUI uses PAKAR_ENDPOINT (prefer http://ledboard.local), PAKAR_USER and PAKAR_PASSWORD. Optional remote-server bearer credentials use PAKAR_FIRMWARE_TOKEN, not a command-line argument or query string. Native/Web token inputs are cleared when queued, and are not saved in preferences. The authenticated Telnet shell supports info, telemetry and the same firmware commands; its cancel syntax is firmware cancel JOB_ID BOOT_ID. Do not expose plaintext HTTP/Telnet outside a trusted LAN/VPN. JSON cloud commands travel through the configured authenticated MQTT command topic or outbound WebSocket connection, using command values firmware-fetch, firmware-status, firmware-apply and firmware-cancel.

## Direct JSON API

All maintenance and full telemetry require device-owner authentication. POST uses application/x-www-form-urlencoded; no credentials in the URL. Same-origin checks apply to browser writes.

| Route | Contract |
| --- | --- |
| GET /api/firmware/remote | Schema 1, job_id, boot_id, phase, targets, manifest_sha256, actual byte counts, error, busy/cancellable |
| POST /api/firmware/fetch | url, sha256, targets=1 ESP / 2 AVR / 3 both; apply=false by default; optional bearer and explicit allow_downgrade=true |
| POST /api/firmware/apply | job_id, boot_id, sha256; only exact ready job |
| POST /api/firmware/cancel | job_id, boot_id; downloading/ready only, never cancel an erase/commit |
| GET /api/telemetry or /info | Full structured device/board/client snapshot; /info?format=text provides a text heading plus JSON |
| POST /api/clients/report | report=JSON; authenticated allowlisted operational metadata, maximum 1024 bytes |
| GET/HEAD /file/download | artifact=esp-running, esp-staged, avr-staged, avr-flash, avr-eeprom; Range supported, no arbitrary paths |

Local WebSocket /ws accepts a read-only {"type":"telemetry"} request or JSON remote commands. Use job_id/boot_id/sha256 for apply and job_id/boot_id for cancel. Never replay an uncertain mutation. Both HTTP and WebSocket report the same updater state, not independent jobs. Only one maintenance operation can own the boards/storage; concurrent clients can inspect state. Artifact read leases prevent replacing files mid-download.

## Publisher contract

Create an immutable directory containing manifest.json, esp.bin and avr.bin. Deliver the manifest hash to the owner through a separately trusted channel. The manifest hash is mandatory; HTTPS alone is not a firmware signing scheme. No unpinned auto-update feed is enabled.

```text
node tools/create-remote-manifest.mjs build/esp8266-busy-wire/esp8266.ino.bin build/avr-busy-wire/avr.ino.with_bootloader.bin https://updates.example.test/PakarController/ 0.2.7 busy-wire OUTPUT_DIRECTORY
```

For polling use the build/esp8266 and build/avr artifacts and link_mode polling. Do not serve one profile under the other profile's URLs. This tool generates checksums and copies publisher files; it neither deploys them nor flashes hardware. Replace the illustrative example.test URL before use. Never overwrite a live directory while a client is downloading.

The living manifest requires product PakarController, profile pakar-nodemcu-atmega328p and link_mode busy-wire/polling, without schema or protocol generations. Both components declare url, size, lowercase sha256, version and model. AVR additionally declares signature 1E950F and bootloader_included=true. Its raw image is exactly 32768 bytes including Urboot; application-only HEX is rejected. The ESP raw image must match the running flash mode. Both versions must match for target all. Single-board updates are same-version repair only; explicit downgrades do not bypass profile, signature, digest, Prog, EEPROM or pair verification.

Publisher requirements: direct HTTPS URLs; a chain anchored in ISRG Root X1; correct device clock; positive exact Content-Length; no redirects, chunked transfer, compression or content encoding. Bearer authorization is sent only to the original manifest authority, never a different image host. Bounded endpoint URLs are 383 characters, manifest 4096 bytes; timeout is 20 seconds idle / 300 seconds for the complete download transaction. HTTP firmware fetching is disabled unless PAKAR_ALLOW_HTTP_FIRMWARE=1 is explicitly built (native/Web surfaces continue to prefer HTTPS).

ESP8266 TLS is memory-constrained: the fetcher uses 16 KiB records only with enough contiguous heap, otherwise 4 KiB receive records. The publisher must then support MFLN or limit TLS records to 4 KiB. Unsupported TLS/heap/clock/CA conditions fail before any board erase; there is no setInsecure fallback. Live endpoint compatibility and peak heap with many clients remain acceptance gates, not assumed from a PC HTTP test. ESP filesystem/recovery/auth storage is not replaced by this updater. Signed image query URLs can exist inside the stored pinned manifest; that manifest is not exposed by the artifact allowlist and should not be included in public exports.

## Transaction and recovery boundaries

1. Journal ownership; download exact-length manifest and every selected image to LittleFS; verify SHA-256, profile, reset/header/bootloader and storage capacity.
2. Re-read image hashes and check ESP OTA capacity before any AVR erase. Require actual AVR UART/profile/version and acknowledged durable Prog arm. Play an acknowledged update-start cue, then create, flush, reopen and byte-verify flash + EEPROM + fuse metadata backups.
3. ISP checks the ATmega328P signature and readable lock state, and compares the current EEPROM to its durable snapshot before erase. Restore EEPROM first even when EESAVE is off, verify it, write and byte-verify all flash, then verify EEPROM again. No fuses are changed.
4. Require the new AVR UART version. Commit the checksum-verified ESP image; after reboot verify running ESP raw SHA-256, version and flash CRC, and the actual matching AVR version/profile.
5. Journal release intent, then release Prog exactly once and play the success cue. A lost release ACK is uncertain, not permission to repeat it. Reboot during release intent never replays release.

Downloads and ready staging can be cancelled. Interrupted applying states fail rather than automatically resuming erase/write/release. Staging and recovery files remain available for inspection; a fresh explicit job may replace staged images. Keep the known-good flash/EEPROM exports before recovery. The three backup files are verified individually; their multi-file rename is not an atomic set. No erase occurs if backup commit fails.

If Prog is already armed, a new owner-requested recovery requires target both, stage-only download and a separate pinned Apply. It retains the original recovery files rather than replacing the known-good flash backup with a half-updated AVR. The retained EEPROM must still equal the target before erase. Missing/changed snapshots or absent AVR UART refuse remote recovery; physical ISP recovery is then necessary. A newly authorized job is not an automatic retry of an uncertain earlier job.

Two physical chips cannot be updated atomically. Power loss after erase or during EEPROM restoration can require manual ISP recovery from the retained snapshot. The system does not claim guaranteed rollback or guaranteed Prog preservation under arbitrary power loss. If a mutation is uncertain, inspect actual Prog/version/backups; do not repeatedly press Apply. A missing AVR UART/profile/backup refuses erase—even if SPI alone would answer. This conservative remote path is not blind recovery of an entirely unresponsive AVR.

## Full operational telemetry

ESP: actual chip/flash/core/SDK/version/integrity, uptime/reset, heap/fragmentation, OTA space, filesystem use, UART/USB counters and pinout. Network: actual STA/AP mode, connectivity, SSIDs, IP/MAC/netmask/gateway/DNS, RSSI/channel/DHCP/AP client count and discovery state. Clock: valid/source/configured NTP/observed SNTP callback, UTC and last-setting uptime; no fabricated RTC. Board state includes cells, effects, inputs/outputs, seven-segment mirror and Prog.

AVR firmware 0.2.7 adds a read-only 46-byte telemetry response (0x58 request, 0xB1 response), preserving the existing PCController tunneled dialect. It reports actual uptime, free SRAM snapshot, raw ADC6, buzzer, board settings/persistence, UART errors and lost events. Poll every five seconds; after twelve seconds values are stale/null, not invented. Older AVR versions do not advertise this extension.

Native Windows clients report application/version, session UUID, hostname, OS/architecture, process ID/uptime, CPU parallelism, physical/available RAM and primary-screen/monitor information. Web clients report only available browser/platform/CPU-hint/screen data. The ESP observes the source IP; every other client field is explicitly self-reported, not attested. Capacity is eight clients, recent TTL two minutes. A different peer cannot overwrite a live client ID. No passwords, cookies, bearer tokens, personal documents, file inventories, screen captures or arbitrary PC access are collected. Missing measurements are null. This is complete operational metadata for these transports, not all private information on a computer.

Configured cloud connections retain the existing status feed and additionally deliver full telemetry. WebSocket sends living telemetry JSON. MQTT full snapshots use non-retained QoS0 packets at BASE/HOSTNAME/telemetry/part: type, boot-unique snapshot_id, index/count/total_bytes, sha256, encoding=base64 and data. Raw chunk size is 768 bytes; snapshot limit 16384 bytes. tools/telemetry-reassembly.mjs rejects mixed metadata, invalid encodings, missing parts, checksum failures, conflicting duplicates and expired snapshots. Never present partial packets as current board state. A failed publication waits for the next snapshot; no guaranteed-delivery claim is made. The retained /status feed is unchanged.

## Acceptance checklist

- [x] Both firmware profiles compile against the locked toolchain; native protocol/policy/actual ISP driver emulation tests.
- [x] Built desktop CLI loopback tests: authenticated staging, pinned identity, wrong ACK and no replay; 77 Rust tests, including accepted-job notifications not being marked completed.
- [x] Production Web remote controls parse and pin exact identity; publisher and MQTT reassembly tests.
- [x] Actual RemoteFirmware module host tests, with real host SHA-256 and doubled network/FS/updater: interrupted journals, lost release ACK, staging/postboot tamper, complete pair, cancellation and failure cases. Not TLS, flash hardware or physical power-loss proof.
- [x] All 50 reviewed PU850 route registrations have concrete source/target mapping or exclusions.
- [ ] Actual HTTPS publisher compatibility and complete download/cancel/hash rejection on ledboard.local.
- [ ] Full eight-client telemetry snapshot, peak heap, MQTT and WebSocket cloud reception on hardware.
- [ ] Real two-board update, EEPROM/Urboot preservation, post-reboot pair/readback/Prog release, audible cues and peripherals.
- [ ] Controlled power-loss/recovery acceptance with a known-good exported snapshot and safe electrical setup.
- [ ] Broader donor file/UI/mobile parity remains separate and open; this route feature does not certify the whole project.
