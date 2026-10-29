# Native ESP32-S3 SSH shell

Tracking: [#33](https://github.com/atomicdeploy/PakarController/issues/33).
This is an optional transport for the existing device shell, not an operating
system shell. It does not execute host programs or require an ASUS gateway.

## Build and enable

Use the existing board's reviewed hardware profile with `-S3Ssh` added to
`tools/build-firmware.ps1 -Target esp32s3`. Do not change flash, PSRAM, LED,
USB, game-role or ISP flags just to add SSH. With no flag, SSH code and its
crypto dependency are excluded from the image. Networking-disabled builds
reject this flag. The pinned Arduino dependency is LibSSH-ESP32 **5.10.0**.

The included service starts **disabled** on boards without saved SSH settings.
It never changes HTTP authentication or silently exposes a new listener.
After a separately approved installation, enable it through the existing
USB/Telnet shell:

```text
set ssh.enabled true
get ssh.enabled
get ssh.port
```

Or use the ordinary authenticated API surface:

```sh
curl http://keycontroller.local/api/ssh
curl -X POST http://keycontroller.local/api/ssh -d enabled=true -d port=22
ssh -tt <configured-device-user>@keycontroller.local
```

`/api/shell` also reports the SSH build/listener state. The shell's Device TUI
page includes SSH enable/port when it is built. Port changes and enable state
are persistent, with NVS write/readback verification and identical-save
suppression. Telnet and SSH cannot own the same TCP port. API paths remain
unversioned. The ordinary API's existing authentication policy still applies;
enabling SSH does not make an otherwise public management API private.

## Authentication and identity

- SSH **always requires** the configured device username and password, even
  when HTTP authentication is disabled. Anonymous authentication is rejected.
  No credentials from the upstream demonstration are used.
- A unique ECDSA P-256 host key is generated on first enable and saved in the
  `pakar-ssh` NVS namespace. Its SHA-256 fingerprint is reported by `/api/ssh`;
  private key bytes never pass through the filesystem/file-download APIs.
  Existing malformed host keys fail closed, rather than being regenerated.
- Ordinary OTA preserves NVS and therefore the host identity. Erasing NVS or
  replacing hardware changes it. NVS is not claimed encrypted at rest unless
  the actual device's encryption configuration supplies that protection.
- Changing credentials or disabling the service revokes sessions and pending
  input. Do not suppress an OpenSSH changed-host-key warning: verify the new
  board fingerprint before accepting it.

## Shared commands and resource limits

The same `DeviceShellSession`, line editor, eight-command safe history, TUI,
validators and maintenance exclusion are reused. `help`, `status`, `get`,
`set`, `watch on`, `tui`, game controls and the other existing commands apply
according to board capabilities. SSH bytes do not contain Telnet negotiation.

There is **one authenticated SSH session/channel** for shell, exec or transfer.
PTY dimensions and resize requests are bounded. Host commands use the same
device-command dispatcher as interactive SSH, Telnet and framed USB:

```sh
ssh <configured-device-user>@keycontroller.local status
ssh <configured-device-user>@keycontroller.local 'get hostname'
ssh <configured-device-user>@keycontroller.local 'beep 880 100'
ssh -t <configured-device-user>@keycontroller.local tui
# `edit` is also a direct TUI alias. Use -tt when forcing PTY allocation.
```

One exec request carries one printable-ASCII device command, at most 128 bytes.
It is not `/bin/sh`: pipes, chaining, expansions and host programs are not
interpreted. Exec without a PTY emits no welcome banner, editor echo or prompt,
and starts without ANSI color. After the result drains, the channel reports
exit status 0 for an accepted command, 1 for command/validation/maintenance
failure. A queued AVR operation is acceptance, not proof of physical completion;
use the normal live state/event APIs for its outcome. An invalid request rejected
before channel start or a disconnected transport may produce OpenSSH's own
failure status instead. Closing host stdin does not discard an accepted result.

Direct `tui`/`edit` requires a PTY; without one it returns an actionable error
and nonzero status. With a PTY it remains interactive, accepts navigation and
window changes, and closes SSH when the TUI is exited (Esc or Ctrl-C outside
an edit). Opening it alone does not start a game or alter outputs. A normal
`ssh -t HOST` still opens the interactive command prompt instead. Single-command
session settings such as `watch on` end with that command; use an interactive
session for ongoing watches. Agent/port forwarding and extra channels remain
rejected. Separately built SCP accepts only its exact file commands; separately
built SFTP accepts only `sftp`.

## Optional SCP uploads/downloads and SFTP

Add **both** `-S3Ssh -S3Scp` to the reviewed build profile. The raw flags are
`PAKAR_SSH_ENABLED=1` and `PAKAR_SCP_ENABLED=1`; SCP without SSH is a build error.
Both are excluded by default. Add `-S3Sftp` (`PAKAR_SFTP_ENABLED=1`) for SFTP,
which also requires `-S3Ssh`. The portable build wrapper accepts `--s3-ssh
--s3-scp --s3-sftp` as well. Transfers share the SSH listener/authentication and session limit;
it creates no additional port. `/api/ssh` reports `scp_download` separately from
`scp_upload`, `sftp`, `ftp` and device-command `exec` support. It also reports
`exec_max_bytes` and `exec_tui_requires_pty`.

Files published through the existing Files workspace/API can be downloaded:

```sh
scp -O -P 22 <configured-device-user>@keycontroller.local:/user/example.bin ./
scp -O -P 22 ./example.bin <configured-device-user>@keycontroller.local:/user/example.bin
sftp -P 22 <configured-device-user>@keycontroller.local
# In sftp: put example.bin /user/example.bin
#          get /user/example.bin
#          ls -l /user
```

Modern OpenSSH defaults to SFTP; `-O` explicitly selects legacy SCP when that
subsystem was built instead. The SFTP subset implements binary put/get/list/stat,
file timestamps before close, and a single open file/directory handle. It accepts
the default OpenSSH 32 KiB WRITE size; each loop writes at most 512 verified bytes.
Reads return bounded short chunks. Resume, random-access editing, append,
rename/delete, links, subdirectories and arbitrary permission changes are not
supported; clients receive an explicit failure, never fake success. No shell
expansion or wildcard processing is performed.
Only one exact public user-file name is accepted (ASCII letters/digits, dash,
underscore and dot; no leading dot, `..`, subdirectories or shell syntax).
Each file is limited to the shared store's 128 KiB. Listing and chunk I/O use
the same loop-owned file API, immutable content identity and maintenance/flash
guards as HTTP/USB. Reads make no filesystem changes. Missing/changed files,
auth revocation and interrupted clients cannot become successful downloads.
Output is byte-preserving, ACK-driven and backpressured in 128-byte chunks;
success exit status follows the final client ACK and drained output.

Uploads explicitly replace the named public file, not arbitrary paths. Chunks
are staged, immediately read back and hashed; close/final SCP ACK publishes the
verified content and index atomically. Interrupted/auth-revoked/oversize uploads
do not replace existing files. A C++-only streaming capability calculates the
digest when standard clients cannot supply SHA-256 in advance; public JSON/API
uploads still require their existing declared hash/revision/replacement guards.
An exclusive upload lease excludes OTA/network changes, expires after 60 seconds
without progress, and retries cleanup when the LED flash fence is busy.

Private configuration, SSH host keys, firmware partitions and arbitrary LittleFS
paths are not accessible. These services are **public file transfers, not firmware
flashing**; use the existing verified OTA owner for firmware. Legacy SCP `-p` and
recursion are not implemented. SFTP can preserve modification time with `put -p`
when the requested permissions are the store's fixed `0644`; other chmod requests
fail explicitly. Creation attributes are restricted by the server's fixed safe
permissions. An interrupted standard transfer is not resumable.

## Optional passive FTP

Include `-S3Ftp` (`PAKAR_FTP_ENABLED=1`) independently of SSH. It is excluded from
ordinary images and starts disabled without a saved opt-in. Enable/configure through
the existing management API, whose ordinary authentication policy still applies:

```sh
curl -X POST http://keycontroller.local/api/ftp -d enabled=true -d port=21 -d passive_port=50009
curl http://keycontroller.local/api/ftp
curl --user '<configured-device-user>' --upload-file example.bin ftp://keycontroller.local/example.bin
curl --user '<configured-device-user>' ftp://keycontroller.local/example.bin -o downloaded.bin
curl -X POST http://keycontroller.local/api/ftp -d enabled=false
```

FTP's login directory is `/user`; [curl FTP URL paths](https://curl.se/docs/url-syntax.html)
are relative to that login directory, so the examples intentionally do not add
another `user/` segment.
FTP always requires the board's configured credentials, including when HTTP auth
is disabled, but has **no TLS**: use SFTP on untrusted networks. No anonymous,
active PORT/EPRT, third-party/bounce or raw filesystem access. EPSV/PASV data
connections must originate from the control connection's IP. The configurable
passive port must be 49152–65535 and distinct from HTTP/SSH/Telnet/control ports.
One control/data session, binary TYPE I, STOR/RETR/LIST/NLST/SIZE, 15-second passive
connect and 60-second idle deadlines; nonblocking partial socket writes preserve
unsent bytes. EOF commits STOR; socket errors cancel it. Plain FTP has no declared
file size/hash, so an orderly early EOF is indistinguishable from a deliberately
short file. Use the SHA-256 API if end-to-end sender-declared integrity is required.
FTP timestamps are not preserved (unknown = 0); use SFTP/API for dated files.
Enable/ports persist with readback verification. Credential changes revoke sessions.
`/api/shell` reports built/enabled/listening state even without SSH built.

Cryptography and sockets run on a low-priority worker; all shell commands stay
on the application loop. Bounded queues carry boot-local session tokens and
security revisions. Partial writes preserve unsent bytes; slow readers cause
backpressure and eventual closure, not silent truncation or command replay.
Handshake, authentication, idle and output-drain deadlines are enforced.
SSH suspends for maintenance. Telnet and framed USB remain separate recovery
paths and share no SSH authentication session.

The worker requests a 24 KiB stack and starts only with at least 90,000 free
internal heap bytes. Insufficient resources are reported explicitly. This is
a conservative admission threshold, not a measured worst-case handshake budget.
After initialization, there is no periodic flash write for an idle session.
An authenticated SFTP subsystem allocates a 33,024-byte bounded receive buffer,
preferring PSRAM with an internal-memory fallback. Allocation failure closes the
subsystem, not the main API. Its response buffer is 1,536 bytes; writes still
reach the flash owner in 512-byte chunks. These known allocations are not a
measurement of worst-case cryptographic or live heap usage.

## Measured build footprint

Canonical cached SDK builds on 2026-10-28, Arduino-ESP32 3.3.12 and LibSSH-ESP32
5.10.0. All three use the same ledcontroller/table profile: 16 MiB flash, OPI
PSRAM, UART0 host transport, W5500 included, status RGB48, both LED backends with
RMT default, acknowledged ISP support and BLE excluded. These settings describe
this measurement profile, not permission to copy its electrical/ISP assumptions
to another board. OTA slot capacity is **3,342,336 bytes**.

| Included services | Raw image bytes | Static RAM bytes | Flash / RAM delta vs ordinary |
|---|---:|---:|---:|
| Ordinary (all four flags off) | 2,387,744 | 140,156 | — |
| FTP only | 2,401,440 | 144,748 | +13,696 / +4,592 |
| SSH + SCP + SFTP + FTP | 2,694,000 | 147,556 | +306,256 / +7,400 |

The full option uses **80.60%** of the OTA slot. All flags remain off by default;
when included, listeners still require explicit runtime enable (or a previously
saved opt-in). Static RAM is **not live free heap** and excludes worker stacks,
queues, socket/crypto allocations and the on-demand SFTP buffer above. No live
ESP heap/transfer/LED coexistence result is inferred from these builds.

Raw image SHA-256 identities:

```text
ordinary        d8c3a5db4ca137dddcbfc0d42d4936dea40c3e95ad74c7e18e895f298409e047
FTP only        c226067dea2d1b5322eb993506115f2927ea6dc2d1c2f73b4a014cc06abb06d0
all services    ca1a24f4432731d0924599a9202c158750a345b0bac0a2c22cee5233d78840e3
```

Reproduce with the same reviewed hardware profile through
`tools/build-firmware.ps1`, first with `-S3Ssh -S3Scp -S3Sftp -S3Ftp`, then
`-S3Ftp`, then without those switches. The last build restores the ordinary
artifact in the same canonical cache. Exact profiles, timestamps and ELF-derived
size observations are retained locally in `build/evidence/ssh-transfer-footprint.json`.
Arduino may invalidate vendor objects when its resolved dependency/include list
changes between SSH-included/excluded variants; no cache clean is required.

## Dependency and acceptance

Upstream: [LibSSH-ESP32](https://github.com/ewpa/LibSSH-ESP32/tree/5.10.0),
commit `343fce1c5d1d0e3d629807dece6ae1b68ef1d322`, based on libssh 0.11.5.
The registry archive SHA-256 is
`e1b6ab338ff2087ab4427363350849416c371f90ae675d1304c6b78721e5b9e3`.
The library retains its LGPL licensing; consult upstream `COPYING`/`BSD` and
preserve notices and relinking/source obligations when distributing a linked
firmware. The sample server describes its example code as public-domain;
Pakar adapts its bind/auth/channel flow, not its password logging, static
credentials, standalone network initialization, echo loop or SPIFFS formatting.

Upstream recommends disabling SDK hardware SHA for concurrent operation. This
task does not rebuild/change the SDK or certify hardware SHA/TLS/LED coexistence.
That remains a real-device acceptance gate along with memory pressure, OTA,
wrong-password handling, host-key persistence, PTY resize, reconnect and the
uninterrupted operation of AVR/LED/API services. A host declaration compile or
policy test is not an encrypted OpenSSH session on the ESP.

Run `tools/test-ssh.ps1` after installing the pinned dependency and ArduinoJson
for policy/NVS tests, actual transport API compilation with SCP/SFTP included/excluded,
SCP byte-state tests and actual SCP/SFTP/FTP adapters with FS/transport
doubles. Real OpenSSH clients exercise binary/empty SCP uploads and a default-buffer
SFTP put/list/get byte roundtrip over a stdio fixture, not encrypted ESP sockets.
CI quality invokes it and the firmware job builds the opt-in transports
before restoring the ordinary artifact. The worker explicitly marks its listening
socket nonblocking: the pinned library's `ssh_bind_set_blocking` alone changes
only a flag and cannot prevent a select/accept race from blocking revocation.
These checks and an SDK link are not a real encrypted board login, hardware
coexistence or deployment. Those remain issue #33 acceptance gates.

The exec regression fixtures compile the actual transport connection/pump and
the actual shared command dispatcher/SSH loop against libssh queue/transport
doubles. They cover immediate stdin EOF, short writes/backpressure, exactly-once
dispatch, rejection/exit status, credential revocation, non-PTY output, direct
TUI/alias entry, resize and exit, in both table and keycontroller roles. No
encrypted ESP login or live deployment is inferred from these host fixtures.

Protocol references: [SFTP v3 wire format](https://datatracker.ietf.org/doc/html/draft-ietf-secsh-filexfer-02),
[FTP RFC 959](https://www.rfc-editor.org/rfc/rfc959),
[OpenSSH release notes](https://www.openssh.org/releasenotes.html).
