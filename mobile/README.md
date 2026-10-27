# PakarController mobile

Current source identity is **0.2.8+23**, shared with
`../interfaces/client-release.json`; read the installed package to identify an
actual phone/tablet. Historical build/test receipts below do not establish the
current installation. See [system architecture and gaps](../docs/ARCHITECTURE.md).

The Table and Keys roles are explicitly advertised build profiles. Table is
480 pixels; the newer first-wins Keys profile is216 pixels. Several diagnostic
and transport controls remain table-specific; universal role parity is not
claimed. Native Android USB CDC is also a selected offline path with the
limited scope in [USB-OTG](../docs/USB-OTG.md), not a generic CH343/CH340 driver.

ESP32-S3 Bluetooth LE is a separate offline connection choice in Devices and
connection settings. Android requests Nearby Devices (legacy Location) access,
verifies INFO/full board identity, and routes normal controls/settings/live state
through GATT without board HTTP fallback. The contextual Board Bluetooth LE
switch persists the radio setting. Direct firmware binaries/raw streaming are
not exposed over BLE; manual remote firmware jobs use the board updater.
See [Bluetooth contract and acceptance](../docs/BLUETOOTH.md).

The Devices page scans mDNS/DNS-SD, SSDP and MNDP into one searchable directory
with saved local/remote profiles. Compatible observations merge by board ID,
UDN, normalized MAC and IP/origin aliases; conflicting board IDs are not merged
just because DHCP reused an address. Saved names survive discovery, and saved
profiles not reached in the latest scan dim without disabling a manual retry.
One board can retain distinct AP, station, Ethernet and hostname origins. The
Devices route dropdown preserves the exact selected IP/scheme/port across
rescans; a saved origin is not replaced by the merged board's preferred name.
Availability is per origin from the current sweep, not inherited by all aliases.
Saved aliases receive separate bounded public probes, with no credentials or
commands. Connect verifies and selects one exact route, not every interface.
DNS-SD retains the actual ESP hostname/IPv4 correlation when ordinary Android
`.local` resolution is unavailable. Passwords are optional: new profiles need
none, existing profile edits can keep/replace/clear one, and connection prompts
only after HTTP401. Credentials stay in secure storage, never advertisements.
Actual board type/capabilities
are detected after selection, not invented by profile labels. The app-bar
context menu opens separate Firmware and Advanced MQTT/bridge pages. Firmware
supports uploads, SHA-256-pinned coordinated remote staging/apply, stored AVR
flash/EEPROM export and digest-checked running ESP image export. The Android
bridge supports TLS MQTT (including WebSocket transport), QoS, broker subscription
ACK, availability/LWT, an outbound WebSocket and authenticated local Android
HTTP/WebSocket clients. It is explicitly foreground-only and loopback-only;
remote commands/firmware staging are opt-in, duplicate/retained/stale requests
fail closed, and no lost command is replayed. See
[the contracts and remaining acceptance](../docs/MOBILE-NETWORK-WORKSPACE.md).

Discovery now performs bounded, credential-free public XML/status probes before
showing a nearby candidate. Generic HTTP/SSDP/router MNDP advertisements are not
compatible-board proof. XML compatibility is not authentication: selected-peer
capability, hostname, board and boot checks still own control authorization.
Read-only probes and authenticated API reads do not follow redirects.

`app_theme.dart` and `workspace_widgets.dart` own the shared Studio typography,
surfaces, stable field outlines, page headers, grouped sections and badges across
Devices, connection preferences, device settings, firmware and Android bridge.
The bridge uses Session/MQTT/WebSocket/Routing navigation with retained drafts.
Renderer captures use that same application theme rather than a mock palette.

Selection/highlighting, Find text, effect choice and both table scroll axes are
user-owned board-ID workspaces, not telemetry or socket state. Transient loss
keeps the last acknowledged layout with hardware writes disabled. Backgrounding
pauses optional reads/reconnect timers without closing a healthy socket; resume
probes it before HTTP fallback. Small UI-only state is saved when changed on
backgrounding, never per telemetry frame. Explicit Disconnect still stops access;
neither reconnect nor cache restoration replays a command.

For an acknowledged480-pixel streaming board, open the device context menu →
Pixel diagnostics. Show or Previous/Next sends a complete black frame with only
the selected physical pixel (or aligned six-pixel group) coloured. All off sends
480 black pixels. Custom RGB, direct1–480 entry and boundary-safe1/6 stepping
are supported. Mode/colour edits alone send nothing; readback is verified before
reporting success. Disconnect never replays a frame. Closing leaves the last
frame; use All off first when desired. Phone/optical acceptance remains separate
from the simulated layout and transport tests.

