# HTTP and streaming API

Host mini-apps use [host-owned presentation](HOST-PRESENTATION.md) for logical or
physical group colors/effects, durable layers, expiring overlays and atomic
frame fallback through the same HTTP, WebSocket, MQTT and USB command contract.

ESP32-S3 also exposes the same living command/state contract through a selected
Bluetooth LE control transport. See [Bluetooth LE](BLUETOOTH.md) for its GATT
attributes, exact advertised routes, ownership rules and explicit binary limits.

Optional ESP32-S3 [native SSH and read-only SCP](SSH.md) reuse the device shell
and public file service. Build flags include them; the persistent listener stays
disabled until explicitly enabled. `/api/ssh` reports configuration, actual
listener state, host fingerprint and supported transfer directions.

Client self-updates, enrollment, remote operational telemetry and bounded client
management use a separate configurable unversioned service. See
[Client management](CLIENT-MANAGEMENT.md) for its routes, controls and live
acceptance boundaries; these are not ESP/AVR firmware commands. Board auth is
unchanged. Remote administrative client management requires its own enrollment.

All interfaces use [one living, unversioned contract](API-POLICY.md).
Numbered API namespaces are rejected with 404, never redirected or aliased.

Open your configured hostname (`http://ledboard.local/` for this instance;
factory default `http://pakarcontroller.local/`, with IP only as a recovery fallback).
Authentication is disabled by default. Web and native clients read `enabled`
from `/api/auth/info` and do not ask for credentials when it is false. Enable
**Require authentication** in Device access to protect HTTP routes and `/ws`
with browser sessions or explicit Basic credentials. `/`, `/description.xml`
and `/api/auth/info` are public reads; `/login` accepts a form POST.
Discovery clients can also GET/HEAD `/board-icon.png` without signing in;
`/description.xml` advertises the matching table/key PNG and presentation URL.
See [UPnP discovery](UPNP.md) for metadata, interface behavior and verification.
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

## Application firmware inbox, file delivery and observed progress

Native: Device settings → Firmware & storage. Android: device context menu →
Firmware. Both support a selected local `.bin` and a direct HTTP(S) binary URL
with mandatory lowercase file SHA-256. Downloads use a separate client, never
the board's credentials or identity headers; redirects/oversized/encoded or
hash-mismatched files are refused before upload. The bounded image stays in RAM,
not a new disk cache. ESP uploads use the existing `/update`; complete AVR
images use `/api/avr/image` and remain stage-only until explicit programming.
Pinned manifests retain the existing stage → exact-job Apply workflow.

The advanced remote firmware subscription supports WebSocket and genuine
Socket.IO over WebSocket (Engine.IO `EIO=4`, namespace CONNECT and heartbeat).
Configure an explicit WSS URL, protocol, exact event, namespace and trusted
HTTPS download origin. The optional Bearer is memory-only; Socket.IO also sends
it in CONNECT auth. Plain WS is loopback-test only. Start is explicit, not app
startup; Stop, Android backgrounding and board/boot/connection retirement revoke
the listener. A disconnected listener is restarted explicitly, never a queue of
offline firmware commands. Native saves public settings only after valid Start.

The subscription carries `type:"subscribe"`, `events:["firmware-offer"]`, a new
`session`, and actual `board_id`/`boot_id`. Socket.IO wraps it in the `subscribe`
event. Publish a JSON WebSocket message with the configured `type`, or emit the
configured Socket.IO event with this data:

```json
{
  "type": "firmware-offer", "id": "unique-offer-id",
  "session": "<issued listener session>",
  "board_id": "<selected board>", "boot_id": "<selected boot>",
  "expires_at_ms": 1800000000000,
  "kind": "binary", "target": "esp",
  "url": "https://updates.example/esp.bin", "sha256": "<file SHA-256>"
}
```

