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

The initial implementation provides **one interactive SSH session/channel**.
PTY dimensions and resize requests are bounded. Arbitrary exec requests
(`ssh host cmd`), SFTP/subsystems, agent forwarding and port forwarding are
rejected. Use an interactive terminal and type commands there. The separately
included read-only SCP source is the only accepted exec request.

## Optional SCP downloads

Add **both** `-S3Ssh -S3Scp` to the reviewed build profile. The raw flags are
`PAKAR_SSH_ENABLED=1` and `PAKAR_SCP_ENABLED=1`; SCP without SSH is a build error.
Both are excluded by default. The portable build wrapper accepts `--s3-ssh
--s3-scp` as well. SCP shares the SSH listener/authentication and session limit;
it creates no additional port. `/api/ssh` reports `scp_download` separately from
`scp_upload`, `sftp`, `ftp` and arbitrary `exec` support.

Files published through the existing Files workspace/API can be downloaded:

```sh
scp -O -P 22 <configured-device-user>@keycontroller.local:/user/example.bin ./
```

Modern OpenSSH defaults to SFTP; `-O` explicitly selects the supported legacy
SCP wire protocol. No shell expansion or wildcard processing is performed.
Only one exact public user-file name is accepted (ASCII letters/digits, dash,
underscore and dot; no leading dot, `..`, subdirectories or shell syntax).
Each file is limited to the shared store's 128 KiB. Listing and chunk reads use
the same loop-owned file API, immutable content identity and maintenance/flash
guards as HTTP/USB. Reads make no filesystem changes. Missing/changed files,
auth revocation and interrupted clients cannot become successful downloads.
Output is byte-preserving, ACK-driven and backpressured in 128-byte chunks;
success exit status follows the final client ACK and drained output.

Private configuration, SSH host keys, firmware partitions and arbitrary LittleFS
paths are not accessible through SCP. Uploads, recursion, `-p` timestamp transfer
and SFTP are **not implemented**; continue to use the existing integrity-checked
file upload API. FTP is not implemented or advertised: this pass provides
encrypted SCP downloads rather than a new plaintext credential/data listener.
Bidirectional SFTP or transactional SCP uploads require a separate implementation
of the existing hash/revision/explicit-replacement contract, not direct writes.

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
for policy/NVS tests, actual transport API compilation with SCP included/excluded,
SCP byte-state tests and the actual SCP/public-file binding with FS/transport
doubles. CI quality invokes it and the firmware job builds the opt-in transport
before restoring the ordinary artifact. The worker explicitly marks its listening
socket nonblocking: the pinned library's `ssh_bind_set_blocking` alone changes
only a flag and cannot prevent a select/accept race from blocking revocation.
These checks and an SDK link are not a real encrypted board login, hardware
coexistence or deployment. Those remain issue #33 acceptance gates.

Protocol reference: [OpenSSH release notes](https://www.openssh.org/releasenotes.html).
