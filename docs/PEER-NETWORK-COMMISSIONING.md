# Detached peer network commissioning

ESP32-S3 network builds can explicitly commission another PakarController AP
without keeping the initiating client online. This is a bounded provisioning
transaction, not an unrestricted Wi-Fi repeater or a firmware transfer relay.

`POST /api/peers/network` accepts a JSON job; `GET /api/peers/network` reads its
receipt. The existing authenticated, peer-fenced command dispatcher also accepts
`peer-network-provision` with the same job nested in `settings`, and the read-only
`peer-network-status` command (optional `job_request_id`). MQTT mutations retain
their normal outer board/boot/request/deadline/release envelope.

Once both boards share a LAN, `peer-directory`, `peer-relay` (nested `request`)
and `peer-relay-result` (optional `job_request_id`) expose the SAME restricted
HTTP peer relay over MQTT/remote WebSocket. They do not add unrestricted URLs,
recursive hops, credential forwarding or mutation retries. An inner target
command retains its own board/boot/command-clock deadline and request ID.
Outbound cloud receipts are bounded32KiB, status remains16KiB, and incoming
cloud commands remain4KiB. Oversized replies fail rather than truncating JSON.

Job fields:

| Field | Meaning |
| --- | --- |
| `request_id` | Unique 32-character hexadecimal job ID; never reuse |
| `confirm` | Exact value `configure-peer-network` |
| `board_id`, `hostname`, `role` | Expected target identity, bare hostname, baked-in role |
| `ap_ssid`, `ap_password`, `ap_bssid` | Explicit target AP credentials and exact BSSID |
| `station_ssid`, `station_password` | Target infrastructure credentials |

The initiating board reserves maintenance ownership and persists admission before
responding. Its loop then suspends cloud sockets and discovery before starting
the worker. Its own AP temporarily stops to avoid the two default192.168.4.1
interfaces colliding. The station join never changes its saved network settings.
The worker verifies the peer's board/boot/hostname/role/release and HTTP identity
headers, snapshots redacted network policy, records dispatch intent, and sends
ONE station+AP+DHCP configuration POST. Neither authentication rejection nor an
uncertain response triggers a second POST. An uncertain job must be inspected.

After success or failure it reapplies the initiating board's untouched saved
network. AP association is bounded20s and source reconnection observation30s;
individual HTTP dial/body operations are bounded. `settings_verified` means the
target's persisted-policy readback matched, not that its infrastructure association
has been observed. Verify the target on the infrastructure network separately.

The private `/peer-commission.json` receipt contains no passwords/request body.
General file APIs expose only `/user/`, not this journal or its pending file.
Reboot starts normal saved networking and marks a pending job interrupted; it
NEVER replays provisioning. A durable16-ID budget rejects retired IDs rather
than silently evicting replay protection. Receipt storage failure fails closed.

The worker retains an8KiB internal stack after first use, like the existing peer
relay. Admission requires internal heap headroom. This capability must not be
declared deployed merely because its source/fixtures or SDK build pass.