The timestamp is illustrative: use current UTC milliseconds +30 seconds,
maximum +60. IDs are 1–64 ASCII letters/digits/underscore/hyphen. Duplicate IDs,
wrong session/peer/boot/origin, expired offers and full inbox/session budgets
cannot dispatch another upload. Binary delivery rechecks expiry and actual
board ownership after download, immediately before upload. No board credentials
are forwarded to the source. `kind:"manifest"` uses the pinned manifest hash;
`target` is `esp`, `avr` or manifest-only `all`. Socket binary attachments,
arbitrary RPC and polling transport are not advertised by this firmware inbox.

Default is review-before-delivery. Explicitly arming delivery permits fresh
binary ESP offers to upload/reboot that selected board. AVR binaries and
manifests stage only; no unattended AVR erase or paired Apply is introduced.
Hardware firmware delivery currently requires the selected network update
route. USB-only control does not silently open Wi-Fi; USB snapshots still carry
the same board update observations. Direct USB firmware forwarding remains open.

Rust, Android and embedded Web show `update` (local HTTP OTA) and
`remote_firmware` (board-initiated/pinned updates), including changes initiated
by another client or cloud source. These are consumed from normal peer status:
WS first, bounded existing read fallback; no redundant per-tick job refresh.
App download/upload byte counters describe that transfer stage, not flashing.
Verification/reboot/programming without a measured denominator is indeterminate;
an accepted request or 100% transfer is never verified update completion.
Native CLI-triggered uploads share the tracked file uploader; TUI shows observed
board update stages. Flutter's ordinary table connection/state is not reset by
the firmware listener. Actual phone/endpoint/power-loss acceptance is separate.

## AVR programmer paths

The integrated S3 advertises the compiled methods under
`avr_maintenance.methods`. ISP and UART Urboot share one exclusive, loop-owned
maintenance job, not separate programmer firmware or a second serial reader.
Urboot programs the AVR application, not the ESP. ESP self-update remains OTA.

Read `GET /api/avr/maintenance`. To start, POST form fields `method=isp|urboot`,
`operation=probe|backup|program`, a unique ASCII `request_id` (1–40 letters,
digits, underscore/hyphen) and `expires_at_ms` from the current board's
`command_clock_ms + 55000`. Both `X-Pakar-Board` and `X-Pakar-Boot` are
mandatory. Program also requires the exact complete staged 32768-byte image's
lowercase `sha256`. Upload it using the existing `/api/avr/image` first.
Admission is HTTP202, not completion. Poll this same job or observe the normal
status/WebSocket/USB `avr_maintenance` object. A consumed ID never writes again;
64 accepted receipts per ESP boot is a finite session budget, not flash wear
rotation or automatic retries. A reboot changes the required boot pin.

Program first validates source/hash/vector layout and obtains the running AVR's
durable Prog ACK. As with the existing ISP coordinator, its programming recovery
capture includes the armed Prog record; normal settings are preserved. Urboot
backs up and verifies all flash/EEPROM through UART before page writes, checks
that the candidate's entire384-byte boot region matches the current bootloader,
rechecks the source digest, writes complete128-byte application pages (page0
last), then reads all32768 flash and1024 EEPROM bytes back before returning to
the application and releasing Prog once. The flash-program operation never
issues chip erase, EEPROM write, fuse write or a boot-region write. Only the reviewed stock MiniCore
ATmega328P profile is accepted; custom/larger bootloaders require explicit ISP
review. Raw UART restores38400baud and a fresh COBS parser on every exit.

ISP uses the existing full recovery/EEPROM-preserving erase/program/readback
driver. Its cooperative callback allows read-only progress requests while reset
is asserted. No measured byte counter is invented for its indeterminate
`backup_isp`/`program_isp` stages. ISP may replace the bootloader; no method
silently writes fuses. Exact same-board/version/profile recovery must follow
flash/EEPROM verification before terminal success. Failed writes never retry
or switch methods. `write_attempted`, `programmed`, `verified` and
`application_recovered` distinguish uncertainty and the separate gates.

