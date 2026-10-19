# One selected connection

The native application has one connection selector and one Connect/Disconnect
action. Choose **USB serial** or **Wi-Fi**. Changing the method disconnects; it
does not silently connect the other method. The selected method, enabled intent,
endpoint and serial port are saved. There is no automatic cross-method fallback
or Both mode.
Preferences shows only the selected method's summary and opens this same
connection dialog. Unrelated Preferences saves preserve the live connection
owner; external changes update all draft baselines without discarding other edits.

## Automatic LAN selection and sticky routes

Automatic connection is a default-on **client preference**, not permission to
switch connection methods or send commands. On startup, try the previous network
target first. If it is unreachable, a completed, bounded discovery sweep may
select exactly one verified, compatible physical board. Multiple interfaces of
the same verified board count once; two boards, authentication requirements or
an incomplete verification sweep do not authorize picking an arbitrary reply.
Discovery history, an XML description, a hostname or an MNDP advertisement alone
does not verify a board. A healthy selected board is never replaced by discovery.

Explicit Disconnect suppresses automatic selection for the current session until
Connect. On the next app open, the automatic-connection preference governs startup;
disable it for persistent manual connection. USB/BLE selection remains authoritative:
automatic LAN selection never starts Wi-Fi on an offline USB route. No uncertain
mutation is replayed during discovery, route replacement or reconnection.

The logical endpoint remains a hostname. A new session prefers that name and
retains the last verified numeric address as a recovery hint. After connecting,
HTTP and WebSocket reuse the successful numeric route without an idle DNS expiry.
Cached hostname requests retain HTTP Host and TLS SNI/certificate identity;
Flutter's unencrypted HTTP recovery may use the verified numeric origin internally,
while its saved logical endpoint stays named. HTTPS recovery always retains the
original logical hostname for certificate validation. A transport failure
permits bounded read-only name/address recovery; HTTP rejection or authentication
failure is not permission to replay a write. The cache follows the selected-board
session, not each short-lived HTTP socket. Raw discovery data never seeds command
authority or overwrites a verified route.

Active hostname changes are adopted only from verified same-board state, not a
pending requested name. Durable hostname/IP metadata updates after verified
recovery, while aliases preserve distinct interface addresses. Unsaved settings
drafts stay drafts when another client changes the board.

## Configuration notifications

Firmware coalesces successful durable commits into `configuration-changed`
WebSocket/cloud/BLE events with board/boot identity, boot-scoped revision, scopes,
and saved/applied/failed phase. Active and configured hostname are distinct;
addresses describe only currently ready interfaces. Credentials are never included.
`applied` means the local stack/driver accepted configuration, not association,
Internet, optical or hardware acceptance. Status carries `configuration_revision`;
`/api/configuration` provides authenticated nonsecret metadata. USB uses its existing
binary status/identity refresh hint, not a new unframed JSON event.

Apps coalesce read-only refresh after a new same-peer revision; clean settings can
reload, while dirty drafts retain their external-change/conflict ownership. MNDP
refreshes relevant identity/interface changes promptly with a 250ms coalescing
limit rather than waiting for the normal 30-second announcement. Lost events are
not replayed as commands; reconnect reads current state.

The existing MQTT topic suffix still follows firmware hostname. A remote gateway
pinned to the old exact topic and broker ACL must be reconfigured for that rename;
these LAN recovery changes do not silently widen tenant subscriptions/ACLs.
Production firmware/client deployment and actual rename/DHCP/router acceptance
remain separate from source/fixture proof under the production hold.

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

## Live ASUS checkpoint and power gate

Source/build checkpoint `11d6b8ac44dcc26419da23ffb7eb49e995645273`, version0.2.7,
produced the staged executable SHA256
`e63c1bb0a7dca68f3d1d2ea16c28bd7bdffddf58d0f66ccde7836090da365f89`.
Guarded Release/stage/CLI probe and the full source-quality phase pass. Final
guarded native tests pass181/181 on the recorded rerun. One prior run failed
the unchanged TUI loopback409 assertion; its isolated run and full cached rerun
passed without source/assertion changes. Its intermittent cause is unresolved,
not silently treated as a fixed defect.

ASUS runs that exact staged executable, PID4296/session1, on `ledboard.local`.
Owner/draft checks and actual internal1920x991 framebuffer capture verified one
Wi-Fi dropdown/Disconnect control, contextual Table identity and live480-pixel
preview. This is native runtime/render evidence, not optical LED acceptance.
The owner appeared before the guarded launcher and was not forcibly replaced.
Its active profile was not overridden: normal runtime changes are preserved,
and the exact pre-start original settings plus previous executable remain backed
up. The separate staged candidate profile remains unchanged.

The local candidate is staged, but local GUI launch was rejected by tool policy
after graceful shutdown of the old owner. No alternate launch was attempted;
manual user launch is requested. This PC's owned loopback58080 forward reaches
the board hostname through ASUS. No global DNS or hosts change was made.

App-mediated LED checks submitted only cell-mode and cell1-red IPC actions.
Connectivity expired during readback; no cell80/rainbow action or OTA followed,
and no uncertain write was replayed. The first helper predicate incorrectly
accepted the pre-existing white logical cells while rainbow was still active;
this is NOT a passed all-white write. The private helper now also requires the
global effect to stop, but has only been syntax-checked, not rerun against hardware.

Multiple boot changes were observed without our issuing reboot/OTA. Actual
telemetry then reported reset reason9, uptime162710ms, verified boot integrity,
and sketch MD5 `9ec4ce6a0f2a0d4ad982468acfe46f2e`. The installed ESP32-S3 SDK3.3.12
enum identifies9 as `ESP_RST_BROWNOUT`; this does not locate the electrical fault.
The user confirmed power entering the ESP3V3 pin from the AVR controller supply.
The supply/wiring path is a plausible cause, not a measured diagnosis.

**Wi-Fi-only handoff and OTA remain on hold.** With both boards off, isolate the
ESP's external3V3 feed before powering it through the ASUS USB-to-UART cable.
Keep common GND and appropriate separate AVR power; do not join independent
regulator outputs. ESP USB does not power the480-pixel strip: its correctly rated
supply remains separate. Espressif documents the DevKit power inputs as mutually
exclusive and recommends at least500mA for an ESP32-S3 supply:
[DevKit power options](https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1-v1.0.html),
[S3 supply design](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html).

One requested PBX announcement completed and was answered; no retry. The final
read-only ASUS inventory still contained only a Samsung modem, not an ESP USB
port. Wait for user isolation/re-power confirmation before opening a port.
Next: passive boot/reset diagnostics and stability, actual offline USB control,
explicit switch back to Wi-Fi, app LED readback, then one verified self-OTA/new
boot/image/settings check. AVR programming, resets and audible tests stay deferred.

The Android CI failure found alongside this checkpoint was a separate parser
defect, not a settings-lease timeout: newly declared optional DHCP/static fields
were rejected when absent, so network settings never loaded. The mobile parser
now preserves absent optional groups, validates present types and complete
addressing, and retains exact ACK/readback/owner fencing. Guarded analysis and
all11 focused device-settings tests pass. No APK build/deployment or full mobile
suite was performed for this bounded fix; native package bytes are unchanged.
