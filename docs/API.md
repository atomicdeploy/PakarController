# HTTP and streaming API

All interfaces use [one living, unversioned contract](API-POLICY.md).
Numbered API namespaces are rejected with 404, never redirected or aliased.

Open your configured hostname (`http://ledboard.local/` for this instance;
factory default `http://pakarcontroller.local/`, with IP only as a recovery fallback).
Authentication is disabled by default. Web and native clients read `enabled`
from `/api/auth/info` and do not ask for credentials when it is false. Enable
**Require authentication** in Device access to protect HTTP routes and `/ws`
with browser sessions or explicit Basic credentials. `/`, `/description.xml`
and `/api/auth/info` are public reads; `/login` accepts a form POST.
Local HTTP and Telnet are not encrypted; never expose them directly to the Internet.
The recovery AP Wi-Fi password is independent of the device login.
See `ACCESS.md` for persistence, CLI/TUI, native editor and physical USB recovery.

## Application API bridge / repeater

Native **Preferences → Connection → Share connection** enables an optional
listener (default TCP port `8789`) and individually configurable mDNS/DNS-SD,
SSDP and MNDP. Save to apply. It is off by default: ordinary USB operation
remains completely offline. Choose a unique configured hostname; an mDNS name
conflict is reported, not silently renamed. The page shows actual started
protocols, failures and verified peer endpoints. Discovery never connects or
changes the selected owner automatically. No firewall rules are added.

For example, select USB on the owning PC and connect another native/Flutter
client to `http://<configured-hostname>.local:8789`. The downstream board keeps
its own board/boot identity and name. A network-connected instance can repeat
this endpoint with its own distinct hostname. Revoking the selected connection
revokes bridge commands too; forwarding loops, paths longer than four instances,
stale peers and expired commands are rejected. The existing serial worker is
the only USB reader/writer; API requests wait for ACK and actual board readback.

Implemented compatible reads include `/api/status`,
`/api/cells/mapping`, public `/description.xml` and `/api/discovery`.
Table cell/batch, brightness, mode, effect and LED output mutations use the same
existing routes and require `X-Pakar-Board`/`X-Pakar-Boot` fencing. `/ws` and
`POST /api/command` use the existing command envelope; HTTP recovery accepts
the existing URL-encoded `packet` field. A repeated identical request ID retrieves
its receipt rather than dispatching a second command. USB capabilities expose
only these implemented table operations, not peripherals on a separate network
route. Selected network routes also relay supported form/JSON reads and commands.
They additionally preserve the actual `/info` and `/api/telemetry` full snapshot,
cloud configuration, guarded remote updater, `/update` multipart upload,
`/firmware/download` and `/file/download` binary GET/HEAD/Range/ETag contracts,
and AVR probe/backup/image/program/contracts/result routes. Upload staging,
queued commands and verified programming retain the coordinator's distinct
status/receipts; no second programmer is introduced. Reset is the existing
`POST /api/avr/contracts` form `opcode=53&payload_hex=00`, followed by its
correlated result and actual AVR reconnection/reset telemetry. It is not a
flash or bootloader repair command. These management transfers require the
selected network owner; USB never opens an implicit Wi-Fi fallback.

Sharing is explicitly opt-in and permits clients on the bound network to command
the selected board. This checkpoint does not implement TLS/authentication or a
cross-origin browser server. USB-only maintenance transfers, binary pixel streaming,
generic PCController JSON-RPC/Socket.IO interoperability, Android LAN publishing
and additional WS-Discovery/NetBIOS adapters remain open in issue #2. Network
OTA/ISP are advertised only when the selected board ACKs them; streaming and
unsupported USB maintenance capabilities remain suppressed. Android
currently consumes native bridges, while its own advanced bridge remains
foreground-only authenticated loopback/MQTT/outbound WebSocket, not a LAN server.

## Board Silent mode

