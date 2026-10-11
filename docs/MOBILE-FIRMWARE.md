# Mobile firmware and full AVR backups

Owning issue #7, broader UI/parity #4/#3, production master #2. The Firmware
settings page is no longer a Web-address-copy placeholder. It uses the actual
integrated device contracts; no separate ISP firmware or invented filesystem
browser was introduced. This is app/fixture acceptance, not physical flashing.

## Actual source mapping

| Fully read source | Real target / retained mechanism |
|---|---|
| Pealayer web_ui/src/components/ServerFilePicker.tsx | mobile/lib/documents.dart, Android MainActivity.kt and firmware_page.dart: single selection, cancel, bounded/disabled state, stale ownership, review before acceptance. Android's document provider owns navigation/search instead of an ESP endpoint that does not exist. |
| Pealayer web_ui/src/preferenceFiles.ts | Actual case-insensitive dot-extension suffix rule in documents.dart, executed by the firmware picker. File extension is not authenticity. Provider content URIs are not converted to filesystem parent paths. |
| Canonical Pakar desktop/src/maintenance.rs | firmware.dart: explicit selected targets, HTTPS manifest and lowercase SHA-256 validation, exact job/boot/hash/target pins, phase guards, positive ACKs, full backup/image bounds, truthful stage/apply/completion and no replay. Adds fresh identity read before apply/cancel. |
| Actual ESP RemoteFirmware JSON, MaintenanceRoutes and upload handlers | Exact /api/firmware/remote/fetch/apply/cancel, /api/avr/probe/backup/image/program, stored backup GETs and /update multipart fields. No synthetic defaults or unadvertised EEPROM/fuse restoration controls. |

Original-byte donor hashes, normalized target/test hashes and exact omitted code
remain in file-comparison-evidence.json. Both picker files now have a specialist
mobile-firmware route and a required build gate. This adds two Pealayer reads,
not two RayanLamp reads; RayanLamp remains six fully compared files/2349 paths.
All donors remain read-only. No entire-project parity or superiority certificate.

## Safety and real controls

- Remote staging requires a direct HTTPS manifest, lowercase SHA-256, optional
  in-memory obscured bearer and individually acknowledged ESP/AVR targets.
  Staging always sends apply=false. Downgrades require an explicit switch.
- Apply/cancel confirm the observed job, then reread its identity/state before
  one mutation. A changed job/boot/hash/target sends nothing. A reply must match
  that exact identity and the expected preparation/cancel state.
- Device-reported received/expected bytes are download progress, not fabricated
  flash percentage. Accepted/staged/preparing is never called completed.
- Each ISP/OTA domain uses current selected-board capability ACKs, including
  after awaits. Lost capabilities/changed owners/disposed views reject stale
  replies. Confirmation locks settings navigation but does not show fake
  operation progress; real operations have separate progress state.
- Manual actions first inspect remote ownership. A busy/ready pinned job blocks
  manual staging/programming. ISP probe resets AVR despite being HTTP GET:
  single-dispatch transport never sends it through discovery retries.
- One-dispatch HTTP has response size limits, a total deadline, stream
  cancellation and no redirects. Device credentials stay in headers; remote
  bearer stays in POST body, never query/error logging. Basic auth on plain
  LAN HTTP is still unencrypted in transit and needs a trusted network.
- Any uncertain maintenance failure locks further writes until explicit
  inspection. Background status reads cannot silently unlock or queue retry.
- AVR import is exactly 32768 raw bytes including the intended bootloader.
  It stages only. ESP import requires uncompressed E9 format, 16 bytes–2 MiB.
  A second confirmation reviews the selected name/length before upload.
- Stored flash/EEPROM exports must be exactly 32768/1024 bytes before a file
  is created. They are existing device backups, not claimed fresh live reads.
  Android ACTION_CREATE_DOCUMENT produces a new destination; a one-use token
  can write only that user-created URI. No arbitrary path/URI write, broad
  storage permission, deletion, overwrite-existing path or automatic replay.
- Picker cancellation sends no board mutation. Owner changes before export
  discard the token; a new empty document can remain and is reported. Failed
  provider writes can leave an incomplete new document; never hidden/deleted.

## Concrete limits and remaining acceptance

- Manual AVR staging is shared and has no advertised hash/CAS in the current
  firmware. A local filename is NOT proof of the current stored image. The
  program confirmation states that race and recommends pinned remote jobs.
- Direct ESP upload is not a pinned manifest/profile/pair-authenticity workflow.
  Its positive updated/rebooting ACK is not proof of the running postboot version.
- Manifest hash pins content supplied by the user, not an independently trusted
  publisher signature. mDNS/SSDP and Basic auth are not cryptographic identity.
- Read-only Flutter/model/HTTP/MethodChannel fixtures cannot prove Android
  provider behavior, rotation/process death, TLS publisher trust, actual flash,
  Urboot/EEPROM, Prog latch recovery, power-loss handling or physical peripherals.
- Remembered provider-folder hints, full localized review/preview, generic
  donor server-browser consumers, mobile appearance/back-draft protection and
  production Android signing remain open. No storage permissions added.
- Source version stays 0.2.7 across ESP/AVR/native/mobile. Old production desktop
  and hardware were not replaced. Fresh ASUS SSH localhost:1022 was refused.

Analyzer clean and all 73 Flutter tests passed through build-mobile.ps1. Actual
simulated tablet renderer was captured with Vazirmatn, Phosphor and MaterialIcons,
inspected, and asserts zero board mutations. Phone capture was also inspected;
all four maintenance cards/controls fit its scrollable width without overflow.
Android assembleRelease compiled the actual Kotlin SAF bridge successfully.
The warm cache is 1.029 GiB/4303 files; no new SDK/dependency/cache/clean. Native
desktop and board firmware were not built. Source-quality suite passed.

![Simulated tablet firmware UI, not hardware](screenshots/mobile-firmware-tablet.png)
![Simulated phone firmware UI, not hardware](screenshots/mobile-firmware-phone.png)

Preview APK: 53971590 bytes, SHA256
16d5c7d421d60cbc737dbd947cee56fb9219c8e1181ef47447934f1934c09432.
Template debug signing, not production. The previous settings APK was preserved
by verified recoverable rename, not overwritten/deleted. The older rollback was
also retained. Exact evidence is in FILE-COMPARISON-RECEIPTS.md.

The authoritative inventory is 4237 paths: 70 completed comparisons, 4167 pending,
44 known parity gaps. Those counts are a boundary, not a completion claim.
