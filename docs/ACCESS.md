# PakarController device access

## Sign in and change credentials

Use the saved hostname, e.g. `http://ledboard.local/`, with an IP only for recovery.
Authentication is disabled by default. `/api/auth/info` reports `ready` and
`enabled`; Web and egui only show login fields when authentication is enabled.
Connection/storage failures are not login requests. Browser Direction and Touch
feedback belong to Preferences; unsupported haptics are hidden.

A valid old factory-credential record migrates to disabled. An old explicitly
changed credential record retains protection; use Device access or `auth disable`
to opt out. Corrupt records still fail closed, never silently disable access control.

Open **Device access**, select **Require authentication**, then enter a new
username, password and confirmation, then save. If authentication is already
enabled, its current password is required, including when disabling it. This applies to HTTP/API,
inbound WebSocket and Telnet. AP and station Wi-Fi passwords are separate.
The native editor is in **Network & cloud → Device access**, uses your current
connection password, and reconnects after the ESP confirms persistence.
The password is not saved in desktop preferences.

Username: 1–32 ASCII letters/digits or `._-`. Password: 8–64 printable ASCII
characters. Credential POST bodies are used, never password query strings.

## CLI, TUI and API

CLI/TUI commands: `auth info`, `auth change USERNAME` (enable/change), and `auth disable`.
For CLI: `pakarcontroller.exe --command auth info` or
`pakarcontroller.exe --command auth change David`.
Current password: `PAKAR_PASSWORD`; new password: `PAKAR_NEW_PASSWORD`, not
command-line arguments. Set `PAKAR_USER` for the current username and
`PAKAR_ENDPOINT` to override the hostname. Clear sensitive environment
variables after use. Successful TUI changes update in-memory credentials;
new CLI processes need the new `PAKAR_USER` and `PAKAR_PASSWORD`.

Public `GET /api/auth/info` returns readiness, `enabled`, username, enabled-only
default-password status and route names, never the stored password.
`POST /api/auth` accepts `enabled=true|false` and, when required,
`current_password`, `username`, `password`. It confirms `updated`, `enabled`
and `reauthenticate` only after persistence succeeds. Disabling needs no new
credentials. Enabling from off requires an explicit valid username/password. Basic authentication remains for native/API clients.
Browsers use `/login` and an HttpOnly SameSite=Strict cookie.

At most eight browser sessions; 30-minute inactivity expiry includes
WebSocket activity. Saving credentials revokes all sessions and closes
WebSocket/Telnet connections. Sign-out revokes that cookie and closes sockets;
other signed-in clients may reconnect. Cross-origin browser login and
protected operations are rejected.

## Physical recovery

For a forgotten password, keep ESP USB connected, close other clients using
its COM port, and run:

```powershell
.\tools\set-device-access.ps1 -Port COM9 -Username admin
```

The script prompts privately, uses PCController-style COBS/CRC framing and
waits for the matching acknowledgment after every chunk/commit. Opcode
`0x54` is accepted only by the ESP physical USB host link, never a network
or cloud route or the AVR UART service. Payload action 0 clears staging;
1/2 append username/password bytes in at most 47-byte chunks; 3 commits.
This does not erase Wi-Fi settings, mappings, AVR flash or backups.

Damaged `/access.bin` fails closed instead of enabling the public default.
Staging, read-back verification and filesystem rename precede a live change;
write/rename failures keep prior credentials. ESP firmware-only OTA preserves
the filesystem. Filesystem OTA and full-flash download remain disabled to
avoid overwriting or disclosing secrets.

If the filesystem cannot mount, this script cannot repair it. Automatic
formatting is disabled: preserve/recover storage and authorize a separate
physical install before formatting. A blank fresh ESP needs explicit
filesystem provisioning; existing Pakar devices already have it.

## Security and verification boundary

HTTP, WebSocket and Telnet do not encrypt passwords/traffic. Basic auth is
not encryption. Use a trusted LAN or an authenticated encrypted tunnel;
do not expose these device ports directly to the Internet. LittleFS passwords
are not encrypted flash. MD5 detects transfer corruption, not a malicious
image. Raw UDP streaming is unauthenticated; use WebSocket for access control.

Native tests exercise production persistence and queue code with explicit
filesystem/transport test doubles. Web tests execute the actual embedded
script with DOM/HTTP fixtures. Compilation and tests are not physical proof
of reboot persistence, OTA, simultaneous clients or peripherals. Those live
acceptance tests remain required when the ASUS/device path is reachable.