ISP captures retain the existing backup downloads. UART captures additionally
use `GET /file/download?artifact=urboot-flash` or `urboot-eeprom`, with the
existing finite read lease/Range implementation. Hardware update operations
require the verified S3 reset/ISP profile; SoftwareSerial is not advertised as
a reliable Urboot uploader. Dedicated native/mobile method-selection controls,
UART-dead interrupted-write recovery and power-loss acceptance remain separate;
the API and shared progress do not certify them.

Explicit EEPROM provisioning uses the same queued maintenance endpoint with
`method=urboot`, `operation=eeprom-write` or `eeprom-initialize`, matching
`confirm`, a complete 1024-byte `data_hex` (2048 hex characters), and its lowercase
SHA256. Initialization uses the caller's explicit image; it never invents factory
settings or erases automatically. Existing board/boot, request ID, deadline and
maintenance ownership checks apply. The bootloader must advertise the reviewed
EEPROM capability. Full flash/EEPROM backups precede writes; unchanged chunks
are skipped and changed chunks verified. No retry follows an ambiguous write.
Download these per-request backups using `artifact=urboot-flash|urboot-eeprom`
and `request_id` on `/file/download`. Retain the request ID across ESP reboots.

An ESP32-S3 build may include `-AvrRecoveryImage <complete-32768-byte-bin>`.
`GET /api/avr/recovery-image` reports availability/hash. POST the same endpoint
with board/boot headers, `sha256` and `confirm=STAGE AVR RECOVERY` to stage it
without touching AVR. Then use the existing explicitly confirmed ISP recovery
workflow. No boot-time auto-flash or fuse guessing occurs. A virgin MCU whose
clock/fuses differ from the reviewed board profile requires separate provisioning.

Cell export at `GET /api/cells/assignments` uses semantic `idle/lost/won/off`,
not rendered RGB. Import at POST accepts an `assignments` array and requests an
immediate scene checkpoint; `persisted` and `persistence_pending` describe storage
separately from accepted in-memory state. Ordinary edits retain coalesced saving.

## LED output implementation capability

Peers advertising `leds` additionally report optional
`capabilities.led_output = {"backend":"gpio-static","rmt":false,"dma":false}`.
The S3 RMT-DMA build reports `backend:"rmt-dma", rmt:true, dma:true`;
ESP8266 reports `backend:"fastled", rmt:false, dma:false`. This describes the
selected wire implementation, not the independent `static`/`effects` renderer
mode and not whether output is enabled, initialized or transmitting. Actual
readiness/activity/faults remain under `led_output.driver`.

The same manifest travels in HTTP, WebSocket, MQTT telemetry and full USB table
snapshots, and is preserved through native bridge serialization. Rust, Flutter
and Web show its explicit driver label only when valid and LED support is ACKed;
old/malformed/unknown optional metadata is unknown, never inferred false from a
missing field or the board's CPU model.

S3 runtime-capable images additionally advertise `available_backends` and
`runtime_switch`. GPIO and RMT/DMA are independently included with
`PAKAR_S3_LED_GPIO_ENABLED` / `PAKAR_S3_LED_RMT_ENABLED`; both default to zero.
The canonical dual-driver build passes `-S3LedGpio -S3LedRmtDma` and defaults
to GPIO when no saved choice exists. A no-driver image never owns the LED pin.

