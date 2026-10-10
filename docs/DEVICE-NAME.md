# Device naming

The project remains **PakarController**. This board's intended network name is
**ledboard.local**, stored as the DNS label `ledboard`. There are no chip IDs,
dates or random suffixes in its hostname.

Firmware 0.2.3 adds persistent naming; older firmware has no rename route and
must be updated first. Naming changes neither station/login passwords nor AVR
flash, EEPROM or backups. The station DHCP hostname, recovery AP SSID, mDNS,
SSDP friendly name/presentation URL and MQTT client/topic identity follow the
new label. SSDP's hardware UUID stays unchanged. Outbound WebSocket reconnects
and introduces the new device name. The old MQTT availability topic is marked
offline if connected. Broker ACLs must allow the new name's topic paths.

## CLI / TUI (Windows)

Run the supplied executable from this directory. Use the **current** board URL
for the rename request, not its new name before it has been applied:

```powershell
$env:PAKAR_ENDPOINT = 'http://pakarcontroller.local'
$env:PAKAR_USER = 'admin' # or your configured username
# For a configured password, enter it privately rather than placing it in argv:
$credential = Get-Credential -UserName $env:PAKAR_USER
$env:PAKAR_PASSWORD = $credential.GetNetworkCredential().Password
try {
    .\PakarController.exe --command device name
    .\PakarController.exe --command device rename ledboard.local
    $env:PAKAR_ENDPOINT = 'http://ledboard.local'
    Start-Sleep -Seconds 3
    .\PakarController.exe --command device name
    .\PakarController.exe --status
} finally { Remove-Item Env:PAKAR_PASSWORD -ErrorAction SilentlyContinue }
```

CLI defaults are `admin` / `pakarcontroller` only when no credentials were
configured; they are not guaranteed to match your running board. Use
`--command auth info` to see advertised username/default status (no password).
If mDNS is unavailable during recovery, use the current known IP *only* as the
initial endpoint. This does not repair multicast routing across SSH/proxies.

For later changes:

```powershell
$env:PAKAR_ENDPOINT = 'http://ledboard.local'
.\PakarController.exe --command device rename newname.local
```

The TUI (`.\PakarController.exe --tui`) accepts `device name` and
`device rename newname.local`. It switches its own endpoint after confirmed
success. CLI invocations are separate processes: update `PAKAR_ENDPOINT` yourself.
Network → Device name in the native app offers the same setting and switches
that app's saved endpoint only after a confirmed persistent rename. The Web UI
provides a new-address link after success; sign in again at the new origin.

## Authenticated HTTP API

Exact paths:

- GET `http://ledboard.local/api/v1/device/name` reads the configured name.
- POST `http://CURRENT-NAME.local/api/v1/device/name` renames the board.

POST body is **application/x-www-form-urlencoded**, field `hostname`:

```powershell
curl.exe --noproxy '*' --user admin `
    --data-urlencode 'hostname=ledboard.local' `
    'http://pakarcontroller.local/api/v1/device/name'
# --user admin prompts for its password, keeping it out of command arguments.
curl.exe --noproxy '*' --user admin 'http://ledboard.local/api/v1/device/name'
```

Authentication is the same configured Basic credentials used by other APIs;
the same-origin Web UI also accepts its authenticated session cookie. The
rename route is not public. HTTP on the LAN is not encrypted.

Expected POST response: HTTP 202 and, for this target,

```json
{"hostname":"ledboard","fqdn":"ledboard.local","canonical_url":"http://ledboard.local","active_hostname":"pakarcontroller","persisted":true,"accepted":true,"apply_pending":true,"apply_after_ms":1000,"error":""}
```

`active_hostname` is the current name before deferred reconfiguration; do not
confuse acknowledgment/storage with successful DNS resolution. Firmware waits
one second before changing interfaces so the acknowledgment can be delivered.
Wi-Fi/AP, WebSocket, Telnet and cloud connections may briefly drop. DHCP/DNS
caches can take longer. Retry read-only checks, not an ambiguous mutation on a
different discovered board. Never claim a completed live rename without checking
the new endpoint and persistence after restart.

Errors: 400 invalid hostname, 401 missing/wrong authentication, 409 update/ISP/
network reconfiguration busy, 503 storage failure (old name stays active).
Labels are 1..63 ASCII letters/digits/hyphens, lowercase after normalization;
no leading/trailing hyphen, URL, underscore or arbitrary domain. Optional
case-insensitive `.local` is accepted. `local` and `localhost` are reserved.
For NetBIOS too, stay within 15 characters; longer DNS names disable NetBIOS
instead of advertising an incorrect truncated alias.

## Physical USB recovery / Telnet

After installing updated firmware and closing competing COM-port clients:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File .\set-device-name.ps1 -Port COM9 -Name ledboard.local
```

COM9 is the previously observed ASUS ESP port: verify it before using it.
USB uses the existing 115200 baud COBS/CRC custom host protocol, opcode 0x55:
payload `[0]` clears staging, `[1, ASCII chunk...]` appends up to 47 bytes per
frame, `[2]` commits and schedules apply. The tool requires matching valid
acknowledgments. This is physical access, not a cloud/AVR passthrough command.

Authenticated Telnet on port 23 also accepts `device name` and
`device rename ledboard.local`. Reconnect using the new name afterward.

## Initial-instance build

Generic firmware keeps the factory default `pakarcontroller`. For this board:

```powershell
.\build.cmd -LinkMode busy-wire -DeviceName ledboard -Jobs 2
```

The optional ESP-only initial-name build flag initializes `/identity.bin` only
when no identity record exists, and persists it immediately. It never overwrites
an already saved name. Subsequent generic OTA retains the record. `/network.bin`
is separate and untouched by renaming. No filesystem formatting is requested.

## Current verification boundary

Source tests/build evidence is recorded with the deliverables. At this checkpoint,
the ASUS SSH tunnel on localhost:1022 refused connections, the previous local
board IP timed out, and mDNS resolution failed. **No live board rename or flash
is claimed.** Restore reachability before deploying and testing `ledboard.local`.
