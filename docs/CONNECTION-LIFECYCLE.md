# One selected connection

The native application has one connection selector and one Connect/Disconnect
action. Choose **USB serial** or **Wi-Fi**. Changing the method disconnects; it
does not silently connect the other method. The selected method, enabled intent,
endpoint and serial port are saved. There is no automatic fallback or Both mode.
Preferences shows only the selected method's summary and opens this same
connection dialog. Unrelated Preferences saves preserve the live connection
owner; external changes update all draft baselines without discarding other edits.

USB uses only the selected serial port and the board's framed protocol. It does
not require a valid Wi-Fi endpoint, network access, discovery, HTTP status or a
WebSocket. Network-only features remain unavailable on that route; provisioning
over USB does not imply saved network readback. Opening USB does not assert DTR.

Wi-Fi uses HTTP and WebSocket as components of the same selected method, without
opening any serial port. A green connection indication requires fresh selected
board status, not merely an open socket. Disconnect revokes queued commands and
late telemetry; uncertain mutations are not replayed. Historical operation
receipts may remain in Events without becoming current board state.

The admission gate retains each old method's actual resource lease until its
request/socket/serial handle closes. HTTP idle pooling is disabled. Already
dispatched OS I/O cannot be recalled: handoff waits for that owner to release
rather than opening the other method concurrently. Existing request deadlines
apply; OS DNS/WebSocket connection acquisition can still delay release. These
are source ownership guarantees, not a measured hardware timing claim.

Unsaved Preferences/device drafts and pending identity/settings/maintenance
operations block connection changes before endpoint or port mutation.

## Running-application controls

These commands address the existing same-user/session application and never
start a second GUI or focus it. IPC acceptance only queues an operation; use
`--app-status` and actual board readback to check its result.

```powershell
PakarController.exe --wifi --endpoint http://ledboard.local
PakarController.exe --usb --port COM22
PakarController.exe --disconnect
PakarController.exe --app-status
PakarController.exe --led-effect rainbow
PakarController.exe --led-mode cell
PakarController.exe --led-cell 1 red fade
PakarController.exe --update-esp C:\firmware\esp32s3.ino.bin --confirm
```

The update command requires a fresh acknowledged Wi-Fi peer, OTA capability,
an absolute local image path, explicit confirmation and no pending draft/write.
It uses the existing identity-pinned maintenance queue once. The current native
image preflight accepts E9 binaries up to 2 MiB; it is not full target validation.
Only use a verified image for this exact board, and retain recovery access.
Standalone `--status`, `--command` and `--tui` remain explicit HTTP interfaces;
they are not selected-route controls for the running application.

## ASUS hostname forwarding

When only ASUS resolves `ledboard.local`, an owned SSH forward may bind local
loopback to that hostname on ASUS. Use that loopback URL only on this PC and
`http://ledboard.local` on ASUS. Do not change global DNS/hosts, expose the
forward on all interfaces, or save a transient device IP as its identity.
The forwarding process must remain available for this PC's Wi-Fi connection.

## Acceptance boundaries

Unit fixtures exercise network suppression in USB mode, no fallback, resource
handoff and stale generations. Real egui tests exercise the single contextual
control and lazy serial enumeration. Source tests cannot prove physical USB,
Wi-Fi loss recovery, an actual OTA, externally powered reboot or optical output.
Record those separately in the LED-first checklist; AVR integration stays deferred.

Guarded native Check and all 181 native tests pass for this checkpoint. The
connection wiring source guard (including six negative mutation cases), HTTP
peer-fence source guard, exact comparison generation/check and mandatory donor
build gates also pass. This is source/fixture evidence, not the live handoff.
The final update warnings and upload filename are architecture-neutral; the
existing E9/2 MiB preflight is unchanged and is not an exact-target image check.