Native: **Preferences → Board → Silent mode**. Flutter and Web expose **Board
sound** in Preferences after the connected AVR acknowledges both settings and
buzzer capabilities. This is the AVR EEPROM Silent flag, not Windows/phone/browser
notification volume. Programming safety remains a separate forced mute.

Changes use the existing PCController contract: GET_SETTINGS (`4`) →
SET_SETTINGS (`5`, complete preserved 21-byte Pakar record with flags bit0 changed)
→ actual AVR ACK → GET_SETTINGS with matching values and `persisted=true`.
Native USB uses the same inner requests through opcode `0x56`, response `0xB0`,
without requiring or opening Wi-Fi. HTTP uses `/api/avr/contracts` and its
correlated, consumed `/api/avr/contracts/result` receipts. No new API namespace
or shadow host-owned mute preference is introduced. Unknown/stale/unconfirmed
state is not Off; uncertain mutations are never automatically replayed.

Existing network CLI/TUI commands: `avr settings`, `avr set silent on`, and
`avr set silent off`. A persisted no-op does not rewrite EEPROM. Silent suppresses
welcome, menu and melody output; a mute change publishes immediately, and unmute
does not restart the currently interrupted note. Independently configured host
mirroring is not muted by this physical-board preference.

## Routes

| Method | Route | Purpose |
|---|---|---|
| GET | `/api/auth/info` | public readiness, `enabled`, username and enabled-only default-credential warning; never the configured password |
| POST | `/login` | form `username`, `password`; HttpOnly SameSite=Strict session cookie |
| POST | `/logout` | revoke this session and close established sockets |
| GET | `/api/auth` | username, password-set flag and `enabled` |
| POST | `/api/auth` | form `enabled=true|false`, `current_password`, `username`, `password`; atomic persistence; current password required only when presently enabled |
| GET | `/update/info` | PU-compatible product/name/model/hash/size/free plus version and actual OTA phase/received byte count |
| GET | `/info` | redacted firmware, heap, reset reason, uptime and protocol diagnostics |
| GET, HEAD | `/firmware/download` | imported PU850 sketch download with Range/ETag/MD5; full-flash dumps disabled |
| GET | `/api/status` | peer-advertised AVR, mode, winner, cell, brightness, and LED state |
| GET | `/api/time` | NTP synchronization state and UTC epoch |
| GET | `/api/wifi/scan` | visible SSIDs, RSSI, and encryption flag |
| POST | `/api/mode` | form `value` from the peer-advertised modes; Snake is disabled/hidden by default |
| POST | `/api/cell` | form `id=1..80`, `state=white|red|off`, `effect=immediate|fade|pulse` |
| POST | `/api/cells` | atomic selection: form `ids=1,2,80` or `1-80`, `state=white|red|off`, `effect=immediate|fade|pulse` (default immediate); requires advertised `cell-batch` |
| POST | `/api/game/reset` | reset the current game |
| POST | `/api/snake/direction` | form `value=up|right|down|left`; opposite turns are rejected |
| POST | `/api/brightness` | form `value=1..255` |
| POST | `/api/effect` | form `value=none|rainbow|fire|cell-chase`; chase visits logical80 down to1,750ms each, once, on white |
| POST | `/api/buzzer` | form `frequency=20..12000`, `duration_ms=1..5000` |
| POST | `/api/audio` | form `cue` from peer-acknowledged `audio_cues`, including `attention` and donor welcome variations; 202 means queued, not playback completion |
| POST | `/api/display` | `value=-999..9999`, scheduled `text`, or `release=true`; see below |
| POST | `/api/outputs` | form `mask=0..255` for direct 74HC595 diagnostics |
| GET | `/api/network` | AP/station state without returning credentials |
| POST | `/api/network` | form `station_enabled`, `ap_enabled`, `ssid`, `password`; persist and apply AP+station configuration |
| GET | `/api/cloud` | redacted MQTT and outbound-WebSocket configuration plus live connection state |
| POST | `/api/cloud` | persist and apply MQTT/WebSocket endpoints, TLS flags, credentials/tokens, base topic, and telemetry interval |
| POST | `/api/cloud/reconnect` | disconnect and reapply both outbound cloud clients |
| GET | `/api/avr/probe` | ISP signature probe; briefly resets the AVR, requires current peer headers |
| POST | `/api/avr/backup` | store flash, EEPROM, fuses, and lock byte in LittleFS |
| GET | `/api/avr/backup/flash` | download the last 32 KiB raw flash backup |
| GET | `/api/avr/backup/eeprom` | download the last 1 KiB EEPROM backup |
| POST | `/api/avr/image` | multipart upload of one complete 32768-byte AVR+Urboot raw image |
| POST | `/api/avr/program` | backup, erase, program, and byte-verify the staged image |
| GET, POST | `/api/avr/contracts` | capability/contract inspection or queued PCController operation; RESET is decimal `opcode=53&payload_hex=00` |
| GET, DELETE | `/api/avr/contracts/result` | inspect or explicitly consume the correlated terminal receipt; never replay an uncertain reset/write |
| GET, POST | `/update` | authenticated async ESP self-OTA form/upload; optional file `MD5` preceding the firmware file |

