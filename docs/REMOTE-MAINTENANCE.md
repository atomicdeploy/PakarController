# Remote firmware and telemetry

Version 0.2.7 contains one integrated updater for ESP8266 or ESP32-S3 and ATmega328P. This is a release candidate: compilation and host/emulator tests are not physical board, power-loss or production deployment proof. Actual S3 HTTPS Internet staging and a separate Apply through the ASUS PakarController CLI now pass for a changed image. The new boot reports the expected digest, completed job and VALID selected/running OTA slot. Three actual software crashes were diagnosed and fixed; intermittent HTTP read failures, recovery/concurrency/physical acceptance remain open. See the current deployment receipt before attempting a handoff.

## Owner workflow

### Current integrated ISP and Urboot checkpoint

Actual board `esp32s3-907069eac800`, hostname `ledboard.local`, runs the integrated
buffered/static16MB/OPI/verified-ISP S3 image1923888bytes SHA256
`2a6d6e88d21278ee4e7bad661b9f8eff4add4cb9e02baeb425de94c00d00acb3`,
MD5 `69b69a4eb07422c9e8b937582004ca63`. Pinned HTTPS job17 staged the complete
image, then a separate exact-job Apply completed. Boot changed from
`30e3be1e99baa62a962c395187074ab7` to `2107dcd2fe50f447219b807003523b80`;
actual telemetry reports boot integrity verified, app0 selected/running VALID,
expected size/MD5 and completed postboot digest verification. Manifest SHA256
`11baa360c2190c00e271d3ff23f9e2c2b8c3c5ff61f18490828f449a390787c4`.
Local POST aborted at5124/1922976bytes, then a different-image POST aborted
at275092/1923888bytes. Both outcomes were read before another explicit operation;
neither was blindly replayed. A further updater attempt stopped in read-only
preflight. LAN empty replies/timeouts remain a real open delivery limitation.

