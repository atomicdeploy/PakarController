# Bidirectional control

One living, unversioned API carries commands, correlated results, state and
events. WebSocket and MQTT are bidirectional persistent connections. HTTP uses
requests plus cursor-based event reads; HTTP itself is not a full-duplex socket.
Discovery is not authorization. All mutations retain capability, supported-release,
board/boot, deadline and no-replay checks.

## Routes and ownership

| Endpoint | Commands | Observations |
|---|---|---|
| ESP LAN/AP/Ethernet HTTP and `/ws` | Existing `/api/command` packet or typed HTTP routes | Status, typed events, update progress; active diagnostics return actual published pixels |
| ESP configured MQTT broker | `<base>/<hostname>/command` | `/event`, `/status`, `/telemetry/part`, `/availability` on the same prefix |
| ESP configured outbound WebSocket | Cloud command JSON | Hello, correlated results, status, telemetry and board events |
| Internet relay HTTP and `/ws` | Same client packet, translated once to the MQTT deadline | Post-ACK same-boot status, typed board events and verified full telemetry |
| Native selected-owner bridge | `/api/command` via HTTP or `/ws`; USB remains the exclusive serial owner | Immediate admitted panel/segment/action events, status and `/api/events` |
| Android advanced bridge | Explicitly enabled foreground HTTP `/api/request`, local/outbound WS or MQTT `/command` | Immediate sanitized telemetry, `/reply` results and `/api/events` |

Native/mobile normal control uses WS first and configured same-intent HTTP
recovery. Native does not open a second direct broker connection: the Internet
relay translates MQTT for its existing HTTP/WS client. Android's separate advanced
bridge already has direct MQTT and outbound WS options; it is not an unattended
Android background service. These selected-owner bridge boundaries are deliberate,
not implicit USB-to-Wi-Fi fallbacks or public unauthenticated port forwards.

## Client command packet

Read fresh status first. Use the advertised board/boot and command clock, not the
PC uptime. Send this object over `/ws`, or form field `packet` to `/api/command`:

```json
{"type":"command","command":"cells","request_id":"unique-intent-id","board_id":"from-status","boot_id":"from-status","expires_at_ms":123456,"ids":"1,15,80","state":"red","effect":"immediate"}
```

Set `expires_at_ms` to a short deadline after the observed `command_clock_ms`.
The relay subtracts snapshot age before converting that deadline to UTC; it never
extends a delayed intent. Its JSON HTTP body is also accepted. New WS commands,
their HTTP recovery, and other clients share the same bounded request-ID cache.
An identical retry returns the existing outcome; changed content under the same
ID is rejected. A lost result is **unknown**, never permission to resend with a
new ID. Board restart retires the old boot's jobs.

`command-result` includes request, board and boot IDs, `accepted`, and error on
rejection. Accepted relay results include fresh same-boot `status`. Raw MQTT ACKs
are not prematurely forwarded as successful WS results without that readback.
An ACK confirms handler admission, not optical LED or physical peripheral success.
Pixel readback retains its actual `pending` flag; no asked-for colors are fabricated.

The Internet relay supports cells, assignments/lock, effects, brightness, LED
output/diagnostics, game mode/reset, outputs, tones/melodies and display commands
subject to the board's capabilities. It does not falsely advertise opaque binary
streaming, ISP or other HTTP routes that have no broker implementation.

## MQTT / outbound board WebSocket

Cloud mutations use the same intent/identity concepts but UTC seconds because a
message can be delayed in an external broker:

```json
{"command":"cells","request_id":"unique-intent-id","board_id":"from-live-status","boot_id":"from-live-status","expires_at":1790000000,"client_version":"0.2.8","ids":"1,15,80","state":"red","effect":"immediate"}
```

Replace expiry with current UTC + a short lifetime (maximum60 seconds) and use the
actual app release, not this illustrative value. Require a synchronized board
clock. Subscribe **before** publishing; match result request/board/boot identity.
Use QoS0, `retain:false`, no offline command queue, and scoped TLS broker ACLs.
Status/availability may be retained but do not prove current board liveness;
request a fresh read-only `status` or `telemetry` observation after reconnect.
Fragmented full telemetry is accepted only after complete SHA-256 verification.

The installed PubSubClient callback does not expose MQTT RETAIN. The board cannot
claim it rejects retained commands at that layer: broker policy must prohibit
retained `/command` publications. Boot/deadline/replay checks are additional
defences, not substitutes for that broker restriction. Subscription/online writes
are socket-write evidence, not SUBACK/PUBACK. Remote TLS trust checks remain on.

## HTTP events and integrations

Native and Internet relay: `GET /api/events?after=<opaque-cursor>` returns
`{cursor,gap,events:[{cursor,event}]}`. Keep the returned cursor. On a gap, fetch
a new status; never replay commands from event history. Journals hold64 entries,
filter the active board/boot, and are not durable queues.

Android: get `bridge_session` from its authenticated status; call
`/api/events?bridge_session=<session>&after=<numeric-cursor>`. Entries are the same
identity-bound objects emitted live on WS/MQTT, with numeric `cursor`. Commands
retain its existing `id`, `bridge_session`, board/boot, `expires_at_ms` wall-clock,
`type:command`, `path`, `values` and client-version envelope. Result `request_id`
aliases that one `id`, not another dispatch identifier. Foreground retirement
invalidates the session; reflected observations cannot form command loops.

n8n, Telegram and Bale should use these existing endpoints/topics: observe live
status, authorize the requested action, allocate one intent ID, send once, await
the correlated result and readback, then report confirmed/unknown honestly. No
bot token, webhook, automation or bot implementation is included in this change.

## Verification and deployment boundary

Focused tests cover real loopback HTTP/WS sockets, concurrent events during slow
ACKs, duplicate HTTP recovery, peer changes, malformed receipts, bounded history,
secret filtering and foreground session retirement. A separate opt-in real-broker
fixture uses `PAKAR_TEST_MQTT_URL=mqtt://127.0.0.1:<port>` and **only a simulated
board** on isolated topics. It passed through an SSH tunnel to a temporary,
loopback-only Mosquitto2.0.22 on the office host; production services/topics were
unchanged and that owned broker expired automatically.

Read-only live production observations confirmed the same ledboard board/boot via
David-ASUS LAN HTTP and office MQTT/TLS. Its old cloud receipt lacks new identity
fields: deploy the board and relay together only after production approval. No
firmware flash, reboot, LED command or production relay replacement occurred.
Firmware/client builds and transport fixtures do not close production hardware,
LAN/Internet soak, installed Android or physical output acceptance in issue#2.

Focused checkpoint results:22 native bridge tests,9 Flutter bridge/admission tests
(analyzer clean),16 Internet relay tests including the real broker, both ESP32
and ESP8266 CloudServices host fixtures, WebAccess ingress/lifecycle fixtures and
S3 contract guards passed. The existing CI entrypoint imports the new relay suite.
The prior CI compile regression is repaired locally; asynchronous CI is not yet
claimed green. Eight pre-existing native compiler warnings remain.

S3 compile:16MB/OPI, buffered UART, table-selection, rmt-dma, verified ISP flag,
BLE disabled, optional Ethernet. Image2,290,283 bytes; global RAM124,052 bytes.
Candidate SHA-256: `fa7571e25215ab93032411a3138dfbadd19d111aac1b287ebd06b643e16c897f`.
This is a build artifact, not the image observed on the production MCU.