UDP port 7890 accepts bounded `PKAR` chunks **without authentication**;
use only on a trusted LAN. The authenticated WebSocket is preferred.
Each datagram contains the four
ASCII magic bytes, two reserved bytes that must be zero, little-endian pixel offset,
one pixel count, one flags byte, then RGB triples. Bit zero of flags commits
the staged frame. A packet is rejected unless its exact size and pixel range
are valid.

Peers advertising `pixel-diagnostic` accept one `pixel-diagnostic` command via
the existing `/ws` or `POST /api/command` envelope. Fields are zero-based `index`
(0..479), `count` (1 or6, six-pixel groups aligned to a multiple of6), integer
`rgb` (0..16777215) and boolean `off`. It validates before mutation, respects
another stream's staging lease, clears the full480-pixel frame, enables raw
effects output and publishes exactly the requested physical pixel/group with no
fade, logical remapping, AVR command or persistence write. `off:true` publishes
all480 black and leaves the output service enabled. The correlated reply carries
`status.pixel_frame` with the actual full RGB buffer. Its request ID provides
the same deduplicated WS-to-HTTP receipt recovery as cell commands; an uncertain
outcome never becomes a fresh write. This is buffer publication, not optical
proof. Flutter's internal path selector `/api/led/diagnostic` names this command;
it is not a separate REST endpoint.

Normal selection edits are not streamed RGB frames. `/api/cells` validates the
complete selection before changing anything, stages all targets together and
returns `202 {"accepted":true,"count":N}`. Duplicate IDs count once; malformed,
empty or out-of-range selections are rejected without a partial edit. Fade and
pulse timing is generated on the ESP from one common start time (300ms), not by
client timers. Static grouped mode deliberately applies immediately. This scene
edit does not write persistent settings or forward 80 commands to AVR.

Clients must observe `cell-batch`, pin the board/boot identity and confirm every
selected target state through fresh same-board readback after the ACK. An ACK is
not optical confirmation or completion of a fade. An uncertain outcome is not
automatically replayed, nor converted into per-cell requests. USB carries one
boot-fenced opcode `0x5F`: state byte, effect byte and ten-byte 80-cell bitmap
(cell1 is bit0 of the first bitmap byte). Native CLI:
`PakarController.exe --led-cells 1-80 white immediate`; the selected USB method
works offline without a network fallback. MQTT/outbound WebSocket use
`{"command":"cells","ids":"1-80","state":"red","effect":"fade"}`.