Select with `POST /api/led/output` (`backend=gpio-static` or `backend=rmt-dma`),
WebSocket/MQTT command `{"command":"led-output","backend":"rmt-dma"}`, or
USB `0x5c [3,1]` / `[3,2]`. General USB API requests expose the same GET/POST
route. Backend selection cannot be combined with renderer/output mutations.
The accepted receipt confirms durable intent, not physical output. Observe
`led_output.driver.requested_backend`, `backend`, `switch_pending`,
`switch_error`, `backend_revision`, and `ready`, alongside top-level
`led_output.backend_saved` / `backend_settings_error`. A switch is applied only
after complete-frame resend, including the latch interval; failures remain
visible even when a safe GPIO fallback is active. No mutation is automatically
replayed after uncertain delivery. Preferences use a separately validated,
atomic LittleFS record; scenes, mapping and the effects engine remain shared.

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
| POST | `/api/led/presentation` | partial `settings` JSON, e.g. `{"fire_rotation":180}`; table rotation accepts0,90,180,270 degrees, saved per board and returned in `state_presentation` |
| POST | `/api/scene/save` | explicitly flush the current semantic scene;200 `saved:true` confirms verified commit,503 requires inspection of status, never an automatic replay |
| POST | `/api/buzzer` | form `frequency=20..12000`, `duration_ms=1..5000` |
| POST | `/api/audio` | form `cue` from peer-acknowledged `audio_cues`, including `attention` and donor welcome variations; 202 means queued, not playback completion |