On this final boot the actual [queued maintenance API](API.md#avr-programmer-paths)
completed two separate same-image programming transactions, not fallback retries:

- Urboot job1/request `urboot-program`: detected, write_attempted, programmed,
  verified and application_recovered true; phase complete/error empty. Full
  flash/EEPROM were backed up and compared before returning to the application;
  all253 application pages were submitted, page0 last. The384-byte boot region
  was compared and never written. The bootloader may skip unchanged pages:
  this proves the same-image transaction/readback, not a changed-image trial.
- ISP job2/request `isp-program`: write_attempted, programmed, verified and
  application_recovered true; phase complete/error empty. Full32768-byte erase/
  program/readback and1024-byte EEPROM preserve/restore/verify passed through
  the existing driver. This was chosen only after Urboot terminal success.

Both used complete AVR image SHA256
`4f3f00454ec913551c88da4746ea11955087835c9619e6331406c67f15034fb1`.
Final actual AVR is0.2.7, Prog false, UART errors0/lost events0, buzzer frequency0.
Persisted settings fields match the pre-program observation. Signature1E950F,
fuse snapshot F7/D7/FD and lockFF were retained; no fuse-write command exists in
these jobs. Reset/Prog journal records legitimately change EEPROM after the
application runs; do not claim identical post-boot whole-EEPROM hashes.

Initial ISP and Urboot independent full flash exports both matched that image;
their separate1024-byte EEPROM captures and original recovery files are preserved.
On ASUS, `verification/avr-dual-path/urboot-program-result.json` and
`isp-program-result.json` retain correlated terminal receipts. During both jobs
LAN status reads failed; read-only LAN/MQTT observations recovered their outcomes
without another program request. Full checksummed authenticated MQTT telemetry
also confirmed the recovered AVR; an uncorrelated MQTT read event is not claimed
as its own command ACK.

The ASUS native PID21828/session1 runs EXE28463616bytes SHA256
`890158bdac101ec206a1d4aaa18f554789b3ec68390e581f5706a0736cdfc99f`.
No old process existed at the twice-checked replacement boundary; old executable
and settings were preserved and the existing Limited/Interactive production task
launched it. Saved USB COM22 remains selected/offline, not silently switched to
Wi-Fi; these hardware tests used hostname LAN, not fabricated native USB proof.
The native/Web/Flutter status consumers understand both programmer progress
methods; dedicated native/mobile method-selection controls remain pending.
Android preview APK56362977bytes SHA256
`8a9e6e0ecf72a0d5418ba816de303352144712d3d673fc0d442329f8459e030d`
built/analyzed, not installed on the unavailable phone. Versions remain0.2.7.

Still open: changed-image UART programming, dead-UART/power-loss recovery,
all-client ingress/concurrency soak, electrical clock/VCC margin, peripheral/
optical/audio acceptance, reliable LAN transfers, advanced UI selectors and
broader donor parity. A historical aborted local OTA can still mask active
remote progress in existing client precedence; track that separately rather
than claiming all updater display states are complete. No physical output or
full production acceptance follows from these programming checks.

### Current application firmware-delivery checkpoint

Native Device settings → Firmware & storage and Android device context menu →
Firmware now offer local images, SHA-256-verified HTTP(S) binary files, and an
opt-in WebSocket/Socket.IO firmware subscription. See [the inbox contract](API.md#application-firmware-inbox-file-delivery-and-observed-progress).
Download/upload counters and actual board OTA/remote-job observations are
separate stages. A successful transfer ACK is not verified completion. Network
delivery still requires the explicitly selected network route; a USB owner
never silently uses Wi-Fi. USB receives external board update progress.

The actual ASUS native owner PID22736/session1 runs EXE28,425,728bytes SHA256
`bd9318d4c01128abd83908f104342c5069be5352b7d5534297744f3cf34892fe`.
Its old owner answered draft/pending-state IPC, then gracefully quit with exit0.
The old executable and settings were hash-preserved; replacement was atomic.
The existing Limited/Interactive production task launched the new owner, which
reports USB COM22 selected/online with zero pending commands after the update.

Internet stage job16 was admitted over actual CA-validated MQTT TLS, with
manifest SHA256
`79d0836bb7bd329566d639fe5685afcaf7b6a223fd56f15211f0050c807de636`.
The board's download observations reached ASUS over USB:727868/1908480bytes
(38.1%), then1543868/1908480bytes(80.9%). These are native owner IPC observations,
not screenshot fixtures or source-side counters. The ESP independently verified
the staged image and reported the exact job ready. MQTT Apply returned no
correlated acknowledgment; that uncertain command was not replayed. Fresh full
telemetry and the native hostname-based read both still showed the same boot,
exact ready job/hash and no apply in progress. A new explicit pinned Apply via
the ASUS native CLI then returned prepare/busy, not a completion claim.

Actual new boot `a42b1105aba6938b525c416e0d63956c` replaced
`b8a21cd35964daca70f09a9c5a4d3fc2`. Job16 reports complete after running-image
SHA verification, image1908480bytes SHA256
`e35d11c57de89ff99a86a66fd5ecdf0af908e1f755753ce0db1c55fbe3aee083`,
running MD5`d8cd149da1c85eed34e2c405fa99e609`, boot integrity verified, and
selected/running app0 VALID. MQTT reconnected. The same native USB owner now
reports Board update#16 complete/verified with that new boot. GPIO-static,
BGR, GPIO4,16MBflash/OPI profile is preserved; live driver reports ready/active
with zero failures. AVR remains online0.2.7 and Prog clear; no AVR programming,
fuse/EEPROM change, deliberate reset or audio test was performed.

Final Android preview APK56,362,977bytes SHA256
`484dbcbdf3c54758376b0cb0bfb4cef42d62677121c9b36c676998cfdff1050c`
builds after analyzer, six actual delivery/progress/socket/HTTP fixtures and six
firmware-page cases pass. Those include phone/tablet layout, cancellation,
uncertain outcomes and same-board reboot rebinding without replay. A real
Flutter renderer capture of the simulated firmware page was inspected; it is
not phone/runtime or physical acceptance. No authorized Android ADB device is
currently attached to ASUS, so installation remains pending. All component
source versions remain0.2.7 (Android+7).

Native has nine focused firmware tests passing, including real loopback raw
WebSocket/Socket.IO sessions, duplicate-offer suppression and an actual HTTP
download/hash-before-upload fixture. No external WSS service was guessed or
connected: its endpoint and authentication remain operator configuration and
live interoperability acceptance. Android subscription is foreground-only;
listener reconnect is explicit. USB-only firmware forwarding, generic donor
RPC/OS update notifications, power-loss/concurrency/soak, physical LED/display/
audio and whole-donor parity remain open. The missing MQTT Apply ACK remains a
recorded delivery-reliability failure even though the later explicit native
Apply and postboot verification succeeded.

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

The quiet 16 MB quad-flash / 8 MB OPI-PSRAM S3 image is **1,853,328 bytes**,
SHA-256 `29fca3373f32e875ae7181e775ef9b4373b85c898f3ae5375a1d0af6d0cea15c`,
MD5 `57dbbb3f6cfbe03a50289adb50539567`, ELF SHA-256
`ccce33b9afce12c575649d567b6f2925d93fea25dbdb9aec22ce451d19f0ac8b`.
Version remains 0.2.7 across firmware/native/mobile. This is the LED-first quiet
profile: AVR UART/reset/ISP/EEPROM/fuses and sound were not actuated.

Internet job **7** fetched the pinned manifest and this complete image over
HTTPS. A fresh exact-job/boot/hash/length preflight through the installed ASUS
PakarController CLI observed Ready, then its separate Apply returned exit 0 and
a positive prepare-phase acknowledgment. This final image was **not USB-flashed**.
The ESP changed from boot `a7372ca8158212349c4e01295a351446` to
`13acc69e049a90569d631f1bc7950281`; its journal reports job 7 complete and the
expected image SHA. New actual telemetry reports running_offset=65536,
boot_offset=65536 and running_state_code=2 (VALID): app0 is both running and
selected. These additive fields are absent from the previous app1 image.

Trusted manifest SHA-256:
`71cf1a19e786c27eaa0cc84f899cf40d0be070d5b20744ddbec0a551547e0035`.
[Immutable publisher](https://office.digitalogic.ir/pakarcontroller/firmware/esp32s3/29fca3373f32e875ae7181e775ef9b4373b85c898f3ae5375a1d0af6d0cea15c/manifest.json).
Current commissioning is ESP-only (`targets=1`, CLI target `esp`), not all.

Three telemetry observations on that same final boot reached uptime 982638 ms,
reset reason 3 (software restart), station connected and AP enabled. Automatic
SNTP was observed at uptime 131463 ms, not inferred from configured NTP or a
manually supplied clock. All 80 logical cells and the complete 480-pixel command
buffer read back white at brightness 255, mode cell/effect none. This is not
optical proof, an external-power power-cycle test or long/concurrent-client soak.
The owner-authorized replacement station network is persisted; no Wi-Fi password
is included in repository evidence.

Private ASUS receipts: `internet-slot-telemetry-native-apply.json` contains the
actual Ready preflight and positive Apply response;
`internet-slot-telemetry-handoff.json` contains the final slot/clock/network and
white-buffer observations. The original fetch observer lost one read late in
download; later Ready and Complete observations, not replaying Fetch, establish
the outcome. Earlier jobs 5 and 6 were explicitly cancelled before job 7.

The subsequent boot-fenced running-artifact readback passed: **29 exact HTTP206
ranges, all 1,853,328 bytes, SHA-256 matching the final image above**, same board
and boot before/after. Each response's Content-Range/Length and MD5 ETag were
checked; bytes were aggregated into SHA in RAM, without a second image on disk.
Private receipt: `internet-slot-telemetry-range-readback.json`. Four read failures
were retained (three empty replies and one partial-range timeout); only complete
verified ranges entered the digest. No mutation was retried.

An initial single-stream read truncated after 269428 bytes, and an initial range
attempt failed after 262144 bytes. The boot stayed unchanged; these failures are
not silently counted as successful readback or diagnosed as new panics. Small
GETs also sometimes return empty replies. Reliable single-stream/client resume
remains open despite the successful complete bounded readback. Preserve USB
recovery availability until HTTP/discovery/AP recovery, external-power/optical
and concurrency tests pass. MQTT/cloud commands without the ASUS relay and AVR
integration are still open.

The existing ASUS interactive app (PID9796, executable SHA e63c1bb0...) was
reconnected using guarded typed IPC, not replaced or force-stopped. Actual
readback reports exclusive Wi-Fi online, the expected hardware and final boot,
white cell mode, no AVR and no pending maintenance; the connection uses
`http://ledboard.local`. Original installed settings and rollback are preserved.
Private receipts: `internet-final-desktop-connect.json` and the later guarded
`internet-final-desktop-inspect.json`, both reporting the exact peer online;
enabled alone was not counted as online. Native executable identity is unchanged
by this firmware fix. Small copies of the receipts are retained on the build PC
as well as ASUS; raw crash cores remain private because RAM can contain secrets.

| Private receipt | SHA-256 |
| --- | --- |
| internet-slot-telemetry-native-apply.json | f6262620a0055ade7c23331428ff74d33e62027d1be535770b852f8ab6a4516e |
| internet-slot-telemetry-handoff.json | 7af4eb8cbe525bb13d1f5f56c0bbdb08610fb567111f7d2bb4c2e9fd7671ac06 |
| internet-slot-telemetry-range-readback.json | 712e54c4e12c93e49705cc247754be1d69d1fd7a064fb43f05945020f44612ed |
| internet-final-desktop-inspect.json | 4c9f59d89bb83cbd627bee11e6081b349907e3a77323149f1b611db178a11aa5 |

### Historical Internet staging and failed Apply (superseded image)

The previous quiet16MB/OPI S3 image was1852128bytes, SHA256
`8acf99396902bec038d5da5503c9cd8338f778e8ff6c52ef27afa61d9ddf5a7e`,
MD5 `b24ce509a9bcfa3894efd023e435de12`. Selected-app1 USB bootstrap and
esptool raw flash hash verification passed; this bootstrap is not Wi-Fi upload
proof. Boot `9df32324800985664510a69762524f75` synchronized automatically via
SNTP at4208ms, with station connected/AP enabled/AVR disabled/white default.
The router periodically overwrites DNS, so the initial NTP and new firmware
connection paths now run immediate validated DNS preparation, in addition to
periodic background repair.

Actual Internet job5 downloaded the419byte pinned HTTPS manifest and all1852128
image bytes into the inactive OTA slot and reached Ready without a reset.
Full staged-artifact export then returned the exact expected SHA above in
72.708seconds, with the same hardware/boot before and after. An earlier export
returned an empty reply and is retained as failed evidence, not hidden by the
successful read-only check. Private receipts: internet-boundary-fetch.json,
internet-boundary-stage-hash.json (failed), internet-boundary-stage-readback.json
(passed). Raw bytes were hashed in memory rather than duplicated onto SSD.

Trusted manifest SHA256
`771059cb334ad09a73805fa2f003543b55ab64a9775033ffd2006d6405eca4b5`;
[historical immutable publisher](https://office.digitalogic.ir/pakarcontroller/firmware/esp32s3/8acf99396902bec038d5da5503c9cd8338f778e8ff6c52ef27afa61d9ddf5a7e/manifest.json).
That commissioning was **ESP-only** (`targets=1`, CLI target `esp`), not all.

HTTP Apply lost its response. Read-only telemetry subsequently established the
same exact job5/boot still Ready, not applying/complete. The installed ASUS
PakarController CLI's fresh state-bound attempt also reported an unknown remote
request outcome; subsequent same-boot readback again stayed Ready. No Apply
completion/reboot was claimed and no command was automatically replayed. That
stage was later explicitly cancelled after the dispatch-race fix bootstrap.
Auth Telnet inspection also failed its response deadline before any Apply;
neither its mere TCP connection nor configured discovery flags certify the shell.
Ready then advertised cancellable=false despite the explicit cancel API
supporting Ready; this mismatch was subsequently corrected and cancellation
was positively acknowledged and read back before the final job.

### Historical commissioning attempts (superseded images)

The guarded 16 MB/OPI quiet S3 candidate is 1,850,096 bytes, SHA-256 `5b5d775a77e66d853566e56b4b1af8ccadaabfd30272e52845f0b4d5c4739f12`. A corrected live Wi-Fi upload returned HTTP200 with updated/rebooting=true. After explicit hostname recovery, boot `62122282967feaf4192b9e2f9fa7646e` reported matching size and MD5 `6b2b76343d415a3a1a3584f4bb181137`, idle upload state, all80 cells white and all480 committed RGB pixels ffffff at255. This is actual command-buffer evidence, not optical proof.

Initial full streams failed, and a65536-byte range returned206 correctly in17.3 seconds. The later full HTTP200 application download completed in19.24seconds with all1850096bytes and the exact candidate SHA above. It was pinned to boot `dc2b802cd3318072494ae7282e8c9e46`; its telemetry independently reported candidate MD5/integrity and reset reason4 (panic). An uncertain earlier Internet fetch left no acknowledged/persisted job in that new boot. A bounded180second zero-transmission USB console capture during the successful download contained no new console output; this does not identify or clear the earlier panic. Do not declare the USB recovery path unnecessary.

A content-addressed public HTTPS manifest is available at [the scoped publisher](https://office.digitalogic.ir/pakarcontroller/firmware/esp32s3/5b5d775a77e66d853566e56b4b1af8ccadaabfd30272e52845f0b4d5c4739f12/manifest.json), with separately recorded manifest SHA-256 `b828e7dfd3833461aa7e83bc1281761569f973828d69d7fb4f4aa08005e231c9`. Server file and HTTPS manifest hashes were checked; board-side fetch/apply remains separate acceptance. Broker/cloud-command acceptance is also separate.

After fresh-transport recovery, Internet stage job1 was positively acknowledged in manifest phase with0 received bytes, ESP-only and apply=false. Boot `c900bd0d775661e6ab978a527590fba1` then recovered the journal as failed/interrupted, preserving its pinned manifest hash and never replaying it. No Apply was sent. This is actual interruption handling, not a successful HTTPS download. The previous pooled-client failures left boot/job unchanged; do not conflate that transport failure with this acknowledged on-device transaction. Obtain the actual crash dump before another download attempt.

The old running image (MD5 `9ec4ce6a0f2a0d4ad982468acfe46f2e`, 1,843,376 bytes) timed out during full application download. Its S3 chunk callback recomputed full-image verification for every chunk; the candidate captures the verified response bound once instead. A new panic-reset boot was observed during that interval, but causality requires the actual backtrace. A later one-shot upload returned HTTP400, `Another operation is active or multiple files were supplied`; no update was acknowledged and it was not blindly replayed. The ASUS app was explicitly disconnected to quiesce its requests. Its reconnect is still required before handoff.

ASUS joined the ESP recovery AP, preserving Ethernet management and existing Wi-Fi profiles. Hostname resolution on that AP initially failed; explicit current-address recovery at192.168.4.1 was pinned to the actual board/boot. The latest privately authorized station settings were then saved with a202 ACK, station association and AP-enabled readback. After the OTA boot, the settings persisted and ledboard.local resolved through ASUS to the station interface. AP advertisement/configuration remain present, but its HTTP recovery address stopped responding after reboot, so full AP recovery is not accepted. The user reports correcting the SS36 voltage drop; external-power stability, exhaustive discovery, optical white boot, robust download/OTA/recovery and board-side Internet TLS/apply remain open.

## Full operational telemetry

### S3 DNS crash recovery checkpoint

The actual preserved crash record has a valid CRC and matching fault-ELF hash
prefix. Its backtrace reaches `NetworkManager::hostByName` → `dns_clear_cache`
→ SNTP callback → UDP allocation without the required lwIP core lock. The
project-only `DnsCoreLock.cpp` linker wrapper acquires that lock only when the
caller does not already hold it; DNS/TLS/SNI/CA checks remain enabled. SDK files
are unchanged. Actual linked disassembly and the production-adapter host fixture
verify the wrapper path and existing-lock cases.

Candidate1850176bytes/SHA256
`87a62afd005c104ab713a013ff51069325fd5dd97d7c3ed1587cd1ed02700d58`
was installed using a **USB bootstrap**, after the LAN attempt aborted without
commit. Only the CRC-selected app1 application's bytes were written; NVS,
LittleFS, partition table, OTA selection and crash record were preserved.
Boot `fa9d4563d68224b0f87919a17c0f618c` reports the exact size/MD5,
station connected, AP enabled, and AVR disabled. A subsequent Internet job2
failed cleanly without a boot change; this is not a successful Internet apply.

Live DHCP supplied IPv6 DNS `2a02:4544:e0:200::1012` while board hostname
resolution failed. ASUS successfully resolved the same publisher through the
network gateway192.168.1.1. The S3 DHCP DNS recovery policy preserves working
resolvers/static configuration, promotes a usable IPv4 backup or uses that
network's gateway only when no primary resolver is usable. It never installs a
public resolver, changes AP settings or writes a settings record. It is
build-disableable with `PAKAR_S3_DNS_GATEWAY_FALLBACK=0`. Live DNS/NTP and the
final candidate's delivery remain pending until explicitly recorded below.

DNS recovery was actually observed on candidate
`82ba2e463536b6a86935f629300ddbb78a80f88d7ef89efba1c10a2cfc311554`:
boot `b8a67e1bf0d56ee8f5e5b788008dda4e`, DHCP resolver192.168.1.1,
publisher hostname resolution, and automatic SNTP callback at41154ms with
source=ntp/ntp_observed=true. Its Internet job3 parsed the manifest, then
crashed before image staging; it was recovered as failed and was never applied.
The new CRC-valid crash record/backtrace identifies a second defect:
HTTPClient::setTimeout inspected a freed client left behind by end() on an
already-disconnected connection. The stricter HTTP fixture now preserves that
pointer and asserts liveness instead of unconditionally nulling it. The fix
keeps a stopped socket object alive until rebinding and destroys HTTPClient
before the socket. TLS resources are stopped; idle network connections are not
retained. Manifest→image, cancel→new job and destruction fixtures pass on both
updater profiles. The subsequent candidate is1851120bytes, SHA256
`94d227ed2e5a070e22a7a52e9e2ab273f4d7d332fb6f3b050192f555bfecef41`.
Its guarded SDK build and app1-only USB write/flash hash verification pass;
Internet stage/apply and raw export require their own live evidence.

ASUS now uses Ethernet only for this board path: its extra ledboard Wi-Fi client
was disconnected, preserving the profile and ESP AP. The manually set ASUS UTC
clock is explicitly **not SNTP proof**; `/datetime/get` source reporting after
manual set also remains incomplete. Do not use a PC's HTTPS result, configured
NTP flag, or accepted fetch request as board download/apply evidence.

## Telemetry contract

ESP: actual chip/flash/core/SDK/version/integrity, uptime/reset, heap/fragmentation, OTA space, filesystem use, UART/USB counters and pinout. S3 OTA additionally reports actual selected/running partition offsets and nullable SDK running-image state, not an inferred boot slot. Network: actual STA/AP mode, connectivity, SSIDs, IP/MAC/netmask/gateway/DNS, RSSI/channel/DHCP/AP client count and discovery state. Clock: valid/source/configured NTP/observed SNTP callback, UTC and last-setting uptime; no fabricated RTC. Board state includes cells, effects, inputs/outputs, seven-segment mirror and Prog.

AVR firmware 0.2.7 adds a read-only 46-byte telemetry response (0x58 request, 0xB1 response), preserving the existing PCController tunneled dialect. It reports actual uptime, free SRAM snapshot, raw ADC6, buzzer, board settings/persistence, UART errors and lost events. Poll every five seconds; after twelve seconds values are stale/null, not invented. Older AVR versions do not advertise this extension.

Native Windows clients report application/version, session UUID, hostname, OS/architecture, process ID/uptime, CPU parallelism, physical/available RAM and primary-screen/monitor information. Web clients report only available browser/platform/CPU-hint/screen data. The ESP observes the source IP; every other client field is explicitly self-reported, not attested. Capacity is eight clients, recent TTL two minutes. A different peer cannot overwrite a live client ID. No passwords, cookies, bearer tokens, personal documents, file inventories, screen captures or arbitrary PC access are collected. Missing measurements are null. This is complete operational metadata for these transports, not all private information on a computer.

Configured cloud connections retain the existing status feed and additionally deliver full telemetry. WebSocket sends living telemetry JSON. MQTT full snapshots use non-retained QoS0 packets at BASE/HOSTNAME/telemetry/part: type, boot-unique snapshot_id, index/count/total_bytes, sha256, encoding=base64 and data. Raw chunk size is 768 bytes; snapshot limit 16384 bytes. tools/telemetry-reassembly.mjs rejects mixed metadata, invalid encodings, missing parts, checksum failures, conflicting duplicates and expired snapshots. Never present partial packets as current board state. A failed publication waits for the next snapshot; no guaranteed-delivery claim is made. The retained /status feed is unchanged.

## Automatic network updater and measured progress

Use the guarded entry point from this checkout:

```powershell
# Plan only: validates the local raw S3 application, no device contact or writes.
./tools/update-firmware.ps1
# One selected update, then actual new-boot/running-image verification.
./tools/update-firmware.ps1 -Origin http://ledboard.local -Run
# Explicit pinned Internet stage and separate apply; same expected local image.
./tools/update-firmware.ps1 -Mode remote -Manifest HTTPS_MANIFEST_URL -ManifestSha MANIFEST_SHA256 -Run
# After inspecting an already verified ready job; never fetch again.
./tools/update-firmware.ps1 -Mode remote -Manifest HTTPS_MANIFEST_URL -ManifestSha MANIFEST_SHA256 -ResumeJobId READY_JOB_ID -Run
```

`auto` is the default. It prefers the advertised local Async HTTP upload path;
if unavailable before dispatch, a configured pinned manifest may select the
advertised Internet fetch/apply path. It does not silently change the app's
exclusive USB/Wi-Fi connection setting. USB bootstrap/recovery remains a separate
inventoried-device operation: no guessed COM port, reset, erase-all or automatic
second firmware write after an uncertain network result. Direct LAN and Internet
transfer times and total verification time are reported separately; there is no
current-device speed benchmark while ASUS/device access is unavailable.

An explicit resume requires the same ready job, current boot, manifest SHA,
ESP-only target, image size and SHA. These are rechecked immediately before one
apply. It neither re-fetches nor retries a mutation; interrupted/unknown applies
are not resumable through this option. Only transport-failed GET/HEAD may retry
once with the identical pinned address and headers. HTTP rejection, auth/identity
failure and every upload/POST remain non-replaying. Resume timing is explicitly
unknown for the earlier download rather than presented as a new speed result.

No authentication is enabled by this workflow. It requires explicit
`/api/auth` `enabled:false` before and after, pins board/boot/TCP identity before
every mutation, and verifies WebSocket status both before and after. A local POST
is not completion: a new boot, matching image size/MD5, verified image integrity,
selected/running VALID OTA slot and full range-read SHA256 must all agree. The
existing access configuration remains default-off; an explicitly protected saved
record must be disabled through the existing owner settings, not bypassed by the
updater. Corrupt storage continues to fail closed.

Local uploads may supply `X-Pakar-Firmware-Size` (raw file bytes, never multipart
Content-Length) and `X-Pakar-Firmware-MD5`. The tool also places the MD5 form field
before the firmware file. A declared short/oversized stream is aborted before
boot commit. Uploads without an exact denominator remain indeterminate.
`/api/status.update`, WebSocket status and `/update/info.update` expose the same
coherent measured byte snapshot. `progress_percent` describes that transfer
stage; `completion_verified:false` deliberately leaves reboot verification to
the client. `/update/info` uses boot-captured image metadata instead of hashing
flash during every poll. Status/ping reads remain available during local OTA;
mutations are rejected.

The main loop, not an async callback, coalesces finite scheduled TM1637 `P000`
through `P100` frames (maximum four admissions/second, unchanged-frame refresh
once/second,1200ms lease), or `UPdt` when indeterminate. This requires a fresh
TM1637 ACK and no Prog/AVR upload owner; UART enqueue is not physical display
proof. The ESP-side optional LCD uses the same measured transfer. An AVR held in
ISP RESET cannot run its own display, so Prog safety takes precedence and the
ESP LCD/network clients cover that stage. No extra AVR firmware or audible cue
was introduced by this progress change.

Focused host tests cover the real progress encoder and unchanged AVR consumer,
atomic publication, rollover, lost-reply/no-replay cases, auth-off guards,
same-board reboot/full-SHA verification and actual loopback multipart/fragmented
WebSocket traffic. These are not current physical OTA, timing, power-loss,
concurrent-client soak or seven-segment presentation acceptance. Keep USB until
the current candidate has actually been installed and verified on the identified
board, with the external power and Wi-Fi/AP recovery path checked.

## Acceptance checklist

### LED wire capability delivery checkpoint

The existing manifest now includes optional `capabilities.led_output` with
explicit `backend`, `rmt` and `dma`, derived from the actual compiled output
worker. Backend selection is independent of renderer mode and live readiness.
Missing/invalid metadata remains unknown in Rust/Flutter/Web, not inferred false.

On board `esp32s3-907069eac800`, separate MQTT-requested HTTPS stage and pinned
Apply completed job15. Image1,905,952bytes SHA256
`3db7e927ff95e839605db74b0d0e5944d57e70423002f7fa4be4462eeb9d309c`,
manifest SHA256
`83b4a5fc92f549facc0c4cdf3c1f132ea68f5fa6ea9fbd93738534e42687f584`.
Boot changed from `28d93c4bc35b065244d5426f87f17c73` to
`b8a21cd35964daca70f09a9c5a4d3fc2`. Actual telemetry reports completed
postboot running-image verification, MD5 `d8af5b183242d86d6ff4f7d21e1c7e5c`,
integrity verified, app1 selected/running VALID. Full telemetry9,851bytes was
reassembled and SHA256-checked. The first read probe missed its correlated
reply but retained complete telemetry; no uncertain mutation was replayed.

Actual MQTT telemetry, HTTP `/api/status`, WebSocket `/ws` and the native
USB COM22 owner all report
`{"backend":"gpio-static","rmt":false,"dma":false}`. The driver separately
reports ready/active with zero failed frames. The deployed static/16MB/OPI
profile, BGR order and GPIO4 are unchanged; no physical output claim or RMT-DMA
hardware certification follows from these metadata observations. AVR remained
online; no AVR programming, fuse/EEPROM write or explicit reset was performed.

ASUS's tunnel dropped during package transfer. On reconnection, the complete
native candidate's hash/length was checked; no Pakar owner was running. The
installed hash was checked, settings and prior binary preserved, replacement
was atomic, and the existing Limited/Interactive production task launched it.
Actual PID23128/session1 owner IPC reports USB selected/online and the new
peer-owned LED metadata. Native EXE27,875,840bytes SHA256
`7bfb08d595e0c0cc3c7a79cfb9c936d482c32018130d3e391f13ff752598a8a3`.

Flutter preview APK56,215,521bytes SHA256
`02dd66fecac611322986172452ec1634fd831d790517e799faf18423c0a364b9`
builds with analyzer/six focused peer-context cases passing. Its ASUS transfer
was incomplete and authorized wireless ADB is now offline. No installation or
mobile runtime/human acceptance is claimed. All source versions remain0.2.7.

### MQTT Internet delivery checkpoint preceding LED metadata

The identified `ledboard.local` (board `esp32s3-907069eac800`) now connects
directly to `mqtt.digitalogic.ir:8883` with CA-validated TLS and separate scoped
device/control ACLs. No anonymous broker access, passwords or bearer tokens were
published. Operator credentials remain in a mode0600 broker file. Existing
tenants and rollback files were preserved. `tools/provision-mqtt-device.py` is
an explicit administrator operation, not an app startup action.

`tools/mqtt-live-probe.py` publishes one non-retained command and requires its
matching request ID and actual accepted reply. QoS0 socket write alone is not
acceptance. It reassembles full telemetry with the existing768-byte framing,
16KiB bound and SHA256 validation. Real ESP/AVR/network/client telemetry and a
correlated read command were received, not just broker connectivity.

MQTT-requested HTTPS stage and separate pinned Apply completed job14, image
1,904,912bytes/SHA256
`2b6bf27ba5b57e2419dbe8086a4757c3ef84ef14a5db8815c0fc26a48462aad6`,
manifest SHA256
`b7bde50c55aff03f6ec5f93dd361b00167a918d118412a8eedc8961387465380`.
Boot changed from `360d2ef9857b3220b0c037c3ea6671e0` to
`28d93c4bc35b065244d5426f87f17c73`. The ESP reports exact running size,
MD5 `15f77eebf281274fe7ed740afb99ddc3`, integrity verified, app0 selected/running
VALID, completed postboot SHA verification and MQTT reconnection. AVR remained
online; no AVR flash, EEPROM/fuse write or deliberate reset was performed.
An independent complete running-application export also matches that SHA256.
The first HTTP export timed out at1,080,768bytes/35seconds; a boot-fenced Range
resume completed the remaining bytes without a new firmware write. Reliable
single-stream throughput remains open, not hidden by this successful checksum.
Historical failed job12 was inspected before the new explicit job; no uncertain
write was replayed. This is actual Internet delivery, not a USB substitute.

ASUS received real mDNS A and DNS-SD PTR/SRV/TXT records, SSDP UUID/location
responses and MNDP hardware MAC/hostname replies on its Ethernet LAN interface.
The diagnostic now defaults to an ephemeral reply port to avoid Windows5353
socket contention; shared5353 probing remains configurable. A separate Wi-Fi
interface probe received no packets; universal multi-interface/router/firewall
acceptance is not claimed. Actual stored AVR backup exports returned32768-byte
flash (SHA256 `4f3f00454ec913551c88da4746ea11955087835c9619e6331406c67f15034fb1`),
1024-byte EEPROM (`774819bd02bcfbb563ba20f9f905f711e27dc5e3884708ca8c0ee491a5aee89c`)
and fuse/signature metadata. These downloads are not a fresh ISP capture.

The updated ASUS native executable SHA256
`163b9acd203de4c982e822739f5c05ee6573e3472fcea84aeb9270259cadb7da`
is running in interactive session1, PID21244, with actual USB online.
User explicitly discarded drafts after old-owner IPC failed. Only that verified
old process was stopped; replacement waited for its actual exit. Settings and
prior executable were hash-preserved. Sharing remains opt-in/off, not silently
enabled. Network repeater opaque transfers have16 passing socket/bridge tests;
USB-only maintenance forwarding remains unsupported rather than opening Wi-Fi.

Remaining gates include intermittent empty/timed-out HTTP responses and slow
raw export, explicit AVR reset and
real programming/EEPROM preservation, all-client concurrency/soak, physical
display/audio/LED acceptance, and outbound WebSocket/eight-client telemetry
hardware reception. Actual MQTT and LAN evidence does not close these gates.

- [x] Both firmware profiles compile against the locked toolchain; native protocol/policy/actual ISP driver emulation tests.
- [x] Built desktop CLI loopback tests: authenticated staging, pinned identity, wrong ACK and no replay; 77 Rust tests, including accepted-job notifications not being marked completed.
- [x] Production Web remote controls parse and pin exact identity; publisher and MQTT reassembly tests.
- [x] Actual RemoteFirmware module host tests, with real host SHA-256 and doubled network/FS/updater: interrupted journals, lost release ACK, staging/postboot tamper, complete pair, cancellation and failure cases. Not TLS, flash hardware or physical power-loss proof.
- [x] All 50 reviewed PU850 route registrations have concrete source/target mapping or exclusions.
- [x] Actual S3 remote updater host/SDK-double tests cover a larger-than-LittleFS image, no boot selection while staging, explicit apply, resumed stage validation and injected transfer/write/hash/journal failures; guarded S3 SDK compile passes. These are not flash/TLS/power-loss acceptance.
- [x] Identified S3 Wi-Fi `/update` happy path: positive commit ACK, actual new boot, persisted network settings, white480-pixel command buffer, matching MD5/size and complete running-application SHA download. This does not accept intermittent panic/AP recovery or the abort/concurrency/power-loss matrix.
- [x] Actual ESP-only HTTPS publisher compatibility, exact-length Internet stage and complete staged-image raw SHA readback on the identified S3. Manifest/image remain pinned; no insecure TLS. This is not Apply, cancel, wrong-hash or external-power acceptance.
- [x] Changed-image Internet stage, separate exact-job native CLI Apply acknowledgment, new same-board boot and completed postboot digest verification; actual app0 selected/running VALID. No final USB install substituted for this path.
- [x] Complete boot-fenced raw running-image SHA-256 after the final Internet update:29 exact206 ranges/all1853328bytes, with four failed read attempts recorded. This does not establish reliable single streams or automatic client resume.
- [ ] Reliable single-stream/resumable client export and intermittent empty/partial HTTP response diagnosis; retain the failures separately from successful digest/slot checks.
- [x] Ready cancellation capability now agrees with the API; obsolete stages were explicitly cancelled and read back, not automatically replayed.
- [ ] Broader hardware cancel/wrong-hash rejection, AP HTTP recovery and reliable all-client maintenance progress/outcome reporting.
- [ ] Full eight-client telemetry snapshot, peak heap, MQTT and WebSocket cloud reception on hardware.
- [ ] Real two-board update, EEPROM/Urboot preservation, post-reboot pair/readback/Prog release, audible cues and peripherals.
- [ ] Controlled power-loss/recovery acceptance with a known-good exported snapshot and safe electrical setup.
- [ ] Broader donor file/UI/mobile parity remains separate and open; this route feature does not certify the whole project.

## Graceful upgrade presentation

`GET /api/update/lifecycle` and `status.upgrade` describe the current loop-owned
operation. Local WebSocket/cloud `update-progress` packets carry `update.phase`
or coalesced `update.progress`, board/boot/source identity, operation/revision,
actual stage bytes, explicit `progress_known` and nullable `progress_percent`.
Clients merge newer same-peer revisions without renewing hardware-state authority.
Receiving100% is not verified installation. Prepare, verification without measured
counts, reboot and application recovery are indeterminate, not fabricated100%.

The matrix keeps its semantic scene/mapping/output preference. An enabled output
shows measured progress, a700ms per-pixel shutdown fade and a final complete black
frame; normal reboot waits for its wire receipt, bounded at2s if output fails.
Startup fades back to the scene, then a finite verified/error presentation clears.
Disabled output stays disabled. AVR programming first checkpoints the scene and
confirms black; the output worker is held and cloud writes deferred throughout
raw Urboot ownership. No visual animation is allowed to starve its UART deadlines.

The existing AVR Timer1 update-start cue is descending G5/90ms,45ms pause,
E5/90ms,45ms pause,C5/180ms (450ms total), sent once before reboot or Prog/reset.
Missing buzzer/capability/available UART is not an erase-safety acknowledgement.
Local success requires the durable `/local-ota.json` upload receipt to match the
new boot's actual running size, computed MD5 and integrity. Notice consumption is
persisted before one cue enqueue; a crash may omit sound, never replay it.
`completion_sound=enqueued` is serial delivery intent, not acoustic proof.

The identified S3 now runs the guarded16MB/OPI/buffered/static candidate,
1958288bytes, SHA256`490876452384ef9bacf411df767e1b5ab4cf689f7476e2c4b8e7e210c8b88696`.
Two local OTA passes used hostname-first ASUS access. First transfer18.205s,
complete SHA/VALID-slot verification49.405s. Second transfer19.414s, with59 ordered
live WS events and no repeated/reordered revisions. New boot
`d1170ed71821eda19075c19d70bad8c0` confirms exact durable receipt, integrity,
consumed completion notice, saved scene, AVR online, persisted silent=false and
LED driver failure count0. The second updater's later export timed out; that is
not erased by the successful boot receipt and must not trigger another flash.
Subsequent read-only recovery verified the entire1958288byte running image in30
exact ranges, zero read retries, against the same SHA and current boot. There
was no third flash. The original updater deadline remains recorded separately.
Physical optical/audio, interrupted-power and actual AVR-flash acceptance remain open.
