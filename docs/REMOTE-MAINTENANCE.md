# Remote firmware and telemetry

Version 0.2.7 contains one integrated updater for ESP8266 or ESP32-S3 and ATmega328P. This is a release candidate: compilation and host/emulator tests are not physical board, TLS endpoint, power-loss or production deployment proof. The S3 inactive-slot implementation and publisher compile and pass host tests. A corrected live Wi-Fi upload and full running-image SHA readback passed, but intermittent stream failures and a later panic remain unresolved. See the current deployment receipt before attempting a handoff.

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
| GET /api/firmware/remote | job_id, boot_id, phase, targets, manifest_sha256, actual byte counts, error, busy/cancellable |
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

# S3 ESP-only commissioning; '-' deliberately omits the AVR component.
node tools/create-remote-manifest.mjs build/esp32s3-buffered/esp32s3.ino.bin - https://updates.example.test/PakarController/ 0.2.7 buffered OUTPUT_DIRECTORY esp32s3
```

For polling use the build/esp8266 and build/avr artifacts and link_mode polling. Do not serve one profile under the other profile's URLs. This tool generates checksums and copies publisher files; it neither deploys them nor flashes hardware. Replace the illustrative example.test URL before use. Never overwrite a live directory while a client is downloading.

The living manifest requires product PakarController and the actual hardware profile: pakar-nodemcu-atmega328p with busy-wire/polling, or pakar-esp32s3-atmega328p with buffered. These are hardware identities, not API generations. Components declare url, size, lowercase sha256, version and model. The optional AVR component additionally declares signature 1E950F and bootloader_included=true. Its raw image is exactly 32768 bytes including Urboot; application-only HEX is rejected. S3 requires a hashed raw application image with chip ID9, not a merged flash dump. The ESP image must match the running flash mode and OTA capacity. Both versions must match for target all. Single-board updates are same-version repair only; explicit downgrades do not bypass identity, signature, digest, Prog, EEPROM or pair verification.

Publisher requirements: direct HTTPS URLs; a chain anchored in ISRG Root X1; correct device clock; positive exact Content-Length; no redirects, chunked transfer, compression or content encoding. Bearer authorization is sent only to the original manifest authority, never a different image host. Bounded endpoint URLs are 383 characters, manifest 4096 bytes; timeout is 20 seconds idle / 300 seconds for the complete download transaction. HTTP firmware fetching is disabled unless PAKAR_ALLOW_HTTP_FIRMWARE=1 is explicitly built (native/Web surfaces continue to prefer HTTPS).

ESP8266 TLS is memory-constrained: the fetcher uses 16 KiB records only with enough contiguous heap, otherwise 4 KiB receive records. The publisher must then support MFLN or limit TLS records to 4 KiB. Unsupported TLS/heap/clock/CA conditions fail before any board erase; there is no setInsecure fallback. Live endpoint compatibility and peak heap with many clients remain acceptance gates, not assumed from a PC HTTP test. ESP filesystem/recovery/auth storage is not replaced by this updater. Signed image query URLs can exist inside the stored pinned manifest; that manifest is not exposed by the artifact allowlist and should not be included in public exports.

## Transaction and recovery boundaries

1. Journal ownership; download the exact-length manifest and selected images; verify SHA-256, profile, reset/header/bootloader and storage capacity. ESP8266 and AVR stages use LittleFS. S3 writes the ESP stage in bounded chunks directly to the inactive OTA partition, using sequential erase; its 1.85 MB application does not fit the identified board's 1.5 MB LittleFS. SDK finalization validates the image but does not select it for boot. A resumed ready stage is revalidated against the actual inactive partition and complete image before apply.
2. Re-read image hashes and check ESP OTA capacity before any AVR erase. Require actual AVR UART/profile/version and acknowledged durable Prog arm. Play an acknowledged update-start cue, then create, flush, reopen and byte-verify flash + EEPROM + fuse metadata backups.
3. ISP checks the ATmega328P signature and readable lock state, and compares the current EEPROM to its durable snapshot before erase. Restore EEPROM first even when EESAVE is off, verify it, write and byte-verify all flash, then verify EEPROM again. No fuses are changed.
4. Require the new AVR UART version. Commit the checksum-verified ESP image; after reboot verify running ESP raw SHA-256, version and flash CRC, and the actual matching AVR version/profile.
5. Journal release intent, then release Prog exactly once and play the success cue. A lost release ACK is uncertain, not permission to repeat it. Reboot during release intent never replays release.

Downloads and ready staging can be cancelled. Interrupted applying states fail rather than automatically resuming erase/write/release. Staging and recovery files remain available for inspection; a fresh explicit job may replace staged images. Keep the known-good flash/EEPROM exports before recovery. The three backup files are verified individually; their multi-file rename is not an atomic set. No erase occurs if backup commit fails.

If Prog is already armed, a new owner-requested recovery requires target both, stage-only download and a separate pinned Apply. It retains the original recovery files rather than replacing the known-good flash backup with a half-updated AVR. The retained EEPROM must still equal the target before erase. Missing/changed snapshots or absent AVR UART refuse remote recovery; physical ISP recovery is then necessary. A newly authorized job is not an automatic retry of an uncertain earlier job.

Two physical chips cannot be updated atomically. Power loss after erase or during EEPROM restoration can require manual ISP recovery from the retained snapshot. The system does not claim guaranteed rollback or guaranteed Prog preservation under arbitrary power loss. If a mutation is uncertain, inspect actual Prog/version/backups; do not repeatedly press Apply. A missing AVR UART/profile/backup refuses erase—even if SPI alone would answer. This conservative remote path is not blind recovery of an entirely unresponsive AVR.

The explicitly built S3 quiet profile (`PAKAR_AVR_LINK_ENABLED=0`) permits ESP-only maintenance without AVR Prog, backup, UART or sound. It does not permit paired maintenance or pretend that AVR is present. Integrated profiles retain those gates. Ready S3 staging blocks competing local `/update` ownership and can be exported through the bounded `esp-staged` artifact reader. Apply selects the verified inactive partition only after a separate pinned confirmation; actual running SHA/version/integrity must pass after reboot. This is not an automatic rollback guarantee.

## Current deployment receipt

The guarded 16 MB/OPI quiet S3 candidate is 1,850,096 bytes, SHA-256 `5b5d775a77e66d853566e56b4b1af8ccadaabfd30272e52845f0b4d5c4739f12`. A corrected live Wi-Fi upload returned HTTP200 with updated/rebooting=true. After explicit hostname recovery, boot `62122282967feaf4192b9e2f9fa7646e` reported matching size and MD5 `6b2b76343d415a3a1a3584f4bb181137`, idle upload state, all80 cells white and all480 committed RGB pixels ffffff at255. This is actual command-buffer evidence, not optical proof.

Initial full streams failed, and a65536-byte range returned206 correctly in17.3 seconds. The later full HTTP200 application download completed in19.24seconds with all1850096bytes and the exact candidate SHA above. It was pinned to boot `dc2b802cd3318072494ae7282e8c9e46`; its telemetry independently reported candidate MD5/integrity and reset reason4 (panic). An uncertain earlier Internet fetch left no acknowledged/persisted job in that new boot. A bounded180second zero-transmission USB console capture during the successful download contained no new console output; this does not identify or clear the earlier panic. Do not declare the USB recovery path unnecessary.

A content-addressed public HTTPS manifest is available at [the scoped publisher](https://office.digitalogic.ir/pakarcontroller/firmware/esp32s3/5b5d775a77e66d853566e56b4b1af8ccadaabfd30272e52845f0b4d5c4739f12/manifest.json), with separately recorded manifest SHA-256 `b828e7dfd3833461aa7e83bc1281761569f973828d69d7fb4f4aa08005e231c9`. Server file and HTTPS manifest hashes were checked; board-side fetch/apply remains separate acceptance. Broker/cloud-command acceptance is also separate.

After fresh-transport recovery, Internet stage job1 was positively acknowledged in manifest phase with0 received bytes, ESP-only and apply=false. Boot `c900bd0d775661e6ab978a527590fba1` then recovered the journal as failed/interrupted, preserving its pinned manifest hash and never replaying it. No Apply was sent. This is actual interruption handling, not a successful HTTPS download. The previous pooled-client failures left boot/job unchanged; do not conflate that transport failure with this acknowledged on-device transaction. Obtain the actual crash dump before another download attempt.

The old running image (MD5 `9ec4ce6a0f2a0d4ad982468acfe46f2e`, 1,843,376 bytes) timed out during full application download. Its S3 chunk callback recomputed full-image verification for every chunk; the candidate captures the verified response bound once instead. A new panic-reset boot was observed during that interval, but causality requires the actual backtrace. A later one-shot upload returned HTTP400, `Another operation is active or multiple files were supplied`; no update was acknowledged and it was not blindly replayed. The ASUS app was explicitly disconnected to quiesce its requests. Its reconnect is still required before handoff.

ASUS joined the ESP recovery AP, preserving Ethernet management and existing Wi-Fi profiles. Hostname resolution on that AP initially failed; explicit current-address recovery at192.168.4.1 was pinned to the actual board/boot. The latest privately authorized station settings were then saved with a202 ACK, station association and AP-enabled readback. After the OTA boot, the settings persisted and ledboard.local resolved through ASUS to the station interface. AP advertisement/configuration remain present, but its HTTP recovery address stopped responding after reboot, so full AP recovery is not accepted. The user reports correcting the SS36 voltage drop; external-power stability, exhaustive discovery, optical white boot, robust download/OTA/recovery and board-side Internet TLS/apply remain open.

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
- [x] Actual S3 remote updater host/SDK-double tests cover a larger-than-LittleFS image, no boot selection while staging, explicit apply, resumed stage validation and injected transfer/write/hash/journal failures; guarded S3 SDK compile passes. These are not flash/TLS/power-loss acceptance.
- [x] Identified S3 Wi-Fi `/update` happy path: positive commit ACK, actual new boot, persisted network settings, white480-pixel command buffer, matching MD5/size and complete running-application SHA download. This does not accept intermittent panic/AP recovery or the abort/concurrency/power-loss matrix.
- [ ] Actual HTTPS publisher compatibility and complete download/cancel/hash rejection on ledboard.local.
- [ ] Full eight-client telemetry snapshot, peak heap, MQTT and WebSocket cloud reception on hardware.
- [ ] Real two-board update, EEPROM/Urboot preservation, post-reboot pair/readback/Prog release, audible cues and peripherals.
- [ ] Controlled power-loss/recovery acceptance with a known-good exported snapshot and safe electrical setup.
- [ ] Broader donor file/UI/mobile parity remains separate and open; this route feature does not certify the whole project.
