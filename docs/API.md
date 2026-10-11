# HTTP and streaming API

Open your configured hostname (`http://ledboard.local/` for this instance;
factory default `http://pakarcontroller.local/`, with IP only as a recovery fallback).
The public sign-in page replaces the unexplained browser Basic-auth prompt.
First-install credentials are `admin` / `pakarcontroller`; change them in
**Device access**. Protected HTTP routes and `/ws` accept a browser session
or explicit Basic authentication for native/API clients. `/`, `/description.xml`
and `/api/v1/auth/info` are public reads; `/login` accepts a form POST.
Local HTTP and Telnet are not encrypted; never expose them directly to the Internet.
The recovery AP Wi-Fi password is independent of the device login.
See `ACCESS.md` for persistence, CLI/TUI, native editor and physical USB recovery.

| Method | Route | Purpose |
|---|---|---|
| GET | `/api/v1/auth/info` | public readiness, username and default-credential warning; never the configured password |
| POST | `/login` | form `username`, `password`; HttpOnly SameSite=Strict session cookie |
| POST | `/logout` | revoke this session and close established sockets |
| GET | `/api/v1/auth` | authenticated username and password-set flag |
| POST | `/api/v1/auth` | form `current_password`, `username`, `password`; persist, revoke sessions and reconnect with new credentials |
| GET | `/update/info` | PU-compatible product/name/model/hash/size/free plus version and actual OTA phase/received byte count |
| GET | `/info` | redacted firmware, heap, reset reason, uptime and protocol diagnostics |
| GET, HEAD | `/firmware/download` | imported PU850 sketch download with Range/ETag/MD5; full-flash dumps disabled |
| GET | `/api/v1/status` | peer-advertised AVR, mode, winner, cell, brightness, and LED state |
| GET | `/api/v1/time` | NTP synchronization state and UTC epoch |
| GET | `/api/v1/wifi/scan` | visible SSIDs, RSSI, and encryption flag |
| POST | `/api/v1/mode` | form `value=idle`, `cell`, `first-wins`, or `snake` |
| POST | `/api/v1/cell` | form `id=1..80`, `state=white|red|off`, `effect=immediate|fade|pulse` |
| POST | `/api/v1/game/reset` | reset the current game |
| POST | `/api/v1/snake/direction` | form `value=up|right|down|left`; opposite turns are rejected |
| POST | `/api/v1/brightness` | form `value=1..255` |
| POST | `/api/v1/effect` | form `value=none|rainbow|fire` |
| POST | `/api/v1/buzzer` | form `frequency=20..12000`, `duration_ms=1..5000` |
| POST | `/api/v1/audio` | form `cue` from status `audio_cues`; adds `welcome-puzzleboard` and `welcome-40led` on AVR >=0.2.5; 202 means queued, not playback completion |
| POST | `/api/v1/display` | `value=-999..9999`, scheduled `text`, or `release=true`; see below |
| POST | `/api/v1/outputs` | form `mask=0..255` for direct 74HC595 diagnostics |
| GET | `/api/v1/network` | AP/station state without returning credentials |
| POST | `/api/v1/network` | form `station_enabled`, `ap_enabled`, `ssid`, `password`; persist and apply AP+station configuration |
| GET | `/api/v1/cloud` | redacted MQTT and outbound-WebSocket configuration plus live connection state |
| POST | `/api/v1/cloud` | persist and apply MQTT/WebSocket endpoints, TLS flags, credentials/tokens, base topic, and telemetry interval |
| POST | `/api/v1/cloud/reconnect` | disconnect and reapply both outbound cloud clients |
| GET | `/api/v1/avr/probe` | read and validate ATmega328P signature through ISP |
| POST | `/api/v1/avr/backup` | store flash, EEPROM, fuses, and lock byte in LittleFS |
| GET | `/api/v1/avr/backup/flash` | download the last 32 KiB raw flash backup |
| GET | `/api/v1/avr/backup/eeprom` | download the last 1 KiB EEPROM backup |
| POST | `/api/v1/avr/image` | multipart upload of one complete 32768-byte AVR+Urboot raw image |
| POST | `/api/v1/avr/program` | backup, erase, program, and byte-verify the staged image |
| GET, POST | `/update` | authenticated async ESP self-OTA form/upload; optional file `MD5` preceding the firmware file |

UDP port 7890 accepts bounded `PKR1` chunks **without authentication**;
use only on a trusted LAN. The authenticated WebSocket is preferred.
Each datagram contains the four
ASCII magic bytes, two reserved/version bytes, little-endian pixel offset,
one pixel count, one flags byte, then RGB triples. Bit zero of flags commits
the staged frame. A packet is rejected unless its exact size and pixel range
are valid.

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
`{"type":"ping"}`, and accepts the same bounded binary `PKR1` chunks as UDP.
This is the supported browser and desktop streaming path; multiple read/control
clients may remain connected concurrently.

The outbound MQTT client uses MQTT 3.1.1 and validates TLS with ISRG Root X1.
It publishes retained `<base>/<hostname>/status` and
`<base>/<hostname>/availability`, publishes command acknowledgements to
`event`, and subscribes to `command`. The outbound WebSocket client supports
configurable host, port, path, TLS, and bearer token; it sends the same status
JSON and accepts the same JSON command objects as MQTT. Supported commands are
`status`, `mode`, `cell`, `brightness`, `effect`, `outputs`,
`display`, `buzzer`, `audio`, `snake-direction`, and `reset`. Secret values are write-only through
the API; blank omitted password/token fields preserve the stored value.
Cloud `display` also accepts the same text scheduling fields above or
`{"command":"display","release":true}`. An omitted repeat means one-shot.

The password-authenticated Telnet shell on TCP 23 exposes `status`, `mode`,
`cell`, `brightness`, `effect`, `display`, `audio`, and `reset`. It uses the
same validation and state transitions as cloud JSON commands. The password is
the HTTP development password; the service is advertised through mDNS as
`_telnet._tcp`. USB serial remains a framed machine interface rather than a
text shell, so Telnet cannot interfere with the simultaneous desktop link.
Telnet supports `display stop` and
`display text <once|loop|interval> <speed_ms> <hold_ms> <pause_seconds> <scroll 0|1> <TEXT>`.

The NodeMCU USB UART is a simultaneous 115200-baud PC endpoint using the same
COBS, CRC-8/ATM, sequence, opcode, and bounded-payload envelope as the board
link. It exposes hello/status, modes, cell actions, reset, brightness, effect,
shift outputs, buzzer, audio cues, and TM1637 display commands. The HTTP status
record includes separate USB byte, frame, and error counters.

`/api/v1/avr/program` also provides the audible start/success boundary and
durable `Prog` safety latch. Repeating the request while the latch is armed
resumes programming without replacing the preserved previous-good backup;
cloud reconnect work remains paused until verification clears the latch. The
status record exposes `programming_latch`,
`tm1637_ack`, AP and station addresses, station association, NTP state,
WebSocket/Telnet client state, link mode, and separate ESP, AVR, and USB UART counters.
Passwords are never returned.
## Persistent device name (firmware 0.2.3)

Authenticated GET/POST `/api/v1/device/name`; POST form field `hostname=ledboard.local`.
CLI: `PakarController.exe --command device name` / `--command device rename ledboard.local`.
See [device naming](DEVICE-NAME.md) for complete CLI, authentication, recovery,
name validation, connection changes and verification instructions.