USB status byte45 optionally advertises immediate command-state readback.
After dispatching a boot-fenced table command, the ESP retains the original
opcode/sequence ACK/error and appends `0xB7` with the same sequence: inner opcode,
16-byte boot identity, mode, winner, selected cell, brightness, pattern, output
enabled, output mode, then twenty bytes packing four two-bit cell states each
(low bits first). The 44-byte payload is observed board state after dispatch,
not an echo of requested values. A matching success ACK, current serial owner,
boot and verified requested values are all required for completion. Older
firmware and a missing companion use a new full readback, never mutation replay.
Full metadata/pixel reads yield between chunks and resume afterward; older
captures cannot overwrite newer command state or masquerade as current RGB.
Partial readback never renews full peripheral/capability observations.

Native `--app-status` exposes the last successful `usb_command` timing, including
physical session, sequence, opcode, queue, wire ACK, board confirmation and total
milliseconds. IPC `accepted:true` alone only means the GUI queued the action.
`tools/measure-usb-roundtrip.ps1 -Run` measures actual IPC-to-confirmed-USB changes
on the verified running owner and restores its original cell/search state;
`-DelayMilliseconds` exercises background reads between commands. No second
serial handle, HTTP command fallback or inferred physical LED result is used.

Display text accepts `text` (0..40 printable ASCII bytes), `speed_ms=80..5000`
(default 260), `hold_ms=0..60000` (default 1500),
`repeat=once|loop|interval` (**default once**), `force_scroll=true|false`
(default false), and `interval_seconds=1..255` (default 5). Long text scrolls
automatically; short text scrolls only when forced. Static hold zero is an
explicit persistent lease. A one-shot scroll renders a terminal blank frame
and returns to the local page; Interval exposes the local page during its pause.
`release=true` cancels host text; presenting a number replaces prior text.
Invalid repeat names are rejected rather than silently changed. The USB path
uses PCController scheduled target 5 and explicit release target 4 on opcode
`0x38`. See `FRONT-PANEL-PARITY.md` for reuse and verification boundaries.

`/ws` is the authenticated same-origin WebSocket. It pushes the complete JSON
status record to every connected client, accepts `{"type":"status"}` and
`{"type":"ping"}`, and accepts the same bounded binary `PKAR` chunks as UDP.
This is the supported browser and desktop streaming path; multiple read/control
clients may remain connected concurrently.

The outbound MQTT client uses MQTT 3.1.1 and validates TLS with ISRG Root X1.
It publishes retained `<base>/<hostname>/status` and
`<base>/<hostname>/availability`, publishes command acknowledgements to
`event`, and subscribes to `command`. The outbound WebSocket client supports
configurable host, port, path, TLS, and bearer token; it sends the same status
JSON and accepts the same JSON command objects as MQTT. Supported commands are
`status`, `mode`, `cell`, `cells`, `brightness`, `effect`, `outputs`,
`display`, `buzzer`, `audio`, `snake-direction`, and `reset`. Secret values are write-only through
the API; blank omitted password/token fields preserve the stored value.
Cloud `display` also accepts the same text scheduling fields above or
`{"command":"display","release":true}`. An omitted repeat means one-shot.

The password-authenticated Telnet shell on TCP 23 exposes `status`, `mode`,
`cell`, `cells`, `brightness`, `effect`, `display`, `audio`, and `reset`. It uses the
same validation and state transitions as cloud JSON commands. The password is
the HTTP development password; the service is advertised through mDNS as
`_telnet._tcp`. USB serial remains a framed machine interface rather than a
text shell, so Telnet cannot interfere with the simultaneous desktop link.
Telnet supports `display stop` and
`display text <once|loop|interval> <speed_ms> <hold_ms> <pause_seconds> <scroll 0|1> <TEXT>`.
Atomic selection example: `cells 1-80 red fade`; melody example: `audio welcome`
(only a cue advertised by the connected AVR is accepted).

The NodeMCU USB UART is a simultaneous 115200-baud PC endpoint using the same
COBS, CRC-8/ATM, sequence, opcode, and bounded-payload envelope as the board
link. It exposes hello/status, modes, cell actions, reset, brightness, effect,
shift outputs, buzzer, audio cues, and TM1637 display commands. The HTTP status
record includes separate USB byte, frame, and error counters.