The Flutter control surface targets Android phones and tablets with the same
truthful board-state contract as the native and embedded Web interfaces.

It provides the 80-cell table with the physical/logical identity
editor and interleaved half-cell column stagger, the board's one baked mode, front-panel marquee and live
seven-segment preview, hardware/discovery state, and persistent authenticated
connection preferences. The user-facing endpoint defaults to
`http://ledboard.local`; any valid renamed `.local` hostname is supported. An SSDP origin is used only as a matching identity-checked
transport fallback when the Android resolver cannot resolve mDNS.

The Phosphor font is vendored in `assets/fonts/Phosphor.ttf`. Only used glyphs
are retained in release builds. The visual colors follow Pealayer's Studio
palette roles. Logical-ID search supports exact numbers and ranges with
one shared selection; alternating colors are on cells, not column backgrounds,
and odd columns use +0.5. Finding or tapping selects without physical writes;
explicit bulk output stops at the first failed/uncertain command. LED indicators
default On and multiselect defaults Off; both are saved per board. The display starts `1, 11, 2, 12…`: cell 1 is
at the upper-left, and 11 is to its right and half a cell lower. There are20
visual columns and four rows. Rows share exact column centers with1.0-cell row
pitch; there is no alternating horizontal row shift. The first column is
1,21,41,61 and the second is11,31,51,71. This follows `lib/table_layout.dart`.
RTL changes application text direction, not the physical table orientation. The
canvas includes the final half-cell offset and pans horizontally rather than
shrinking phone targets. Display coordinates never rewrite logical IDs or the
device's independent physical chain mapping. Neutral animated inside outlines do not
change cell geometry or conflict with selection.
Unknown board state is not displayed as zero or a confirmed off cell.

Passwords use `flutter_secure_storage` 11.2.0 (Android platform encrypted
storage), not SharedPreferences. A legacy plaintext password is removed only
after secure migration succeeds; no plaintext fallback is permitted.
Android app backup is disabled. HTTP Basic/Telnet over the ESP's plain HTTP
LAN services still require a trusted network; encryption at rest is not
transport encryption. Discovery is not cryptographic device identity.

Mutations are sent once, after a read-only identity check. A lost response
reports an unknown outcome and is never replayed by discovery. Marquee text
defaults to Once without forced scrolling; Loop and Interval are explicit.

```powershell
./tools/build-mobile.ps1 -Mode Test
./tools/build-mobile.ps1 -Mode ApkPreview
```

The release APK is written to
`build/app/outputs/flutter-apk/app-release.apk`.

The current release-mode APK uses the template's debug signing identity and
is a **preview**, not a production-signed Android release. A durable signing
key, physical phone/tablet verification, full preference/menu parity and
actual board round-trips remain open in GitHub master issue #2. No keystore or
device credentials are committed. Flutter/Dart versions used for this
historical checkpoint were 3.47.3/3.13.3; the current Dart SDK constraint is
^3.13.3. The shared source identity at this documentation review is0.2.8+23.
An already-installed package reports its actual package version.

Peer version admission is enabled by default with a 0.2.8 support floor;
`PAKAR_ENFORCE_PEER_VERSIONS=false` disables only version checks in a build,
not identity, authentication, capability, replay or update integrity checks.
Use the canonical wrapper's `-DisablePeerVersionChecks` switch with
`-Mode ApkPreview -BuildOnly` for such an APK; omitting it keeps enforcement enabled.
HTTP and WebSocket identify the app, installation and process session;
bounded public hardware/settings reports refresh at most once a minute while
the selected board is being observed in the foreground. API paths stay unversioned.
An IP is self-reported only from the genuine selected board connection's direct
socket. Bluetooth, simulated, proxied or unavailable routes omit it. The peer
sharing HTTP API cannot expose its local interface and omits it too; the actual
observed peer address remains separate. Mobile currently forces dark mode;
there is no user-configurable theme preference to infer.

App sharing preserves an explicit read-only recovery exception: the existing
`app.package.info` and `app.package.read` methods on `/api/rpc` remain available
to older clients, so they can retrieve a newer APK. Source, origin, size,
signer and digest checks remain; this does not authorize any `app.upgrade.*`
mutation, Android installation or board control. Those retain admission and
the existing local approval/capability/OS-consent boundaries.

The wrapper runs from the repository root using the existing documented SDK,
canonical mobile/build cache, reviewed context/storage gates and locked resolved
packages. It does not install an SDK, upgrade dependencies or clean caches.
Only acknowledged capabilities are displayed; stale/changed connections revoke
controls and pending replies. Device application title and bounded inline artwork
override the shared desktop/Web PNG fallback. See docs/MOBILE-CONTEXT.md.

