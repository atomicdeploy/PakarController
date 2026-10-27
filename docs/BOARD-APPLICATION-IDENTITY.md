# Board and running application identity

Three different values now have separate owners:

| Value | Owner | Settings/API |
|---|---|---|
| Board display name | User, persisted atomic schema-2 record | Board identity page and `display_name` |
| DNS hostname | User, same persisted record | `hostname`; optional .local on input |
| Application ID, name, title, PNG | Running firmware | Read-only `application`; APP_ID / APP_NAME / APP_TITLE / APP_ICON_PNG_BASE64 build macros |

The table-selection build reports Pakar Table by default; the first-wins build
reports Pakar Keys, with Pakar Controller as the generic non-Table fallback.
APP_TITLE defaults to APP_NAME. No identity is inferred from
hostname, LED count, previous selection or saved client preferences.

Native header, viewport title/window/taskbar artwork, tray artwork/Show label,
tooltip, About and IPC application snapshot follow the selected fresh peer.
Web header, document title, application description, favicon and touch artwork
follow that same manifest. Disconnect/auth loss/stale status resets to client
fallback, not another board. The package/executable, update product ID, instance
GUID/IPC ownership, installed shortcut and Windows app registration stay
PakarController: changing these per selected peer would break single-instance
ownership and host update identity. They are explicitly the host, not board firmware.

## HTTP / CLI / shell

Authenticated GET `/api/device/name` returns hostname/fqdn/canonical_url,
display_name, revision, persisted state, application and apply status.
POST uses form fields `hostname` and optional `display_name`; omitted display
name preserves it for old clients. Both fields validate before one verified,
atomic commit. Credentials and network configuration are not rewritten.
Display-name-only and no-op changes do not restart Wi-Fi. Hostname changes
schedule network/AP/cloud/discovery rename after the response. mDNS application
TXT and SSDP friendly/model names also report the appropriate owners; human
names are XML-escaped in the description document.

```powershell
PakarController.exe --command device identity
PakarController.exe --command device rename ledboard.local
PakarController.exe --command device label Living room table
PakarController.exe --command device configure ledboard.local Living room table
```

Telnet: `device identity`, `device rename HOST`, `device label DISPLAY NAME`.
The existing authenticated hostname route is retained, not replaced with an
incompatible endpoint. Human names are 1..63 UTF-8 bytes with no C0/C1 controls
or ASCII edge spaces. Hostname validation remains strict DNS-label normalization.

## USB

Identity capability bit 13; append-only status byte 37 carries revision low byte.
GET 0x59 payload `nonce,offset_u16LE`; reply 0xB2 carries
`nonce,revision_u32LE,total_u16LE,offset_u16LE,up-to-39 JSON bytes`.
The living JSON is revision/hostname/display_name/application, no secrets.
Client checks request sequence, nonce, revision, size <=8192, ordered offsets,
UTF-8/JSON and identity fields. Two-second per-response and eight-second whole
read deadlines prevent a hung transfer. Ten-second read-only refresh catches
revision-byte collisions; unchanged refresh retains current artwork.

Existing name write 0x55 retains reset=0, hostname chunk=1, commit=2;
display-name chunk=3 is additive. Each staged write waits for matching sequence
and opcode ACK, then both names must match authoritative readback. Uncertain
mutations are not replayed. USB metadata belongs to that port, never Wi-Fi.

## Actual donor code and explicit remaining work

Pealayer src/branding.rs was completely reread: actual icon_data_from_bytes,
source signature, cached native multi-viewport publication and title/tray update
lifecycle are adapted; playback state becomes peer application metadata.
HeaderBar.tsx's application-name/artwork ownership binds real reported values.
PNG is inline, <=4096 base64 characters with IHDR dimensions 8..256 before
decode; external artwork URLs/path reads and per-frame/disk caches are absent.
Same master generates firmware, pre-login Web and host fallback artwork.

Pealayer config.rs resolver and PU LocalLib/Commands.cpp Hostname are screened
slices, not complete comparisons. Entire-file review remains pending in the
existing ledger. Local filesystem icon overrides/presets, high-DPI editable
masters, full WebPlatform shell actions and generic PCController adapter are
still useful open work, not silently excluded or asserted complete.

## Historical acceptance checkpoint

The following0.2.7 images/cache/host observations belong to that earlier pass.
Current source is0.2.8/client build23; see ARCHITECTURE.md for current role and
deployment gaps. Do not reuse an old image/host observation as current readiness.

Source versions stay consistent at 0.2.7. Both ESP link-mode candidates compile:
busy-wire RAM 56460/80192, IRAM 63055/65536, IROM 726604/1048576;
polling RAM 56436, same IRAM, IROM 726440. Candidate SHA-256:

- busy-wire: `ffc80e8f0e49355646312468467353614b27d61cdfd77cc46d6539eb6c3d1aca`
- polling: `994ca7a64853b5d0fd79075395a9b44a5c0495874a0796c2d8d67e59c87e835a`

These are local build candidates, not deployed identity or signed authenticity.

- [x] Actual embedded Web identity/access/context/branding tests: owner rebind,
  title/header/favicon, UTF-8 and invalid artwork bounds, no-op/label-only save.
- [x] Actual DeviceIdentity host tests: atomic paired fields, schema-1 migration,
  reboot, no-op writes, failed migration/commit preservation, corrupt recovery.
- [ ] Rust unit/CLI/USB runtime and native window/tray acceptance: wrapper stops
  before compilation at 13.49 GiB cache versus 8 GiB policy. No bypass/replacement.
- [ ] Physical Wi-Fi/USB identity/OTA/AP/discovery: ASUS SSH localhost:1022 refuses;
  no deployed-board acceptance is claimed.

CI is asynchronous; source and build candidates are not the running executable
or board. See issue #10 and the preflight receipt for current execution results.