`/api/avr/program` also provides the audible start/success boundary and
durable `Prog` safety latch. Repeating the request while the latch is armed
resumes programming without replacing the preserved previous-good backup;
cloud reconnect work remains paused until verification clears the latch. The
status record exposes `programming_latch`,
`tm1637_ack`, AP and station addresses, station association, NTP state,
WebSocket/Telnet client state, link mode, and separate ESP, AVR, and USB UART counters.
Passwords are never returned.
## Bounded LED/API benchmark

Run `node tools/benchmark-led-api.mjs --run --origin http://ledboard.local --backend rmt-dma`
on a host that resolves the board. Use `gpio-static` for that compiled backend;
the tool never switches the physical driver silently. Optional Basic credentials
come only from `PAKAR_BENCH_USERNAME` / `PAKAR_BENCH_PASSWORD` environment variables.
Default workload is8seconds each in static/effects rendering, two read clients
and a single5Hz writer touching cells1/80 (not full-table flashing). Reports
request/write throughput, p50/p95/p99 latency, optional minimum observed free heap,
and submitted/transmitted/revision/error deltas. Frozen old-frame retransmission
is not success. Unknown outcomes stop without replay or automatic restoration;
success restores captured logical targets, pattern and mode, not arbitrary raw
stream pixels, animation phase or proof of physical light output.

## Persistent device name

Authenticated GET/POST `/api/device/name`; POST form field `hostname=ledboard.local`.
CLI: `PakarController.exe --command device name` / `--command device rename ledboard.local`.
See [device naming](DEVICE-NAME.md) for complete CLI, authentication, recovery,
name validation, connection changes and verification instructions.

## Display detection and LCD presentation

