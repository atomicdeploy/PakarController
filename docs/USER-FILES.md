# Managed files on the board

The canonical Web workspace has a **Files** panel, advertised by `managed-files`.
It offers a filterable, paginated index, explicit upload/replacement, download,
byte progress and a single **Download** menu (file or timestamp-preserving archive).
Opening the panel does not write anything.
There is no second Web application and no configuration migration.

## Scope and protection

This is a managed user-file area on LittleFS, not an unrestricted filesystem
explorer. Names are 1–64 ASCII letters/numbers/dashes/underscores/dots; leading
dots, consecutive dots, separators, percent escapes and absolute paths are
rejected. Up to 32 files, each up to 128 KiB, are accepted subject to actual free
storage. No filename is interpreted as a native filesystem path.

Credentials, settings, journals, firmware staging and AVR backups are never
listed, uploaded over or read through this API, even with authentication disabled.
Use their existing typed configuration/export and firmware-backup APIs instead.
Those retain their own authentication, redaction and maintenance ownership.
The legacy read-only `/api/files/read` operation remains restricted to `/user/`;
it is not the managed catalogue and is not broadened by this feature.

Generic HTTP access inherits the same-origin/authentication/version/board/boot
checks. USB requires the existing physical transport and boot-bound API envelope.
Neither grants access to private files. Upload leases expire after 60 seconds of
inactivity or an access-security revision change. Unknown/stale ownership fails
closed; uploads are not automatically restarted on another peer.

## Bytes and timestamps

Uploads send exact bytes, source `modified_ms` (Unix milliseconds), total length
and SHA-256. Each sequential 512-byte chunk is read back before acceptance;
repeated identical chunks are idempotent. A mismatching replay is rejected.
Commit checks the complete digest and publishes a checksummed index only after
all bytes verify. Listings and reads do not modify dates or write flash.

Immutable data objects live under `/.user-files/`; logical names are in alternating
`/.user-index.0.json` and `/.user-index.1.json` catalogues. Two generations preserve
the previous record through interruption without requiring FAT rename-overwrite.
Only verified retired-generation objects unreferenced by either retained index
are reclaimed. A power loss can leave an unreferenced staged/object file; no
automatic broad sweep or filesystem format is performed. Both invalid catalogues
produce an error, not an empty replacement. A valid older catalogue may be used
when the newer one is damaged. This mechanism is host-tested, not power-cut
verified on hardware.

Ordinary browser downloads cannot set the destination OS file timestamp. The
**Download archive (.tar)** option emits a PAX archive containing the original bytes and fractional
`mtime`; a PAX-aware extractor restores the original modification time. Browser
uploads preserve `File.lastModified`, not creation time, permissions or ownership.
No clock/NTP dependency is introduced. The Web SHA-256 fallback works on plain
HTTP origins where SubtleCrypto is unavailable.

## Shared HTTP and USB contract

Send this envelope through physical USB's existing generic API, or POST JSON to
`/api/maintenance/request` with the normal peer headers and authentication:

```json
{
  "board_id":"<observed board>", "boot_id":"<observed boot>",
  "method":"POST", "path":"/api/storage/files",
  "body":{"volume":"littlefs","action":"list","offset":0}
}
```

`/api/storage/files` is the inner API operation, not a second HTTP route.
Every response includes `accepted`; rejection includes `error`.

| Action | Additional fields | Result |
| --- | --- | --- |
| `list` | `offset` (default 0) | Up to 8 `files`, `revision`, `next_offset`, `more`, limits |
| `begin` | `name`, `size`, `sha256`, `modified_ms`, catalogue `revision`; existing file requires `replace_sha256` | Transfer `token` |
| `write` | `token`, `offset`, lowercase `hex` (1–512 bytes) | Accepted `received` offset |
| `status` | `token` | Actual progress and `committed` |
| `commit` | `token` | Digest/date/revision and `committed:true`; repeated commit is read-only |
| `cancel` | `token` | Ends lease; removes only its uncommitted stage/object |
| `read` | `name`, expected `sha256`, `offset`, `count` (0–512) | Exact `hex` plus file metadata |

Readers verify the complete resulting SHA-256 before exposing a download. A
changed file rejects old expected hashes. Lost mutation replies must be inspected
with status/list rather than blindly opening another upload. No arbitrary file
delete, rename, executable hosting, directory recursion or remote URL fetching is
exposed. WebDAV is deliberately **not advertised**: the Web browser and bounded
API provide the requested file functions without an incomplete DAV server or a
second authorization model. Future DAV must reuse this same namespace/ownership.

## Optional microSD

Normal images define `PAKAR_MICROSD_ENABLED=0`: no SD pins are assigned or probed.
For an ESP32-S3 with reviewed wiring, the build wrapper accepts `-S3MicroSd` plus
explicit `-S3SdClk`, `-S3SdCmd`, `-S3SdD0`. All three must be distinct spare pins
from 1, 2, 39–42. These are candidate common-DevKit GPIOs, not wiring approval;
check the exact board and any custom peripherals before selecting them.

The flags enable 1-bit SD_MMC at 10 MHz, with **format-on-failure disabled**.
The same API uses `volume:"sd"` and a separate on-card catalogue. The UI enables
that choice only when `microsd-files` is advertised. An absent/unmounted card
returns `volume_unavailable`; hot-unplug errors never initialize/format storage.
Reconnect/remount currently requires boot. Physical SD performance, power-cut
durability and hot-removal recovery remain hardware acceptance items.

## Evidence and delivery boundary

`tools/test-user-files.ps1` covers transaction/private-path/lease behavior and
Web SHA-256/PAX/transfer UI with isolated test doubles. Canonical Web assets use
`tools/build-web.mjs`. Work is tracked in GitHub issue #28. Source work and build
candidates do not change any currently running board or its saved values.

The scoped ESP32-S3 buffered/16 MiB/OPI build with both LED backends, Ethernet
built and BLE disabled passes: 2,300,263 bytes firmware; 138,724 bytes static RAM.
This compile did not embed an AVR recovery bundle and is not a deployment receipt.
Both full/onboard Web builds share revision
`8eccb63f901ce960356af96d49d7a2afee756b8b931ee54dcf3486f7cb188d18`.
The isolated browser fixture lists files and completes a verified archive download;
it is not a live board transfer. SD-enabled SDK compilation and physical SD tests
are still pending.

Public static assets use `StaticAssetService.h`, separate from this managed file
API. Compared with PU850's reviewed root/download routes, the shell adds gzip,
byte-derived ETags/304, exact GET/HEAD routes, MIME/nosniff and bounded ACK-safe
flash streaming without serial-pumping waits. Compression negotiation and Vary
follow RFC 9110; compressed-only page/font return 406 when unsupported, while
`/basic` remains uncompressed. Runtime throughput is not yet benchmarked.