Fire is an explicit global animation in both static-grouped and effects output
modes. Selecting `fire` again restarts from a cold/black frame. On table builds
its two-dimensional heat field follows the staggered logical layout, then the
saved logical-to-physical mapping, custom group lengths and pixel corrections.
`fire_rotation` rotates the flame direction (0° up,90° right,180° down,270° left),
not cell identities. This preference lives in `/presentation.json`; existing
palettes default to0°. Changing direction restarts Fire only if it is active;
reading status never restarts it. Group colors/states and correction tables are
not rewritten. The same `state-presentation` command/settings object is available
through the existing WebSocket, MQTT and encapsulated USB API command paths.
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
(within the board's used pixel range), `count` (1 or one complete physical group), integer
`rgb` (0..16777215) and boolean `off`. It validates before mutation, respects
another stream's staging lease, clears the full480-pixel frame, enables raw
effects output and publishes exactly the requested physical pixel/group with no
fade, logical remapping or AVR command. Diagnostic pixel bytes are not saved;
output-mode/enable changes retain their normal coalesced scene policy. `off:true` publishes
all480 black and leaves the output service enabled. The correlated reply carries
`status.pixel_frame` with the actual full RGB buffer. Its request ID provides
the same deduplicated WS-to-HTTP receipt recovery as cell commands; an uncertain
outcome never becomes a fresh write. This is buffer publication, not optical
proof. Flutter's internal path selector `/api/led/diagnostic` names this command;
it is not a separate REST endpoint.

### Per-group pixel counts

`pixel-group-layout` advertises runtime physical-group sizing. `GET /api/pixels/groups`
returns `revision`, `default_count`, `group_count`, `capacity`, `used_pixels`,
zero-based `starts`, effective `counts`, `overrides` and `index_base:0`.
Physical group IDs in overrides are one-based. Logical cell renaming/remapping
does not move these overrides: the cable topology is independent of cell names.
Table builds default to 80 groups of 6 (480 total capacity); keycontroller builds
default to 18 groups of 12 (216 total capacity, two groups per box).

POST the complete replacement list as form field `configuration`:

```json
{"revision":0,"overrides":[{"group":1,"count":5},{"group":2,"count":7}]}
```

Omitted groups or count zero inherit the default. Effective counts are 1–255,
the cumulative sum cannot exceed the compiled capacity, and duplicate group IDs,
stale revisions or active maintenance/preview ownership reject the whole change.
The independent checksummed file uses staged readback and atomic rename; ordinary
scene states and logical mapping are not reset. Any unused tail is sent black.

Diagnostics on these peers include `group_layout_revision` from the current
layout; a custom layout requires it. A full-group probe must use its exact
`starts[group-1]` and `counts[group-1]`, not fixed multiples of six or twelve.
Published RGB frames carry capture-time `group_layout_revision`, `group_counts`
and `group_starts`; clients must not relabel an old frame using a new topology.
USB uses the same guarded API via its generic API command, with no TCP fallback.
Compact status may mark `pixel_groups.partial:true` and omit redundant starts
and overrides; derive starts only from validated counts and fetch the full group
resource before editing. An oversized USB snapshot explicitly omits RGB with
`pixel_frame_partial:true` / `rgb_omitted:true`; on-demand `/api/pixels/frame`
provides the complete captured frame without increasing the bounded snapshot.

Normal selection edits are not streamed RGB frames. `/api/cells` validates the
complete selection before changing anything, stages all targets together and
returns `202 {"accepted":true,"count":N}`. Duplicate IDs count once; malformed,
empty or out-of-range selections are rejected without a partial edit. Fade and
pulse timing is generated on the ESP from one common start time (300ms), not by
client timers. Static grouped mode deliberately applies immediately. This scene
edit does not forward80 commands to AVR. The loop coalesces semantic scene edits
into persistent storage rather than writing once per cell or animation frame.

The ESP remembers all80 logical white/red/off states, mode/effect, brightness,
output On/Off and static/effects selection across reboot. Its initial no-record
fallback remains white. The old brightness-only record is read-only migration;
validated scenes and physical mapping restore before the first LED publication.
Animation phases, Snake progress, raw streaming frames, diagnostic pixel bytes,
client ownership and pending commands are not saved or replayed.

`/api/status.state_persistence` reports `restored`, `pending`, `saved`,
`writes` (this boot), `save_delay_ms` and nullable `error`. Edits normally save after1500ms
quiet, with a10s maximum coalescing delay; active maintenance defers writes.
Wait for `saved:true` before removing power, or explicitly flush with current
board/boot headers. Staging/readback/rename failures keep the previous committed
record and retry no faster than10s. LittleFS atomic replacement and checksum
fixtures are not a claim of physical abrupt-power-loss or optical acceptance.

Native, Flutter and embedded Web remember passive per-board workspace choices.
They never restore cached cells/modes as a new command; the freshly connected
board remains authoritative. Secret edits and uncertain writes are excluded.

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

The Telnet shell (default TCP 23) exposes `status`, `mode`,
`cell`, `cells`, `brightness`, `effect`, `display`, `audio`, and `reset`. It uses the
same validation and state transitions as cloud JSON commands. Authentication
follows the saved access policy; when enabled, the shell requires its password.
The service is advertised through mDNS as `_telnet._tcp`. USB provides the same
text shell encapsulated in a boot-bound COBS channel, alongside the normal
machine protocol. It never switches the AVR UART into raw text mode.
See [terminal commands and TUI](TERMINAL-UI.md) and `tools/device-shell.ps1`.
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

## ESP32-S3 onboard status RGB

`GET /api/status-rgb` returns the selected board/boot, persisted `enabled`,
`pin`, `pin_confirmed` and `brightness`. POST accepts only
those four configuration fields as form strings; the same shared owner handles
Bluetooth RPC. Allowed GPIOs are38/48, independent from the matrix on GPIO4.
Enabling requires an explicit confirmed pin. USB-only editing is not silently
rerouted through Wi-Fi. An unidentified pixel stays unavailable.

`status.status_rgb` and compact WebSocket `type:"status-rgb"` packets report
actual already-scaled RGB, semantic state, effect, nullable measured progress,
revision and output timestamp. Compact packets carry board/boot identity and do
not renew full-device freshness or command-clock authority. Native, Web and
Flutter reject foreign/old-boot or non-increasing revisions and never apply
brightness twice or reconstruct their own animation. Successful driver
submission is not optical verification. Generic custom profiles/cues remain
pending rather than being advertised as supported.

Lifecycle priority is fault, update, startup, station connection, required AVR
loss, ready, AP-only, warning. Disabled AVR builds do not manufacture peer loss.
Initial Off and disable/reassignment submit black to the confirmed pin. The
writer exposes real bounded RMT completion, not unconditional success from a
void SDK helper.

## ESP peer relay (bounded LAN candidate)

`GET /api/peers` reports the actually implemented transports and limits. S3
network builds advertise `peer-relay-http`; offline builds do not. This is an
application relay, not NAT, a Wi-Fi extender or a completed mesh.

`POST /api/peers/request` accepts JSON: `request_id` (32 lowercase hex digits),
`hostname`, destination `board_id`, `boot_id`, `method`, `path`, and optional
`packet`, and an optional private IPv4 `address` hint. Only approved read paths and `POST /api/command` are forwarded. For
public `GET /api/peer/info` only, boot_id may be empty to learn the current boot
of an explicitly identified board. Other calls require that observed boot.
Commands carry the original JSON packet string with the same request ID,
destination identity and expiry; no new command is manufactured on timeout.

`GET /api/peers/result?request_id=...` returns queued/complete/failed transport
status and a verified downstream JSON string. `response_verified` is NOT an
actuation ACK: inspect the downstream `accepted` and hardware result. WebSocket
clients receive `peer-relay-result` completion notifications. USB maintenance
API envelopes can use these same request/result routes. Large downstream JSON
may exceed the existing USB API response limit; no binary USB relay is claimed.

One background job and one hop are allowed. One lazily allocated worker is
reused; eight receipt fingerprints retain SHA256, not full request bodies.
Unexpired IDs cannot be evicted; retired results are never resubmitted
automatically. `.local` uses bounded mDNS directly (900 ms), not router DNS;
unicast resolution has the same caller deadline and safe late-callback storage.
A destination's identity-verified address is reused until connect failure,
then bounded hostname recovery is attempted. Host headers retain the configured
hostname. An operator-supplied address is only a dial hint: the downstream
identity must be verified before any command. After a cached connection fails,
a different supplied hint can recover a DHCP change even when mDNS is unavailable;
it receives a fresh identity probe before the requested read or command.
Cached same-boot GETs need one
HTTP read, not a redundant identity preflight; POST commands still preflight.
Identity/auth/HTTP errors never redirect a command and POST is never replayed.
No credentials are forwarded; authenticated
destinations reject access normally. This LAN HTTP identity check does not
provide cryptographic peer authentication. Raw files, firmware writes, general
URLs and recursive relay endpoints are not admitted.

`GET /api/peers` includes saved/observed routes, verification age/boot, current
internal free/largest-block memory and the separately labeled historical minimum.
Old low-water marks are not attributed to a cause or presented as current failure.
Explicit `POST /api/peers/routes` JSON `{ "action": "save", "board_id": "...",
"hostname": "keycontroller.local" }` saves only a route verified this boot.
`action: "remove"` removes that saved route and its RAM hint. Four routes fit in
`/peer-routes.json`; stage/readback/rename publishes changes, and identical saves
don't write flash. No observed/discovered address is persisted automatically.
Saved addresses are reverified after reboot; boot IDs are never persisted as
authority. These routes also work through the fenced USB maintenance API.
No settings/scene/correction values are rewritten by a route change.

Result receipts include `elapsed_ms`, `resolution_ms`, `http_ms`, `route_source`,
`address`, and `command_dispatched`. These measure the relay job, not optical
output or the separate observer/admission/polling latency. The read-only probe
`node tools/check-peer-relay.mjs --origin http://ledcontroller.local
--target-hostname keycontroller.local --target-board TARGET_BOARD_ID
--target-address 192.168.1.201 --samples 3` records both timing scopes without
LED commands or route persistence. Use the currently observed target address,
not the example address blindly.

Pending: automatic discovery, continuous peer events and a fully selectable
relayed device/transport in Flutter/Rust/Web. The endpoint deliberately keeps
`discovery` and `event_subscription` false until implemented. Also pending:
BLE-central/ESP-NOW/USB-host adapters, remote TLS peers, and resumable verified
file/firmware transfer. Existing direct OTA remains the update owner.