TM1637 remains on AVR A4/DIO and A5/CLK. Fresh status adds
`tm1637_detection_known` and nullable `tm1637_detected`; stale/missing AVR
evidence is Unknown, not a retained positive ACK. LCD uses separate S3 GPIO15/SDA
and GPIO16/SCL, not the same AVR connector. See the
[display wiring and commissioning](ESP32-S3-MIGRATION.md#tm1637-and-optional-i2c-character-lcd).

GET `/api/display/lcd` (also `lcd` in status) returns configured/started,
detection_known/backpack_ack, initialized/available, address,16x2 lines or null,
pending, requested_revision/presented_revision and last_error. A backpack ACK
does not certify LCD glass/contrast. The lcd capability appears only after
initialization and a complete frame. Clients must not synthesize it from an
address/config flag or a cached preview.

POST form uses `action=present`, printable ASCII `line1`/`line2` (≤16 each),
`hold_ms=1..60000` (default5000). A202 queues output; it is not write confirmation.
Read matching revisions with pending=false. `action=release` returns to default
hostname/cell/update text; `action=rescan` clears old detection and starts the
bounded0x27/0x3F probe. Existing authentication, same-origin, current board/boot
headers and maintenance admission apply. Neither operation touches AVR reset.
Raw arbitrary I2C, different backpack mappings/geometries and full client/USB
LCD editing are not advertised by this slice.

## Standalone Internet and Ethernet

Standalone operation requires no PC, USB owner or mobile bridge. Saved station
and cloud configuration are loaded at boot. Outbound MQTT/TLS or WebSocket/TLS
provides commands, status, complete checksummed telemetry and HTTPS firmware
fetch/apply/cancel. Do not port-forward plaintext HTTP/Telnet onto the Internet.
Use per-device broker ACLs, credentials and a trusted TLS endpoint; discovery is
local-network only, not an Internet device directory.

For MQTT/outbound WebSocket, read current board_id, boot_id and utc_epoch from
status, then add these fields to every command except exact status, telemetry
and firmware-status (local REST/USB framing is unchanged):

~~~json
{
  "command": "cells", "ids": "1-80", "state": "red", "effect": "fade",
  "board_id": "<current board_id>", "boot_id": "<current boot_id>",
  "request_id": "<new unique ASCII ID>", "expires_at": 1800000000
}
~~~

expires_at above is an example, not a reusable deadline: set it to current UTC
seconds +30 (maximum +60). IDs are 1..40 ASCII letters/digits/underscore/hyphen.
The same ID cannot dispatch twice through either cloud transport. Expired,
wrong-boot, duplicate, invalid-clock and full-cache requests are rejected with
correlated code/request_id; a duplicate may already have executed. A lost reply
is not permission to retry a mutation automatically. Commands are not queued
across disconnects. Use fresh non-retained command publications. Identity and
deadline checks supplement authentication, not replace it.

Additional cloud management commands use that envelope: cloud-config and
network-config return redacted settings in the reply's JSON message;
configure-cloud and configure-network accept a partial typed settings object
using /api/cloud and /api/network field names. device-identity accepts hostname
and optional display_name. Saves verify before active RAM is replaced; changes
apply after the reply. Internet-side cloud configuration requires TLS for every
enabled service. Secret fields are write-only; omission keeps them, explicit
empty values clear them. Configuration and firmware maintenance cannot overlap.
MQTT client IDs use hardware identity, not the friendly hostname. Topic paths
remain base/hostname: give devices distinct paths on a shared broker and update
their scoped broker ACL when changing either field. Graceful changes publish
offline to the original availability topic before disconnecting; a failed write
closes TCP without suppressing the broker's Last Will.

For ESP-only Internet updates, send firmware-fetch with HTTPS manifest url, its
pinned sha256, targets:1, apply:false; inspect firmware-status, then send
firmware-apply with returned job_id, current boot_id, manifest sha256 and a fresh
envelope. Inactive-slot staging, image checks and post-reboot running-image SHA
verification remain mandatory. An unlatched ESP-only update needs no AVR;
paired updates retain backup/Prog gates. Existing cloud sessions carry staging
progress/cancel; new TLS handshakes and full captures defer during maintenance.
Flash/apply steps can interrupt service; reconnect and read the actual outcome.

ESP32-S3 builds may opt into native W5500 support using -S3Ethernet.
DHCP Ethernet is preferred while IPv4-ready, with station fallback and the AP
kept local. NTP, cloud sockets, API, streaming and downloads use the same native
stack. Route/address changes invalidate old cloud sessions. Uplink/Ethernet
telemetry distinguishes configuration, carrier and IPv4 readiness from actual
cloud connectivity. This does not certify a connected W5500 or WAN.
GET/POST /api/network/ethernet and cloud ethernet-config/configure-ethernet
expose an independent persisted DHCP/static profile using dhcp, address, gateway,
subnet, dns1 and dns2. Changes use the same typed settings object for cloud and
form fields for HTTP. Static DNS is not rewritten by DHCP recovery; changing
Ethernet does not restart Wi-Fi. Dedicated graphical Ethernet editors remain
pending; the API and redacted telemetry are available when compiled in.

Cheap `/api/status` includes `ethernet.configured`, `initialized`, `started`,
`link_up`, `ipv4_ready`, `dhcp`, `ip`, `gateway`, `subnet`, `dns1`, `dns2`, `mac`
and `last_error` when compiled in. Address fields are null while disconnected;
`configured` alone is never a connectivity ACK. Disabled builds report only
`ethernet.configured:false`. Configuration replies expose `apply_pending` and
`apply_error`; persistence acceptance precedes actual link/address readiness.
An explicit Ethernet settings apply can recover a failed initialization without
restarting Wi-Fi. Normal link loss never triggers periodic SPI resets.

The hostname-first `tools/check-ethernet-live.mjs --run --origin http://ledboard.local`
probe verifies the real wired TCP peer and local serving. Optional pinned HTTPS
manifest/image arguments verify ESP-only Internet staging with Wi-Fi station
disconnected, but never apply firmware. See the complete
[Ethernet commissioning procedure](ESP32-S3-COMMISSIONING.md#w5500-ethernet-commissioning)
for credentials, unknown-outcome handling and separate physical acceptance gates.