Device settings now include actual Wi-Fi, MQTT, outbound WebSocket, full board
identity and access editors with explicit confirmation, positive ACK and public
readback. Phone wrapping navigation and tablet rail share the real catalog.
See docs/MOBILE-SETTINGS.md for the actual Pealayer PreferencesTab mapping,
46 passing Flutter tests, inspected captures and concrete remaining omissions.
Mobile firmware now has pinned remote jobs, explicit apply/cancel, ISP/full
backups, document import/export and ESP OTA. See docs/MOBILE-FIRMWARE.md for
actual donor mappings, safety boundaries and remaining physical acceptance.
Complete localization/appearance and Android-back draft protection remain open.

```powershell
./tools/build-mobile.ps1 -Mode Test -CaptureView SettingsTablet -CapturePath C:\absolute\outputs\PakarController\mobile\new-settings.png
```

Capture paths must be new files within this task's outputs/PakarController.
SettingsPhone, FirmwareTablet, FirmwarePhone and Table are also supported; captures assert zero mutations and
do not control an installed phone or the production Windows desktop.

FrontPanelTablet and FrontPanelPhone capture the real peripheral mirroring UI
with an explicit simulated peer and a silent injected audio output. Front panel
and actual 7-segment masks are independent previews; phone-speaker mirroring
synthesizes the advertised AVR Hz and timestamp cadence and is opt-in,
foreground-only and peer/boot/AVR-session fenced. See
[MOBILE-MIRRORING](../docs/MOBILE-MIRRORING.md) for contracts and live-test gates.

Pixel diagnostics is in the connected board's context menu. Show isolates one
physical pixel or aligned six-pixel group; Previous/Next step by1/6, the six
numbered sectors jump directly, and All off publishes a complete480-pixel black
frame without losing the cursor. Colour/group changes apply immediately after
a successful Show. Confirmed positions advance only after matching full RGB
readback; reopening preserves the board's draft cursor/colour without replaying.
The optional `pixel-diagnostic` firmware capability uses one atomic acknowledged
board command over WebSocket with deduplicated HTTP recovery. Older firmware is
labelled and retains its socket streaming path; no HTTP path is invented for it.
Logical labels come only from the verified board mapping, never a guessed order.

For adjacent-PC transport diagnosis without copying a Flutter SDK, build
`./tools/build-mobile.ps1 -Mode PixelProbe`. Its standalone executable requires
explicit `--endpoint`, `--board`, `--boot` and a new absolute `--receipt` path.
It refuses to overwrite a nonblack raw scene, then tests first/boundary/tail,
six-pixel and All off using the actual mobile API. Building never contacts a
board. This tool does not certify Android UI, physical light output or signing.

Default-port WebSocket URLs explicitly preserve80/443 when converting http/https
to ws/wss; the socket connector also supplies those defaults. Initial sockets
and HTTP refresh are independent, and a full same-origin socket status can
establish the board/boot identity without a preceding HTTP read. Native hostname
connection precedes any manual lookup and caches only a successfully connected
address; Host and TLS identity remain unchanged.

## Updates and support without ADB

Use the single **Updates & support** menu entry. Data-only presentation updates
are shown first; package transfer, automatic policy and troubleshooting share
that workspace. The default presentation source is
`https://office.digitalogic.ir/pakar/api/presentation`; the default live feed is
`wss://office.digitalogic.ir/pakar/api/events`. Existing explicitly saved sources
are retained. Checks/downloads/install requests are default-on; Android still
requires its installer consent. Same/older releases are quietly ignored.

Presentation updates currently contain validated palette roles only, not Dart,
HTML, executable patches or arbitrary files. They apply atomically without an
APK. Compiled Flutter/native behavior needs a newer same-signer APK; this project
does not claim a hot-code patch runtime or a silent Android installer.

Start a foreground, 15-minute support session to use authenticated
`/api/support/health`, `/api/support/request` or `/api/support/ws`. Session/deadline
envelopes support `health`, read-only `board.status` and `board.mapping`;
`assets.check`/`assets.apply` and board `command` require their separate session
options. Secrets, arbitrary shell execution and remote installer calls are not
exposed. Stop/background revokes the token; ADB is recovery, not this API's transport.

Operators publish a newer data bundle with
`node /opt/pakarcontroller-management/services/client-management/operator.mjs presentation JSON_FILE`
on the office host. `node tools/verify-presentation-service.mjs` verifies actual
HTTPS digest/bytes and the WSS session-bound notice without contacting a device.
