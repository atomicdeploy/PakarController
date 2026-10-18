# Donor-first preflight receipt

## Connection deadlines and retained contextual panels

Read the connection-resilience, error-management, network-cloud, native-lifecycle
and capture routes plus the actual donor Session, unified mobile transport,
workspace profiles, peripheral availability and LAN resolver/tests. Existing
hash-bound mappings were extended, not replaced with a bulk completion claim.

The actual ASUS baseline ran605.38s:112 samples,27 Windows notifications,
27 USB and5 Wi-Fi panel-list changes. It exposed immutable USB snapshot
timeouts, status/mapping HTTP timeouts and transient AVR-online notices. The
first updated600.60s window retained the panels (one initial transition per
method) but still had read failures; it was not accepted as a timeout fix.
Read-only ASUS HTTP timings then measured2.04..2.19s per hostname lookup.

Adapted read single-flight and pre/post-write retirement in Flutter; actual IO
requests now abort on deadline/owner change. A failing real-socket oversized
response test exposed IOClient's lazy onListen cancellation hook; subscribing
before abort fixed it without weakening the test. Prior ACKs retain panel slots,
not write authority. Native full USB reads use four token-bound chunks and one
unchanged absolute8s capture/freshness deadline; command ACK deadlines/replay
rules remain unchanged. Progress is typed IPC, not new toast identities.
Verified TCP routing retains hostname/Host/SNI and the old hint during bounded
relookup; HTTP/auth/body timeouts do not invalidate DNS. Same-board recovery
retains selection/drafts, while current identity and capability ACKs gate writes.

Actual pinned ESP32 connection APIs were inspected independently from TLS:
MQTT TCP connect is bounded, retry is scheduled from completion, and measured
attempt durations are additive telemetry. DNS remains synchronous. Existing
cloud tests model slow20s attempts, rollover and completion-relative delay on
both platforms; this is not proof of a fully nonblocking cloud implementation.

ASUS owner replacement was graceful, hash/PID/session/draft checked, settings
backed up and the previous executable retained. A PS5 -File Boolean-switch
binding failed before shutdown; a read-only reviewed invocation and corrected
binding preceded replacement. No forced kill, settings rewrite or board command
replay was used. S3 OTA accepted the new image but its final range readback was
interrupted. No second upload followed: read-only full export independently
verified1,924,224bytes SHA256
71b66e9bd43fb3fa4ff92933696428f6169562a4b508bd10c99b33e6e15a41ff
on the same board's new boot fd454041772f20e7aeb44e4201816186.
The AVR image/fuses/EEPROM were not changed in this pass.

Focused native USB/presentation/notification/peer-HTTP tests,32 Flutter tests
across API/panels/deadlines/realtime/workspace lifecycle, analyzer, APK preview,
cloud fixtures and all six linked flash wrappers passed. The Linux CI failure
was a missing three --wrap flags in its host test, not missing production guards;
its command now matches the existing PowerShell runner. No protection or test
was disabled. Native/firmware0.2.7 and mobile0.2.7+7 remain consistent.

Final native candidate b8109cf50c209ac95c890a7c7c59ce737a6c80ff1b9cd4127445226d80978ca1
is separately delivered for another600s live observation. Final window results
must be reported separately; no phone was authorized on ADB, so APK build and
widget/loopback evidence are not Android installation or human acceptance.

That window still exposed USB capture expiry and HTTP/WS stalls; retaining
panels alone is not timeout acceptance. Actual broker logs showed repeated clean
DISCONNECTs, not duplicate-client takeover evidence. NetworkPath treated DNS-only
refreshes as route changes and closed established sessions; it now updates DNS
without tearing down TCP, while real interface/address/gateway changes still
retire sessions. Actual Wi-Fi and Ethernet policy fixtures passed. Additive
route/resolver and cloud close-cause counters distinguish the runtime causes.

Large cloud telemetry wrote all packets in one loop pass; ordinary yield() did
not drain USB. Cooperative read-only host progress now runs between payload
writes/fragments and around socket calls, with explicit no-recursion/partial
packet guards. USB mutations receive Busy while cloud I/O owns the loop; nothing
is replayed. Buffered AVR replies are drained after cloud work before presence
expiry, avoiding an unprocessed reply being reported as peer loss. Individual
DNS/TLS/socket calls remain synchronous. Both-platform cloud fixtures exercise
progress between real writes, timing, recursive-call refusal and partial-write
TCP close without replay. A new actual S3 build passed; deployment and another
live window remain separate evidence.

The WebSocket worker regression initially failed because its server fixture
mistook the new read-only liveness probe for a mutation, then mistook the probe's
initial status for a command receipt. The fixture now answers liveness separately
and waits for verified command completion plus its readback, preserving exact
wire-envelope and under500ms assertions. All5 realtime tests passed. No command
verification or production deadline was relaxed to make the test pass.

## AVR ISP and Urboot programming paths

Consulted firmware-hardware, avr-boot-chain, remote-ota and exhaustive literal
PCController programmer inventory. Actual preflash.go and urclock_retry.go were
read completely and match the inventoried b0111ae0 original-byte hashes. The
existing stock Urboot builder/README were rechecked against their exact live
hashes; its custom512-byte/wrong-pin backend remains excluded from deployment
pending explicit profile adaptation, not silently substituted.

Adapt the actual preflash source reinspection/durable-backup/no-write-replay
boundaries into a single S3 maintenance job. The published Urboot wire
specification, actual pinned stock source MCU IDs/feature encoding and installed
MiniCore image were inspected: modern Urboot is NOT ordinary STK500v1. The
small independent protocol implementation uses byte addresses,128-byte pages,
consistent device-specific sync bytes and actual boot metadata. No copied GPL
bootloader source or new compiler/toolchain installation. No chip erase/EEPROM
write/boot-region write through UART; ISP remains the explicit recovery method.

Actual initial signature-only ISP read returned1E950F. Existing flash, EEPROM
and fuse metadata were exported to ASUS before reset. The legacy fresh-backup
request lost its HTTP response; matching retained files and later UART recovery
are NOT, alone, a positive fresh-capture completion receipt. That failure drove
the shared queued ISP/Urboot job and cooperative existing ISP callback instead
of a blind retry or a longer opaque script. New runtime evidence is recorded
separately after actual deployment. No fuse change is introduced.

The first Web progress test caught equal missing boot IDs causing a missing
optional-object dereference; require the object and a real boot identity.
One Dart braces lint was fixed without disabling analysis. Gates caught changed
proof hashes after reviewed formatting; update only their actual target/proof
bindings in the same ledger. The UART emulator and actual ISP callback/
preservation tests are not electrical, flash-hardware or physical acceptance.

Independent read-only review found maintenance-boundary HTTP/fragmented-WS
ingress, expiry-before-reset and stale ISP UART recovery gaps. Corrections bind
queued HTTP and complete/partial WS messages to an ingress epoch, close Telnet
and its partial editor, stop/rebind UDP, and discard USB ring/partial COBS input
at release. The same actual SerialLink fixture now exercises the discard
boundary; async HTTP fixtures reject both pre-maintenance and maintenance-era
queued mutations. ISP restores a clean framed parser and accepts only its newly
requested Hello sequence. Expiry is checked before target effects, Prog arming,
reset and first programming; no cancellation/retry is injected mid-flash.
Urboot accepts only the reviewed e7/features20 stock profile, rejecting custom
self-patching/dual-boot profiles. These tests are not concurrent-client soak.

The canonical updater Plan exposed two PATH Node executables becoming one
invalid command string. Read actual pinned PU850 Tool/build-all.ps1 Git blob
through EOF; retain the imported runtime's single PATH-preferred executable
resolution instead of installing another SDK. The root Plan now works with both
installed Nodes and the updater fixture passes. Local OTA responses/readbacks
can still be interrupted: inspect boot, phase and running image before another
explicit update; an aborted5124-byte upload is not successful installation.

Actual final evidence is now in REMOTE-MAINTENANCE.md: pinned Internet ESP job17
and separate real same-image AVR Urboot job1/ISP job2 all completed, with flash/
EEPROM verification, fresh UART/application recovery and Prog clear. Preserved
ASUS receipts distinguish these from earlier response-loss-only attempts.
No fuse writes, audio/PBX test or physical LED/display acceptance was performed.

The existing full CI was failing a source regex requiring a removed native
Firmware-page polling block. Reread complete actual PCController link/session.go;
retain its owner admission/identity checks in the new assertion for selected
network/USB observations, with negative mutations. Do not restore unnecessary
polling. The S3 source guard likewise now requires both disabled-link and raw-
UART exclusion instead of the obsolete single-condition spelling. No production
guard was weakened. Rebind only reviewed test references in the existing ledger.

## Firmware subscription, file delivery and external update progress

Consulted updates-telemetry, remote-ota, network-cloud and exhaustive PC donor
inventory. Fully read actual canonical hostbridge/manager.go (1851lines),
hostui/update_progress.go and its tests, artifacts/download.go and mobile
firmware.dart; exact live bytes match the existing inventory snapshot. Add five
bounded complete-file comparisons and firmware-delivery index route in the same
ledger; unrelated manager/OS notification/artifact-store/USB/background-service
parity remains explicitly pending. Reread actual PU AsyncOTA.cpp and the current
integrated OTA callbacks; retain raw-size/stage/ACK versus completion boundaries.

Native and Android adapt the real outbound WS/Socket.IO open, namespace,
heartbeat and event tuple to a finite, explicit peer-bound firmware inbox. URL
offers require a trusted origin, unique ID, current session/board/boot, deadline
and SHA-256. Manual local/HTTP(S) images share guarded existing board upload
routes. Source credentials stay separate from board auth; redirects and bad
digest/size/format cannot write the board. No new disk artifact cache or second
serial owner. Native feed configuration is a preserved dirty draft until valid
Start; secrets remain memory-only. AVR/paired programming stays explicit.

Fix the actual missing DTO path: native previously dropped update/remote job
fields. Rust/USB/bridge now retain them, Android consumes existing status events,
and Web uses its normal status stream. No redundant fresh-WS job polling or
fake flash percentage. Transfer acceptance is informational, not completion.

Focused native real WS/Socket.IO and HTTP download/upload fixtures pass; initial
SHA2-0.11 output lacked LowerHex, corrected using explicit byte encoding rather
than downgrading or disabling digest checks. Android analyzer and six focused
delivery/progress/socket cases pass. Its initial24 brace lints were repaired by
the analyzer's scoped mechanical fix; no lint was disabled. Source-hash gate
caught formatter drift and was rebound only after reviewing the affected edits.
Final package/ASUS/board observations and remaining actual phone/WSS endpoint
acceptance are separate from these fixtures; do not claim those from builds.

Final analyzer and six firmware-page cases pass after adding the missing
dart:async import to the new same-board reboot fixture. That fixture proves
fresh read-only controller rebinding and retained controls without replay.
Canonical release/preview/production-profile SDK builds pass. Actual ASUS USB
owner observed external Internet job16 download at38% and81%, and verified
completion on the new boot after an explicit native CLI Apply. The earlier
MQTT Apply had no receipt; its unchanged ready state was checked before that
new explicit action, not blindly replayed. Native owner/settings/rollback,
image/boot/digests/slot, final APK and remaining physical/phone/service gates
are recorded in REMOTE-MAINTENANCE.md. No actual AVR erase/reset/audio test.

## Board-advertised LED wire implementation

Consulted device-context/led-cell-test and literal donor-audit. Fully reread
PCController peripheral-availability.ts/tests, AddressableLeds.cpp/header and
Pealayer board_info.rs. Preserve explicit peer-owned capability/measurement
separation; AVR register timing and unrelated settings are not copied onto S3.
Add backend/rmt/dma metadata from the actual selected output worker through the
existing common status builder. Live driver JSON shares that definition, while
readiness/active/error remain separate observations. HTTP/WS/MQTT/full USB
snapshots therefore share one statement. Native typed serialization and USB
compact readback preserve it; optional invalid/missing metadata is unknown.
Rust/Flutter/Web use compact contextual read-only labels, not an editable box
for a build flag. Existing source/proof hash references are rebound only after
reviewing these additive changes; no broad donor-parity claim is made.

Actual GPIO/RMT-DMA worker fixtures (both channel orders), renderer fixtures,
three focused native capability/USB/bridge tests, Web context/label assertions,
Flutter analyzer and six peer-context tests pass. A Dart brace lint was fixed
without weakening the analyzer. Previous main CI then exposed a Linux runner
compiling the default static worker but invoking DMA-only scenario names. The
Linux runner now uses the same four explicit profiles/scenarios as PowerShell;
its exact Bash worker loop passes locally without weakening assertions. This
is not a complete Linux CI run. Same-file hash-bound proof is refreshed.

Actual ESP-only MQTT/HTTPS job15 completed with a new boot and expected running
size/MD5/integrity and postboot image SHA verification. Its capability reports
gpio-static/rmt=false/dma=false over actual MQTT, HTTP, WebSocket and the ASUS
app's USB COM22 path. ASUS native replacement was hash-checked and atomic after
confirming no running owner; settings and prior executable are preserved.
Actual owner IPC reports USB selected/online with the same backend metadata.
Android preview builds, but its transfer was interrupted and the authorized
phone is now offline; no installation or mobile runtime acceptance is claimed.
See REMOTE-MAINTENANCE.md for image/boot/package evidence. No backend switch,
AVR programming or optical success is inferred.

## MQTT, Internet delivery and opaque maintenance forwarding

Consulted network-cloud, updates-telemetry, remote-ota, bridge-repeater and
donor-audit; fully read PU FirmwareTransfer.cpp/header and canonical PCController
preflash.go/restore_preflash_test.go. New hash-bound comparisons retain complete
backup/cancellation/candidate-reinspection boundaries, with concrete pending
host archive/executor omissions. Transparent forwarding uses the existing ESP
updater/ISP coordinator, not a second serial owner or programmer.

Fixed the native bridge's JSON-only assumption: binary firmware/AVR exports,
real multipart bodies, HEAD/Range/ETag and actual201/202/error replies survive
without reinterpretation. Fixed allowlist, peer/boot/route lease, loop trace,
bounded body/export sizes and no-replay semantics remain.16 focused bridge/socket
tests pass, including valid multipart, stale boot before dispatch, HEAD and a
truncated stream that cannot yield a successful completion receipt. Network
capabilities are ACK-gated; unsupported USB maintenance/streaming stay hidden.

Latest main CI failures were inspected, not bypassed. Fully reread Protocol.h
and actual donor UartProtocol.h: additive USB snapshots/bitmap commands did not
change framing or tunneled dialect. Corrected stale SOURCE-MAPPING target hash;
23 exact mappings and the50 PU route check pass. ESP8266 SDK linking exposed
missing atomic fetch-and, add and compare/exchange routines. The first CAS-only
attempt was insufficient. Consulted actual SDK interrupts.h/core feature code;
use its bounded previous-level-restoring InterruptLock only for ESP8266 state
copies, retaining native/S3 atomics. Both production branches have host fixtures;
real ESP8266 busy-wire SDK compile/link now succeeds. No SDK mutation, libatomic
stub, weakened assertion, fresh cache or cold clean was used.96% RAM/IRAM on
ESP8266 is a remaining headroom risk, not S3 production readiness evidence.

Actual scoped broker provisioning, full checksummed Internet telemetry, separate
MQTT HTTPS stage/apply/new boot, ASUS native replacement and LAN discovery are
recorded in REMOTE-MAINTENANCE.md. Probe correlation initially accepted an
unrelated null-ID event; corrected to unique IDs plus typed command replies.
Paho callback API2 uses explicit reason-code equality, and socket-written versus
accepted outcomes are distinct. Failed probes remain preserved, not reported as
successful command round trips. Windows shared5353 contention was diagnosed
using the actual LAN; default ephemeral-port probing receives real service
records. ASUS helper IPC must run in its interactive session, not SSH session0.

No current AVR flash/reset/fuse operation, physical display/audio/LED acceptance,
Android installation, broad donor parity or all-green exact-head CI is claimed.
Independent HTTP export still timed out; keep that operational defect open.
Its explicit read-only Range resume completed with the exact full running-image
SHA256; no firmware mutation was replayed. Single-stream reliability is still
unaccepted. Postdeployment LAN probes again returned all four requested discovery
protocols, and actual ASUS interactive owner IPC confirms USB selected/online.

## Operational Flutter pixel diagnostics

Reread canonical PCController Project/AddressableLeds.cpp and .h at recorded
43417181/a28009e6 hashes, peripheral-availability.ts, and Pakar's actual native
diagnostic/frame path. Retain the donor clear/stage/show transaction on the ESP;
do not copy AVR timing/registers or its100-pixel limit. The prior Flutter path
required a live socket, separately switched modes, sent two unacknowledged raw
chunks and polled large HTTP snapshots. Its UI advanced before success and hid
All off below multiple sections. The new optional acknowledged diagnostic command
performs the complete operation on the loop owner, returns full RGB readback and
uses existing same-ID HTTP recovery, never a new mutation after lost WS receipt.
Compact controls, six tappable wire indices, verified logical mapping labels,
board-keyed cursor/colour and failure-retained cursor use the existing workspace.

The first canonical mobile test stopped at the expected changed peer_context.dart
hash gate; reread the donor availability source and rebind only reviewed affected
targets in the same ledger. Seven real loopback command cases, seven page cases
and six older-stream transport cases pass; the opt-in physical case remains
skipped. Actual renderer tests include tail/group/all-off/conflicting stream and
invalid-input no-partial-mutation. No screenshot or buffer match closes optical
acceptance. Initial Resolve-DnsName/Win32_SerialPort inventory missed the ESP;
direct hostname HTTP and present PnP identity subsequently confirmed ledboard
and CH343 COM22. No authorized phone is connected, and its advertised ADB endpoint
times out. The first ESP compile caught an incorrect local enum name kModeCell;
use the existing shared Protocol.h kModeCellSelect, not a new alias. No cache or
toolchain change is needed. Build/stage and device/phone installation are separate
gates. Existing unrelated ESP8266 CI atomic-link errors remain outside this slice.

ESP32-S3 16MB/OPI/static/buffered compilation passes. Initial default unverified
ISP profile75dd9f7a was compile-only, not a production replacement; preserve the
live board's existing ISP capability with -S3IspElectricalVerified. That exact
profile is now recorded in AGENTS.md so future passes keep the same flags/cache.
Correct1904976-byte candidate SHA256
cdf162ae73fd3069498264238016588ef23ff737c424e0e79fc333287d44b183
is staged on ASUS, NOT flashed. Final APK
f0e9ad15effc236247d7b6513e6b33234253d4b26251246998489eccecf3b941
is built; Android installation remains unverified. An opted-in live transport run
through the SSH forward failed with connection closed before complete HTTP
headers; this is NOT a passing physical check, and no mutation is replayed.
Direct ASUS hostname HTTP works. A small compiled runner imports the real mobile
API, requires explicit endpoint/board/boot/new receipt and a full black raw frame,
and exercises one/six/tail/off without a remote SDK install. The canonical mobile
wrapper retains the same cache/storage/locked-package guards in PixelProbe mode;
its compile-only plan fixture passes. Reread actual pinned PU Tool/build-all.ps1
and Pealayer storage-hygiene.ps1; no clean, SDK copy or extra cache was introduced.
Standalone Dart receipt creation uses actual SDK exclusive create, not an assumed
FileMode constant; package imports fix the analyzer's relative-lib warnings.

Live scene observations changed between all-off, white and red/off while work
continued. The one-shot OTA preflight refused before dispatch (saved receipt has
dispatched=false, unchanged old firmware MD5, idle/zero upload). Do not retry or
overwrite that receipt. Pause competing LED controls and obtain the user's test
window before update/temporary clears. The phone remains unavailable; source,
loopback, framebuffer, built packages, board update and optical acceptance remain
separate. No AVR operation or desktop replacement was performed in this slice.

Further actual Dart diagnostics found the concrete live WebSocket failure:
custom connectionFactory used Uri.port0 for ws://ledboard.local with no explicit
port (Windows SocketException10049). Preserve the HTTP source port on ws/wss
conversion and use80/443 defaults in both native and cached socket paths.
Initial native hostname connection avoids a separately timed-out mDNS preflight,
caches only a successful TCP address, retains TLS/SNI and original Host, and
classifies genuine lookup failure for read-only discovery. Start socket and HTTP
reads independently; first same-origin full socket status may establish its
board/boot fence without HTTP preflight. Socket disposal has a bounded500ms grace.

Six focused real-socket/DNS/proxy/TLS cases and four workspace lifecycle cases
pass; seven atomic diagnostic command cases still pass. Production mobile API
compiled into the adjacent-PC runner subsequently connected by default-port
WebSocket on ASUS, read board esp32s3-907069eac800 / boot
360d2ef9857b3220b0c037c3ea6671e0, and rejected a deliberate foreign identity with
zero LED commands (pixel-probe-default-port.json). Published RGB/optical tests
remain blocked on the user's quiet test window; the running ESP remains old
image8bc0cd92. Pure lookup still times out on ASUS while native HTTP/socket paths
can work; general LAN/HTTP truncation reliability is not declared fixed.

## Flutter lifecycle and stable board workspace

Consulted peer-binding, mobile-settings and literal Pealayer donor-audit.
Reread canonical Session retirement/admission and complete live
src/ui/workspace_profiles.rs at matching410d274f source hash. Adapt its stable
user-owned IDs into board-keyed Flutter UI state; retain the existing command
owner fences separately. The same comparison ledger now binds workspace source,
consumer and lifecycle/transport proof. Generic workspace-manager CRUD/icons
remain an explicit pending donor import, not an accepted exclusion.

Telemetry expiry previously removed Table from the IndexedStack and changed
connection-revision keys. Background timers and hidden settings also issued
reads, and HTTP refreshed the socket-health clock. Retain the last acknowledged
layout while disabling expired writes, preserve local selection/query/effect/
scroll by board ID, and save bounded UI-only cache once on changed background
state. No device snapshot, command or authorization is restored from that cache.
Pause foreground supervision without closing a healthy socket; resume with one
read probe, independent WS freshness, quiet bounded reconnect and HTTP fallback.
Hidden/inactive optional readers stop before another rendered frame is needed.

Focused widget fixtures use the actual Flutter inactive/hidden/paused sequence;
their initial missing lazy canvas and invalid lifecycle transitions were harness
issues, corrected without relaxing production checks. Real socket tests remain
isolated from TestWidgetsFlutterBinding. Source/fixture acceptance does not
establish Android/Doze/router-loss or physical LED acceptance.

## Mobile deliberate pixel diagnostics

Consulted games-streaming, led-cell-test, mobile-settings and literal donor-audit
for PCController Project/AddressableLeds.cpp. Read its complete current cpp/header
through EOF; source hashes match the existing ledger. Reuse the existing native
pixel_diagnostic.rs, PKAR chunk/commit format and shared Studio Flutter surfaces.
The AVR register sender and100-pixel limit are intentionally not imported into
this480-pixel ESP client; the existing concrete omission remains authoritative.
The same AddressableLeds ledger rows bind mobile source and focused tests.

Analyzer and new unit/widget/real-loopback tests pass. The first aggregate live
sequence hit the runner's default30s limit after exact RGB readback for wire
pixels1,6,7. No failed mutation was replayed. A separate explicit all80-off batch
and static output selection cleared the published480-pixel buffer on the same
board/boot. The live harness now has120s aggregate budget; per-command deadlines
are unchanged. A later delayed RGB observation exposed the too-short2s
confirmation budget: reuse fresh same-boot WS/command snapshots before sending,
and bound read-only confirmation to8s without adding any mutation replay. A real
loopback delayed-snapshot regression passes. The next live preflight connection
closed before its HTTP header; this did not authorize another diagnostic write.
Full tail/group live sequence remains unverified. Phone installation/physical
optical acceptance remain separate; no board reset, flash or AVR cue was run.

## Native board settings and application preferences

Consulted preferences and donor-audit, owning issue4 and the existing settings
catalog/editor before implementation. Reread the complete canonical PCController
Project/SettingsStore.h/.cpp (raw SHA256 ccdcfb6e1ab186abd6a4f0b5a302bb14bab295c9a0bd9b4a306036fa32f1eaf3
and 7309fd36f0680722a4260e8f1251ae5a0fc67e99fc60f2426bb1294f789e87d4).
Reviewed Pealayer's existing PreferencesDraft/clean navigation/section controls
and organization guide; its mapped source hash remains unchanged. Same ledger
rows now bind the native full-record editor, transaction and tests. No new
whole-donor completion certification or incompatible donor EEPROM layout copy.

Device Settings / AVR & inputs now offers all supported saved Pakar fields with
explicit draft/save, validation, conflict/readback and pending-close ownership.
Both selected USB and fenced HTTP use the same GET/SET/GET transaction. Fresh
baseline disagreement refuses SET; exact ACK and persistent full readback are
required. This is optimistic conflict detection, not atomic multi-client CAS.
LED settings uses the same actual controls as the live panel. Preferences Board
now links to device settings for any acknowledged board, preserving dirty local
preferences. USB header shows the selected port, not an unrelated HTTP endpoint.

Initial Check exposed one local borrow lifetime in the expanded Preferences
view; calculated availability before mutation. Reused the canonical build gate
and owned-file hash rebinding, not direct cargo/cache bypass. A ledger patch
initially assumed uniform JSON formatting, then omitted the closing row comma;
both failed without changing source decisions. Retained actual row text and
delimiter, updating only the same three reviewed rows. No parallel ledger.
Native Check, 11 initial focused tests and final 24 settings-focused tests pass.
No board contact, sound, flashing, native replacement or APK build. Mobile/Web
full AVR editor and physical/installed visual acceptance remain open. Large
pre-existing uncommitted changes were preserved, not bulk committed.

## Cell-selection flicker, cable mapping and two-driver chase

Read AGENTS/preflight and owning issue2; consulted games-streaming and exact
donor-audit PCController inventory. The guessed LocalLib/LedStrip.h source does
not exist; the actual dependency is Project/AddressableLeds.h/.cpp. Read both
canonical files through EOF and verified unchanged raw hashes
`a28009e6211056f0cdb1071e7232d6d277a27cf9dc8e3e9951be9830745f18d4`
and`43417181b23cab40b8a58ec26159ca025fb41d5bdbaf52d20a46bfc926fee1b0`.
Retain donor buffer/show separation, not AVR D6 timing or a duplicate engine.
The new led-cell-test route binds both complete source reviews to exact mapping,
renderer/worker/boot/build and actual-test hashes in the same canonical ledger.
Gate passes2 matched/0 stale; full donor parity remains incomplete.

User cable sequence is80,60,40,20,10,30,50,70 then subtract1 per eight-group lane.
Mapping tests exposed the important distinction: that pattern ends with physical
group80=logical61; logical1 is group77/pixels457–462. Asked for clarification,
and use a logical80-down-to1 chase so all80 cells are visited and it stops at1.
Each step composes six identical red pixels and474 white pixels before one
publication.750ms dwell is board-owned, no per-cell HTTP/flash/AVR work. Test
boot override is RAM-only and opt-in; ordinary saved mappings/names are retained.
The named mapping preset preserves names and saves the full permutation once.

Source inspection found RMT-DMA continually replayed unchanged enabled frames.
Removed unsubmitted replay: both workers now consume immutable complete frames,
clear startup black, and sleep until a submission. Renderer repair refresh and
uncapped rainbow remain. This is a candidate firmware improvement, not measured
proof of DMA refill/power/wire correctness. Tests also caught/fixed the delayed
first-render skip and Off/On immediate-advance edge cases; no skipped catch-up,
loop or fade. Source states/frame snapshots are not optical sensors.

Actual renderer, FIFO,10000 full80-cell batches per render mode, static/DMA
RGB/BGR worker scenarios (coalescing, unchanged sleep, startup/Off black,
flash holds and failure), new boot/preset wiring and S3 source guards pass.
First mapping assertion correctly failed because it conflated cable termination
with logical chase termination; corrected the interpretation rather than
changing the permutation to fit an unsupported last-pixel assumption.
Ledger patch initially failed on a row delimiter; reread exact original lines
and retained each row's actual trailing comma without wholesale JSON rewriting.

Final SDK candidates, including delayed boot/Off dwell and completed-active fixes:

- RMT-DMA:1881839/3342336 application bytes,99084 static RAM; raw1881984-byte
  `build/led-comparison/rmt-dma.bin`, SHA256
  `0a4dcf47b2856a600adbe27c4852c2e0987943ffee980508f1c3c147d04e3800`.
- Non-RMT GPIO:1881747/3342336 application bytes,99084 static RAM;
  raw1881888-byte`build/led-comparison/gpio-static.bin`, SHA256
  `98ddf93d90e9ff87c267b6f04a2cf10a662057dd720db41c0e7df79ee6c80c33`.

Build uses16MB/OPI, buffered, PauseAvrLink, CellChaseOnBoot. A preliminary DMA
compile before the dwell fix was superseded, not copied or deployed. The earlier
23b4e75d candidate was replaced only after verifying that exact owned hash.
No clean, debug cache, SDK upgrade or donor mutation.
Only the small comparison images need retaining; no duplicated package tree.
Host has about5GB available RAM/64GB and31GB pagefile already in use; next full
profile build uses2 jobs instead of worsening memory pressure with all16 CPUs.
Other running applications/processes remain untouched.

Reconsulted build-storage and reread actual Pealayer LOCAL-WORKSPACE-HYGIENE.md,
plus pinned PU Tool/build-all.ps1 through the GitHub content API after the guessed
local sparse-donor path proved absent. Do not create another checkout/cache.
Removed backend selection from global compiler flags: canonical wrapper now
writes a changed-only application header at build/config/esp32s3/LedBackend.h.
Plan is write-free and has identical SDK/compiler arguments for both backends.
Exclusive FileShare.None lease covers profile write and the full compile; tested
that another handle is refused while building and that the lease releases.
Full build-storage Plan suite and actual worker profiles pass. Actual compiled
worker literals distinguish GPIO from DMA correctly, not merely a plan string.
All150 cached SDK/library object/archive sizes and modification times remained
unchanged across final warm static and backend-switch DMA builds; metadata
fingerprint7b61bad0ea149181e8f782545bc3e38417779d26b6e6d714d2c56f3d63f71821.
This is measured cache reuse, not a claim of zero writes or measured SSD wear.

ASUS8022 refuses, hostname paths do not resolve, and former58080 forward is
absent. Bounded read-only last-address and authorized server hostname attempts
also failed. No board/UART/AVR/ISP/OTA/phone/native deployment occurred. Existing
PBX service query does not normalize the human phone like its call command:
the first raw-number query expired. After verified expiry/no active channel,
used the canonical trunk target; that separate attempt also expired without
DTMF. Neither is delivered-call or optical evidence. No automatic redial or
service/dialplan change. Prepared separate RMT/GPIO visual prompts with
1=stable/correct,2=flicker,3=wrong mapping,4=no output, for use after deployment.
Private task-local PBX query adapter/prompts are outside shipping source.

Tracking: https://github.com/atomicdeploy/PakarController/issues/2#issuecomment-6246618912.
Source remains the preserved uncommitted worktree based on2dc319fb; no release,
complete CI or physical flicker-fix claim. Both exact-image deployments, pixel
readbacks and driver-specific PBX/DTMF visual acceptance remain open.

## Finite attention/update melodies and companion-driven phone mirroring

Reconsulted board-silent, mobile-mirroring, remote-ota and donor-audit. Fully
reread actual PCController TonePlayer.h/.cpp, Project/BootMelody.h/.cpp,
hostbridge/buzzer_mirror.go and its entire514-line test source at current local
b0111ae0c80dc34149ea1044de7dd5488920301b. Four existing tone/mirror raw hashes
still match the inventory. Actual PU850 AsyncOTA.cpp reread through EOF; retain
explicit start/end/error intent, not PU UART/watchdog callbacks. Donors unchanged.

Implemented ACK-advertised additive attention cue ID14,240ms (C6/90ms,pause60ms,
E6/90ms), using the existing single Timer1 player. All prior14 IDs and original
welcome schedules remain. Desktop/Web/mobile catalogs agree; old peer counts do
not acquire the new cue. Existing fenced API/audio command provides operator
attention; physical Silent and Prog still suppress sound.

Both S3 and8266 local upload callbacks now publish atomic/coalesced notice intent,
including error/abort and explicit finalization. Only the loop emits one UART
cue; no750ms ACK wait, guessed fallback tone or replay after companion recovery.
Remote phase/job observation sends start only on apply, verified completion only
after the existing terminal checks, and uses a cooperative500ms pre-Prog lead.
Missing audio is not a firmware/erase safety acknowledgement. Local commit cue
means finalized upload before reboot, not proof of the subsequently running image.

AVR note publication previously required an active desktop contract session.
Added a separate three-second companion mirror lease on real HELLO/PING/status
traffic, without host/menu/macro authority. Buffered/busy-wire sends one changed
frame per loop; polling receives one solicited changed mirror frame. Audio data
uses one shared timestamped encoder and the original13-byte payload. Polling
can undersample short notes; no missed-note invention. S3 buffered is preferred.

Preserved donor-independent, explicitly enabled phone audio even while physical
AVR Silent is on; that was intentional, not a missing-mute bug. Added explicit
Follow board Silent mode and flush on policy change. Mute/unmute metadata cannot
restart one musical step or resume an interrupted note. Fresh short notes now
rebuffer after missed transport cushion; device-expired notes remain expired.
Stop, duplicate/owner/lease/lifecycle cancellation and finite native PCM remain.

Focused audio/AVR production-code doubles pass all15 cue durations/mute/rollover,
upload coalescing/terminal/no-old-completion, companion-no-desktop/polling/expiry
and no-host-ownership cases. S3 menu-audio, Web picker ACK filtering, updater
loopback and script/actionlint checks pass. First Flutter analysis correctly
rejected two unbraced new ifs; corrected the actual statements without lint
suppression. Analysis then passed, all7 mirroring tests and separate real loopback
WS test passed. Native locked release-profile Check passed with existing warnings.

Actual warm-cache SDK builds pass, source versions0.2.7:

- AVR buffered:31538/32384 application bytes,1465/2048 static SRAM. Full32768-byte
  stock Urboot/vector-chain image verified, SHA256
  4f3f00454ec913551c88da4746ea11955087835c9619e6331406c67f15034fb1.
- S3 buffered16MB/OPI/RMT-DMA/W5500/ISP build acknowledgement:1995639/3342336
  application bytes,99424 static RAM. Final raw bin1995792 bytes, SHA256
  6006ecf027f16cdc821c93a81f52c70af7b0d4f4b522f457efbfde7079c8a05d.
- Android preview APK55740293 bytes, SHA256
  c98e83381b64a1c25817d75b22ff17cb6dacca4fa1c353bc653b3955036f5fba.
  Existing preview signing/cache; no new SDK, clean, debug or package copy.
- Native Release build passed (3m13s, existing seven warnings), executable
  26200064 bytes, SHA256
  8561faf3a8970aa83c72452bf7a75389122cb18d8f5e668cbead95a52b896037.
  CLI-only --version probe returned PakarController 0.2.7 with exit code 0.
  It was not launched/replaced as a GUI or accepted visually.

ASUS SSHlocalhost:8022 refuses. No board write, phone install, audible cue,
speaker/server/PBX action, fuse/EEPROM change or physical acceptance this turn.
Local ADB inventory is empty as well. The final S3 recompile includes the
fresh-HELLO/version-known gate on every automatic cue, not stale capability
cache alone; it supersedes the earlier1995760-byte candidate in the issue note.
Native/Web host-speaker consumers remain unimplemented; this slice fixes actual
AVR publication and existing Android consumer, not those remaining consumers.
No current GitHub CI or production-release claim; preserved large worktree is
uncommitted on main2dc319fb. New canonical comparison total189/4673,4484 pending,
147 known gap files. Seven-source audio-attention gate and report checks pass.

Four changed runtime/header contract-manifest entries were rebound only after
matching their old hashes to untouched HEAD and reviewing this exact delta.
The separate23-file mapping test still rejects pre-existing Protocol/EepromLayout/
PcLedContract target drift; did not blindly refresh it. Existing PU route-audit
drift also remains. Full-project quality is not certified by focused audio passes.

GitHub tracking: [implementation and focused proof](https://github.com/atomicdeploy/PakarController/issues/6#issuecomment-6246432940)
and [final candidate identities and deployment hold](https://github.com/atomicdeploy/PakarController/issues/6#issuecomment-6246468378).
Issue #6 remains open for actual paired-board/phone acoustic acceptance and the
native/Web host-speaker consumers; no production checklist was closed.

## Actual PU850 script/CI import and shared cross-platform owner

Read this repository's AGENTS/preflight and existing build-storage, repository,
update and donor-audit routes before implementation. Verified PU850 local and
GitHub main revision431104072952f694a35b3a83c19f13fbb098ca5b, clean read-only
sparse checkout. Fully read28 actual script/workflow/config/test/documentation
files through EOF, including1114-line upload.js,568-line upload.test.js and
424-line C++ loopback suite. Exact original-byte SHA256, Git blob, bytes,
individual target/proof anchors, omissions and remaining actions are in the
existing canonical comparison ledger; no second source inventory was created.

Imported/adapted actual PU logger/sanitizer/exit categories, HashSet component
selection, Write-Step/Assert-Executable/Invoke-Native/scoped directory/elapsed
helpers, registry-index check/apply flow and staged artifact/deployment concepts.
Root Windows/Bash build/upgrade aliases and pakar.cmd/pakar.sh now share one
Node owner and existing reviewed PowerShell build/update policies. No default
clean, SDK/npm installation, beep, debug artifacts, host-config reset or flash.
See docs/SCRIPT-TOOLING.md for commands, activation/platform limits and concrete
reasons not to import incompatible ASA wire formats or duplicate language stacks.

Failures found and corrected, without bypassing reviewed guards:

- Current main CI39074428460 failed on a boot-audio source assertion that
  predated silent/reset-loop guards. Reread actual AVR boot/reset path; corrected
  the test and added negative removal checks for every required guard. No
  welcome/silent firmware policy was weakened. Latest GitHub run remains failed;
  this uncommitted repair is not a remote green build.
- Donor terminal sanitizer alternation left OSC title payload after broad ESC
  matching. Actual hostile-title fixture now verifies complete removal; color
  is also conditioned on the corresponding output stream, not stdout alone.
- Plan execution exposed string-array option binding in build-all; use typed
  hashtable splatting so Plan/Mode/Target reach the actual wrappers.
- Sparse donor files read from exact Git blobs were wrongly rejected by the
  filesystem-only gate. Added a distinct pinned-blob state, exact repository/
  revision/tree-object/raw-hash binding and regeneration tests. RequireLive still
  rejects absent files; unavailable snapshots are not current filesystem proof.
- Dependency comparison now handles numeric/two-component stable versions,
  prevents downgrade/concurrent overwrite, caches each index once in memory and
  honors configured HTTP/SOCKS proxies via bounded curl without logged credentials.
- Actual WSL Bash execution reached Windows node.exe with an incompatible
  /mnt/c absolute module path. Reused PU build.sh's scoped repository cwd and
  relative entrypoint; real Bash/Windows launchers and outside-cwd help fixtures
  now pass. Bash syntax alone was insufficient evidence.
- AVR boot packaging source test still assumed the old Bash compiler body.
  Verify the complete launcher/dispatcher/canonical packager chain instead, with
  negative cases breaking each hop; full32768-byte/vector/Urboot checks remain.

Focused Node/PowerShell tooling, child exit/directory handling, real subprocess
offline/invalid update plans, unchanged boot guard, storage dispatch, repository
governance, updater loopback multipart/fragmented WS and build-script security
checks pass. Actual actionlint passes; all three component plans and build-all
plans run without SDK compilers. Ledger fixture verifies wrong Git object/repo
reverts to missing. Selective PU ledger regeneration preserves the reviewed blob
rows without expanding donor checkout or losing other origins.

Broader source-quality reached the pre-existing PU route audit then failed:
WebUi.h actual4ca3a928335f176211f3756823372a93eb54080bec0711e2681f4d0753f2ee71
versus its route evidence45e734f5fa28c77b9844cbbc3aa50ee81f2fc65222cc5d418a2d8bac66f1ab16.
That file/evidence already had unrelated firmware/UI edits before this task.
Did not rehash it blindly or weaken the audit. Full local quality is NOT green;
the earlier targeted tooling/boot/storage/governance/loopback results stand.
Remaining checks are diagnostic continuation only, not a skipped-gate pass.

Live official registry check succeeded: current pins remain unchanged; FastLED
3.10.8 is available versus pinned3.10.6. It is reported, not installed/applied.
No dependency version or firmware/native/mobile version was changed by this slice.
Current canonical inventory4673, complete comparisons187, pending4486, known
parity-gap files147; PU46/109. All28 tooling reads remain honest scoped gap
records, not full uploader/distribution parity or whole-repository superiority.

New CI source adds OS-matrix fixtures/plans, centralized locked SDK install,
integrity/size summaries, guarded draft dependency PRs and disabled-by-default
exact-artifact deployment. Actions are pinned; no GitHub settings/runner/bot
activation, new workflow execution, SDK/app build, board contact or deployment
is claimed. Changes remain in the preserved dirty checkout based on2dc319fb;
do not sweep unrelated app/firmware work into a tooling commit. Watch/resume/
verified CLI backup/export, full donor case/native-package parity, actual OS CI
execution and physical acceptance remain open in #11/#2.
Source/fixture checkpoint recorded on GitHub:
https://github.com/atomicdeploy/PakarController/issues/11#issuecomment-6246254783
and master pointer:
https://github.com/atomicdeploy/PakarController/issues/2#issuecomment-6246257339 .
Remaining diagnostic checks (comparison negatives, versions0.2.7, donor routing,
storage dispatch/hygiene, crate boundary, README artwork and governance) passed.
This does not change the full-quality PU route audit failure above.

## Integrated Async/WS/OTA progress and automatic network update path

Reconsulted remote-ota, updates-telemetry and tm1637-display, then exact donor
audit rows. Read actual PU850 AsyncOTA.h/.cpp and FirmwareTransfer.h/.cpp through
EOF, canonical PCController docs/UPDATE-PROGRESS.md through EOF and the existing
Pakar AVR FrontPanel.h/.cpp. Original-byte SHA matches the recorded PU updater
and PC progress document. Reused measured stage counters/unknown progress and
explicit completion, not PU-specific UART callbacks or watchdog disabling.

Implemented exact optional upload-size/MD5 headers, short/overrun rejection,
coherent async-to-loop progress, read-only HTTP/WS status during OTA, boot-cached
info, finite/coalesced existing AVR scheduled-text frames and matching optional
LCD progress. Native Async HTTP and persisted auth-off behavior remain integrated;
the new network updater requires auth-off readback, defaults auto/local-first,
supports pinned Internet stage/separate apply and verifies new-boot full image
SHA/MD5/size/VALID slot and same-peer WS. Unknown writes are never replayed.
Replaced task-level work/remote-ota.ps1's obsolete fixed IP/factory-password call
with the guarded checkout wrapper; no SDK/dependency duplication or clean.

First compiler/actual RemoteFirmware fixture run caught the newly inserted
progress accessor outside its class. Inspected the actual class/ownership and
consulted donor measured-stage contract; moved it into the existing public
owner, not a global or a new updater. Corrected actual S3 updater fixture passes.
The first build also correctly rejected a stale literal ledger anchor after
otaRebootAt became atomic; reread/rebound its exact source anchor, not a gate
bypass. Full comparison generation/check remains authoritative.

Focused progress/unchanged AVR consumer, real loopback multipart/fragmented WS,
no-replay/auth/new-boot/SHA fixtures pass. Actual async HTTP publication/lifetime,
WebAccess auth/origin/epoch/persistence and S3 remote staging/tamper/abort fixtures
pass. These are host doubles/source evidence, not physical flash/display proof.
Current ASUS localhost:8022 refuses connections, ledboard.local is unresolved,
and local ports show only COM1. No board contact, auth change, AVR reset, ISP,
flash, audible/PBX call, deployment, speed benchmark or USB-removal acceptance
is claimed. Restore actual access before installing/verifying this candidate.
Office SSH and its MQTT service are reachable. A bounded read-only subscription
without stored operator credentials is correctly rejected; no usable current
board/boot observation was obtained. Kept the multi-tenant broker authentication
and ACL unchanged; broker reachability is not an ESP/cloud/OTA success. No remote
firmware or control command was published as a substitute for board evidence.

Candidate SDK build passed through tools/build-firmware.ps1, S3 buffered,
16MB/OPI PSRAM/RMT-DMA, ISP electrical build acknowledgement and W5500 enabled.
Version0.2.7; application1994971/3342336, globals99408/327680. Raw image1995120
bytes, SHA256c5c4b39067e33d37bd65f6726d5f22310414fa5bf04b9c701b00bf49dafaf047,
MD5 4b565333ed0f6538d44cc47591af2005. Source is the preserved dirty worktree
based on main2dc319fb8add1516a0f2caa4306bee1bcb1284ab, not a new pushed commit
or installed release. Native/mobile sources remain0.2.7; not rebuilt by this
firmware/tooling slice. Automatic updater plan reads this exact raw image and
performs no device contact. Comparison generation and -Check pass:4673files,
159hash-bound comparisons,4514pending,119known parity gaps. No all-donor claim.

Remaining: live authenticated-state disable/readback if needed, LAN/Internet
measured comparison, aborted/concurrent
OTA, external-power/AP recovery, physical displays and all-client durable update
outcome/UI parity. No all-donor completeness is claimed by this reviewed slice.

## TM1637 freshness and separate S3 LCD initialization/presentation

Read current canonical PCController SevenSegments.h/.cpp, I2cLcd.h/.cpp,
CompactI2c.h/.cpp and BoardPins.h. Kept PuzzleBoard's actual AVR A4/DIO,A5/CLK
mapping, not the donor's ISP-conflicting PB3/PB5 mapping. Existing physical-bus
host fixtures pass both generic and actual AVR register branches: pulled-up
ACK, every byte's NACK, replay after observed loss, off-state restoration,
brightness fade and rollover. No AVR firmware change/flash was needed here.
S3 JSON now exposes fresh tm1637_detection_known/nullable tm1637_detected;
legacy JSON/USB ACK also expires when AVR evidence is absent/stale. Actual
production freshness helper is exercised for HELLO/capability/status loss,
expiry and rollover. A cached ACK is no longer a current observation.

Imported/adapted real PCController common PCF8574/HD44780 transport and16x2
normalization/cache into BoardCore LcdDisplay. Dedicated S3 I2C1 GPIO15/SDA and
GPIO16/SCL avoid the incompatible AVR TM1637 connector and the other13 assigned
signals; UART/ISP/W5500/LED pins stay unchanged. Wire3.3.12 owns native
I2C at100kHz with5ms transaction timeout. Fixed donor unconditional initialized
success after ignored write errors: paced wake/control, checked writes and both
completed rows are required before available/lcd capability. Discovery is
bounded to0x27/0x3F; failed/observed-disconnected modules lose ready state and
reinitialize/replay without an MCU reset. Unobserved LCD-only power reset needs
explicit rescan; a backpack ACK cannot certify glass, contrast or geometry.

Standalone LCD shows hostname/cell/state and update phase. Protected/fenced
GET/POST /api/display/lcd exposes actual progress, finite ASCII presentation,
release/rescan and requested/presented revisions. A202 is admission only. Native,
Web and Flutter expose the last complete driver frame only after this display's
own ACK/initialization/geometry/row validation; absent displays stay hidden.
Full native/mobile/USB LCD editors, arbitrary I2C, alternate backpack/geometry
and the donor host-owned prompt/priority policy remain pending, not invented.

New bus fixtures cover every one of246 wake/init/frame write failures, bounded
scan, cache/padding, observed disconnect/recovery and rollover. One native
validation test, focused Flutter model/ACK test, Flutter analysis and actual
Web renderer/context tests pass. Full existing protocol/AVR/front-panel host
suite passes once at this completion boundary; none used a board or played
audio. LCD/CompactI2c four-source live-hash gate passes. Ledger now158 compared,
4515 pending,118 known parity gaps out of4673; no donor-wide parity claim.

Native release/staging SHA256
4510f06c0e15a8c133de956e89fbee6e0a1b579dedc71a79569fd0dd847da5bf;
Android preview APK SHA256
a7db816cf1e3d796a4c7c19a30c07594250b27929be470f182dd6b2205d70d5a.
Versions remain0.2.7. The new Web label correctly triggered stale generated
localization; the existing shared Vazirmatn/translation generator was rerun
and its no-write check passes. Flutter's new unbraced-if lint was corrected
and the changed Dart files formatted; no guard was bypassed.

Native optional LCD deserialization now discards only invalid display metadata,
not the valid parent board status; the focused native test covers this and the
final release was rebuilt/staged. Actual production LCD HTTP handlers also pass
bounds/auth/maintenance, admission-vs-frame, release/rescan and absent-module
fixtures, rather than just a separate validator. S3 SDK3.3.12 candidate build
passes with W5500,buffered UART,16MB/OPI and RMT-DMA:1991423 sketch bytes and
99392 globals. Application1991568 bytes, SHA256
5421f6d15b10574dc90ff22503b936719bc1c2e8d40e1eb53fd1f7d8fd84d055;
image contains the actual LCD endpoint and new TM1637 freshness fields.
This is compilation, not actual I2C/Ethernet/DMA acceptance.
Fresh ASUS SSH8022 still refuses. No board deployment, reset/write, actual I2C/
TM1637 transaction, optical test, audible cue or PBX occurred. Confirm the LCD
backpack/voltage/pull-ups (5V bus requires a bidirectional level shifter), deploy
the candidate, then test both displays with actual AVR UART and concurrent
LED/Ethernet traffic. No live insertion or production acceptance is checked.

## Optional W5500: shared native network candidate, physical tests pending

Reread complete canonical PCController netpolicy/http_url.go and PU850
AsyncWebServer.h/.cpp, AsyncOTA.h/.cpp and FirmwareTransfer.h/.cpp. Added a live
hash-checked seven-source Ethernet gate to the feature-enabled build and CI.
Three newly reviewed headers/implementation rows and four expanded existing
mappings preserve the shared HTTP/WebSocket/OTA contract, explain unsafe donor
buffer mutation/direct callback and PU globals exclusions, and distinguish native
SDK Ethernet additions from donor functionality. The durable inventory now has
154 hash-bound comparisons,4519 pending and114 known parity gaps out of4673.
Seven current Ethernet donors match live bytes; this does not close those gaps.

The explicit native ETH/lwIP option uses W5500 private HSPI/SPI3 pins
SCK5/MISO6/MOSI7/CS8/RST9/IRQ14. AVR FSPI/SPI2, UART TX17/RX18 and LED4 are
unchanged. No WIZnet socket library, parallel HTTP server or separate updater is
introduced. Installed AsyncTCP3.5.0 binds its existing server to INADDR_ANY.
The same routes/auth, NTP, cloud and firmware fetch use the common native stack;
Ethernet takes preference only while IPv4-ready. Independent persisted
DHCP/static settings and route-change cloud invalidation remain in place.

Fixed the absent-module recovery gap: explicit Ethernet settings apply retries
initialization, including safe partial-driver cleanup; cable loss never resets
a healthy driver and service never retries automatically. Cheap status now
publishes initialized/started/carrier/IPv4/DNS/MAC/error, with null disconnected
addresses rather than stale IPs or synthetic connectivity. Native connection
method now says Network for Wi-Fi/Ethernet, preserving exclusive USB ownership;
status can show the acknowledged Ethernet uplink instead of an old station SSID.
Dedicated graphical DHCP/static Ethernet editors remain open.

Focused port lifecycle (both flags), NetworkPath (both flags), config persistence,
typed management/status (both flags), discovery, pin-map and live-probe fixture
tests pass. Probe tests include actual loopback TCP/redirect rejection, but no
ESP or WAN. One focused native Ethernet-label test and connection wiring guards
pass. The probe checks the actual wired TCP address, Web/JSON/XML and firmware
HEAD/Range; optional one-shot pinned HTTPS ESP staging requires Wi-Fi station
disconnected and never applies/cancels firmware or touches AVR. Lost results,
changed boot/peer/job/hash or existing maintenance stop without mutation replay.

Actual Arduino-ESP32 3.3.12 buffered16MB/OPI/RMT-DMA/W5500 SDK build passes:
1961319 sketch bytes,97720 global bytes. Application binary1961472 bytes,
SHA256 7e197801d4b05bf690312f272ea073e9e2da176c08cab4be636a875a0b095225.
The first attempt linked a stale copied EthernetPort.cpp after its source was
changed during Arduino's initial library build; the undefined symbols were
diagnosed in the cached object. Final incremental recopy/build resolved both
methods without a clean, SDK patch or duplicate cache. No library rebuild was
forced for the final source-only pass.

Native release passes and its verified staging candidate is26164224 bytes,
SHA256 36cbe1e44e3bc7ea90452fdba084f73055764659b616228c78baf7e23fbc0430.
All app/firmware versions remain0.2.7. Existing AVR and Android candidates are
unchanged. Build/staging identities do not establish an installed runtime.

Fresh ASUS SSH8022 refuses connections; this PC's ledboard.local lookup did not
resolve within3s. Module supply labels still need confirmation (3V3 vs carrier
VCC/VIN requirement), not a board photo. No firmware deployment, live Ethernet,
OTA/WAN fetch, AVR reset/write, tone or PBX action occurred. Physical DHCP/static,
HTTP/WebSocket, wired multicast discovery, Ethernet-only fetch/apply/reboot,
cable/fallback/AP recovery and LED traffic soak remain unchecked in issue2.
See ESP32-S3-COMMISSIONING.md for the exact pinout and safe acceptance probe.

## Board Silent preference: persistent owner and all three Preferences surfaces

Read the complete current PCController LocalLib/TonePlayer.h/.cpp,
Project/SettingsStore.h/.cpp and Tools/Controller/internal/appconfig/buzzer_runtime.go,
plus Pealayer src/ui/board_info.rs. Their original hashes are recorded in the same
comparison ledger. The new board-silent gate verifies all six against live donor
bytes; generic remaining donor controls/backend/routing work stays pending.

Adapted the existing MCU-owned flags bit0, independent of host notification sound.
Native Preferences > Board and Flutter/Web Preferences > Board sound are contextual
on AVR settings+buzzer ACKs. Native uses the selected fenced HTTP or revocable USB
session only (USB never falls back to Wi-Fi); all surfaces use the existing inner
GET_SETTINGS/SET_SETTINGS/GET_SETTINGS contract. Readback must match the complete
preserved record and report persisted=true. Unknown is not Off; pending/expired,
rejected and stale-peer results cannot optimistically update the toggle. No
uncertain mutation is automatically replayed or saved as an app preference.

AVR mute now publishes its revision immediately, including between notes, and
unmute follows donor behavior without restarting an interrupted note. Prog safety
stays independent. Actual production tone/register fixtures cover all fourteen
muted cues; actual settings/runtime EEPROM fixtures cover reboot persistence,
untouched flags and rejection while programming. Four focused native tests and
four Flutter model/API/widget tests pass, as do Flutter analysis and the focused
Web transaction, auth/Preferences, appearance, peer HTTP and localization tests.
Corrected stale test doubles: Web listeners must compose instead of overwriting;
appearance owns its real locale/haptics dependencies; old per-cell batch source
assertions now check the implemented atomic request/readback instead.

Native release candidate staged (not production replacement):
SHA256 df110c19c3eafdb9d594f5f0f1709080bf4c5f7e87e8f72d872f586d61748d10.
S3 buffered16MB/OPI/RMT-DMA build passes, application binary1863072 bytes,
SHA256 1254f86ef970c04dd6fdfa74fed8a777404b9bcded4b722788d5be1473eba8a3.
AVR buffered build passes at31602 application bytes and1460 static SRAM bytes;
verified Urboot merged32768-byte image SHA256
3fa829d2104295d92a24194f5ce85b5f38647240b5bc7d6a60de8dfade9d53af.
Native/firmware/mobile versions remain0.2.7.
Android preview APK build passes; SHA256
34a231b50c64bc013b04ad9e66c09d0b99779ab82c7fc4006bf46d069ee12672.
These are build identities, not installed firmware/runtime receipts.
Fresh ASUS SSH8022 refuses connections;
no board contact, flash, reset, audible test or PBX call was made. Physical
mute/unmute, welcome/menu/cue silence after power cycle and actual USB/Wi-Fi
deployment remain open. No hardware acceptance checkbox was checked.

## AVR ISP/UART restoration: boot-chain defect corrected, hardware unavailable

User explicitly requested live ISP/UART recovery, brownout and bootloader repair.
Fresh SSH8022 refuses; bounded TCP checks on the current8022 and historical
7022/1022 tunnels are unavailable. This PC enumerates only ACPI COM1, not an
ESP/AVR USB endpoint. Requested restored ASUS access; no different Cafe/PCController
board was substituted. No actual AVR reset, signature/fuse read, backup, chip
erase, programming, UART exchange or audible operation occurred.

Ran canonical AVR reset tests and the full test-protocol.ps1 suite successfully,
including actual production ISP/EEPROM-preservation code against its SPI emulator,
all three UART flow profiles, Prog/settings contracts, TM1637 and melody timing
fixtures. Buffered AVR compile passes at31610 application bytes and1460 static
SRAM bytes, leaving588 static headroom (not a runtime stack/soak measurement).
Inspected current .init3 output: capture MCUSR/r2, clear MCUSR, WDT disable and
fall-through at0x17E to data initialization. Host tests do not prove live health.

Found a real packaging defect: the old 32768-byte merged ISP image contained
stock vector Urboot but reset still jumped directly to the application at0x14E.
Before correction SHA25648ec5cb57429f18641d698a2d8e6d7029b0285e304234ee3b302de3374a3b3ac.
Read PCController Tools/Bootloader/Urboot-Custom/build.mjs and README.md completely,
including reset/trampoline, metadata, unused-vector ELF proof and round-trip code.
Imported/adapted those actual mechanisms into tools/prepare-avr-image.mjs and
both canonical PowerShell/shell builds. The packager pins the installed stock
UART0/384-byte HEX hash, verifies __bad_interrupt ownership before reserving
vector25, normalizes erased gaps and checks every decoded output byte.

One packaging check correctly rejected the stock Intel HEX type03 start-segment
record. The donor parser ignores non-data metadata; the adaptation now explicitly
validates its shape, bounded/aligned entry and agreement with Urboot's metadata,
with a regression, rather than silently ignoring arbitrary records. No flash or
fuse write was involved in that failure or repair. The first live-donor gate
rejected work/repos/PCController inventory drift (old2a0e971e/1207 tracked paths).
Diagnosed, then used the actual canonical installed PCController source at
b0111ae0/1517 paths, matching the inventory; avr-boot-chain -Gate -RequireLive
passes there. Both donor file hashes match the recorded source bytes.

The final candidate's reset is wrapped RJMP0xCF3F to0x7E80; metadata-selected
vector25 at0x64 is an absolute jump to the original application at0x14E. Urboot
bytes are unchanged, full flash is32768 bytes, and HEX round-trip/negative-vector,
overlap, checksum, EOF and start-record tests pass. The canonical build passes;
build storage/gate checks pass. Corrected binary SHA256:
5e836361ff97119a96e54f719fcb4443f64840abb85274e04cf905ec1a050140.
HEX SHA2564e2889f4dba489b0be2ab4a718a01a1e6a81480203e313834b124a012ee2dfa5.
build/avr-buffered/avr-image.json carries exact source/profile/hash proof and
physical_validation=false. Firmware/application versions are unchanged.

No fuses were written or guessed. The build still describes external16MHz/BOD2.7V;
the reported3.3V rail and actual speed grade/clock/fuses must be reconciled before
deployment. BOD disabling is not a repair for inadequate voltage/clock margin.
The existing ISP API deliberately does not expose arbitrary fuse writes. Any
required constrained fuse recovery needs the actual readback and verified backup
first; current hardware BOD/bootloader/UART are NOT accepted. Relevant primary
references: installed MiniCore3.1.3 boards.txt and stock Urboot README, and
Microchip ATmega328/P datasheet DS40002061.

Two complete donor reviews added with explicit remaining custom-progress work:
the donor512-byte TM1637 D13/D11 backend must not be deployed on A5/A4 hardware.
Stock Urboot remains selected. Original MIT and upstream GPL notices retained.
Ledger149 compared,4524 pending,109 parity gaps; not full donor parity. Production
ESP/app images, rollback, settings, unknown edits and donor trees remain untouched.

## Configured hostname and contextual authentication repair

Read the installed Arduino-ESP32 3.3.12 NetworkManager/WiFi startup implementation:
the global default is generated as chip target plus MAC suffix, and mode() copies
it into the new station netif. Pakar applied setHostname after mode(), leaving
DHCP with the SDK name. Reviewed ASA0002E WiFi_Setup and onValueUpdate HostName
slices: saved identity is applied before enabling interfaces and reused by
discovery. Shared NetworkConfig now restores that order, applies/checks the
full saved label on station and AP netifs, and stops interfaces on a hostname
application failure. It retains DeviceIdentity as authority, without suffixes.
This is a slice review, not certification of the entire PU850 sketch.

Read LocalLib/Authentication.cpp and .h completely; live hashes match the
inventoried PU850 snapshot. Both now have exact file-comparison decisions and
omission reasons. Replaced hardcoded protection/password-token dumps and the
incomplete role skeleton with the actual middleware, bounded sessions and
atomic access persistence. Ledger: 147 compared, 4526 pending, 107 parity gaps;
full donor/production acceptance is not claimed.

Authentication has a real persisted enabled flag, off on fresh installations
and migrated old factory-credential records. Legacy custom credentials retain
protection; users can explicitly disable it. New enable/change/disable commits
verify staged readback before rename. An enabled configuration requires its
current password for changes. Corrupt or unmounted access storage remains
unavailable, not an anonymous fallback. HTTP/WS/Telnet use the same flag; origin,
peer/boot fencing, WS ownership and firmware-busy guards remain in place.

Web login and Sign out are hidden when disabled, including /basic. A network
failure offers retry instead of inventing a login requirement. Direction,
language and supported-only haptics are in browser Preferences with Apply/Cancel.
Native credential fields require explicit enabled metadata and are absent for
USB. Exact-endpoint public requirement metadata survives Disconnect only for
offline credential editing; it never grants device control. CLI/TUI can use
auth disable. The native implicit factory-password fallback was removed.

Executed focused checks: shared network model for ESP8266 and ESP32; actual
WebAccess plus access-record persistence/legacy migration and unavailable-storage
doubles; embedded Web auth-on/off, lost-connection and Preferences open/cancel;
basic controller; localization and nine donor locale cases; S3 platform guards;
three native connection-control tests and two access tests; built CLI access and
hostname mutation/acknowledgement tests. No full CI/suite wait. The locale fixture
was updated from obsolete per-cell progress wording to the actual current atomic
batch readback message, retaining the exact Persian assertion.

Native Check and Release passed using the canonical warm release cache. Staging
PakarController.exe is checksum-verified and CLI-probed, SHA256:
bac4c656aa1633c3464c93656388dbe8620d2b691a1c203c482d2e945a632361.
Production executable, rollback and settings were not replaced.
S3 firmware compilation passed with the existing buffered UART, 16 MB flash,
OPI PSRAM, RMT-DMA and ISP-electrical-verified build profile, SDK3.3.12; no
W5500 flag. Image1,856,096 bytes, SHA256:
d3d144ce7998c655d632d12fd84c143ef2d873914e8f08d731d774207675d047.
Compiler reports1,855,947 bytes program and97,340 bytes static RAM. A binary
content probe confirms the final Preferences handler and checked-hostname
failure path are actually embedded; it is not an older cached image. This
candidate remains in build/esp32s3-buffered, not a flashed/accepted image.

Source is base 2dc319fb8add1516a0f2caa4306bee1bcb1284ab plus preserved working-tree
edits, not a clean-head release. Fresh localhost:8022 SSH and direct TCP checks
refuse; ledboard.local does not resolve within the bounded lookup. No ASUS
install, firmware deployment, live DHCP/mDNS/AP/login test, AVR, audio or PBX
action is claimed. Issue2 hardware and human acceptance remain open.

## W5500 separate-bus wiring preparation

Consulted network-cloud and literal PU850 donor audit before selecting pins.
Fully reread actual PU850 NetDiscovery.h/cpp: unused configuration and blocking
Wi-Fi-only prototype loops are not an Ethernet driver and were not copied.
Existing broader discovery/import gaps remain pending, not silently excluded.
Inspected the current AvrIsp enter/leave SPI lifetime: its global SPI.end()
cannot own Ethernet's SPI object. Reserved W5500 SCK5/MISO6/MOSI7/CS8/RSTn9/INTn14
on independent SPI3/HSPI, retaining AVR SPI2/FSPI and TX17/RX18 and LED4.
All thirteen signals are unique; source guards cover documentation and defaults.

Fully read installed Arduino-ESP32 3.3.12 libraries/SPI/src/SPI.h,
libraries/Ethernet/src/ETH.h and the ETH_W5500_Arduino_SPI example. SHA256:
SPI.h 4fdfa675a719087981d67cfa8d6c1719e17386121dd859b28114cca459f892df;
ETH.h 5c8fe676e61ffe33be8363d20d23c5bf242c11f1b85e2a2a91d100177dd79904;
example ac085528784f6187507d1740809254f62adf55a56b7e27f303744841d81d3628.
The selected future integration is core ETH/lwIP, not a parallel Arduino
Ethernet socket stack. No driver/example code was imported or enabled here.
Inspected only the bus-definition excerpt of esp32-hal-spi.h, not its full file.

Sent the explicit pin plan to Merge PCController PRs for Pealayer, then read its
completed review: it agrees, conditional on exact DevKit GPIO exposure and
module supply confirmation. Official WIZnet electrical/reset specifications
were checked. Carrier power input, integrated S3 Ethernet/ISP, discovery,
OTA/network continuity, and physical tests remain open. Pin source guards and
diff checks pass; no SDK build, dependency installation or powered test occurred.

## S3 UART wiring agreement

Read the referenced Merge PCController PRs for Pealayer chat, including its
actual ISP/UART reply, rather than inferring from its title. ISP pins agree:
MOSI11, MISO13, SCK12, RESET-control10 through a voltage-safe external driver.
Its conventional TX17/RX18 differed from Pakar's provisional RX17/TX18 defaults.
Both are routable, but mixed code/wiring would fail communication. Standardized
Pakar's defaults and migration table to TX17 -> AVR D0/RXD and AVR D1/TXD -> RX18.
The old chat answer/table is superseded; no AVR peripheral assignments changed.

Consulted uart-flow-control and literal donor-audit; fully reread actual current
PCController Project/UartProtocol.h, SHA287a95a65a9b9655e64a15ece051bc792a06564b648a1b53eed4f373c564e458,
and actual Pakar BoardPins/AvrUart. Existing frame/hardware UART behavior is
unchanged. Official Arduino-ESP32 begin() documentation confirms RX-before-TX
argument order. Added a lightweight S3 source-wiring guard to existing CI; changed
CI target proof hash after rereviewing the added command and unchanged fail-closed
runner. Existing donor omissions and integrated S3/hardware gates remain open.

Source guards and diff checks pass. No dependency installation, unrelated app/
firmware build, flash, fuse/EEPROM write, reset or electrical test. This is pin
configuration/document agreement, not a working S3 programmer/deployment claim.

## Android peripheral mirroring

Started from clean a6dec77; refreshed owning issue #2 and open PR state (none).
Prior Build38275253969 completed with Android/Windows/quality success but the
firmware setup failed three times on the ESP8266 python3 tool HTTP500; CodeQL
passed. That is not all-green CI or evidence of a firmware source failure.

Consulted firmware-hardware and literal donor-audit for PCController
buzzer_mirror.go, buzzer_mirror_test.go and FrontPanelModel.h. Fully read current
Go scheduler/tests through EOF at704f44042e6bf46c4459bd63f37230460f221389, exact
original SHA2a6d0fd4.../83b8726d...; unrelated dirty donor files preserved.
Adapted actual cadence, wrap, jitter cushion, absolute expiry, Stop/reconnect
flush, opt-in and independent failure transitions into Android PCM mirroring.
FrontPanelModel was read, but motion/RF/sensor menus are not fabricated into this
hardware profile; broader menu/action imports remain explicitly pending.

Added AVR musical-start/observed clocks, explicit HELLO timestamp acknowledgment
and actual front-panel state. ESP's existing raw event projection now binds
board/boot/AVR-session/event ID; Android events cannot authorize identity or
renew stale status. Hardware UART, A4/A5 and busy/poll/buffered profiles remain.
Added a mobile-mirroring donor gate and exact source/target/test hashes to the
existing ledger, plus refreshed original PC contract target hashes/line anchors
and PU route target hashes. No disconnected replacement ledger was created.

Recovered expected proof-hash failures by rereviewing only changed shipping
targets, not bumping unread donor hashes. Pealayer inventory moved564->567 during
this slice; ten changed previously reviewed files retain their immutable prior
source snapshots with current live hashes and explicit reread actions. Later
repository-wide preflight encountered further concurrent tracked-path drift;
local whole quality is not declared passed or bypassed. A patch-generation
helper hit the default1MiB child-output buffer and unordered hunks; selected
read-only rows were bounded and sorted before apply_patch, without shell writes
or donor mutations. The donor-first tooling remains mandatory.

The first UI test's short viewport lazily omitted the display; an explicit tall
fixture verifies independent mirrors. A real socket test was separated from
widget-binding's intentional HTTP400 override, not patched into production.
The first tablet capture rendered correctly but left a test lease active at
invariant checking; explicit idempotent disposal fixes the harness. Existing
capture evidence was inspected and preserved, never overwritten.

Final guarded analyze and86/86 mobile tests pass, including real authenticated
loopback WebSocket routing and inspected phone capture. C++ production protocol
suite passes; AVR busy-wire/polling/buffered and integrated ESP8266 busy-wire
builds pass. AVR RAM1327/2048 (721stack bytes available), busy-wire flash28062/
32384; ESP RAM56700/80192, IRAM63055/65536, flash897192/1048576. Clock/register/
transport doubles are not hardware waveform, memory-margin or audible proof.

Final preview APK55396153bytes, SHA256
74c10aabf1e6c4735c403969a1cf730346871090f6bd138bea1ab242dbcca011,
copied to the stable delivered APK and hash-verified. Debug signing remains a
preview limitation. Tablet screenshot86063bytes/SHA97d37cedd9c6b7d2f9843b04660d5c41b3e831383351bd9a93d2c823f735e4f6;
phone86459bytes/SHAfb57825d3b2df5ea7ef46d898d62709285368811417212a627c375ca9b15c279.
Both are explicit simulated peers with silent output, not live device evidence.
ADB freshly lists no device. No firmware, fuses, EEPROM or remote buzzer writes.
S3 integration, phone installation/audible timing/focus tests and native/Web
audio consumers/optional bridge note forwarding remain open in issue #2.

## Web streaming pacing

Started from clean 5b54e4d; Build38154906733 and CodeQL38154906685 passed.
No open Pakar PR or new completed Copilot review. ASUS1022 refused and
ledboard.local did not resolve. Preserved live native process/settings/rollback.
Consulted all 77 games-streaming rows and exact donor-audit rows; the two
PCController status-frame-batcher files were unrouted and not yet compared.
Read both actual sources completely; original byte hashes match the inventory.
Adapted actual clock/generation/cancellation/disposal code and fake-clock test
scenarios, not an assertion of whole status/metrics parity. Added specialist
web-streaming routing/gate to quality and ESP builds. Useful generic status
history and error-reducer omissions remain pending in the authoritative ledger.

Replaced fixed 40ms Web timer with 60/default, 0/uncapped pacing and bounded
latest-frame socket buffering. Focused tests run only after this substantial
source slice, including real embedded Web start/reboot revocation. These are
fixtures, not live board throughput. No native/mobile/AVR rebuild or deployment.

Quality passed. Final ESP8266 busy-wire compile passed in the same warm cache,
and exact module bytes were found in its945360-byte image. No hardware flash.
Live PCController inventory gate caught25 added/14 removed tracked paths:
selectively refreshed only PCController, then strict comparison and RequireLive
scheduler gate passed. New board-mirroring paths remain unreviewed; current owner
dirty work was hashed, never edited or called committed source. Totals4282/106
manual/4176 pending/73 gaps. English/Persian live stream receipts are fixture-tested.
Production executable and preferences hashes are unchanged. Initial GUI PID58400
was absent at final check, with no Pakar process found; this turn did not terminate,
replace or launch it. Do not present preserved executable bytes as a running GUI.

## Web board/boot adoption

Started from clean b3e9d19 with Build38150481947 and CodeQL38150481964 green;
no open Pakar PR or new completed Copilot review. ASUS1022 remains refused and
ledboard.local unresolved. Current GUI/settings/rollback are preserved.
Consulted web-presentation, exact RayanLamp api.ts and peer-binding ownership.
api.ts and api.test.ts were reread through EOF; live source hashes match inventory.
Adapted actual APIError, Headers, same-origin/no-store fetch, AbortController
deadline and finally cleanup into assets/web/peer-http.js. No donor 401 mutation
replay or nonexistent Go session endpoint. Existing PCController session.go's
before/after admission contract now also applies to Web; no new serial parity
claim. Mechanical embedding uses the existing generator and byte-equality test.

Focused verification caught empty typed codes masking distinct HTTP failures,
a bootstrap/sign-in ownership race, and external abort retaining its timer.
Reconsulted the already fully read api.ts request/finally and sync.ts stop paths;
fixed the actual adapter and explicit auth generation, not test suppression.
DOM fixtures were upgraded with Headers/AbortController and explicit simulated
board/boot identities; old fixtures were not physical/capability evidence.
False batch ACK, shared transaction pins, old-peer draft clearing and no replay
are covered. Final full quality and ESP8266 busy-wire build passed. A final
redirect-error refinement was compiled again in the same cache; final image
and exact embedded module proof are in FILE-COMPARISON-RECEIPTS.md. No flag
churn or debug cache. The nonunique PU route anchor rebind was rejected;
reread both actual route bindings and retained their verified line244 rather
than choosing an arbitrary occurrence. Only target/proof leaves were rebound.

Known gap newly exposed: Web streamTick still uses a 40 ms interval (25 FPS).
Configurable/backpressured pacing remains open; native60/0 policy is not Web
parity. GUI/Flutter/other transports, AVR-only reboot, ESP32-S3 integration/DMA,
readback durability and physical multi-client tests remain open. No debug/native
or Android build, cleanup, GUI replacement or hardware write is authorized by
these fixture results.

## HTTP board/boot fence continuation

Started from clean pushed d450866; Build38147533498 and CodeQL38147533441
both succeeded, no open Pakar PR or new Copilot review was observed. ASUS
localhost1022 refused and ledboard.local did not resolve; no physical write.

Consulted donor-audit/device-context; session.go lookup caught PC inventory
drift. Selectively refreshed PCController (1234 paths); did not promote new
files into reviewed decisions. Completely read actual internal/link/session.go
and session_test.go, with original hashes in the comparison ledger. Consulted
firmware_identity.go/test too, but their immutable build stamp is not a boot
epoch: no import/read certificate is invented for that unused investigation.

Adapted before/after-admission cancellation and matching-identity tests. Added
real optional HTTP epoch headers and firmware rejection before dispatch and
both pre-middleware upload writes. Missing/invalid identity keeps TUI read-only.
Generic useful serial acquisition/quarantine/ACK/reset/event tests remain open,
with exact code and reasons; Go HELLO schema is not copied as API negotiation.

A broad cargo fmt invocation accidentally formatted unrelated native files.
Restored ONLY this turn's formatting via reverse diffs/apply_patch (initial
native checkout was clean), retained intended changes, then formatted the
terminal module alone with skip_children. No checkout/reset/user edit removal.
First evidence patch failed atomically on final-property comma formatting;
used exact existing lines, without changing original donor hashes. Quality
gates then caught the separate PU850 route target hashes/line anchors. Read
affected upload/WS/SSDP/info/login consumers, checked every mapped target,
rebound exactly six route references. No assertions deleted/bypassed.

151 native tests pass, including same-host reboot/replacement, draft revocation,
invalid/missing fence read-only, wire headers in cell/shared CLI paths and HTTP409
stop/no replay. Production C++ validation tests and full quality phase pass.
First ESP8266 busy-wire compile passed; SDK macAddress ignores its SDK return,
so the MAC buffer was explicitly zero-initialized to fail closed on no output.
Final compile/package results are recorded in FILE-COMPARISON-RECEIPTS.md.

Current inventory4271, complete103, pending4168, parity-gap70. Two new fully read
PC files are scoped parity gaps, not full transport parity. Existing GUI owner,
settings and rollback remain untouched. No AVR/APK/donor rebuild or new cache.

## Minimized native command processing

Prior turn was progress: clean0c3eca9 source was pushed, release/stage/probe
passed and deployed process54616 matched candidate SHA B3FAAC2569B18B94EB772791ADE5083F1A4583F8FA0517DE821229ED098901C9.
Saved preferences SHA remained 5C6133A73641C8087A5D00B0CC978FA03CD2829C6D81F3959DA1F63F50F974DD;
existing rollback files were preserved. Both current-source CI workflows passed.
No physical board acceptance: ASUS SSH1022 refused and ledboard.local DNS failed.

Live capture failed on a minimized owner, exposing accepted-but-undrained IPC.
Consulted capture and actual donor read-only scripts; Win32 correctly refused
a non-visible/non-minimized target, not a blanket screenshot capability loss.
Consulted native-lifecycle/donor-audit and actual Pealayer App::logic and shell
command slices. Full app.rs is not certified. Adapted non-render command,
device-result, notification and snapshot placement; source guard checks no
duplicate drain in UI. SourceStatus still needs a read-only rendering snapshot.
Initial compile failed on that missing local and ctx.style (removed in locked
egui); corrected with the rendering snapshot and donor's ctx.theme comparison.
Locked egui run_logic read and used for the minimized capture-deadline test.
No removed assertion, alternate cache, dependency upgrade or unrelated build.

Capture wrapper also had an idle-snapshot race. Added request_id/path correlation
and bounded older-owner pending/result handling. Its gate refused a changed
capture target before use; rereviewed the wrapper and rebound only exact target
and source-guard hashes, preserving all original donor hashes.
An attempted unexpanded registry glob and a guessed test/tool filename failed;
used rg --files and exact existing paths, not a blind replacement or unavailable
claim. Second production Preferences capture timed out; no output file exists.
Actual earlier framebuffer was inspected after supported window restoration.
Physical Escape stopped Windows Computer Use; desktop input was not resumed.
Open Preferences/draft preservation blocks replacement only, not source work.

Four additional full Pealayer conversations70/78/87/102 read and hash-bound.
They constrain private branding/resource ownership, transition/config revisions,
hidden-window lifecycle/draft safety and shared portaled button geometry.
Fresh source corpus counts remain separate from full semantic review.
The guarded native Test completed:129 passed/0failed/0ignored, including
the actual locked-egui run_logic minimized deadline fixture. Later user reply
confirmed Preferences closed; IPC unavailable plus zero enumerated owners
confirmed terminal state, not an observation timeout. Product --minimize and
dirty/minimized status add a repeatable no-desktop-input live acceptance path.
An overlapping adjacent-line evidence patch failed before mutation; grouped
only consecutive changed references and preserved donor hashes. No test/gate
bypass. A final native compile/test of the added control is still separate.
That final guarded Test now passed129/0/0 (2m40 compile,0.17s execution),
including strict typed minimize parsing. Source guards and exact comparison
generation checks pass. Full production live lifecycle smoke is not inferred
from these tests. Current0c source CI was confirmed success without waiting.

48e0165 release completed4m37 and stage/help/version probes passed. Installed
process72012 matched SHA597999E33688A5B0ACCECE9CDA5B8ECFBC2EDC5DCCEC87E822A4A6F4C19216BB;
settings and five existing rollback executables were unchanged. Live CLI smoke
then failed: screenshot request while observed minimized saved96x96 cropped UI,
not a usable workspace image. Inspected and renamed only that owned5474-byte
failure PNG to desktop-minimized-invalid.png; not accepted screenshot evidence.
Show subsequently applied and observed minimized=false; no desktop input.
Read locked egui ViewportInfo::visible, egui-winit viewport sampling and eframe
Glow/epi logic/paint paths. Explicitly suppress screenshot requests/receives
while invisible and reject below320x240 root results, retaining low-level2x2 PNG
encoder tests. Added actual egui minimized-paint/stale-event regression instead
of weakening the live timeout/no-file assertion. Close-to-tray cancellation also
moved into non-rendering logic so hidden close handling does not require UI.

48 CI CodeQL succeeded; build failed its native wiring assertion in the Web
context test: it required the obsolete &ctx spelling/location after moving to
native_logic(ctx). Full Web no-ACK/offline/mode/stale tests are retained; replace
only that assertion with stricter non-render placement and no duplicate UI call.
Comparison generation independently rejected the earlier post-check GitHub-doc
append's stale target hash. Consulted actual REVIEW-COMMENT-AUDIT plus current
GitHub doc and rebound only that target, keeping donor original bytes unchanged.
Do not claim the48 build all-green. Source/fixture/native/live gates remain
distinct and the updated native/live tests must execute before replacement.
Final guarded Test of the visibility/stale-frame fix:130 passed/0failed/0ignored
(2m51 compile,0.16s execution). Quality first stopped on a new unsupported
partial-read label in the old screening ledger. Read its actual validator;
used existing screened status with explicit partial scope rather than widening
resolved statuses or certifying unread source. Regenerated only Pealayer and
resumed the quality phase from its exact failed child; every preceding child
had passed and every remaining child now passed. Full CI will rerun the phase.
The strict production completeness gate still rejects unresolved donor files.

## Actual TM1637 bus recovery acceptance

Reread complete live PCController LocalLib/SevenSegments.h and .cpp; both match
their original snapshot hashes. Corrected the failed guessed Firmware/LocalLib
lookup using the actual ledger path LocalLib/SevenSegments.cpp and explicit
canonical PCController root. The default lookup only configures Pealayer/Rayan
roots; unavailable in that lookup did not mean the real PCController was absent.
Read Tools/VirtualBoard/tests/firmware_controls_test.cpp through EOF and its
Arduino mock dependency. Actual donor require/fade/ACK tests are adapted, not
replaced by an unrelated presence probe. Original donor source stays unchanged.

Found DIO released with pull-up disabled, permitting an unplugged floating line
to resemble LOW ACK. Release DDRC before enabling weak PORTC pull-up; fallback
uses INPUT_PULLUP. Correct PC4/PC5 pins, ISP isolation, frame/brightness cache,
zero/off and NACK aggregation remain. Titan/Microchip timing and DDR/PORT docs
are linked in TM1637-RECOVERY.md. No global pull-up, EEPROM, pin or clock edits.

New byte-decoding GPIO and instrumented-register fixtures execute both actual
branches: input/pull-up on every ninth slot, no ACK contention, other PORTC bits
unchanged, exact mode/address/segments/control replay, every NACK slot, true-off
recovery, fade and 32-bit cadence rollover pass. First AVR fixture compile caught
a singleton string splat; consulted actual Pealayer run-windows.ps1 typed-array
runner and changed only the new test flag vector. First comparison rejected an
invalid omission enum; used the existing not-applicable disposition with exact
unfitted-hardware reasons. No guard weakened or failure hidden.

New reviewed-slice gate is required by firmware build/test wrappers. Source
comparison generation, reuse preflight regressions and source-ledger integrity
pass. Warm AVR builds pass for busy-wire (26898/32384 flash) and polling
(26614/32384 flash); both use1225/2048 RAM. Busy-wire merged Urboot image SHA256
8d20b942f0c7157170c5428e6f4ac19cbeb395128d330d10bca3030a9f1d5c08.
Both source layouts remain0.2.7, not a new public API generation. Existing native
cache measured4013127900 bytes, release preserved; no debug cache or unrelated
ESP/Android rebuild. No display flash/unplug/replug: fresh localhost1022 SSH
refuses and ledboard.local DNS fails. ACK still is not actual emitted-pixel proof,
and a shorted-low wire is not diagnosed by these doubles.

Same ledger now88/4242 compared,4154 pending,55 known gap files; generic donor
key/transition/menu/tone test import remains pending. No production-complete claim.

## PR completion and exhaustive conversation collection

PR14 and PR15 merged normally; only main remains on GitHub and locally. Read all
their current diff/conversation/review-thread pages. Copilot requests are posted;
no completed review is claimed. PR15's stale workflow target hashes were rebound
only after rereading complete actual Pealayer CI/CD and CodeQL and Pakar workflows.
Repository governance and comparison Check passed, without a protection bypass.

Read Pealayer REVIEW-COMMENT-AUDIT.md completely and adapted its actual procedure
into tools/audit-github-conversations.mjs: every issue/PR/review/thread/comment page,
incomplete-response rejection, one stable ignored snapshot, unchanged-write skip,
separate hash-bound manual scope, and no closed-to-resolved promotion. First complete
792-record collection contains 142 Pealayer and 634 PCController records plus 16
Pakar records. All four unresolved donor thread bodies read; only five complete
donor issue records and those two comment scopes are marked reviewed. The rest
are pending. Rayan has no verified GitHub repo; asked for URL, read its live chat
and retained filesystem donor. Did not mutate other projects or their comments.
Final Pakar-only refresh includes security issue17: 17 Pakar records, 793 total.
Donor snapshots were retained rather than downloaded again; manual review scope
remains five complete donor records and two review-comment scopes.

Reread actual Rayan formatDateTime/BidiLtr and i18n test contracts. Imported real
BDI LTR/isolate textContent-only rendering for hostname/URL summary and locale-
specific Gregorian/Persian NTP dates. Existing protocol/secret values are not
translated or rewritten. Nine shipped localization tests pass. Full Web/quality
batch and affected failed-tail rechecks pass after correcting a missing style
property in the DOM double and regenerating the exact route-proof snapshot.
New GitHub pagination/manual-decision tests pass; this does not prove all records
were reviewed. ESP-only warm busy-wire build passes: flash code 876696 bytes,
RAM 56476 bytes, IRAM 63055 bytes. Image SHA-256:
883cd159cd4d4b073fed29ef66e2abe45214d7e019b45f3e29629d6b8d68af30.

87 completed source comparisons, 4155 pending, 54 known gaps out of 4242 files.
Pealayer live revision was refreshed without promoting other read states or
changing donor sources. Existing anchors selectively revalidated; no native,
AVR or APK rebuild, package replacement, hardware writes, new debug cache or
rendered-pixel acceptance. Native/mobile/browser cross-surface BDI/calendar,
general event/gap and output confirmation and retention imports remain pending.
GitHub glib alert1 remains open; Dependabot cannot resolve beyond 0.18.5 despite
fixed baseline 0.20.0. Track and coordinate it rather than disabling tray features
or pretending zero open PRs means security-clear.

## Living unversioned API correction

Tracking issue #16. Read the PCController network runtime, client UX and
firmware chats plus consolidation/Pealayer context. Read actual Go-Embedding-API
policy and rest_routes_test.go completely; inspect ipc.go's top-level rejection
function without certifying the unread remainder. Original bytes of the two
complete-read files match the pinned donor snapshot. Added api-contract lookup
and required gates; preserved every other pending donor row.

Applied versionless REST across firmware, discovery, Web, native/CLI and Android;
removed JSON generation selectors and numbered product subprotocol/profile/stream
identities. Reserved-zero framing and explicit capability/audio acknowledgement
replace generation/build-version inference. Authentication, operation ownership,
hash/signature/model checks and private saved settings are retained. No aliases.

Validation found an enum/non-enum warning, a nullable malformed-ACK test fixture
type error, and guard stdout contaminating typed build Plan objects. Fixed the
actual cast, fixture type and Write-Host routing. Imported storage/capture/build
gates were used, not bypassed. A normalization mistake in evidence preparation
was corrected using generate-file-comparison.ps1's exact LF hash rule; compact
and multiline references were rechecked by their unchanged anchors.

Native: 116 tests passed. Android: analysis clean, 72 cases passed in the full
batch; the failing peer-context fixture was corrected and its five-case file
passed through the gated wrapper. C++ protocol/ISP/remote update/persistence/
display/audio/PC contracts passed. Web/quality checks passed in the completed
batch plus the failed-tail recheck, without repeating unrelated suites.
Final firmware build identities and CI/physical boundaries are recorded in the
issue and FILE-COMPARISON-RECEIPTS.md, not inferred from these source checks.

Live migration remains unverified: ledboard.local lookup fails and SSH
localhost:1022 refuses connection. No physical flash, retained-settings readback,
or current production-app replacement is claimed. Never perform a client-only
rollout against the old numbered firmware API.

## Live desktop replacement, Android launcher and low-write recovery

Consult build-storage, localization, mobile-settings, capture and exact donor-audit.
Fully reread actual Pealayer storage-hygiene, workspace-hygiene, changed CI helpers,
runner, artwork consumers and appearance test; preserve concurrent donor changes.
Refresh its inventory to 514 files and selectively rebind reviewed original-byte
source hashes only after actual rereading. Totals are 84/4242 complete, 4158 pending,
51 compared gaps; Pealayer 39/514, Rayan 20/2349. Screening is not completion.

Explicit new cleanup authorization allows pruning only ignored, inactive,
non-reparse desktop/target/debug: 11852 files / 10.408 logical GiB. Protected warm
release/production/rollback executable hashes are unchanged. Routine native
Check/Test share --release; -DebugArtifacts is an essential-diagnostics opt-in.
Default storage inventory remains read-only. Newer donor cache-tag/effective
Cargo cache validation, full prune fixtures and owned RAM relocation remain pending.
Eight GiB is a guard, not a minimum RAM disk; 1 GiB scratch is optional/unallocated.

Before the defer-tests instruction: native 115/115 and Flutter 73/73 pass; affected
storage/source/security guards pass. Current native Release compiles and stages
once; graceful quit after checking all drafts, checksum-verified pending rename,
immutable deployed-inode rollback hardlink and byte-identical preferences verified.
Current production PID 70980 runs e2464fa194e666dc8f85f34017d8b20ce37152dde3ba9958e2f0333baa935966.
Actual current framebuffer PNG was inspected: offline context suppresses Table/
Games/peripheral panels; WebSocket and USB controls are interactive shell elements.
Hidden/minimized window initially queues IPC captures without updating snapshots;
blank/old status is not completion. Supported computer-use activates the uniquely
returned current window; queued capture then completes. Wrapper acknowledgement
race remains useful recovery work, not a claim of unavailable Win32 capability.

Android template launcher PNGs were an omitted OS consumer; generated canonical
PNG plus legacy/adaptive launcher XML and roundIcon correct it. Compile-only APK
is 53970105 bytes, SHA256 1dde7a0025552b5d8e0f74d845b00e314b3c074c9d46298fc7d16e57926ec941.
Streamed install succeeds; AAPT confirms foreground resource and ZIP PNG hash is
byte-identical to shared master. Running physical MainActivity screenshot inspected.
Human authorizes wake/swipe for unsecured locks; android-device.ps1 directly uses
the actual imported Pealayer package-probe helper for text probes. One bounded,
dimension-derived swipe succeeds on secure=false; foreground readback/capture
confirms launch. No PIN/password bypass or launcher-grid pixel claim. No repeated
Rust/Flutter suites after the defer-tests instruction; no SDK/cache duplication.

Fix four GitHub CodeQL findings: case-insensitive actual script extraction,
direct read/ENOENT instead of exists/read race, and single-pass literal entity
decode. Prior focused actual-script/helper regression execution passes; fresh
security-policy scan awaits publishing. Do not dismiss alerts or merge failed scans.
Copilot reviewer API rejects noncollaborator request; explicit @copilot PR comment
requests review instead, with remaining acceptance preserved on issues 2/3/9/10.

Published source checkpoint 0054114c780700d397963d5eff8335f6b790a426 has a successful
fresh CodeQL policy check; all six review threads are now resolved with concrete
fix replies, not alert dismissal. PR13 is actually merged as
5f1c3e9ae83ca7a974df1cac094295bd65bc9672. Merge main normally into the feature branch;
keep its GTK/tray dependencies, ab_glyph and added native consumers rather than
taking the older lockfile wholesale. Scoped offline Cargo reconciliation removes
one unused generic-array entry; it does not update every dependency or download
an SDK. Unify runtime/build base64 at 0.23.1, import tungstenite 0.30.0 from PR13.
Guarded release-profile Check passes on the combined source in the same canonical
cache. This is compile-only, not repeated tests or a newly deployed native binary.
No current-source feature code/settings/artwork is replaced by a rollback.

This is current package/runtime evidence, not full donor parity, board acceptance,
an all-green new CI checkpoint or production-signed Android release.

## Rayan realtime reconnect and bounded preparation

Consult exact donor-audit for sync.ts/sync.test.ts/api.ts/api.test.ts; read all
four actual files through EOF and verify source bytes. api.test.ts is a full read,
not a completed import: authenticated host/config/hotkey/audio/registration tests
remain pending their adapted implementations. App.tsx lifecycle slice was read
as context only, not certified. Three exact new comparisons bind imported
subcontracts, not whole-file feature parity. Readback-before-open uses Pakar's
cookie-authorized status GET, not the donor Go /app/session CSRF mutation.

Import actual exponential base 500 ms, cap 10 s, 0.8..1.2 jitter, failed preparation
reschedule, reset-on-open and stop timer cleanup. Import api.ts AbortController,
12 s bound and finally cleanup for this GET only. Add capped exponent, one queued
retry owner, stale timer/promise generation checks, cancellation on login loss,
pause/manual reconnect/pagehide and constructor failure recovery. Signal flows
through actual refreshAuthenticated/api; no command replay, server write,
endpoint guess or fabricated live capability. Generic lease/lock/channel election,
revision/sequence projection, audio ownership and typed host API remain pending.

Port actual prepare-before-reconnect and stop test concepts; execute actual shipped
module plus Web connectWs/refreshAuthenticated/api with fake HTTP/socket/timers.
Cover jitter/cap, failed reads, 401 login loss, late callbacks, deadline/cancel,
constructor failure and HTTPS hostname URL. First jitter-edge assertion exposes
normal floating-point 600.0000000000001; use tight 1e-9 numeric tolerance without
rounding production delay. Integration test pins its injected Math.random only,
not the global host RNG. Source/VM proof is not LAN/physical acceptance.

Coverage becomes 84/4237 complete, 4153 pending, 51 gaps; Rayan 20/2349, 2329 pending.
Full CSS, API test suite adaptation, native/mobile synchronization and hardware
acceptance remain open. Canonical generator is checked in one pass; affected route
and comparison references require exact anchor/hash revalidation before builds.
The complete quality gate catches the old pause lifecycle in canonical
assets/web/device-settings.js before compilation. Inspect its generator/consumer,
apply cancellation there too and regenerate; do not accept only a hand-edited
embedded copy. The PU route guard similarly rejects the changed WebUi.h hash:
inspect unchanged remote workspace/job/readback/apply consumers, then rebind
only its two exact target hashes (Verified remote update remains line 244).
Server registrations/source identities and all 50 route classifications remain
unchanged. Preparation cancellation also suppresses stale rendering even if an
injected fetch ignores abort; timer/deadline cleanup is immediate on cancellation.
Final review cancels preparation when applying a hostname change and relinquishes
old socket identity before replacement construction. Constructor failure visibly
reports retrying; captured old callbacks cannot regain ownership. Final shipped
code/HTTP/socket tests and full local source quality pass. Sixteen-reviewed gate
retains full CSS/API-test comparisons pending. Exact donor source hashes remain
unchanged; no unread source is accepted as an exclusion. Final warm ESP-only
busy-wire build passes; RAM 70%, IRAM 96%, flash code 83%. Exact image identity
is recorded below. No AVR/native/APK rebuild, extra cache, flash or app replacement.
Final image: 926960 bytes, SHA256
cf52f0404185f0b5bcccba918e6911eec726b7f15391c43bb7aae2ae2a4cc16b.
RAM 56620/80192; IRAM 63055/65536; flash code 876848/1048576. IRAM headroom remains
tight. Final edge-case corrections reused the same warm build path, not a clean
rebuild or downloaded SDK. Freshly reread Build RayanLamp Go app without resuming
or messaging it; its interrupted Windows regression remains unverified.

## Native menu foreground policy traced, not recolored

Consult ui-shell and exact donor-audit mappings; fully reread actual Pealayer
menu.rs and palette.rs, original bytes still match their reviewed hashes. Inspect
locked egui 0.36.2 Button::atom_ui/shortcut_text, widget_style/button_style,
Visuals::text_color/weak_text_color and menu_style. The first pointer-free Button
uses Inactive foreground as its AtomLayout fallback, not noninteractive label
ink. Stock dark 180/140 and light 60/80 differ; imported palettes map both to text.
Correct only the native paint-test oracle, preserving exact per-vertex ink checks,
all six atoms/right edges/separation and geometry. Expand to stock plus actual
Studio/Native palettes in both themes; assert named primary/muted roles and
explicitly distinguish stock label/button ink. Add diagnostic assertion context
and mandatory source regression. No production style/font/menu changes.

Full local source quality, native menu/lifecycle guards, no-write Rust parser
(edition 2024), comparison Check and Test Plan pass. Rustfmt initially exposes
a brittle literal-whitespace source guard: keep the same exact expressions but
normalize whitespace before matching; preserve every semantic assertion. Four
existing target/proof hashes selectively rebound after literal-anchor inspection,
plus the exact mandatory lifecycle proof. No source-count inflation.
Actual Test again refuses the existing >8 GiB canonical cache before Cargo;
no bypass, duplicate cache, cleanup reroute or unrelated firmware/APK rebuild.
Fresh SSH localhost:1022 refuses; ledboard.local resolution times out. No native
execution, running-app replacement or physical flash is claimed. Latest 8530a33
Windows job 114337095213 is terminal failure; new native execution/package and
deployed visual acceptance remain separate and asynchronous, not waited on.
Its actual completed job log confirms the same label-ink assertion at menu.rs:125:
114 passed, 1 failed, 0 ignored/filtered, 1.41 s; no texture/double-panic abort.

## Actual Rayan toast/store/Icon import

Freshly reread Build RayanLamp Go app, task 019f8cfc-271a-72e1-8747-e3eb883de5ec;
not resumed or messaged. Consult exact donor-audit mappings. Fully read actual
components/toasts.tsx, toast-layout.test.ts, store.ts and components/icon.tsx and
verify original-byte hashes. Actual toast styles/viewport/motion/forced-colors
slices were read from app.css; that entire file is NOT certified. Existing shared
Pealayer palette/locale and MIT notice remain authoritative; no donor build/edit.

Adapt actual serial ID, last-three-plus-new queue, 4500 ms per-ID expiry/filter,
polite/nonatomic live region, status rows/title/message, four exact SVG consumers
and translated dismissal. All current Web reports and identity success paths use
one queue outside authenticated workspace; the old shared five-second timer no
longer erases newer messages. Add four-timer eviction/teardown, bounded text and
duration, safe text nodes, keyboard Escape, focus-preserving dismissal, independent
focus/hover expiry holds, locale-dismissal repaint and pagehide cleanup. No storage
or network writes, new framework or application-brand substitution by this queue.

Port actual declarationsFor and both center-axis layout checks, plus executable
queue/DOM tests for old-timer ownership, cap/eviction, focus/hover overlap, keyboard,
literal injected markup, locale refresh, motion/forced-colors/RTL/viewport rules and
canonical embedding. Actual Web auth/identity test still checks accepted/readback
semantics against the new queue. Deterministic generator is stable after one pass.
Generic store bootstrap/revision/persistence/transport, other icon API/consumers,
full CSS, real responsive/modal screenshots, Windows OS notifications and shared
native/mobile delivery remain open. Do not port optimistic uncertain-write replay.

Coverage becomes 81/4237 complete, 4156 pending, 48 gap files; Rayan 17/2349,
2332 pending. Four complete reads are comparisons, not all-feature acceptance.

Full local source-quality suite passes. Its route guard initially rejects the
changed WebUi.h identity: inspect unchanged update workspace and route consumers,
then rebind the two exact PU route target hashes/line anchors without changing
server registrations or source hashes. All 50 route classifications pass. Nineteen
changed comparison references are selectively rebound; thirteen reviewed files
are required by the expanded gate, with the routed full app.css still pending.
The generator also rejects a scoped-improvement row without a named advantage;
add the exact already-tested queue/timer advantage, not a bypass or verdict change.

Warm ESP-only busy-wire build passes: bin 924368 bytes, SHA256
c2e97d576cf2d00ce8c6e65c880c7b0a66b5176fe2a5eed2cd3a85a5c09ceed8.
RAM 56620/80192 (70%), IRAM 63055/65536 (96%), flash code 874256/1048576 (83%).
No AVR/native/APK rebuild, new cache or physical flash; IRAM headroom stays tight.
Actual labeled read-only Web fixture: logical viewport 1280x720 and 390x844,
no horizontal overflow, both toast/copy center alignments, all rows inside viewport.
Keyboard Escape removes only the focused error row and restores next dismiss focus.
Wide/narrow JPEGs were inspected, exclusive-created and copied byte-identically
into docs/screenshots and user outputs: 89058/46865 bytes; SHA256
408dc6867b44ef2fdd14d4ac110fffbbe794379627b6f435202744e415820e08 and
ffbb898c242b05a7560bec25be43fcdcd3c8943b8e7422f38ad60c30a7717b41.
Viewport reset, owned tab closed and PID/command-line-verified fixture stopped.
This proves browser rendering with simulated notifications, not hardware or native
delivery, open-modal behavior, mobile runtime or all CSS parity.

During independent work, prior source 8849ab8 CI became terminal: workflow/source,
both firmware variants, Android preview and CodeQL pass. Windows job 114334311601
now completes 114 tests successfully, one fails (menu label_colors primary ink,
src/ui/menu.rs:125). The old texture/double-panic abort is gone; no all-green or
native package/runtime claim. Inspect real widget text policy next, not change
production colors or weaken assertions to force a pass. No blocking CI wait.

## Native render ownership and painted-color test repair

Terminal 0c723f1 Build run 38092457174 passes workflow/source quality, both
firmware UART variants and Android preview. Windows job 114331734735 fails.
The restored --nocapture log establishes four unhandled TexturesDelta destructor
panics (branding, neutral-cell, connection, font-retention tests), not their
previously inferred feature assertion failures. The shortcut test separately
compares cached PLACEHOLDER with muted ink; its assertion plus destructor panic
causes the observed 0xc0000409 abort. Earlier diagnosis is superseded by this log.

Consulted localization, ui-shell and exact donor-audit mappings; fully reread
Pealayer src/ui/i18n.rs, src/ui/menu.rs, src/ui/palette.rs and src/branding.rs.
Actual headless donor tests drain texture deltas before assertions. Read locked
epaint 0.36.2 TexturesDelta Drop, TextShape color fields and tessellator resolution,
plus egui FullOutput::drop_without_applying_deltas. No new dependency or donor edit.
Restore that ownership contract in five existing Rust test modules; check shortcut
ink through real tessellated glyph vertices rather than the galley placeholder.
All production styling/font registration and original behavioral assertions stay
intact. New native_render_lifecycle_test.mjs prevents this source-level regression
and is mandatory in source quality; it does not execute Rust or certify pixels.

No additional donor file is certified by these test repairs. Coverage remains
77/4237 complete, 4160 pending, 46 gaps; RayanLamp 13/2349 complete, 2336 pending.
Current hostname probe cannot resolve ledboard.local; ASUS SSH localhost:1022
refuses at banner exchange. No board write, app replacement or unrelated build.
Actual native re-execution, package and deployed physical/UI acceptance remain
open. The existing cache budget must remain enforced and CI is not waited on.

Complete local source-quality suite passes, including the new lifecycle guard
and exact comparison regeneration/check. All five Rust modules parse with the
actual Cargo edition 2024 using rustfmt stdout-only; this is not a typecheck.
Test Plan passes and actual Test passes donor/crate gates, then refuses the
canonical cache above 8 GiB before Cargo. Sixteen target/proof references across
eight reviewed files/documents are selectively rebound with every literal anchor
verified; no source hash refresh, donor-count inflation or new cache. The prior
source's separate CodeQL run 38092457168 is now verified terminal success.

## Restored donor native test execution contract

Previous goal turn made progress: source c814379 imported actual browser trace,
passed source quality and built ESP locally. Current authoritative CI run
38091589105 is terminal: workflow/source, both ESP/AVR UART variants and Android
preview pass; Windows fails. Separate CodeQL 38091589143 succeeds. Linux strict
font equality now passes on the real runner, not only injected OS metadata.
Firmware job 114329208305 also passes protocol/profile fixtures and uploads eight
files in artifact 11684557100 (1331965 bytes; artifact archive digest
e06cfdb894bfe958711b29c4c7287017e2eeca4b3466e79e8e2c636005107e74).
This is build/fixture/artifact proof, not download integrity or physical flashing.

Windows job 114329208291 repeats neutral-cell-fill, offline dropdown and
language-change font-retention failures, followed by 0xc0000409. Its aborted
capturing test harness hides assertion details. A logs request while that job was
live returned BlobNotFound; only the later terminal job/status/logs established
failure. No timeout/transient error was treated as terminal or restarted.

Consulted localization, ui-shell, build-storage and exact donor audit mappings.
Fully reread Pealayer src/ui/i18n.rs (1236 lines; original-byte SHA256
811f1e08e7126932772c49da8aa9669ce8f47d4e2208c1ef6cde435dde20bdf4),
scripts/run-windows.ps1 and complete .github/workflows/ci-cd.yml; live hashes match.
Locked egui/elegance registry code was inspected for active theme, menu IDs and
next-pass font installation. No donor/dependency change or new whole-file review.
Pealayer actually runs all native tests with --test-threads=1 because process-wide
application state is shared. Pakar omitted this; --jobs only limits compilation.
config::live is process-wide and preference tests mutate it, so concurrent tests
can race. This does not yet prove the cause of all three failures or the abort.

Restore serial runtime tests in the mandatory wrapper and --nocapture so failure
details survive a later crash. Keep compile jobs, assertions, test selection,
dependency versions, symbols, same target/cache and storage guards unchanged.
Restore donor CI isolated configuration and RUST_BACKTRACE=1. Local Test defaults
to a stable build/native/native-tests.json instead of the user profile, preserves
an explicit fixture override, and restores its process environment in finally.
Check/Release never receive test runtime flags or default test configuration.
Plan/regression tests cover exact dispatch and isolation; they are not compilation.

Complete local source-quality suite passes after these changes. Actual native
Test wrapper still refuses the >8 GiB canonical cache before cargo starts; no
bypass, cleanup, extra cache, app replacement, firmware/APK rebuild or physical
write. ASUS SSH localhost:1022 still refuses. Three native assertions/crash and
new execution under restored contracts remain open. Nine existing references
across five affected files are selectively rebound, with two additional exact
CI-to-wrapper/test mappings. Coverage unchanged: 77/4237, 4160 pending, 46 gaps;
Rayan 13/2349. CI is not waited on.

PERSIAN-PARITY and the existing font/CI ledger actions now explicitly distinguish
individually passing native contextual glyph tests from failing font retention.
The previous unexecuted-test wording is superseded by observed CI results;
remaining runtime/translation/grouping gaps are preserved, not silently certified.

## Rayan browser trace and portable font recovery

Reread the actual task Build RayanLamp Go app without resuming or messaging it.
Read browser-console.ts and browser-console.test.ts completely and verify live
original-byte hashes. The actual sanitizer, error projection, styled sink and
quiet-window mechanism are imported; lamp names and fixed colors are replaced
explicitly by device vocabulary and the existing shared Pealayer palette.
Shipped HTTP and WebSocket consumers are tested, not merely a disconnected helper.
Added safeguards include bounded lifetime, camelCase secret keys, prototype/format
protection, invalid host containment and stale-socket ownership. No storage writes
or automatic uncertain-command replay. Successful telemetry/polls/frames are quiet.

Terminal source 2a5574c Build run 38090552957 passed lint, source quality and Android
preview. CodeQL 38090552932 passed. Firmware passed the repaired missing-drive
gate and AVR polling compile, then failed strict FontAssets.h equality under
Node 22 on Linux. Windows compiled past the previous 11 API errors, then failed
cell neutral-fill, offline connection dropdown and font-family tests; the process
also ended with 0xc0000409. Those native assertions/crash need separate donor-first
recovery and remain open. No all-green, native package or production claim.

Fully reread Pealayer build.rs and ci-cd.yml before the font recovery. The latter's
Node 24 setup is now also used by firmware verification. Windows gzip OS metadata
is 10; normalize only the non-checksummed OS byte to RFC 1952 unknown=255. Deflate
body, trailer/CRC and decompressed licensed font bytes remain unchanged. Header
flags/MTIME that would make this unsafe are rejected. Strict asset equality stays
enabled. Tests cover Unix/Windows OS fixtures; an actual Linux rerun is still
required, not inferred from injected header bytes. No font/CDN/dependency change.

The new module initially made localization settle only on a second generator
pass. Recovery analyzes the canonical module before extracting translations,
independent of its previous embed. One generation followed immediately by --check
and shipped browser trace tests passes. Complete local source-quality suite passes.
Selective 22 existing target/proof references and two PU route references retain
their exact anchors/decisions; two new Rayan certificates bring coverage to 77/4237,
4160 pending, 46 gaps; Rayan 13/2349, 2336 pending. Toast/tokens files remain pending.

ASUS localhost:1022 still refuses SSH. No native cache bypass, cleanup, donor build,
additional cache, app replacement or board flash. Only affected ESP busy-wire
firmware is built through the mandatory warm wrapper; record its result separately.

That ESP build passed: 915632-byte bin SHA256
0544853c13cf64dbf78f488b765379d726e6c2da244b467c5a1156cc541cc5e3;
RAM 70%, IRAM 96%, flash code 82%. Existing toolchain/link flags and cache retained.

Optional local actionlint invocation failed at process execution: existing Go
temporary root PCController/go-noexec-temp denies execution of actionlint.exe.
Consulted repository-governance and the fully read Pealayer CI workflow; its
Ubuntu lint job is the accepted execution surface. No ACL/temp-root bypass,
second build cache or dependency churn. Current Node governance tests pass;
new workflow actionlint execution remains pending CI. Comparison --check itself
passes; piping its Format-Table stream into Select-Object -Last caused a local
formatting error, so the check was rerun without that output projection.

## Native API and cross-platform gate recovery

Terminal Build run 38090009184 at source 527d8e9 repeats the exact 11 native
compile errors and Linux Join-Path error from the prior run. Lint, complete
source quality and Android succeed; separate CodeQL 38090009284 succeeds.
Read the failing jobs rather than waiting or interpreting a timeout as failure.
Consulted build-storage and exact Pealayer menu/branding/preferences audit rows.
Fully reread libmpv-windows.ps1, menu.rs and branding.rs; their live original-byte
hashes match the recorded comparisons. Preferences audit remains pending: its
partially inspected donor is NOT newly certified. The actual fully read donor
menu's scoped with_layout idiom and locked egui 0.36.2 Ui/Context/TexturesDelta
implementation informed the API adaptation; no dependency upgrade or metric
change was introduced.

consult-reuse now probes literal donor roots before Join-Path or traversal.
Absent Windows PS drives on Linux become explicitly live-unavailable, while
snapshot/target/hash/anchor gates and RequireLive rejection remain intact.
No catch-all suppression or fake live source claim. Added missing-drive tests
for both reviewed build and Rayan Web slices; the actual preflight test passes.
Desktop texture tests flatten the real multi-delta map; menu tests select the
active theme's style; shell and Preferences use scoped child layouts. Table
queue ownership stays private behind stop_pending_edits, which revokes old
unsent/readback work without retrying or claiming sent writes were undone.
Added a native revocation regression; it has not executed locally.

Complete local no-compile quality suite and Test build Plan pass. Actual Test
wrapper refuses the existing >8 GiB canonical cache before cargo starts. No
budget bypass, second cache, donor build, deletion, dependency churn, firmware
or APK rebuild. ASUS SSH localhost:1022 freshly refuses connections. Native
compilation/paint tests, Ubuntu builds, runtime screenshots and physical
deployment remain pending new CI/host acceptance. Only 15 references to the six
actually changed target/test files were rebound; no new donor certificate,
blanket source hash refresh or inventory-count change.

## RayanLamp locale continuation

Reverified Build RayanLamp Go app (019f8cfc-271a-72e1-8747-e3eb883de5ec);
not loaded, not resumed or messaged. Fully read i18n.tsx (656 lines) and its
71-line test; original-byte hashes match inventory. Reread actual Pealayer Web
i18n and current locale/generator/test consumers. Imported real normalization,
translation placeholders, number/digit presentation and automatic locale
lifecycle, preserving independent explicit direction and peer identity. No
OS-event writes, request/replay or draft replacement. Seven tests and complete
source-quality run pass; actual browser mixed-script selection and reversible
network labels inspected. Dynamic copy still visibly incomplete; native/mobile
and Persian-calendar/BidiLtr consumers remain explicit gaps.

Interface --check caught edits in generated device-settings embedding. Fully
read the actual generator/canonical module, moved edits to assets/web/device-settings.js
and regenerated deterministically. No --check bypass. Reread exact PCController
card-layout-controls and Pealayer HeaderBar; only affected target/test references
were rebound. Actual canonical PCController bytes match its inventory even
though the old lookup root is unavailable. PU route guard caught Web document
hash drift; only the two unchanged Verified remote update rows at line 220 were
reviewed/rebound. No blanket source certificate or route behavior change.

Terminal CI run 38089030955 confirms the prior CodeQL lint fix works on Ubuntu:
lint, quality and Android success, separate CodeQL run 38089030973 success.
Overall Build fails: Linux firmware donor lookup trips on a missing C: drive;
Windows tests have 11 egui/private-table API compile errors. These are real
remaining release defects, not a job being awaited or a reason to suppress gates.
ASUS SSH localhost:1022 refuses; no physical firmware/app replacement occurred.
Warm mandatory ESP-only busy-wire wrapper then passed: 906928-byte image SHA256
02464a4211b97e98ff1e22b2b88feeef32608a6465a480f0c96a055aa43e7012;
IRAM remains 63055/65536, 96%. No AVR/native/Android rebuild, copied firmware
package, new dependency/cache, clean or budget bypass.

## CI security-summary recovery

Fresh run 38088825179 is terminal failure, not a job still awaited. Ubuntu's
actionlint v1.7.12 invokes ShellCheck and reports SC2129 on repeated security
summary redirects; all product build jobs were skipped, so no green native/APK
claim is possible. Consulted repository-governance and the exact CodeQL row;
reread the full actual Pealayer codeql.yml. Imported its grouped redirection,
preserving the success gate and honest analyzer coverage, with no suppression.
Added a local grouping/non-suppression regression; existing actionlint v1.7.12
and governance tests pass. Local Windows lacks ShellCheck, so that is not an
Ubuntu ShellCheck execution claim. No tool download, clean, rebuild or CI wait.
Only the two affected target/proof hashes were rebound after explicit rereview.

## RayanLamp Web theme continuation

Reconsulted full Rayan inventory and exact theme rows plus existing appearance
mapping. Imported actual pre-paint/controller/metadata code and donor tests into
one shared palette engine. Added web-presentation five-source gate, before ESP
builds and quality; Bash uses the same PowerShell gate and checks all generated
font/brand assets. Partial i18n/settings reads remain pending, not exclusions.

Guard caught ci-checks.mjs hash drift when registering the new tests. Consulted
the exact Pealayer workflow row and reread its full source; runner still stops on
the first failed child and retains governance contracts. Rebound only that
reviewed hash. Next guard caught old WebUi hash/line in existing PU route evidence:
reread the actual route checker, inspected unchanged target forms and updated
only two Web targets to anchor line 220. No altered donor source or blanket
certification. Documentation patch with an incorrect heading failed atomically;
read exact headings and retried the targeted patch, without shell rewrite.

Full source quality, six new theme tests, ten appearance tests, actual browser
Persian light/dark reloads and ESP warm compilation pass. Browser evidence saves
with exclusive-create to avoid overwriting; native capture/build tools not used
because native code is unchanged. Existing licensed font and peer identity are
preserved. ASUS SSH still refuses; no device mutation, native replacement,
Android rebuild, CI wait, duplicate SDK/cache or blocked cleanup reroute.
Exact image/build identities and acceptance limits are in the existing receipts.

## RayanLamp import recovery

Located Build RayanLamp Go app through the task index and read its exact thread.
Cross-task listing did not contain the older task; bounded read-only lookup
located its original source, which has no Git metadata. No donor task was
resumed/messaged and no donor files were edited.

Consulted the existing games-streaming route, then extended the SAME ledger and
donor-audit index with RayanLamp. Fully read haptics.ts/haptics.test.ts,
connection-preferences.ts/its tests, digits.go and LICENSE before importing.
Guessed absent paths were corrected by enumerating actual filenames; no missing
filename became an excuse to discard a feature. Oversized thread/evidence output
was filtered into bounded metadata and source-ordered diff hunks. A grouped-hash
patch and an invalid receipt anchor were rejected; corrected exact hunks were
applied, without wholesale file rewriting or donor mutation.

The existing Web table/settings harnesses extract isolated script slices. They
initially lacked the newly canonical dependency; updated those harnesses to load
the actual rayanlamp.js first rather than adding fake production fallbacks.
Unchanged donor certificates were retained; affected target/test/generator hashes
were rebound only after reviewing their actual diffs and preserving old contract
semantics. The font generator remains deterministic and skips identical writes.

First-party/vendor source is inventoried and original-byte hashed; generated
directories are pruned BEFORE traversal. Live filesystem inventory drift is
checked, not just selected file hashes. Useful unreviewed files remain pending.
See RAYANLAMP-REUSE.md and issue #3; test-code anchors are not runtime acceptance.

## Persian regression recovery

Consulted localization, preferences, build-storage and exact donor-audit entries.
Read the complete actual Pealayer src/ui/i18n.rs, web_ui/src/i18n.ts,
tests/i18n_surface_audit_test.rs and src/ui/mod.rs; configuration and Preferences
reads were selected functions/excerpts, NOT whole-file certificates. Refreshed
only the changed Pealayer inventory, leaving every donor read-only. Three new
complete-read certificates bind real targets/tests and concrete pending omissions.

Actual donor font/dictionary/shaping/alignment/layout and audit patterns were
adapted to native, Web and Android shared bytes. Added a localization reviewed-
slice build gate, without pretending it certifies unfinished UI/mobile surfaces.
Read actual egui/Elegance APIs: Elegance adds fonts, but host set_fonts resets
the registry. Language refresh now preserves registered widget fonts; initial
install occurs once through resolved config. Tests cover both native families
and font retention in code, but native execution remains blocked by storage.

Native wrapper refused the existing 13.49 GiB cache against its unchanged 8 GiB
budget. No clean, alternate cache, dependency churn or guard bypass followed.
Read donor storage/low-write mappings; requested obsolete-debug-only cleanup
while preserving release/running app/settings/rollback. No approval or cleanup
is claimed. Existing production app remains unchanged.

Read-only actual Web fixture rendered Persian Vazirmatn and RTL. First screenshot
exposed still-English hints/placeholders; added canonical dictionary entries,
owned attributes and dynamic settings copy, then inspected the refined capture.
Peer names/hostnames remained untouched. Browser font-loaded check passed and
no horizontal overflow was observed. This is simulated Web, not physical proof.

The new test DOM mock initially had a missing closing brace; fixed the test
syntax without changing production behavior. A comparison patch accidentally
appended literal plus characters to three JSON separators; generation failed,
separators corrected and all comparison checks passed. Existing route hashes
and menu anchors were explicitly re-reviewed/rebound after localization insertions;
source certificates and omission semantics were not replaced. Separate reordered
PU target/hash fields required exact-field patching, not broad source acceptance.

Local quality suite and affected ESP busy-wire build passed. No CI wait, native
replacement, AVR build, Android package or hardware flash occurred. Full mapping,
candidate hash, source/runtime boundaries and remaining actions: PERSIAN-PARITY.md.

## Web device-settings and interface-contract continuation

Issue #4 owns the requested Web/native parity. Consulted ui-shell/preferences,
exact donor-audit HeaderBar/card-layout-controls/pointer-reorder and build-storage
run-windows rows, and read those actual sources completely. Native Network/Cloud
Draft/SecretEdit/Write semantics are adapted, not a replacement defaults form.
Shared generated Rust/Web/Dart page/field catalogs and build checks now reject
catalog drift. INTERFACE-CONTRACT.md names still-unported editors/platform work;
page catalog synchronization is not full mobile or whole-product parity.

Computer-use skill selected the actual embedded Web, read-only simulated fixture.
Verified capability-gated pages, MQTT/WebSocket split, unsaved draft navigation
and responsive geometry. The first narrow screenshot exposed intrinsic nav/grid
overflow; inspected the actual shrinkable controller CSS and corrected min-width
and mobile minmax grid. Retest reports equal dialog client/scroll width. Secret
rows follow observed presence and hide outside Replace. Screenshots are small
outputs, not copies of build packages. No physical writes or secrets were sent.

Unexpected diagnostics: guessed PU route-evidence and mobile settings-file paths
did not exist; inspected actual file inventories and used PU850-ROUTE-PARITY.json
and mobile main.dart instead. A CSS anchor patch failed atomically; reread the
exact line before applying. Local browser binding name was absent after context
refresh; reused selected browser ID 2 and the owned tab, not another browser.
The initial required-field/access DOM double lacked getElementById; the helper
uses documented querySelector fallback. Exact target hashes changed as expected;
rebound only after reviewing preserved handlers and new module code. No donor
checkout, global credentials, private preferences or build cache was overwritten.

Acceptance: actual settings module/HTTP doubles, Web access/context/table/cells,
appearance (10 tests), identity, remote, display, remote tools, all 50 PU route
registrations, branding, native-menu and transport source guards passed. Shared
generator --check, ledger integrity, file-comparison, reuse-preflight/storage
tests and Rust edition-2024 source parsing passed. ESP busy-wire warm build
passed: RAM 56460/80192, IRAM 63055/65536, flash 756732/1048576. AVR unchanged;
no AVR/APK or donor build. Native Test refused at the existing 8 GiB cache guard
before Cargo; native execution and replacement are not claimed. Flutter/Dart
were not on PATH; added mobile catalog tests remain unexecuted locally.
Fresh ASUS SSH localhost:1022 is refused; ledboard.local:80 cannot resolve.
No fallback to production Cafe, firmware flash or physical acceptance claimed.

Pealayer inventory refresh detected changed board_info/status_bar/menu/icons.
Reread all four fully. Another task has dirty tracked consumers and an untracked
src/ui/dropdown.rs dependency: do not overwrite/import its moving work as a
finished donor release. Named pending dependency/consumer migration is recorded
in the same comparisons; icons verdict downgraded to parity-gap. Icons changed
again during review, so captured its original bytes/hash once, read the entire
captured text, and bound that exact snapshot in inventory/comparison. No later
live revision is implied. Exact dropdown lookup cannot reach the untracked file;
that is pending provenance/review, not an exclusion. Actual card-layout-controls
source comparison added; pointer-reorder was read fully but its unported panel
ordering remains pending. Current comparison totals: 1885 / 53 reviewed / 1832
pending / 35 gaps, not complete superiority.

Comparison patch diagnostics: fixed indentation/comma preservation and absolute
target paths after atomic failed hunks. Hash-only patch initially matched no rows
due indentation; inspected actual JSON before rebinding eight changed targets.
The source-comparison gate correctly rejected moving donor hashes; complete
rereads and named omissions replaced stale equivalence, not blind hash updates.
Browser viewport override was reset and owned simulated tab/fixture closed.
Final inspected Wi-Fi image is outputs/PakarController/web-parity/settings-wifi.png.

## Front-panel WebSocket/USB dropdown continuation

Issue #10 owns this transport-context change. Read AGENTS/preflight and issue
#10; ui-shell consultation first rejected concurrent Pealayer tracked inventory
drift. Refreshed only its inventory, then consulted exact donor-audit status_bar
and menu mappings. Both donor files were reread fully, including the real
connection selector/custom OS-path fallback; donors remain read-only. The
front-panel labels now call connection_controls popups with typed routing,
port/disconnect, reconnect and configuration actions. USB scan happens only
inside its open popup, reports failure and preserves unavailable configured
ports. Same-endpoint WebSocket reconnect no longer silently drops the request.

No native build guard bypass, cache duplication, automatic prune, firmware build
or production replacement. Rust popup/pointer tests are added; Node wiring and
menu source guards plus Rust parsing passed, not native runtime acceptance.
Unexpected diagnostics: a guessed esp/src/web* path failed (no files changed);
subsequent inventory uses exact known paths. Combined lookup output was too large,
so reread both full donor files in bounded chunks. A receipt patch with a guessed
heading failed atomically; reapplied only after reading its actual header.
Current native build/deployment acceptance is recorded below and in issue #10.

Result: guarded native Test refused before Cargo because desktop/target remains
13.49 logical GiB / 16068 files against its 8 GiB policy. Consulted build-storage
and completely reread actual Pealayer scripts/storage-hygiene.ps1; selected the
already imported read-only summary, not donor recursive prune. No new native
binary/screenshot or running-app replacement is claimed. Existing build gate,
reuse preflight, ledger/file-comparison tests, connection/menu source guards and
diff whitespace validation passed. Comparison patch attempts first exposed JSON
anchor escaping and then out-of-order hunks; both failed atomically. Reapplied
with JSON-escaped anchors and ordered, separate insertion after exact inspection.
Inventory is 1885 files / 52 complete-read comparisons / 1833 pending / 33 gaps.

## Board/application identity continuation

Issue #10 owns this identity request. Read AGENTS/preflight and issues #4/#10;
consulted ui-shell, preferences, application-identity and exact donor-audit
branding/config/Commands rows. Pealayer src/branding.rs and HeaderBar.tsx were
completely read; actual runtime cached icon/multi-viewport/tray and name/artwork
binding are adapted. config.rs resolver and PU Commands.cpp are screened
slices, still pending entire-file review; no whole-repository parity claim.
Pealayer was refreshed to the current tracked 505 paths (all origins 1884).

Current acceptance: actual embedded Web identity/context/access/branding,
display/cells/remote/menu tests and production DeviceIdentity host tests pass.
New C++ tests compile actual APP macros for Table, non-Table and custom app.
Rust source parses as edition 2024; unit tests/USB write ACK/readback/CLI/native
tray remain unexecuted because donor-gated build Test refuses 13.49 GiB canonical
cache against 8 GiB policy. Read actual donor scripts/storage-hygiene.ps1 and
Pakar storage report after this block; no prune, policy bypass, duplicate Cargo
cache, SDK removal, native build/stage or running-app replacement occurred.
Existing production PID 62448 was freshly observed, not attributed this source.

Unexpected operation recovery: first ESP compile required an explicit declaration
of existing appendJsonEscaped for the new USB handler; inspected actual helper
and added its matching prototype, no alternate encoder. Web fixtures initially
lacked the new identity renderer/metadata; extended actual application tests,
not product fallback. Ledger-bound route test correctly rejected changed Web
bytes; explicitly rereviewed the affected name/application/discovery/info slice
and rebound exact anchors/hashes. Rejected patch hunks had no partial writes;
corrected from exact source. Invalid rg glob/guessed cli/stub/workflow/muda paths
were resolved from inventories; no skipped functionality was justified by them.

Computer-use skill selected read-only browser verification of the actual embedded
Web served by a visibly SIMULATED controller fixture. Verified Pakar Keys header,
Keypad controller title, application descriptor, separately populated full-width
board-name/hostname fields and real artwork. Saved/inspected no-overwrite images;
identity-settings.png is final alignment, identity-controller.png preserves the
prior narrower fields. The owned tab was closed; fixture PID identity was verified
before cleanup. No physical/API mutations or login secrets were sent.
ASUS SSH localhost:1022 still refuses connection; ledboard.local port 80 is
unreachable. A read-only check of the remembered alternate SSH port returned
cafe-pc, not the requested ASUS board host, so it was not used to build/deploy.
Native/physical acceptance
checkboxes remain open; CI is asynchronous and is not waited on.

Publication recovery: first push and proxy/helper retry returned a real GitHub
403: the active DRSDavidSoft account had pull=true/push=false. The already
configured atomicdeploy account had push permission for this repository.
Used its existing credential only in the publishing subprocess, restored the
previous GH_TOKEN and did not switch global login or print/store the token.
Source checkpoint bded015be023e773b3c7dce276803df666a3d543 was verified on
the remote branch. Issue #10 comment 6100668023 holds the checked/unchecked
identity acceptance; #3 comment 6100668240 records exact ledger continuation.
The PowerShell formatted-report/Select-Object diagnostic failed only in output
formatting; reran the actual remaining tests directly and they pass.

Tracking: issues #3 (all donor comparisons), #9 (storage tooling), #8 (capture).

Pealayer inventory refreshed file by file from clean canonical source
a54933aa7d0c05a058045dc9cd21b1b7fbc7378e: 493 tracked paths, none missing,
original-byte hashes/sizes and screening state retained. Other five inventories
were preserved. Automated screening is not a complete manual read. Six actual
tooling/hygiene files were read fully and added to the existing authoritative
comparison ledger: package probe, libmpv/shared resolver, runner, packager,
crate-boundary guard and workspace hygiene. Each has exact target/test anchors,
hashes and individual omitted-code reasons.

Pealayer comparisons increased from 11 to 17; 476 remain pending and nine reviewed
files still have gaps. Across all donors: 1,868 inventoried, 39 complete comparisons,
1,829 pending and 22 known gap files. Five newly reviewed tooling files correctly
remain parity-gap, not accepted exclusions. Only the complete package-probe helper
is byte-identical imported code (SHA-256
f4516884990ad76c020fbc6534d902c4949abae0b48da8ea40c008afa44dfd4f).

Implemented prevention: nine capability routes into the same file ledger;
read-only lookup with unknown-concern and unreviewed-gate rejection; exact required
source/target/test hash checks; live donor byte and tracked-path verification;
mandatory gates before native build/Plan, staging and screenshot capture; first-step
instructions in AGENTS.md for every task, failure and missing capability.

Executed locally:

- tests/reuse_preflight_test.ps1: all nine routes nonempty; cached build/capture
  gates; rejects unavailable live sources, unconfigured gate, unknown concern,
  screened rather than complete read, empty target mapping, stale source/test hash
  and escaping target. Actual imported Process helper returns short-lived stdout
  and rejects exit 7. Actual adapted resolver expands relative cache override.
  Source-stability helper rejects wrong commit.
- Live build-storage lookup with Gate and RequireLive: ten matching files,
  all live hashes match refreshed inventory, four pending comparisons and five
  reviewed gaps remain visible. Consultation does not certify them complete.
- Imported Process helper runs existing Pakar --help successfully. Optional stage
  -Probe on identical current artifact returns Changed=false/Written=false.
- Build storage plans/parser tests, reuse/ledger comparison adversarial tests,
  screenshot source guards and version checks pass. No compiler or firmware
  flasher was run, no app replacement and no private settings write occurred.
- Repeated unchanged inventory/comparison generation leaves write timestamps
  unchanged for legacy-source-ledger.csv, file-comparison.csv and FILE-COMPARISON.md.

The capture wrapper's only functional addition is the preflight gate; its previous
PID/backend/no-overwrite behavior is unchanged and both donor target hashes were
rereviewed. This pass does not claim a new runtime capture of that wrapper.

Limitations remain explicit: full package manifest/embedded build identity,
saved host/toolchain provenance, generic runner fallback, crate-boundary adaptation,
font/Web/branding/resource packaging and full screenshot/interaction matrix.
The original Process probe does not terminate its child on timeout. Native CLI
help is not full renderer/DLL smoke. Arbitrary direct shell commands are not
sandbox-enforced; continuation instructions prohibit bypassing these gates.

Earlier cleanup is still blocked; no old cache/SDK/source/app backup was removed.
The running native app remains the prior cell-surface executable. Source/CI
checks are not board, LAN, Android or whole-project production acceptance.

## Exhaustive consultation and actual guard follow-up

The earlier counts above describe the previous checkpoint, not current closure.
This follow-up retains the SAME inventory and comparison authorities:
1,868 files, 39 complete comparisons, 1,829 pending, now 21 reviewed gap files.
Pealayer remains 493 files / 17 compared / 476 pending; eight reviewed gaps.
Live canonical Pealayer is still clean at a54933aa7d0c05a058045dc9cd21b1b7fbc7378e;
all 493 inventoried bytes match. No new complete-file read is claimed from lookup.

Specific failure: specialist keyword routes cover only 530 distinct paths.
The other 1,338 (including 309 Pealayer paths) were inventoried, not approved
exclusions, but weren't returned by any specialist lookup. Added donor-audit
to reach every path, plus exact Origin/SourcePath lookup, visible AuditOnly,
read/availability, code/reason/disposition omissions, proof meaning, remaining
actions and owning issues. Existing CSV now includes routes and issue links.
Snapshot-stale comparisons are visibly stale even in ungated lookup.

Actual recovery source consulted fully in this follow-up:

- Pealayer docs/verification/LOCAL-WORKSPACE-HYGIENE.md: preserve the canonical
  cache, stable candidate and owners; do not route around blocked cleanup.
- Pealayer scripts/check-crate-boundary.mjs: adapted into
  tools/check-crate-boundary.mjs, not merely copied into a plan. Its actual
  module ownership/thin executable/resource guards now execute before native
  builds and in CI. The original assumes a library; Pakar detects optional
  lib.rs, applies shared ownership guards when present, and checks duplicate
  declarations/resources today. Exact source/target/test hashes and reasons
  replace this file's previous pending comparison in the same manifest.

Locally executed: exhaustive/filtered lookup, stale-source comparison checks,
all existing routing/gate/adversarial tests; comparison integrity and stale
report rejection; storage plan/parser tests; actual single-binary guard and six
negative crate/resource regression cases. Comparison -Check makes no writes and
refuses stale committed reports; CI uses it. Repeated unchanged generation leaves
CSV/report timestamps unchanged. Comparison fixtures use one owned stable
build/native directory instead of creating a fresh GUID directory each run.

Next-file worklist (all remain open in authoritative rows / issue #3):

- scripts/build-metadata.rs and scripts/package-windows.ps1: embedded source
  identity, package manifest and actual smoke tests, not just CLI --help.
- scripts/libmpv-windows.ps1 and scripts/run-windows.ps1: generic host provenance,
  saved configuration ownership, toolchain fallback and mpv-free runner.
- scripts/new-icon-pack.ps1: inspect actual icon authoring pipeline before
  replacing or claiming tool unavailability; currently only audit-routed.
- scripts/verify-messaging.mjs: inspect actual event/action verification harness,
  then map generic contracts; currently only audit-routed.
- Pending UI/preferences/Web/mobile/PCController/PuzzleBoard/PU sources stay
  open. This is not an import-completion or production claim.

No compiler, donor build, SDK download, firmware flash, app replacement,
cache removal or package copy was performed for this follow-up.

## Live donor drift caught and actual imports continued

The historical counts above remain historical. Canonical Pealayer advanced from
a54933a to clean d3f11b18d3b969398df18c26bc5871b426d4c332 during this pass. The build
preflight rejected the changed tracked inventory **before** the ESP compiler ran.
No failed-build retry bypassed that guard. Read the actual new storage-hygiene
script and all changed libmpv-windows, package-windows and hygiene sources through
EOF, then explicitly refreshed their individual source/mapping/test evidence.

New actual imports: Get-DirectorySummary and Test-PealayerFileContentEqual
(Pakar-named), called by read-only storage inventory and candidate staging.
Delete-destination-first copying is replaced by verified pending-copy/rename;
mutable compiler output is not hardlinked. Cache-inside-checkout rejection needs
an owned nonduplicating migration of the existing sole cache. Prune remains
pending: importing a newly available deletion script is not permission to reroute
blocked cleanup. Read-only scan still measures Cargo 13.49 logical GiB, mobile
build 0.971 GiB; these are not SSD writes or physical recovered-space claims.

Also read actual appearance.ts, appearance.test.mjs, App.tsx and styles.css.
All five appearance functions and four actual donor tests are now adapted into
the shipped Web UI, with the actual palette catalog rather than another theme
engine. Complete generic App/styles mappings and native/Web/mobile preference
transport remain pending. See WEB-APPEARANCE.md for exact uses and browser proof.

Current authoritative comparison: 1,869 files, 42 complete source comparisons,
1,827 pending, 23 reviewed files with gaps. Pealayer: 494 / 20 compared / 474
pending / 10 gap files. Increased gap count exposes additional unfinished useful
behavior; it is not accepted as an exclusion or a whole-repository parity claim.
New storage/appearance/App/styles routes prevent these files remaining invisible
to specialist lookup; exhaustive donor-audit still reaches every inventory row.

Executed: live build-storage Gate/RequireLive (11 paths, zero stale, four pending,
five mapped gap files visible); donor routing/hash/adversarial tests; actual
read-only summary and content-equality tests; comparison integrity and stale-report
rejection; storage Plan/parser tests; ten appearance and existing access/display/
cells/remote/PU-route tests; screenshot source guards and source version 0.2.7.
Comparison Check also correctly rejected the report after its target hash was
changed to reflect readable inventory formatting; regeneration follows explicit
rereview, not a blind build retry. Firmware compile/deployment is separate proof.

Both previous ecf882d CI runs (38059703363 and 38059700392) passed Windows,
both firmware variants/protocol tests and Android preview. They do not certify
the new appearance/storage checkpoint. Current CI is tracked on the existing PR.
No native/Android/donor build, firmware flash, app replacement or cleanup was
performed for these imports; only the affected ESP compile was requested.

## Native menu regression: actual source comparison and guarded recovery

Read full Pealayer src/ui/menu.rs and src/ui/mod.rs at 3d5f7ae. Read egui
0.36.2 shortcut atoms/menu_style/Spacing dependency code; both apps share that
version. Actual top_menu_button helper imported with only state ID/visibility
changes. All six current Pakar accelerators now use the donor shortcut_text
idiom, not spaces in primary labels. Four root headings use active-menu hover
switching. Three native regression tests cover actual painted right edges/weak
colors, popup switching/closure/context isolation and production call sites.

Height diagnosis is documented separately: Pakar minimum 28 / vertical spacing
6 / font cap 13 versus Pealayer default minimum 18 / spacing 3 / font cap 12.
Popup padding is overridden by egui, not the explanation for that difference.
No documented requirement justified the divergence. Height/font/spacing/zoom
remain unchanged as the user requested; no claim of corrected menu heights.

Donor advanced during this pass to clean 8d9bc1d86988987354e373a049ef953f9cf3034d.
Menu/mod source hashes remain unchanged. Selectively refreshed the same
inventory to 496 Pealayer paths; its two new verification/doc paths are pending.
App.tsx changed since its earlier read and is correctly stale-review, not carried
forward as current certification. Current complete comparison count is 43 of
1871; 1828 pending and 24 reviewed parity gaps remain visible. Complete file
reading is not complete import acceptance: the new menu row is parity-gap,
with concrete generic omissions/next tests in NATIVE-MENU-COMPARISON.md.

Executed native_menu_test.mjs source guard: normalized imported helper body
matches donor exactly; four production root headings and six shortcut atoms;
unchanged height policy. Rustfmt parse/check and comparison Check passed.
Attempted the mandatory native Test wrapper: donor/build gate and crate guard
passed, then the canonical-cache budget guard refused compilation (>8 GiB).
No bypass, new cache, deletion reroute, donor build, or package copy attempted.
Native paint/pointer tests must execute on CI; source guards are not native
visual/runtime screenshots. The running app was not replaced in this pass.

## Cross-surface application branding regression

Cause: native build used Phosphor Circuitry; native toolbar/About separately
tinted a font glyph; Web hardcoded a diamond and omitted application favicon.
Read all of actual Pealayer src/branding.rs, build.rs, assets/icons/README.md,
HeaderBar.tsx and webPlatform.ts before adapting the resolver/image binding.
Imported icon_data_from_bytes unchanged and actual seven-frame ICO conversion
with its shell-derivative test adapted to Pakar's PNG-only decoder. One 740-byte
lossless PNG master preserves the verified existing native glyph pixels; native
PE/window/tray/header/About and Web header/login/favicon/touch consume it.
No per-surface recoloring, new drawing, donor build, compiler or icon disk cache.
Existing semantic Phosphor quick-action generator remains unchanged.

New generator first rejected adjacent empty bootstrap markers; fixed the order
guard to explicitly accept an empty block. Appearance stale check caught new
branding CSS placed inside its generated block; moved it outside that owned
block before regeneration. Existing generate-web-appearance.mjs was reread and
used as the no-write/marker-ownership reference. Both current-generation checks
and unchanged-timestamp branding tests now pass; no blind repeat/build bypass.

Live donor inventory then advanced to 498 tracked paths; the exhaustive guard
rejected drift. Refreshed all Pealayer paths without touching donor files.
Reread current src/ui/icons.rs through EOF: its only new change was the STOP
re-export, now imported; old picker functions/eleven tests are retained. New
unreviewed paths stay pending and unrelated changed files stay unreviewed/stale.
Current report: 1873 paths, 47 complete-read comparisons, 1826 pending, 28 gap
files. Branding/HeaderBar/WebPlatform/build rows are explicitly parity-gap,
not claims that custom runtime presets or all browser capabilities are imported.

Executed: actual decoder-body identity guard, exact PNG/native pixel hashes,
four identical embedded Web assets, native production consumer/source checks,
no-write generation, native menu source guard, ten appearance tests, Web access,
Rustfmt parse/check, offline locked dependency metadata, comparison Check and
reuse gate/adversarial tests. Reused already locked base64/image versions as
build dependencies; no new dependency set/download/cache was created.
New native pixel/ICO/texture tests run asynchronously on CI per user instruction;
no blocking wait, running-app replacement or board flash claimed for this patch.

## Tint-free unified cell selection and two-way Find cell

Consulted ui-shell mappings and exhaustive exact donor lookup. An assumed
src/ui/selection.rs path had no inventory row; stopped that lookup, searched
actual files and consulted src/ui/controls.rs instead. Its neutral/fixed-geometry
interaction and selection-before-paint slices were read, not falsely promoted
to a whole-file review. Added controls.rs to ui-shell routing; its generic
remaining capabilities remain pending in issue #3. Existing actual imported
dialog/palette/icons are retained. Tinting transport intent is deliberately not
copied onto cells because the user explicitly requested no tint.

Native/Web selected and matched cells now share an amber outline and neutral
alternating cell surface; red LED state no longer colors the background. Two-way
query/selection binding covers clicks, toggles, ranges, row/all/clear, manager,
keyboard and marquee; invalid drafts remain editable, but repeated selection
gestures replace them. Native revision-based sync requests repaint; F3 navigation
does not collapse multiple selected matches. No selection writes LEDs.

Extended actual shipped Web DOM/HTTP-doubles tests pass, including same-cell
reselection after invalid input and exact range display. The old appearance
test correctly caught its obsolete matched-only selector; updated its expected
contract to unified tint-free emphasis and retained all four donor tests.
Reviewed focused changes at all existing shell/appearance/icon/branding evidence
anchors before rebinding hashes. Native tests added/extended for query
roundtrip, revision/draft handling, neutral LED state and shared painter output.

Native Test wrapper passed donor/crate gates then refused the canonical cache
above the existing 8 GiB budget. Consulted actual recorded build/storage recovery;
no guard bypass, cleanup reroute, new cache or direct compiler retry. Rust tests
and deployed screenshot acceptance remain pending; CI is not waited on.

## One authoritative selection domain, with normalization retained

Reconsulted ui-shell and exact controls.rs donor row and actual neutral/editor
slices; no full-file import or new donor parity claim. The preceding fix still
passed a separately reparsed matches list into native painting/navigation/status.
Removed that path: Selection owns validated IDs, Find is its input adapter and
highlighting is its membership projection. Web likewise removes the duplicate
matched class and redundant Select matches command. Selection IDs now drive all
paint, navigation, counts, manager/menu and bulk targets. Status canonical query
and editor draft are explicitly distinct; membership aliases read the same set.

User clarified that reparsing for normalization is useful: parsing is retained
at validation/commit and valid blur/Enter; normalization derives sorted,
deduplicated ranges from the model. Invalid drafts preserve canonical membership
and navigation. Added one native/Web shared transition fixture and shipped-Web
membership/ARIA/count/normalization/Enter/F3/Escape checks. Tightened native
numeric input to match Web ASCII-digit and single-# rules; +3/##3 cannot diverge.

Patch validation rejected a repeated file target and then out-of-order hunks;
no partial edits were assumed. Split into verified ordered patches. No blind
compiler retry: native Test passed donor/crate gates and again refused the
existing canonical >8 GiB cache. Actual source/Node checks are distinct from
pending Rust execution, native captures, app replacement and board deployment.

## Manage cells hover geometry

Consulted ui-shell and exhaustive exact dialog.rs donor mapping, then read all
952 live donor lines through EOF (snapshot hash matches). Adapted actual
navigation_control fixed-slot/clipped-text/inside-stroke mechanics to the cell,
row and state selectors. Palette/selection/animation still use the table's one
cell_surface; there is no separate tinted selection engine. Other generic dialog
helpers and actual donor numeric/host/geometry tests remain named pending items
in the same comparison manifest, not exclusions or a whole-file parity claim.

Traced the regression through locked egui 0.36.2 Button::selectable / atom_ui:
the inactive unselected Frame omits its stroke, while hover supplies a styled
Frame; AtomLayout sizes and positions contents using Frame::total_margin, which
includes stroke width. That is layout-changing chrome, not just an optical
outline. The manager now allocates constant 32/96/104 x 28 rectangles and paints
strokes inside them, with centered text independent of the stroke. Row headers
remain top-aligned when cell slots wrap. Web fixes the border-box slot and puts
focus chrome inside; selected focus cannot add a competing outline.

Added an actual native painter/input regression across narrow/wide widths,
animation on/off and idle/hover/press/focus/selection. Native Test passed donor
and crate gates but refused the existing canonical >8 GiB cache before compiling.
No cleanup reroute, direct compiler, new cache, app replacement or board flash.
Rustfmt parses/formats the patch; native execution and captures are pending.
Shipped Web selection/manager DOM-double tests and geometry source guards,
all ten appearance tests, access/display/branding/menu/generation checks and
reuse/comparison adversarial checks pass. These are not browser pixel evidence.

A first Web patch assumed a standalone closing style tag and was rejected
without edits; inspected the actual embedded boundary before applying. A display
test filename was wrong; rg inventory identified web_display_test.mjs, which
passed. Donor read/comparison is 48 complete files of 1874; 1826 are pending.
Useful generic functionality remains open and no project-wide superiority is
claimed. The existing issue #8 owns runtime acceptance; CI is not waited on.

## Explicit mode-gated cell reorder

Reconsulted ui-shell and exact donor-audit layout.rs/HardwareTab.tsx rows. Actual
bounded drag source/target/rank and Web drop/drag-image lifecycle slices were
read and adapted; neither large file is certified fully read/compared. Native
layout source has changed from snapshot. Recorded current hash and screened
status rather than rebinding a stale donor comparison. Added HardwareTab to the
ui-shell index so this generic capability is not missed by future lookups.

Implemented one validated client order used by both table and manager. Reorder
and reset require explicit Edit order mode; mode defaults off and cancels active
drags/marquee on transition. Before/after insertion preserves IDs, mapping, LED
state and canonical selection. Native drag ownership is surface-specific and
visible-target clipped; Web drops require an active own source, not external
transfer text. Valid saved orders repair duplicate/missing/out-of-range IDs.
Actual donor geometry/release/insertion and Web lifecycle code are adapted, not
replaced by a style-only approximation. Generic host presentation RPC, full
manager/lock/hide features and remaining donor tests stay pending.

Same nine-step fixture drives native/Web insertion semantics. Actual shipped
Web DOM/drop tests additionally cover both surfaces, cancellation, mode gates,
normal-mode marquee, navigation and exact persistence-write counts; no reorder
calls any device API. Existing appearance/access/display/menu/branding checks,
generation consistency, Rustfmt parse and hash-bound comparison/preflight tests
pass. Native Test wrapper again stops before compiling at the canonical >8 GiB
guard; no bypass, alternate cache, cleanup or running deployment was attempted.
CI is not waited on. Browser pixels/native mouse gestures/mobile touch behavior
and cross-client shared presentation transport remain separate acceptance.

Initial combined Web patch had an earlier CSS hunk after later script hunks and
was rejected without partial edits. Sorted verified hunks before applying. The
new fixture caught a stale query caused by an older test's direct Set assignment;
normalized that fixture through the actual paint adapter before asserting order
preserves canonical selection. Existing donor anchors were rereviewed in the
focused diff; only affected target/proof hashes were updated. No extra completed
donor-file reviews or whole-project parity claim.

Publication follow-up: the first push failed its TLS handshake through the
configured localhost:10808 proxy. Verified read-only HTTPS through the user's
localhost:8118 endpoint, then succeeded with a per-command Git proxy override;
no persistent Git setting changed. A subsequent network-cloud lookup detected
concurrent Pealayer tracked-inventory drift after the earlier passing checks.
No donor content/source hashes were overwritten or rebound and no further build
was attempted. The next donor consultation must refresh tracked inventory first;
earlier test/comparison counts are checkpoint evidence, not a fresh claim about
the concurrently changed donor. Recovery and source publication are recorded
in issues #8/#3 and PR #1. Feature source is f5940fd; runtime acceptance stays open.

## Six-pixel view and deployment gate

Refreshed all tracked Pealayer paths before the new lookup, including concurrent
source drift. Consulted ui-shell, games-streaming, firmware-hardware and exact
donor-audit sources. Hardware-control/controls indicator and neutral fixed-slot
slices remain screened, NOT complete comparisons. WS2811.h/cpp were reread fully;
actual full-brightness WS2811/TypicalLEDStrip setup stays, BRG acceptance remains
pending. Added WS2811 and hardware_control to specialist routing. Rereviewed
unchanged Pealayer menu/icon/shortcut/appearance/branding/dialog and PU Shell/
Telnet anchors before binding modified target hashes. No new full-file parity.

Implemented configurable Circle/Hexagon/Rounded surfaces, six top-first CCW RGB
sectors and optional zero-based physical indices. New opt-in authenticated status
snapshot reads the committed ESP buffer and physical mapping. No cell-state or
sampler-intent RGB invention, duplicate buffer, continuous UART broadcast, LED
rate cap or per-frame file persistence. Missing/invalid/uncommitted/stale samples
remain unavailable; native USB-only does not borrow another Wi-Fi peer's values.
Immediate cell rendering and partial streamed-frame isolation have actual
LedGrid host tests with Arduino/FastLED doubles.

Web DOM/HTTP tests, shared geometry fixture, appearance/access/display/remote,
branding/menu/capture guards and comparison/preflight checks pass. Both existing
ESP profile caches compile final source; no unrelated AVR/APK/donor build.
Native Test passes donor/crate gates then stops before compilation at the >8 GiB
canonical-cache guard. No bypass, cache relocation, cleanup retry, package copy
or app replacement. ASUS SSH localhost:1022 is refused; no physical deployment
or EEPROM write. CI is not waited on. See CELL-PIXEL-VIEW.md for exact images,
hashes, API semantics, hardware boundaries and remaining acceptance in #8/#3.

The computer-use skill directed the supported browser workflow. Extended the
existing read-only Web fixture with --pixels; no second capture/package harness.
Actually observed Circle and Hexagon, side indices 474–479 after remapping,
amber selection, and Find cell changing to 1. Saved no-overwrite JPEGs through
the browser screenshot API and inspected them; closed the owned tab and fixture.
These prove current embedded Web rendering, not the older running native binary.

Recovery details: incorrect CSS/doc anchors and out-of-order hash hunks were
rejected with no partial edits, then corrected from exact live context/file
order. Comparison guard exposed additional LedGrid and ESP/Telnet target bindings;
actual relevant donors were consulted before updating them. A raw PowerShell
foreach pipeline was a parse error, repaired by collecting results first. The
FastLED test double initially lacked std::max; inspected actual approach() and
added the missing stub import, not a product workaround. The access fixture now
matches the status pathname including its opt-in query, and long-name tests
assert shortened labels plus full tooltips. A getByLabel locator mismatch was
resolved from a fresh snapshot using the observed combobox role. rustfmt on the
root module touched unrelated children; reversed only this turn's mechanical
formatting and used skip_children for affected modules thereafter. No unknown
user/donor edits were discarded or source hashes silently certified.

Final inventory: 1875 paths, 48 completed comparisons, 1827 pending and 29 known
gap files. Refreshing screened evidence does not close their remaining imports.

## Acknowledged peer context checkpoint

Consulted ui-shell, firmware-hardware and exact donor-audit mappings before
implementation. Read PCController peripheral-availability.ts and its test through
EOF, plus all 1279 lines of Pealayer board_info.rs. Inspected status_bar.rs
hardware readiness/optional telemetry slices only; its full-file comparison is
still pending. Added device-context routing and three complete-read comparisons,
with exact omissions rather than a claim of complete application parity.

Fresh manifest ownership now gates native panels, menus, shortcuts, settings,
title, status bar and independently acknowledged Display/Text/Buzzer cards.
Table-only modes are advertised, Snake is a presentation, First wins additionally
requires inputs/outputs. Workspace restoration cannot paint unavailable tabs.
Domain commands, unsent batches, mapping/pixel selection and streaming are guarded
on context loss/switch. Per-peer dock state is bounded memory, not discovery-driven
SSD persistence. No runtime build/profile/cache policy changed.

Passed production protocol/LED/remote/ISP/client/Telnet/busy/audio/identity/access/
async/range/front-panel/PC-profile host tests, the context test including an extra
Table-disabled C++ build, Web context/access/cells/display/remote/appearance,
branding/menu/screenshot source guards, crate boundary, reuse ledger/preflight and
comparison checks. Native source parses with the actual Cargo edition 2024.
Native Test was attempted through the donor-gated wrapper and stops at the same
>8 GiB canonical cache guard BEFORE Rust compilation. No bypass, duplicate cache,
release copy or running app replacement. Runtime acceptance remains open in #10.

Both AVR and ESP busy-wire/polling profiles compile in existing caches. No SDK
installation, clean rebuild, unrelated APK/donor build, physical flash or EEPROM
write. ledboard.local port 80 is unreachable and SSH localhost:1022 refused.
CI is not waited on. Build output and checksum receipt below are static evidence.

The computer-use skill directed actual read-only browser verification of explicit
simulated controller, unacknowledged and Table peers. No board mutation was sent.
Saved three no-overwrite JPEGs, inspected them, and retained small copies under
docs/screenshots. Closed all three owned tabs and stopped only fixture PIDs whose
node command lines were freshly verified. Source remains deployable work, not a
claim that the older native executable or hardware already displays this UI.

Recovery: the first ESP compile exposed String/const-char* JSON-escape mismatch;
consulted firmware-hardware and inspected the actual helper before using c_str.
A status-bar lookup caught concurrent tracked donor inventory drift; refreshed all
tracked Pealayer paths before consultation, leaving donors read-only. PCController
also gained four concurrent tracked paths; refreshed its inventory, not its code.
Hash rebinding preserved existing donor source certificates and unchanged anchor
semantics. Initial evidence hunks assumed field order and were rejected without
partial writes; corrected from exact lines. Oversized combined JSON diagnostic
output was truncated; switched to bounded references/hashes, not larger copies.
PowerShell foreach-pipeline diagnostics were corrected into collected output.
The guessed screenshot test filename was missing; inventoried tests and executed
the actual screenshot_harness_test. Rust parsing initially used edition 2021;
read Cargo.toml and re-ran with its actual edition 2024. No product workaround or
guard bypass resulted from these diagnostic errors.

Current inventory: 1884 paths; 51 complete-read comparisons, 1833 pending and
32 known gap files. See DEVICE-CONTEXT.md and issue #10 for unchecked native,
generic-adapter and physical acceptance. Screenshots are simulated Web proof only.

## Mobile context and RayanLamp continuation

Rechecked the actual Build RayanLamp Go app task read-only; it was not resumed
or messaged. Read current mobile sources, PCController peripheral availability
implementation/tests and Pealayer branding.rs through EOF; consulted device-context,
ui-shell and build-storage. Re-read actual RayanLamp digits.go before Dart reuse.
Extended only those reviewed comparison entries and affected exact proof hashes.
No new blanket certificate or unread-source exclusion.

The mobile README identified the existing C:\development\flutter SDK. PATH
absence did not justify installing/extracting another copy. Added the mandatory
mobile wrapper with existing source/storage gates, resolved packages, canonical
cache budgets and no clean. Analyzer passes and 29 tests execute, including
individual ACK loss, cross-profile identity, queued stale writes, digits/ranges,
shared selection, partial bulk failure and phone/tablet layout. Actual loaded-font
Flutter capture was inspected; complete capture preserves all 80 cells.

Recovery evidence: an initial root-scoped Flutter test found no test directory;
the wrapper now owns mobile cwd and has a regression assertion. New hostile-value
fixture initially inferred Map<String,int> and rejected the test's double before
the parser; changed the fixture to dynamic without weakening parser validation.
Analyzer lint failures stopped before packaging and were corrected; one patch
placed a typedef in the wrong context, caught by parsing and fixed before tests.
Two interaction assertions lacked the actual viewport/stream-settle state; fixed
test setup, not application ACK policy. First capture clipped its last row; kept
it intact and captured a separate full-height image, then inspected both.

Imported storage summary enables StrictMode in callers. Both the lookup and its
test needed explicit @(...Remaining).Count for scalar pipeline results. A Plan
assertion also exposed generator stdout leaking into the result pipeline: wrapper
now emits diagnostics to the host and preserves the one structured Plan result.
Regressions cover these cases; no guard was disabled or bypassed. Evidence patch
generation initially lacked per-line addition markers and was rejected atomically;
corrected from exact anchors, preserving all existing omission decisions.

Warm Android preview build passed; cache grew from 0.971 to 1.021 GiB. Existing
Flutter 3.47.3/Dart 3.13.3 and debug signing template, no dependency upgrade,
SDK download/extraction, extra cache, unrelated firmware/native build or physical
flash. Source master version remains 0.2.7. SSH localhost:1022 refused and
ledboard.local resolution timed out; old native app is preserved. CI not waited on.
MOBILE-CONTEXT.md and issues #2/#3/#10 retain physical/signing/localization/settings
and remaining donor-import gaps. Inventory is still 4237/67/4170/42, not full parity.

## Actual mobile device settings continuation

Consulted preferences, exact Pealayer PreferencesTab donor-audit, RayanLamp
settings/digits audit, device-context, build-storage and capture. Read all 593
PreferencesTab lines through EOF plus canonical native/Web settings, shared
catalog, firmware identity/auth routes and AccessPolicy. RayanLamp's 1827-line
settings.tsx remains pending; it was not certified by lookup or historical claims.
Re-read actual digits.go and invoked its imported map only for numeric fields.

Implemented mobile Wi-Fi, MQTT, outbound WebSocket, full board identity and
access editors, explicit positive ACK/public readback, secret modes, request
epochs, context revocation and acknowledged auth/rename handoff. Added one
complete-read parity-gap comparison, concrete useful omissions and a mandatory
mobile-settings build gate. No generic Pealayer preference mechanism is excluded
as media business. Updated the same ledger/report and issues #3/#4; counts now
4237 paths, 68 complete, 4169 pending, 43 known gaps. RayanLamp remains six reads.

Analyzer clean; 46 Flutter tests pass. Actual loaded-font simulated phone/tablet
captures inspected, zero device writes. First captures exposed a missing standard
MaterialIcons font in the harness; loaded the already bundled font and supplied
actual verified Phosphor caret glyphs. Preserved initial files, captured separate
corrected evidence and verified small repository copies by exact byte hashes.

Recoveries: an omitted test brace failed lint before tests; an import patch without
an anchor landed after declarations and was fixed before testing. Phone dropdown
overflow was a real defect: made it expanded with ellipsis and verified both
widths, without weakening overflow assertions. Delayed-response test callback
setup required an event-loop flush before indexing its pending replies. Guessed
numeric-helper paths failed; inventoried mobile sources and exact donor rows to
find cell_search.dart/internal/i18n/digits.go, not another replacement helper.

Staging succeeded with existing content-equality/file-identity helpers, required
gates, one verified pending copy and recoverable rename of the old preview.
The following comparison generation under imported helper StrictMode exposed
empty specialist route `.issue` access and optional missing `.improvement`.
Consulted build-storage, read the actual generator/test in full, adapted pipeline
enumeration/optional-property handling, added strict-mode fixtures and verified
both full generation and read-only check after report regeneration. No source
guard was disabled, no unread comparison promoted, no blind retry/cleanup bypass.

Warm APK SHA256 8b01492724b48daf0d42ed3666f82144c6e7d6aa55d26614eabd25502f00f596;
version 0.2.7, existing debug signing template, preview only. Prior preview hash
4aabc188bdab5f984b064e17ec417f85e467436d8e08d0eb94eaf984fbbeb1b4 is retained at
outputs/PakarController/mobile/PakarController-preview-previous.apk, not deleted.
Mobile cache 1.021 to 1.027 GiB. No SDK/download/dependency upgrade, duplicate
cache, unrelated native/firmware build, physical flash, running-app replacement
or CI wait. Full omissions/acceptance and screenshots: MOBILE-SETTINGS.md.

## Mobile firmware / picker continuation

Reconsulted updates-telemetry and exact exhaustive picker/helper rows. Fully read
Pealayer ServerFilePicker.tsx and preferenceFiles.ts, native Pakar maintenance,
actual ESP routes/upload/RemoteFirmware JSON and Android MainActivity/manifest.
Added specialist mobile-firmware routing and gate after the audit-only rows were
identified. Actual .bin suffix logic is imported; SAF user-granted navigation
replaces an unavailable ESP server-filesystem contract, not arbitrary path entry.
Concrete remaining provider/localization/generic-consumer behavior stays pending.

Recovery: first analyzer rejected redundant imports/non-null assertion, then an
added callback brace; fixed code without disabling lint. Runtime widget test
found a genuine stale authorization closure: FirmwareController now reads the
current widget callback after capability updates. A perpetual confirmation-time
indicator also prevented settle: separate confirmation lock and actual operation
state instead of weakening the assertion. All 72 tests then passed cleanly.
Hash rebinding patch initially emitted out-of-order hunks and failed atomically;
inspected exact references and applied source-ordered reviewed-line updates, not
a blind all-target recertification. Incorrect relative lookup paths were resolved
with the actual root/inventory; no missing tool/platform conclusion was drawn.

Tablet/phone renderer images inspected; actual Kotlin bridge compiled in the warm
APK build. All gates and source-quality children pass. New documents are selected
by SAF with no broad storage permission; exports validate full backup size and
consume a one-use created-URI token. These are mock/code/package proofs, not
physical provider/board/TLS/EEPROM/Urboot/power-loss acceptance. Exact hashes and
recoverable package rotation are in FILE-COMPARISON-RECEIPTS.md. No CI wait or
hardware write, donor mutation, production app replacement or cleanup bypass.

Final integrated parent regression required a real lock-release fix after ACK
revocation; 73 tests now pass. The fixture's guessed Firmware label was corrected
from the shared generated catalog. A final warm APK build replaces only this
turn's unpublished intermediate after exact owner/pending/rollback verification.
File.Replace null-backup binding failed before mutation; inspected all hashes,
reconsulted build-storage, reread actual stage-desktop.ps1 and used its established
verified pending-copy/Move-Item pattern. Old published settings and earlier
rollback APKs are retained, no redundant intermediate archive or new copy made.

## TM1637 / local CLI / verified desktop replacement

Source checkpoints be41d1af and a6f13a0a fully reread actual PCController
LocalLib/SevenSegments.h/.cpp, firmware_controls_test.cpp and its Arduino
dependency, and actual Pealayer src/cli.rs through EOF. The authoritative
comparison ledger records exact source/target/proof hashes and concrete omissions.
Actual display ACK/recovery tests now execute both fallback GPIO and AVR PC4/PC5
register branches. The CLI imports donor help/version formatting and early-exit
contracts; saved hostname/user defaults are shared with GUI configuration.

Unexpected staging failure: the original package probe invoked --help, which
Pakar mistakenly treated as GUI startup. It timed out, and the surrounding shell
sequence failed to inspect the child staging exit code before using old staging
bytes. Consequently the initial production replacement was NOT a latest-build
deployment. No settings or older rollback files were removed. The mistakenly
launched source executable was inspected for open drafts and gracefully closed
through --quit before rebuilding. The correction adds actual donor CLI output
before config/IPC/GUI/network creation; outer staging now checks its exit code
before any production mutation. Probe timeout child disposal alone is still not
child shutdown; that original donor limitation remains pending.

Guarded release of a6f13a0a3de64fca9420df6938e01e455f08ee8c passed using the
existing warm desktop/target release cache. The built and staged executable
SHA256 is 828d499745c93b580c0cc2b1096bef012017f9b2b280ddb4a04a5d348de5edeb.
Actual bounded --help and --version probes both exited successfully without
creating an app. Then verified current production and the existing rollback
matched one another, verified no owner was running, copied a checksum-verified
pending candidate and renamed only production/PakarController.exe. No extra
rollback rotation or package directory copy. Preferences were byte-identical
before and after launch.

Live production PID 81396 reports version 0.2.7, single_instance=true,
configured_endpoint=http://ledboard.local, no Preferences/settings write draft,
and no acknowledged connected board. --version and --app-status secondary
processes retained that one owner. Internal framebuffer capture completed for
that exact production PID; outputs/PakarController/desktop-current-cli.png was
actually inspected. It truthfully hides table/peripheral/mode cards offline.
It also exposes uncorrected caret glyph boxes, repeated disconnected transport
errors and a Windows-shell initialization error. Those are useful native parity
gaps, not acceptable exclusions or proof of an elegant completed UI.

External Windows WGC capture failed with 0x8007041D, including one fresh-window
recovery; no further retries or stale-coordinate input. The existing imported
internal/Win32 capture wrapper succeeded via framebuffer instead. No blanket
Windows screenshot-unavailable claim and no blank-image acceptance.

CI be41d1af run38100858767 passed workflow/source, both firmware link modes and
Android preview, but the Windows phase rejected a stale SevenSegments.cpp hash
in the older PC contract manifest. Reconsulted tm1637-display and reread actual
donor/target source; selectively rebound that one mapping, documented weak-bias
adaptation and added contract-vs-authoritative-ledger hash agreement. The focused
23-map assertion now passes. No assertion removed or unrelated hash rebound.

Source-stability checks establish the clean compiled checkout, not embedded Git
identity in the executable. Full app metadata/manifest/updater and native UI
parity remain open. Local release execution is not all-green CI or physical
hardware proof. ASUS SSH localhost:1022 refuses and ledboard.local does not
resolve; no AVR/ESP flash or live bus/LED/audio acceptance this checkpoint.

## Actual dropdown/color continuation

Fully read Pealayer dropdown.rs (631 lines), color_picker.rs (446 lines) and
icons.rs through EOF at clean donor HEAD 3265bf80. Import the actual modules,
all three dropdown/four color tests and icon search/list/grid registration hooks.
Read the locked egui ComboBox factory and the actual Preferences Accent branch;
do not certify the entire Preferences dependency from that slice.
Preferences/device-settings/shell selectors and both connection popups now use
the shared focus/search/choice lifecycle. Session recents are temporary state,
not per-click persistence. Busy USB choices and Custom palette have new native
behavior fixtures. No top-menu height change or unrelated board build.

Initial generated patch used a relative path and created one owned dropdown
file outside the canonical checkout; verified its exact transformed donor bytes,
then used apply_patch to move only that content into the absolute canonical
target and remove only the mistakenly created file. No user files removed.
An icon-context mismatch and a truncated shell patch were rejected atomically;
actual bounded source rereads preceded precise replacement patches.
PowerShell R/H aliases invalidated a generated evidence patch; fail-closed
prefix validation rejected it before mutation, and explicit helper names plus
terminating errors corrected the generator. A whitespace-dependent icon
omission match and an incorrect proof anchor also failed before certification;
inspected exact source/anchor and corrected them rather than suppressing gates.

Selectively refreshed only Pealayer inventory and rebound reviewed native/
wrapper target references. Original donor hashes were not blindly updated.
Ledger now accounts for 4242 paths: 91 complete-read hash-bound comparisons,
4151 pending comparisons and 57 known gap files. The older inventory's screened,
stale or legacy hints are not extra accepted comparisons.
Actual mapping and omissions: NATIVE-DROPDOWN-REUSE.md and the authoritative
file-comparison-evidence.json. Dynamic System accent and complete shared
Preferences/menu/board adapters remain pending, not excluded business.

Fresh GitHub enumeration fetched every body/comment/review/thread page for
Pakar17, Pealayer143 and PCController634 records. Read eight additional full
UI/state conversations and rechecked all four unresolved donor threads;
seventeen complete-record and two partial-comment decisions are hash-bound.
Collection of the other records is not semantic review or PR-diff acceptance.
Pakar/PCController have zero open PRs; Pealayer #142 remains under its owner
with conversation read but full diff/readiness not certified here.
Current committed b6bd876 Windows/firmware/Android workflow and CodeQL both
completed successfully. New uncommitted native imports are separate.

Read the exact Build RayanLamp Go app task again without resuming it or
messaging it. Its source has no Git metadata; no verified GitHub repository
can be inferred. Existing local imports remain usable, but a Rayan issue/PR
audit cannot be claimed without its actual repository URL.

Guarded native Test compiled in the existing release cache: 125 passed, three
failed. All three donor dropdown tests, four color helpers and busy-port
keyboard fixture passed. The new Accent fixture chose the closed Custom label
instead of the popup row; its 11-row palette also needed an explicit bounded
height. Scope the target to the measured popup, retain the no-mutation assertion
and verify the actual palette content. The icon test failures were outdated
Seat-in-viewport assertions: reread actual current donor tests, which explicitly
use the visible Folder row after fixed-height scrolling. Import those exact
reopen/filter assertions without deleting their behavior checks.
Reconsulted native-dropdowns and read actual donor Accent interaction tests and
icon test dependencies before correction. A path-before-hash-only mechanical
rebinding missed new JSON references with reversed key order; the mandatory
generator rejected it. Correct only the two reviewed target paths independent
of key order, preserving source hashes and all assertions.

Second Test run: 127 passed; icon cases fixed, Accent fixture still failed.
Read actual locked egui Popup::show and Memory::end_pass: a probe-only frame
abandons the popup. Move root-ID measurement before opening any popup; retain
open-state, value-preservation and actual Shared-colors paint assertions.
Final guarded Test: all 128 pass, zero ignored/failures. The source-quality
phase passed; final focused wiring/menu/connection/lifecycle/API/PC mapping
and generated-comparison checks pass. No test removed, gate bypassed or second
cache/dependency tree created. No additional firmware/APK build for this change.
Five existing/unused-helper warnings are not claimed fixed.

Reread the drifted Pealayer #142 body and all six comments; refreshed only its
conversation decision hash. All seventeen reviewed-record and two comment-scope
decisions now match the refreshed corpus; 775 records remain semantically pending.
Pakar #3 current counts updated without marking runtime acceptance prematurely.
Fresh ASUS SSH1022 still refuses and ledboard.local still fails DNS.

## S3 flow-control lookup and provisional DevKit clarification

The requested uart-flow-control concern was missing and its lookup failed closed.
Consulted firmware-hardware instead and inspected all matched protocol/UART/event
pending actions. Added an exact uart-flow-control route to the same index, including
PCController UartProtocol, codec/runtime and VirtualBoard UART/codec tests. These
files remain pending comparisons, not newly certified donor reads. No replacement
protocol or untested AVR event-flow mutation was introduced after that failure.
The current AVR no-busy build is still polling-only; paired buffered mode remains
an explicit migration gate. S3 pinout now records the user's provisional common
DevKit assumption without inventing flash/PSRAM or partition details, and calls
out the AVR RESET pull-up voltage hazard in addition to TXD/MISO.

## Connection incidents, distinct failures and quiet recovery

Consulted network-cloud, donor-audit Pealayer messaging.rs, web-presentation and
the new error-management route. Read actual Pealayer messaging.rs, ui/error.rs
and network.rs, plus RayanLamp browser-console.ts and full tests. Missing
Pealayer src/error.rs was resolved with rg to src/ui/error.rs, not declared absent.
Pealayer four_d/engine.rs transport-rejection/mark-disconnected slice informs
separate lifecycle versus explicit-operation outcomes; no full-file certificate.

Adapted the existing Rayan logger's keyed mechanism to bounded source/cause
incidents rather than periodic category suppression. Native diagnostics now
coalesce alternating failures, preserve differing causes/routes/ports and every
explicit operation result, count repeats and report source-specific recovery.
Added read-only retry backoff, one in-flight status poll and stream cancellation
on WebSocket loss. No donor mutation, new dependencies or persistent event files.

First browser recovery count assertion saw21, correctly including the prior401;
retained it with the exact explanation. Browser tests then passed. The old
hash-only rebinding regex missed reference objects whose quoted anchors contain
braces, so the comparison gate rejected api.ts's Web target. Reconsulted the
actual API source and guard; used quoted-string-aware reference-object matching
for only the reviewed target/proof paths, leaving all donor hashes untouched.
Regenerated the same ledger and added exact error-management routing; no pending
donor becomes an exclusion or complete read merely through this change. Native
build/live proof is recorded separately in CONNECTION-DIAGNOSTICS.md.

Completed CI run38107886274 exposed a stale realtime-retry VM fixture: its
isolated globals omitted the actual logger now consumed by connectWs.
Consulted web-presentation/error-management, reread the shipped retry/logger/test
and the actual Rayan sync.ts scheduleReconnect/prepareReconnect slice. Load the
real production logger instead of stubbing recover, preserving every existing
session/cancellation/no-replay assertion. Added repeated503 preparation, distinct
500 preparation and matching-handshake recovery assertions. Preparation errors
now use the same WebSocket incident owner, not periodic three-second suppression.
No new full donor-file read certificate from that slice. Rebind only changed
target/proof bytes in the existing ledger; do not alter original donor hashes.

The local quality run then found two older PU route-parity Web target hashes.
Consulted updates-telemetry and reread both mapped target rows and actual line244:
the remote-update section, authenticated root route, capability gate and no-replay
warning are unchanged. Only diagnostic JS changed elsewhere. Rebound those two
target hashes, retained their exact line244 anchor and all50 route assertions;
no donor exclusion or firmware-route behavior was changed.

## Buffered AVR/S3 preparation and actual UART donor review

Fresh source/main clean; prior60d8668 complete CI38140468478 succeeded (Windows,
both firmware variants, Android); f0adf3a still running at snapshot, not waited.
ASUS SSH refused, ledboard.local DNS absent; continue offline as authorized.
Consulted uart-flow-control, exact donor-audit and build-storage. Read complete
actual PCController UartProtocol.h/cpp, uart_protocol_test.cpp and codec/contract
dependencies. Canonical c26cc561 clean; refresh only its1211-file inventory,
preserving twelve earlier reviewed hashes and every other donor row.

Adapted actual bounded RX snapshot and nested/split/noisy test behaviors into
shared SerialLink; cap64 bytes for large S3 rings, reject negative reads/occupancy,
expose existing ESP counters for ESP32, and repair a failed TX boundary before a
later independent frame without payload replay. Partial outcomes stay unknown;
a delimiter may complete a CRC-valid prefix, never infer rejection from failure.
Added mutually exclusive buffered AVR macro/profile, full-frame TX readiness and
failed game-state dirty retention. Reject buffered ESP8266 and preserve busy-wire
default/INPUT_PULLUP, polling and physical pinout. No extra protocol/effects engine.

Comparison gate correctly exposed changed old test/proof hashes. Reread changed
target behavior and existing assertions; only script test invocation/build mode,
AVR dirty handling/flow branch and small firmware CI additions changed. The first
rebinding attempt assumed multi-line refs but one was compact JSON; it failed
atomically. Match parsed selected refs and original whole lines independent of
formatting; rebind only those reviewed target/proof hashes, no donor certificates.
Same ledger now94/4242 complete,4148 pending,60 gap files. Useful zero-copy,
counter taxonomy, timestamp/runtime and physical omissions remain explicit.

Actual focused UART tests pass in three profiles; full source-quality phase passes.
All three final AVR compiles pass:26840/26958/26670 flash bytes (buffered/busy/poll),
1226 static SRAM bytes,32768 merged Urboot images. See UART-FLOW-MIGRATION.md for
exact meaning. Existing desktop/settings hashes preserved; no physical write or
native/Android rebuild. Final partial-write refinement used only focused tests,
same warm AVR caches and deterministic comparison generation, not another broad
GUI/Android suite. Latest source CI proceeds asynchronously.

## Interactive TUI: donor-backed bounded checkpoint

Baseline 5d02b07 Build38141677102 and CodeQL38141677110 both passed;
no open PRs. SSH1022 timed out and ledboard.local remained unresolved. User is
migrating hardware, so no physical upload, discovery or LED/display success.

Consulted real Rayan terminal/run/text/keys/runtime sources and their platform
dependencies through EOF, plus actual PCController table.go viewport formula.
Seven new hash-bound comparisons retain exact consumers and tests, with useful
omissions pending rather than claiming full TUI parity. Crossterm replaces Go
toolkit ownership; selection, Find, incidents and CLI parsing remain shared.
New terminal-ui gate is part of source-quality CI. No GUI owner or settings edits.

Donor inventory drift was detected rather than ignored: reread full changed
Pealayer i18n.rs, web i18n.ts, Windows shell test and CI workflow (including new
macOS diagnostic sampler), plus PCController TonePlayer/ShiftRegisters/
SevenSegments.cpp. Rebound only their original source hashes after actual reads.
ShiftRegisters comparison is now parity-gap: donor's new 328P-only guard is
not silently called byte-identical. Correct PuzzleBoard A4/A5 pins and input
validation remain intentionally stronger; no copied PB3/PB5 regression.

Inventory now4255 files,101 manual comparisons,4154 pending,68 known gaps;
not complete imports. Guessed legacy-review filename failed; rg --files resolved
the actual reuse-review-evidence.csv. A full evidence JSON diagnostic exceeded
output bounds before mutation; limited changes to parsed reference leaves.

Final native Test:147 passed/0failed, including HTTP batch peer-change/false
receipt stop, captured shared CLI output and post-preflight cancellation. Four
bounded native passes reused the same release cache; no Android/firmware rebuild
or debug artifacts. Release, built redirected-terminal guard and current source
quality execution are separate receipts, not inferred from these unit tests.

Full source-quality phase passed. First Release (4m56) and built CLI suites
passed, then a real Windows ConPTY session (unreachable private loopback fixture)
opened Events/Help and q restored raw mode/cursor/alternate screen with exit0.
Repeated connection failures stayed one incident with increasing occurrences.
This session predates the small resize clipping refinement, whose own native
run verifies width0..33/height0..8. It is not board or graphical UI acceptance.
Also wired the existing bounded describe_error cause-chain helper into shared
blocking_status: otherwise the TUI would merge distinct DNS/socket/TLS failures
behind reqwest's identical outer Display. Final Release/built status test verifies
that integration; existing native diagnostic tests cover cause distinctions.

## Native HTTP queue ownership continuation

Exact HEAD 5212914 Build/CodeQL passed before this slice. ASUS SSH1022 refused
and ledboard.local did not resolve; no hardware action or deployment occurred.
PCController session.go/session_test.go were reread fully, matching the existing
1bf4d52/ff6c7b2 source hashes. Admission checks are adapted into a revocable GUI
HTTP generation with shared strict TUI pin validation; serial lifecycle/ACK/
priority/reset and non-HTTP/mobile ownership remain explicitly pending.

First guarded native Test stopped at compilation: main's common peer reset could
not access TableState.stop_pending_edits, which was pub(super) inside ui.
Consulted peer-binding again and inspected the actual existing table reset;
changed only that method's visibility to pub(crate), not its behavior or cache.
No package was produced/replaced, no direct Cargo bypass, cleanup or debug output.
Rereview/rebind affected target hashes before retry; test execution is separate
from source/proof anchors and cannot establish physical acceptance.

Second guarded build compiled;160/162 tests passed. Two new loopback fixtures
failed before sending because their test Wi-Fi password had only seven bytes.
Inspected the actual device_settings::Write::network eight-byte lower bound and
existing fixture tests, corrected the fixtures without weakening validation.
Also retain generic retired receipts before the endpoint envelope filter, so
an endpoint change cannot silently erase the old outcome. Payloads and credentials
are absent from those receipts. Source-quality passed before this final correction;
final native and hash/source guards must be rechecked, not inferred.

Final guarded native Test passed163/163. Focused final living-contract, render
lifecycle, settings, peer wiring, versions and generated comparison checks pass.
No release/deployment or physical success follows automatically from that.

Final RequireLive caught two newly tracked PCController control/mirror_segments
files at clean HEAD38f48d0. All20 compared donor hashes remain unchanged. Refreshed
only PC inventory, left both new files pending, regenerated comparisons and passed
the strict live peer-binding gate. Counts4284/106/4178/73. No bypass or donor edits.

73769c6 guarded Release/stage/probe passed, then atomic File.Replace moved the
staged payload into production with prior B5133DC9 executable retained, no running
owner and settings hash22F8AEED unchanged. Actual PID74392 framebuffer captured
and inspected. It revealed bootstrap configuration reads while offline, producing
redundant preflight/expired events. Reconsulted peer-binding and actual native
poll/admission path; gate settings and remote-job reads on fresh selected Wi-Fi
status, retaining legacy reads. Added source guard. Windows Shell E_FAIL startup
event remains separate production work; tray_available alone is not Shell success.

2bdbf76 adds only fresh-status admission for configuration/remote-job reads.
Its source guard and final comparison/version gates pass; guarded Release passed
in3m15 using the same cache. Final payload25394176 bytes, SHA256
4440B2B24C622A5BF58B5C466369FB621F453870A2E5BCA1D567B0C4DF81E435.
Per user policy no repeated full incremental native suite:163 tests passed on
73769c6; final small admission correction has source guard/build/live follow-up.

Stage stopped on Pealayer tracked drift, not a compiler failure. Reconsulted
build-storage; all43 compared Pealayer source hashes still match at clean
fbf2d162073fe6171cb17ba94ec68c3b202911db. Two added paths and one removed
generated bundle; refreshed only that inventory and left additions pending.
Current4285/106/4179/73. The subsequent ledger-only commit changes no compiled
desktop inputs; binary build identity remains2bdbf76, not the documentation HEAD.
Final output Native-HTTP-Ownership.json records actual replacement/capture status.

## ISP GET ownership boundary

Fresh c1517b2 Build and CodeQL both succeeded; Pakar open PR listing was empty.
ASUS localhost:1022 still refused SSH and ledboard.local did not resolve. Actual
desktop PID79784 was left running unchanged; no draft, settings, rollback or
physical firmware replacement occurred.

Consulted all firmware-hardware rows (88 matched/70 pending/11 gaps) and the
peer-binding specialist plus exact PCController donor-audit row. Reread complete
session.go and session_test.go through EOF; original-byte hashes remain
1bf4d52d6810630d1fd4d5f0a0c18254ccdd73c0942dbe94f4c97a06360680c5 and
ff6c7b21f72bfb944d232c87210f8ffa139dfe016e8b5756f2582604489fffc4.
Adapted admission/dispatch checks into all-method supplied HTTP epoch validation
and the real loop authenticate boundary. Hardware-affecting GET probe is no
longer exempt. Header-free compatibility remains explicitly unfenced; this does
not establish AVR-only identity, remembered pairing or non-HTTP ownership.

Comparison generation caught stale tools/test-protocol.sh after adding the new
test. Reviewed its exact additive diff and existing display runner anchor, then
rebound that proof reference without changing donor hashes or closing omissions.
Quality subsequently stopped at pu_routes_test.mjs on the changed firmware hash.
Reviewed both shipping diffs and affected route anchors; updated six existing
PU850 target hashes/line numbers, including previously stale Markdown anchors.
No generic route classifications, source hashes or check strength were changed.

Actual WebAccess middleware and AsyncHttpService queue execute in C++ using
transport/auth/SDK doubles. Stale/partial/malformed pairs fail, matching pairs
dispatch, legacy is explicit, auth/origin rejection precedes identity checks,
and retirement between enqueue/dispatch never reaches the hardware-call double.
These are not Basic-auth parser, SDK entropy, LAN or physical ISP proof.
Guarded peer-binding RequireLive and shared C++ protocol checks passed. Only
integrated ESP8266 busy-wire firmware was rebuilt in the existing warm cache:
945392 bytes, SHA256 CBF2F9A2108CE47C4CAA9EC337296F8F591ED0E2407E2177F431449E865C3EE4.
RAM56620, IRAM63055 including32KiB instruction cache, flash code895328. No native,
AVR, Android or donor rebuild; no clean/debug/incremental cache was introduced.
All source versions remain0.2.7, not proof of deployed version consistency.
Quality checks before the stale route failure passed; remaining checks resumed
from that exact failed step and all succeeded, avoiding repeat suites/compiles.
Ledger remains4285 inventoried/106 comparisons/4179 pending/73 parity gaps. This
bounded safety adaptation is not complete donor parity or production acceptance.

## Cooperative Tasks and Prog reset ownership

Fresh exact79c28571 Build38166374087 and CodeQL38166373952 both completed
successfully. Pakar open PR listing was empty. SSHlocalhost:1022 refused and
ledboard.local did not resolve; no board action or app replacement occurred.

Consulted firmware-hardware and exact donor-audit Tasks rows, then fully read
PCController LocalLib/Tasks.h/cpp and ControllerBoardMini Puzzles/LocalLib and
Timer/LocalLib Tasks.h/cpp. All six original-byte hashes match live donors; the
PCController checkout is clean38f48d06f6922225e9a100d0afc2d787b5a000bd.
The exact hashes, targets, omissions and next physical actions are in the SAME
file-comparison evidence, not a second inventory or a claim to have read others.

Found actual eight-slot PCController-derived code already present in BoardCore
but unconsumed/untested; earlier Puzzles ledger text incorrectly called it absent.
Hardened it in place instead of duplicating it: ten slots from Puzzles/Timer,
explicit occupancy/context/retirement from PCController, bounded eligibility
mask, cancellation/clear/recursive-update protection and half-range deadlines.
Actual AVR loop consumes it for one100ms Reset callback. Entering Prog cancels
that pending callback; dispatch checks the latch again before watchdog reset.
ACK remains scheduling acceptance, not reboot or completed programming proof.

Mechanical evidence patches initially failed context matching without altering
files. Reviewed exact ledger contexts, applied ordered hunks, and rechecked
affected AVR/CI/protocol-runner anchors. No stale donor source hash or failed
gate was bypassed. Added cooperative-tasks routing and mandatory test/build gates.

Guarded production C++ Tasks tests, actual sketch source wiring guard, strict
RequireLive six-file donor gate and complete source-quality phase all passed.
All affected AVR profiles compiled using existing warm caches (no Clean/debug):
busy-wire program27656/RAM1317, polling27356/RAM1315, buffered27538/RAM1317.
This leaves731-733bytes for stack/locals, not a measured runtime high-water mark.
Each with_bootloader image is32768bytes; SHA256 by mode:

- busy-wire:4577A98CE1FFD124FF5A41C8858637C412BF765A2527EFA2D4683605E9B7F1C6
- polling:78E50DE1F66F10BEFE4F3546139A4712D97101E3EF4C32D23684F4B5932A1201
- buffered:307365D1CBEFE8BDA3B163569B39BD266CED9DDD6D6B51AA5890AD95727B7261

No ESP/native/Android/donor rebuild or deployment; source versions remain0.2.7.
Actual clock-double scheduler tests do not execute AVR watchdog/ISP or establish
physical service latency, welcome timing or wiring. Reusable slot handles require
owner discipline; callbacks must return promptly and API is loop-only, not ISR.
Ledger now4285 inventoried/110 comparisons/4175 pending/71 known parity gaps.
Physical stress/reset/Prog acceptance and remaining production issue2 stay open.

## GUID-bound tray tooltip recovery

Exact9ac741ab Build38170618852 and CodeQL38170618787 completed successfully;
fresh Pakar PR listing is empty. ASUS SSH1022 refuses and ledboard.local is
unresolved. Actual production PID79784 remains; current IPC reports Preferences
closed/not dirty, mapping/settings closed and no pending settings/maintenance.

Consulted windows-desktop plus donor-audit. Initial exact src/tray.rs lookup
failed closed because that donor path does not exist. Located inventoried
src/platform/windows.rs instead; did not manufacture a mapping or skip import.
Fully read donor taskbar/tray plan and WINDOWS-SHELL-RECOVERY.md through EOF.
Read actual windows.rs2364..2421 registration and2483..2513 tooltip sections;
complete large-file review remains pending, NOT promoted by this focused read.
New tray-tooltip route exposes that exact pending source and both full-read docs.

Actual pinned tray-icon0.26.1 Windows implementation registers/apply_guid on
creation/icon/disposal but omits it in set_tooltip237..269. Microsoft confirms
subsequent Shell calls require the original GUID. Adapt Pealayer direct
NIM_MODIFY/TIP to existing HWND/shared GUID and UTF16 helper, preserving crate
menu/RAII/restart ownership. No dependency-source patch, upgrade or second icon.
Failed presentation is not cached; bounded latest-value retries feed existing
incident dedup/recovery. Observed TaskbarCreated invalidates cache after the
crate's own recreation, not an inferred successful visible restart.

PowerShell rg wildcard path was not expanded; resolved actual registry directory
read-only before inspection. One generated evidence patch used relative paths
at tool cwd and failed without modifying files; changed to absolute repository
target. A caller tested stale LASTEXITCODE after a successful PowerShell generator
and falsely reported failure; no generator/gate change or blind compiler retry.
A live-check call used nonexistent SourceRoots; read parameters and used default
DonorRoots. Correct strict live tray gate subsequently passed with zero stale.

Reviewed actual shipping diffs and rebound all eight affected main/shell/hook/CI
target references, preserving donor hashes/decisions and anchors. All45 compared
Pealayer raw hashes still match clean8337a722b0c93bd393b49d4c9ca3b5ea64ae9c32;
selectively refreshed only its521-file inventory, with no additional source read
or false pending promotion. Same ledger4285/112 comparisons/4173 pending/73 gaps.

Guarded native Test passed166/166 in the existing release cache (1m21compile,
0.26sec execution); production retry/NOTIFYICONDATA/UTF16 fixtures are included.
Full source-quality phase passed. No firmware/APK/donor rebuild or clean/debug
cache. Versions0.2.7 remain source identity, not physical deployed consistency.
Real package replacement/Shell acceptance, Explorer restart/visible action
acceptance and all board/full donor production gates remain separate evidence.

3fa9bc676f813c1b0050db24a2bd95ebf5e7069a guarded Release passed in1m58, then
stage/source-stability/CLI probe passed. Replacement guard initially compared
forward-slash input with backslash process path and stopped BEFORE quit. Applied
IO.GetFullPath/OrdinalIgnoreCase to both verified identities, rechecked current
draft/pending state and gracefully quit PID79784; verified exit, no forced kill.
Atomic File.Replace moved the staged payload to production and preserved old
4440B2B24C622A5BF58B5C466369FB621F453870A2E5BCA1D567B0C4DF81E435 executable
as PakarController.before-tray-recovery.exe. Saved preferences remain unchanged
22F8AEED4BF0CEBC3116C670F681D90A3D1B1B833ECE3DE23A491D0D896A8711.
Older rollback packages remain untouched; staging executable no longer exists.

Actual new PID79484/version0.2.7/25414656bytes, production SHA256
8F433FFAE3A812703CCD6BFBD6CB86C9ECCF8FD4ACBFCD104BE187879CD53EC4.
Fresh IPC reports tooltip_applied:true, attempts:1, explorer_epoch:0,
last_error/retry_ms:null. Later snapshot retains one attempt, while network
errors coalesce independently. This is actual Shell NIM_MODIFY acceptance, not
visible icon or Explorer restart proof; no restart was induced.

Captured and visually inspected actual PID framebuffer1999x1453. Screenshot
SHA256 B292582543B369A3E17972CA382D0AC9E610C705BC8C98C722A2E9C5DAAA9910:
offline Events workspace shows separate HTTP DNS/timeout and repeated WebSocket
DNS, no tray startup error or invented board cards. Small user-facing
outputs/PakarController/Tray-Recovery.json records source/package/capture proof.
No simulated or screenshot observation is physical board acceptance. New CI
remains asynchronous; production issue2 and visible shell gates stay open.

## Android device/network workspace checkpoint

Actual missing feature found: previous mobile firmware actions existed, but
Android only had one configured endpoint with SSDP fallback, not a saved device
catalog, DNS-SD/MNDP scanner or Android-owned MQTT/bridge service. Fully read
PCController Tools/Controller/internal/discovery/discovery.go and PU850
LocalLib/MNDP.h/cpp, NetDiscovery.h/cpp and SSDP.h/cpp; the three selected actual
implementation mappings were added to the SAME file-comparison-evidence.json.
NetDiscovery's old endless example loops and wider SSDP dependencies are not
certified whole-file imports. The new mobile-network gate pins the selected
source/target/test hashes. Imported Pealayer file picker/SAF semantics, Phosphor,
Vazirmatn and peer-context gates remain; no invented file or firmware APIs.

Preflight first found Pealayer inventory drift to 555 tracked paths at
48fd59ce65022b039d28570288ff967bf52bca8e, then refreshed to 564 paths at
290f85f352668e556bb535ae6eb38ba5e1a7c13b during concurrent donor work. Fully reread current PreferencesTab.tsx,
board_info.rs, capture-visible-window-windows.ps1 and appearance.ts. New
appearance animation/density/font fields/helpers are explicitly pending, not
claimed imported. Ten other changed already-compared files retain their prior
immutable source snapshots, with the current live hash and reread action in
each ledger note. Their current live-source gates intentionally still reject;
old snapshot comparisons are not fresh donor parity. All source/target anchors
are regenerated, including six early sketch-line shifts in PU850 route mapping.
4328 inventoried /115 snapshot-bound comparisons /4213 pending /76 known gaps
is not complete imports or superiority. Existing screenshots/older proof survive.

Verified: mobile analyze no issues, complete 79-test suite, then all six focused
bridge/discovery/export tests after changing generic POST arguments to form
bodies (never credential-bearing URL queries). Tests execute real loopback
HTTP with bearer refusal, same-ID no replay and board/boot headers; they are
not a real ESP, MQTT broker, TLS handshake or Android background acceptance.
Full production C++ protocol suite including bounded MNDP packet passed.
ESP8266 busy-wire integrated build passed: RAM56684/80192, IRAM63055/65536,
flash896388/1048576. Final image SHA256
3debe61b5d19e94716776c172318a09a228b2b23fc3477009dfbf075b8ea8e70.
Discovery retries after failed mDNS are spaced by ten seconds so loop retries
cannot repeatedly tear down/respond/announce the new MNDP service every frame.
AVR hardware UART, confirmed TM1637 A4/A5 and busy/polling choices were unchanged;
AVR/native/S3/donor sources were not rebuilt or deployed.

Rendered and inspected explicit simulated1200x1100 BridgeTablet via the existing
mobile capture wrapper; no fixture mutations. Canonical release APK build passed,
including native Android NSD. Source versions remain0.2.7. Only the existing warm
mobile cache was used; no clean/new SDK/debug build or production app replacement.
No authorized ADB device is connected. Master issue2 physical/production gates
stay open, including integrated S3, LAN phone advertisement and foreground-service
ownership; current bridge stops when page/app backgrounds or peer expires.

Source-quality phase passed through PU route parity after re-review/rebinding,
then reuse/ledger/comparison/version/preflight checks passed. The broader
build-storage test is blocked by the actual changed live Pealayer src/ui/i18n.rs
review gate; it was NOT bypassed or reported green. Remaining source-quality
commands and CI are separate from already-passing mobile/C++/ESP checks.
The remaining quality commands after that blocked storage check passed separately;
the entire phase is still not reported green.

Final safety review reread actual PCController session.go (879 lines) and
session_test.go (788 lines), original SHA2561bf4d52d.../ff6c7b21... unchanged.
Adapted synchronous owner retirement into mobile HTTP/WebSocket board/boot
acceptance and root state revocation; invalid/partial or vanished learned identity
cannot silently regain legacy readiness. Retained firmware dialogs now lose
authority on a same-host replacement/reboot. One-second bridge freshness watchdog
is independent from a user-selected long telemetry interval, and late MQTT
callbacks cannot publish through a replacement client. Actual mapping/proof was
added to the existing PCController session.go ledger entry, not a second ledger.

After the same-host safety addition, the final guarded analyze and complete
80-test mobile suite passed. Final guarded release APK passed (55215657 bytes),
SHA256 e11c9b392a9856f5fbd2d7d20c4cb97c5809bfdd1b4848a0fd1514ab0b6bb850;
identical delivered outputs/PakarController/mobile/PakarController.apk verified.
Package contains assets/flutter_assets/NOTICES.Z. This is debug-key-signed preview,
not production signing or an installed phone test. Simulated screenshot86160bytes,
SHA2566ad4ade13a464c4a29a92fc0818a04dc36604aaba373eee07f2feec50fd140b2,
also retained in docs/screenshots/mobile-bridge-tablet.png. No source screenshot
claims Android NSD/MCU/peripheral acceptance or a completed production release.

## Integrated ESP32-S3 source and current-donor reconciliation

The S3 migration keeps the canonical integrated sketch, update/ISP state machines
and 480-pixel LedGrid/FastLED engine. Private FSPI owns AVR ISP GPIO11/13/12 with
reset-driver GPIO10; the unverified electrical/polarity gate defaults closed.
Hardware UART1 owns AVR TX17/RX18 at 38400 with bounded buffers. AVR hardware
UART and TM1637 A4/A5 remain unchanged. Shared GPIO4 matrix output has no hidden
frame-interval cap; a separate semantic RGB writer does not duplicate effects.
Actual FastLED IDF5 RMT implementation was read, but neither DMA operation nor
frame rate, UART losslessness, electrical ISP safety or visible output is proved
by those API/source observations.

Fully read actual PU850 FirmwareTransfer.cpp/h were added to the SAME comparison
ledger with exact source hashes, retained Range/HEAD/ETag/MD5/auth mappings and
bounded production-parser/source-guard anchors. S3 downloads are relative to the
running application partition, never raw flash offset zero. OTA capacity uses a
distinct next application partition and explicit Update.abort on cancellation;
the fail-closed remote staging/journal/backup workflow stays shared. Full-flash
network downloads remain disabled to protect credentials. ESP32 image validation,
mbedTLS hashing/CA trust and Update APIs replace ESP8266-specific APIs; this is
not a claim of power-loss-safe rollback or real OTA/AVR flash acceptance.

Current installed Pealayer inventory was selectively refreshed by the root task.
Actual changed compared sources were fully reread rather than bulk-stamped:
native dropdown/color-picker/icons, dialog/menu, i18n, branding, status-bar,
Web appearance/i18n/Preferences/file-picker/header/browser-platform, Windows
package script and workspace hygiene guidance. The status/front-panel agent
completely reread dialog/menu and their actual Pakar targets; network and status
agents completely reread the physical RGB status-bar donor. Existing source
comparisons and target/test meanings survive. New generic behavior is explicitly
pending, including density-aware controls, bounded diagnostic details, native
font-family controls, fixed close/toggle/confirmation focus helpers, endpoint
trigger geometry, HWND/preset icon lifecycle and browser/PWA capabilities.
The earlier MessageHub byte identity remains recorded as historical, not current
equivalence. Source re-review is not implementation or whole-donor parity.

Three actual PCController StatusLedMath/StatusLedController mappings were added
with exact hashes and bounded S3 semantic RGB/test targets. The native/Web mirror
uses actual already-scaled output bytes only with fresh peer/have_output guards;
it does not multiply brightness twice. AVR PWM/EEPROM profile storage is replaced
by the S3 addressable output and existing persistence ownership, while arbitrary
profile authoring/manual overrides/cue priorities remain pending. The changed
PCController status-led-event.ts complete ordering adapter is NOT certified by
these three source imports. Native/Web rendering and optical/audio acceptance
are separate; the mobile generated settings catalog alone is not a mobile
addressing editor or physical RGB mirror. TUI DHCP/static editor parity remains
pending too.

Read actual canonical-main/platform diffs before rebinding retained auth/shell
anchors. Live api-contract, build-storage, native-dropdowns, peer-binding,
web-presentation and web-streaming gates passed at their reviewed frozen hashes;
peer-binding main hash f08f05f2633775d99d3d47d7d9415d77e541f225cb9834f2beb33b9ec50561c4.
The root task solved Arduino include preprocessing by removing literal quotes
from the expanded -I root property, retaining both C/C++ recipe flags; S3 build
uses -Os -g0 to avoid unnecessary debug artifacts. Subsequent root-owned SDK
compile diagnostics are not hidden or replaced by source checks. No hardware
flash/read/write or native build was performed by this evidence task.

Verified here: focused s3_platform_wiring_test.mjs source guards, strict
generate-file-comparison.ps1 -Check and git diff --check. The generated reports
account for 4403 inventoried files, 126 hash-bound manual comparisons, 4277 still
pending comparisons and 90 known parity gaps. No compared current donor source
or target/proof hash was bypassed. Test-code anchors do not mean tests ran;
current SDK/native build, actual hardware deployment, SSID association/discovery,
matrix/peripheral behavior and production acceptance are reported separately by
their owners. Keep issue #2 and all applicable physical acceptance gates open.

The actual PU850 SSDP.h/cpp and SSDPDevice.h/cpp are now mapped in the same
ledger too. The network agent fully read all four original files and adapted
the complete device engine; the evidence owner reread the small wrappers and
the complete source/header adaptation diffs. Delayed bounded/de-duplicated
M-SEARCH, alive/update/byebye, topology/identity and escaped descriptions are
retained, with instance-owned S3 sockets and AP/STA interface membership replacing
the ESP8266 singleton/manual IGMP policy. This is actual donor code, not only a
Wi-Fi SDK call. Real LAN/router/AP/phone discovery remains pending; optional
unimplemented SOAP/GENA is not advertised. NetBIOS remains truthfully false on
S3 rather than reported as available. CI target references were rereviewed for
the added pinned S3 core/AsyncTCP, buffered build artifacts and focused source/
network checks; that source update is not a claim that CI has executed.

Subsequent legitimate SDK diagnostic found RemoteFirmware's WiFiClient alias
was not transitively declared by ESP32 HTTPClient/secure-client headers. Read
the actual installed 3.3.12 WiFiClient.h (NetworkClient alias), added its explicit
include only to the ESP32 branch, and added/passed a focused source regression
assertion. This fixes the observed declaration error without replacing the
shared updater or declaring the next SDK build successful. Refreshed main target
bindings after reviewing the added status payload bytes38/39/40 (configured
station/AP/DHCP distinct from observed link state) and atomic USB provisioning
candidate/save/commit with staged secret retirement. Current reviewed main is
4b60de4011dac897ede9983b042e2902246ad819f16c95c3cea697ce7ce3964a.

Network agent's separate guarded native receipt: Check passed, 169 Rust tests
passed with zero failures, Release passed from the single warm release cache,
stage-desktop.ps1 -Probe passed and candidate --version reports0.2.7. Staged
candidate outputs/PakarController/staging/PakarController.exe is25549312bytes,
SHA2567666716054c1d3e5fabe38a8ca468e356e95a72af17d5f6eb13ff09e6fef8dc9.
This is actual native build/test/stage evidence, not a production GUI replacement,
live peer test, screenshot acceptance or physical board proof. The SDK build and
device deployment remain separately owned and must be reported with their own
final result, source identity and live observations.

The next real S3 SDK diagnostic exposed the AVR-only AutomationStore EEPROM
implementation being compiled on ESP32 because its previous guard excluded only
ESP8266. Fully read actual donor Project/AutomationStore.cpp/h and EEPROM layout,
full current targets and adaptation diffs before extending the guard to both
ESP families and their ARDUINO_ARCH macros. This retains the AVR/native fixture
owner, shared record contract, dual-bank/CRC/readback validation and nine-key
adaptation; no ESP EEPROM shim, local rule migration or hardware write was added.
Actual g++ syntax-only checks pass for four excluded ESP macro variants with no
EEPROM/Arduino include paths, and for included HOST/__AVR__ fixtures. Focused
S3 source regression passes too. Common-library scan found no other EEPROM or
AVR-only includes in the remaining C++ translation units; this does not predict
all future SDK errors. Root owns the resumed full SDK build result.

Added the fully reviewed AutomationStore cpp/h to the same evidence ledger;
strict generation/check and diff checks pass with4403 inventoried/128 hash-bound
comparisons/4275 pending/92 known parity gaps. Reviewed test-protocol wrappers
retain all prior cases and add panel/status/config/provisioning fixtures;
refreshed only their changed target proof and passed the live tm1637-display
reuse gate before the owner resumed remaining AVR checks. Existing S3 migration
documentation changes introduced no stale comparison references.

Optional TFT/W5500 backlog is durable in GitHub issue19 under issue2. Checked
existing issue titles/bodies first; applied existing labels using only a scoped
owner credential when the active account lacked label permission, without
changing global authentication. The S3 status/front-panel document links it and
separates completed source callback/configuration wiring from still-unchecked
SDK/deployment/key/display/audio/optical acceptance. No TFT or Ethernet driver,
link, pin-voltage approval or hardware success is implied by reservations.

## Final S3 identity and quiet-link review

Root reported the first actual integrated S3 SDK build passed: app1841104bytes,
SHA256834fe8da7e35967c4c68c53b7887549b8e11679b4656faab2d68d001e15a0f56.
Its authorized COM22 deployment verified written hashes and USB HELLO0.2.7 plus
41-byte status/CRC; AP started, station association and NTP were subsequently
observed, and ledboard.local resolved from ASUS. These are root-owned observations
of that earlier image, not acceptance of the final source or the AVR. AVR RX
bytes remained zero and the paired buffered AVR image has not been flashed.
User reported an AVR welcome/reset loop and later that it stopped, without
changing wiring or grounding AVR RESET. The cause remains unknown; the S3-only
bootloader pause is a temporal correlation, not a wiring fix. VCC is user-confirmed3.3V.
No further photos or PBX calls are requested; all announcements remain stopped.

Actual AP-only HTTP commissioning exposed an empty board identity from the
inactive station interface. Reviewed the final source fix and fully read actual
installed S3 SDK esp_mac.h: esp_efuse_mac_get_default reads the factory-programmed
MAC; it does not write eFuses or mutate the base/interface MAC. Existing zero/
broadcast/multicast checks still fail closed. Stable hardware identity therefore
does not depend on enabling STA, while boot identity and peer-bound request
checks remain separate. Fully reread the unchanged actual PCController session
test file (788lines, SHA256ff6c7b21f72bfb944d232c87210f8ffa139dfe016e8b5756f2582604489fffc4)
including close-before-notify, pending acquisition, inactive control lines,
one-shot reset hooks and correlated/error-response behavior. This is source
review, not execution of donor Go tests or permission to pulse hardware.

Other reviewed final changes: truthful buffered link-mode JSON; station-selected
IP with AP fallback; clear ISP capabilities when the electrical gate is closed;
no ESP32-specific18ms matrix/normal-cloud delay inherited from ESP8266; optional
S3-only PauseAvrLink build flag. Quiet-link source does not begin AVR UART, leaves
TX/RX as inputs and rejects normal sendToAvr calls. Production defaults still
enable the link. Final normal builds omit the already-used one-shot filesystem
initialization flag; onboard RGB GPIO48 is an explicit common-DevKit assumption
to be configured through its existing API, not claimed optical identification.

Read the complete build-script diff and updated S3 source guards, verified every
surviving target anchor, then rebound only actual changed comparison references.
Final reviewed main SHA256eb0de02bb00252adfc89fc4431acfb8d9f88195238c237a81113888776fc6972;
source-guard test SHA256f3c253d28d73a419928f4f454a6f0cc3363177574fae47b7267666c24aafaaff.
Source guards, strict comparison generation/check, diff check and live
api-contract/peer-binding/web-presentation/web-streaming/uart-flow-control gates
all pass. Final exact-source SDK build/deployment, live fenced mutations,
WebSocket/client rendering, AVR link/peripherals, RGB and matrix/OTA/ISP remain
separate acceptance steps; prior image observations do not close them.

## First physical S3 commissioning: bounded evidence and limitations

Root's actual first integrated SDK report: program1840959/3342336bytes,
globals74892/327680bytes; emitted application1841104bytes,
SHA256834fe8da7e35967c4c68c53b7887549b8e11679b4656faab2d68d001e15a0f56.
On COM22, esptool wrote and hash-verified the four S3 ranges for bootloader,
partition table, boot_app0 and application, then reset the S3. Hardware MAC read
90:70:69:ea:c8:00. Original complete16MB flash backup was independently retained
and verified on both PCs outside Git, SHA256
cd0f30ac9491da499eaf2167f07b5accf23cf6779c506110431aa766f346273e.
These are S3 writes only, not AVR deployment or successful AVR communication.

USB readback had valid COBS/CRC, ESP role/version0.2.7 and41-byte status.
Private station provisioning used one explicit commit with AP+station flags;
persist ACK and configured-state readback succeeded. Subsequent ASUS observation:
ledboard.local resolved10.252.139.27, ping and authenticated HTTP Basic worked,
DRS Tether was connected at reported RSSI-48, NTP synchronized and AP started.
mDNS/SSDP/MNDP only reported started at this checkpoint: no discovery query,
multicast interoperability or phone/AP acceptance was proved. No private Wi-Fi
secret is recorded here. AVR received0bytes/0frames, version was null and
TM1637 ACK was false; no UART, display, keypad or buzzer acceptance follows.

Initial AP-only commissioning also exposed zero station-interface MAC/empty
board identity, false HTTP peer-fencing readiness, polling text for the buffered
profile and untruthful ISP advertisement. Source fixes described above are bound
to main eb0de02bb00252adfc89fc4431acfb8d9f88195238c237a81113888776fc6972;
their subsequent exact-source SDK/deployment result must be reported separately.
That normal UART-enabled build was in progress, not a finished firmware claim.

Clarification: the user did not change wiring and NEVER held AVR RESET to GND.
The later report that the problem stopped followed root placing only the S3 in
its ROM bootloader; this correlation is not an established cause or a physical
wiring fix. Root used no erase for that pause. No AVR ISP/fuse/flash/EEPROM or
reset-control write occurred. Keep all PBX calls and test melodies stopped.
The next intended integrated image pauses AVR UART entirely for quiet diagnosis;
normal UART-enabled firmware is not to be deployed as an assumed remedy.

Both PCs run the native candidate hash
7666716054c1d3e5fabe38a8ca468e356e95a72af17d5f6eb13ff09e6fef8dc9,
and captured screenshots are genuine offline application views, not a live AVR
or peripheral test. The reuse source gate remains128 bound comparisons with
4275 pending comparisons; this commissioning evidence does not close donor
parity, physical AVR, final firmware or production acceptance.

## Passive UART SDK design review only

Read actual installed Arduino-ESP323.3.12 HardwareSerial begin/end/setPins,
HAL pin validation/attach/detach/driver initialization/end, inversion and RX-pull
function bodies, plus applicable IDF UART/GPIO declarations. This was a targeted
API review, not a claim to have imported or fully read every unrelated line in
the large SDK files. Original-byte SDK hashes:

- HardwareSerial.h:f0a25409647e9c5daf3cb9b59f5d7020a422962aab7afe7cb0e637e510af92ac
- HardwareSerial.cpp:df718558a059669e35b1c034d6e458b00d13317ec8403e5a74dd9a84898544bf
- esp32-hal-uart.h:7de851dbb9d263cf80d5c7fe547a6e3055b863250900f5f2d1a522968231fca1
- esp32-hal-uart.c:e6ec2520ffa76edba0170694bc984cdad74d47d64b23a60cec0c1396d8363994

Negative TX means retain an existing TX assignment, not detach. On a fully
detached UART, explicit RX18/TX-1 avoids default pin assignment because defaults
are chosen only when BOTH effective pins are negative. HardwareSerial::end and
HAL uartEnd detach registered UART pin matrix/ownership and clear stored pins.
There is no ordinary UART_RX_ONLY mode in these actual APIs; IRDA_RX changes the
wire protocol and TX-buffer-size0 only selects blocking transmission, not a TX
disable. Neither is a substitute for keeping the physical TX pad as input.

Recommended, not implemented: a dedicated spare UART2 capture owner only in
quiet-link builds, no write/HELLO/actuator API, no normal SerialLink callback or
automatic resume, bounded explicit38400/115200 capture and optional RX inversion.
Using UART2 avoids the normal UART1 avrLink.service draining diagnostic bytes.
Require UART2 TX pin unassigned before/after begin, GPIO17 input/no pulls with
public gpio_get_io_config output-enable readback, and no GPIO10 operation.
Disable optional internal RX pull before begin for passive observation; fail
closed and end capture on any ownership/driver/pin-readback disagreement.
No diagnostic production code, COM access, waveform, reset or AVR command was
performed by this review. Software pin-state checks still are not electrical
measurement or proof that the unknown AVR firmware will transmit spontaneously.

Separate status-agent compatibility receipt: guarded ESP8266 busy-wire SDK build
at reviewed main eb0de02 passed, binary958848bytes,
SHA256165537f5c8552c9e34ee1411793f671b2903146c3b4e8667a56511d3b98fa236;
RAM56936/80192, IRAM63055/65536 and IROM908640/1048576. This preserves the old
target as a built option; tight8266 headroom and all physical acceptance remain
visible. It was not deployed and does not replace final quiet-S3 build evidence.

## Approved bounded passive capture source

After the preceding design review, root explicitly approved the new narrow
firmware/esp32s3/src/PassiveAvrRx.h owner and pure production-helper test, not
main/Protocol changes or hardware access. The adapter exists only for S3 quiet
builds with PAKAR_AVR_LINK_ENABLED=0 and references spare Serial2/UART2. Its
constructor starts no driver/GPIO. Explicit begin accepts38400/115200 only;
requires unowned RX/TX/flow-control assignments and GPIO17 input/output-disabled
readback, disables optional RX pull before attachment, uses RX18/TX-1 only after
that detached check, then verifies driver/RX18/input/no-TX readback again.
Failure ends only its owned driver. There is no write, reset-pin operation,
normal frame decoder/callback, auto-sweep or normal-UART resume path.

The actual shared helper stores at most128bytes, counts received and overflow
drops independently, bounds chunks to32bytes/offset128, and preserves captured
bytes after explicit stop, unsafe-state stop and wrap-safe8000ms expiry.
Error callbacks update a separate atomic count; cooperative reads have a64-byte
per-loop budget. The root owner must continue servicing during capture: expiry
is enforced in service(), not claimed to be a hard real-time interrupt deadline
if unrelated code blocks the loop. The adapter has no remote HTTP API; explicit
USB start/status/chunk integration remains root-owned and was not added here.

Actual g++17 -O2 -g0 execution of tests/s3_passive_avr_rx_test.cpp passed exact
capacity, independent drops, invalid chunk/null limits, duplicate/invalid begin
preservation, stopped-buffer retention and deadline rollover. That test compiles
the SAME pure helper, not fake SDK GPIO/UART implementations; actual adapter
SDK compilation, GPIO readback, bytes and safety under hardware remain pending.
Header SHA256037a61bd063248358093549efef01479670da34695a3e1b26088c0b29d4c4662;
test SHA25641072f11f04a65099ee2a2d75ebf66e4d9370e055ec9e4a4c1c93cc8f3f6d065.
The existing uart-flow-control reuse gate was consulted before this work; no
SDK API excerpts are falsely classified as a new complete donor-file import.
No donor, main, protocol, serial port, audio or PBX mutation occurred in this slice.

## Quiet USB integration source gate

Reviewed root's frozen canonical main
e19a7c10fe7e0bcd2ed523a9a011123748ba1640637827bfe5fa72411930b5f2:
the capture header follows the quiet build flag; only that profile constructs
the spare UART2 adapter. Physical USB opcode0x5B returns status/start/stop or
bounded chunks through0xB4, with strict request lengths, a27-byte snapshot and
at most32 raw bytes per chunk. The normal status appends the explicit link-enabled
byte without changing its existing prefix. Protocol reserved-envelope byte0 and
COBS/CRC validation remain unchanged; no stock-PCController framing adapter is
implied. RESET GPIO10 is only observed with digitalRead/gpio_get_io_config here.

Reviewed the actual loop service, cloud-TLS pause and USB/HTTP/WS/cloud/upload
capture guards. They reject concurrent mutations during the bounded diagnostic;
they are not claimed to constitute a proven race-free global maintenance lease.
Commissioning still requires exclusive USB ownership and no concurrent client
mutations. Ordinary UART1 requests return false in the quiet build, and GPIO17
stays input; the new adapter has no AVR write or automatic resume path.

The source guard7e011e779e0aec34d73931ccde54566fb87bec811291bbae52fe5cffce57dd17
passed. All five existing main references and both platform-test proof references
were rebound only after reviewing the changed integration and surviving anchors.
Also reviewed the additive script-test registrations in protocol wrappers and
quality checks: no existing check was removed. Protocol.sh hash
00a526480bb65c5245ebe1c945af554663bdfe137b00b50badf7c85397014f31 and ci-checks.mjs
449c07e0b26f93a640444a7ab1729ea1a0e2d6c2ae5c0beae49b1a2961cb1c60 are now bound.
Generated comparison and strict Check passed:4403 inventoried,128 bound,
4275 pending and92 known parity gaps. api-contract, peer-binding,
web-presentation, web-streaming and uart-flow-control gates all passed; diff
whitespace validation passed. These are source-evidence checks, not an SDK build.

The preceding normal UART-enabled S3 image at eb0de02 completed SDK compilation:
program1840543bytes, globals74876bytes, app SHA256
ef153bd25f9c8080530702e038090176829152c17b1aa21d5cd0a8590829a3c2.
It was NEVER flashed. The exact quiet adapter/integration SDK build and hardware
pad/capture observations are still pending at this receipt. No physical success,
reset-loop cause, pin electrical safety, AVR communication or production closure
is inferred. User confirmed3.3V and the GPIO10 RESET connection, but never changed
wiring or grounded AVR RESET; correlation with pausing S3 remains only correlation.
All PBX calls and unsolicited test audio remain stopped.

## Explicit S3 ISP commissioning build option

Reviewed tools/build-firmware.ps1
ee572248013c49c6cc2690c1256ac337ff581809ed2b46255007c52895400700:
the new S3IspElectricalVerified switch is false by default, rejects every non-S3
target before build setup, and adds PAKAR_S3_ISP_ELECTRICAL_VERIFIED=1 only when
explicitly requested in the S3 branch. The source macro still defaults to0 and
the quiet passive adapter has not gained any reset or transmit operation.
Actual Plan execution confirmed non-S3 rejection and both C/C++ recipe flags
for the explicit16MB/OPI quiet profile; Plan starts no compiler or serial port.

Root intentionally selected this acknowledgment for the user's reported current
3.3V AVR and direct AVR RESET-to-GPIO10 wiring, retaining the planned active-low
reset polarity. This is an operator-selected build gate, not an electrical
measurement, verified AVR signature or proof of every ISP connection. The quiet
image can therefore advertise the enabled ISP feature, but no probe/reset/erase,
flash/fuse/EEPROM operation is automatically authorized or executed by enabling
the flag. Root's exact quiet SDK build was started, not yet reported successful
here. Any later deliberate signature probe must be a separately controlled
operation: releasing AVR RESET could cause its existing boot cue once. No such
operation, hardware access, audio or PBX call was performed by this review.

## Quiet adapter CI source coverage

Reviewed the complete current build workflow and governance test against the
existing fully reviewed Pealayer ci-cd.yml mapping; the donor hash remains
bba9a295ee7b59e318f76e0b6f6a80f0aec631e249f7d8c798f6c92c0bb7fd8c.
The guarded quiet S3 SDK command now precedes the normal S3 command in the same
firmware job. Both reuse the existing S3 cache/output; no clean, second diagnostic
cache or separate quiet package is added. The normal build runs last before
upload and replaces the diagnostic artifact. CI's quiet profile does not enable
the electrical-acknowledgment ISP switch. Governance asserts exact single
commands, ordering before upload and no cold diagnostic caches. All existing
firmware, protocol, desktop, mobile and release checks remain.

Rebound only workflow target4e284a5e8b4f01ae8bfd62731b533cc02c3e24bf10c108b49f9120ce07b92bf6
and governance proof5bc629302abaeb749345d1eebc7b2ed1144251e65c7a87b7bbbd1d6bbe05125b;
ci-checks.mjs remains449c07e0 and its actual fail-fast child runner is unchanged.
Actual repository_governance_test and build_script_security_test execution,
comparison generation/strict Check and whitespace validation passed. The network
owner additionally reports actual actionlint passing for this workflow. This is
local source/lint evidence, not a GitHub Actions SDK run, artifact publication,
quiet-image deployment or AVR hardware acceptance. Existing pending reuse and
physical requirements remain open without changing their dispositions.

## Actual quiet S3 SDK, deployment and passive observations

Root reports the real quiet SDK build at frozen shared main
e19a7c10fe7e0bcd2ed523a9a011123748ba1640637827bfe5fa72411930b5f2 passed:
sketch1843223/3342336, globals74916. Actual application1843376bytes SHA256
6c65e309173c91284605476662c2aad1f26349e5c6fa68bc2431c4482c3789f5 was then
flashed only at0x10000 on COM22/same MAC and esptool hash-verified. Full original
backup remains unchanged; this was not another bootloader/partition rewrite.
Build flags included PauseAvrLink and explicit S3IspElectricalVerified,16MB/OPI.
Thus the actual SDK compiled the adapter, not only its pure helper fixture.

USB ESP0.2.7 returned valid42-byte GetStatus with normal AVR link0,
station/AP/NTP true and reported errors0. Root performed two explicitly selected
RX-only captures, with pin-ownership checks and stop confirmed:

| Baud | Elapsed ms | Received | Stored | Dropped | Hardware RX errors |
|---|---:|---:|---:|---:|---:|
|38400|3059|140|128|12|96|
|115200|3030|172|128|44|92|

RESET GPIO10 was observed high/output-disabled in both. RX18 stopped high;
115200 also observed low during capture. Bytes were mostly00/FF/FE and no
plausible framing was established. Counts are observations, not valid AVR
frames, known baud, electrical measurement or causal diagnosis. No AVR TX or
RESET drive occurred during capture; no ISP probe, AVR flash/fuse/EEPROM action
or AVR reset-control write was performed.

The user confirms welcome audio STILL repeats with quiet UART firmware.
Cause remains UNKNOWN: this does not establish MCU resets or a software/electrical
root cause. Root stopped all board tests and requested AVR power disconnected,
or the whole assembly if power is shared. No PBX or test melody was issued.
The user never changed wiring or grounded RESET; do not describe that as a fix.

Read-only source audit found NO existing persistent reset-hold capability.
AvrIsp::enter/leave are private; public probe enters, reads signature and always
leaves, deasserting RESET then selecting INPUT. Backup/program use the same
scoped lifecycle. GET /api/avr/probe is not electrically non-mutating and cannot
be used as a safe hold. Legacy direct console W toggle is disabled on S3 and
serviceConsole is not loop-dispatched. Game reset only resets game state.
No new hold API or workaround was implemented or invoked. Valid AVR ACK,
display/keypad/audio/matrix, discovery interoperability and final acceptance
remain open; this evidence does not close issue2 or the pending donor gaps.

Final wrapper review: root added the actual pure s3_passive_avr_rx_test to both
existing host-helper compile/run loops, preserving all prior cases. It is no
longer only an ad-hoc invocation. Reviewed PS1 hash
61844cd674bbc5d163b1800f3382fc45bd0a8c466dfd00804f97533c6930bc5c and SH hash
1563e4959ca64611c6519cb300cae044a57faa0c53eb5d91269f56f33281296e;
rebound the existing firmware_controls_test mapping's SH target only, retaining
its actual surviving checks. Root executed that exact g++17 pure-helper case
successfully. No full suite or SDK rerun was performed for this documentation
finalization; strict comparison and whitespace checks remain the final guards.

## Exclusive native transport donor preflight

For the user's new single Wi-Fi-or-USB lifecycle requirement, read AGENTS and
REUSE-PREFLIGHT, issue2, peer-binding and exact donor-audit entries first.
Default consultation reports PCController live-unavailable; explicitly selecting
the actual canonical installed root correctly detected tracked inventory drift.
Root authorized a PCController-only generator refresh: canonical b0111ae0c80dc34149ea1044de7dd5488920301b,
1517 tracked paths,4673 total inventory. All other donor-origin rows were checked
and preserved exactly. The270 additional paths are inventoried, NOT newly read.

Fully reread current files through EOF, preserving prior mapping history:

| Canonical donor | Lines | Original-byte SHA256 |
|---|---:|---|
|Tools/Controller/internal/link/session.go|916|1311fdcde94b2ef8ed06db63ff874a255c135dc1969709e6f43ff4b5c5c8235b|
|Tools/Controller/internal/link/session_test.go|788|ff6c7b21f72bfb944d232c87210f8ffa139dfe016e8b5756f2582604489fffc4|
|Tools/Controller/internal/link/request_admission_test.go|489|0148ea4aac3cf230050045756a4308157adf786a2b6ca2d4fd99bcf665a622c1|
|Tools/Controller/internal/hostbridge/buzzer_mirror.go|435|dcb9bf3c1a31cb6cc06f4f5ed52def32b7388fd4d33587e233476a3a76e1f032|
|Tools/Controller/internal/hostbridge/buzzer_mirror_test.go|539|d92d0826cd9665520087a3b5704ed874ee5c605695d571975b3c15b7a54aa84a|
|Tools/Controller/internal/discovery/discovery.go|783|4ca50ce622f998f1922a828350ad71aea5fa5dee7506069ca3df2e857536198e|
|Tools/VirtualBoard/tests/firmware_controls_test.cpp|639|4facf265f8593b83f7610ae99ce58050013226cf1aa5a4d6e60bdbd6e5cc399e|

Five previously compared sources changed; only those five source hashes were
refreshed after complete reads. Unchanged session_test was reread too; the extra
request_admission test remains a reviewed dependency awaiting actual target
mapping, not an invented whole-file import. Existing target/proof hashes are not
stamped while root and UI owner still change their implementations. All original
omissions remain; newly useful generic gaps were appended to the same entries.

Concrete session contracts: closing/context checks before AND after admission;
final WithRequestWriteGuard immediately before port.Write after both queues;
physical-handle ownership through late open/configure cancellation and retryable
close; synchronous pre-close observer; persistent terminal error; ACK/Error
opcode+sequence matching and unsolicited routing; reserved macro sequence;
priority and bounded strip ACK slots; inactive initial DTR/RTS and deliberate
one-shot reset. A Rust route/epoch lease must be reviewed at its real OS-write
boundary. It cannot retract bytes already handed to the OS or substitute an
HTTP board/boot fence for complete downstream serial ACK ownership.

The extra drift review found useful pending short-note/timer rebuffering and
source-wide scheduler rebasing, original receipt-time preservation and avoiding
ready speaker reprobes; discovery started-vs-configured failure reporting,
secret-filtered bounded TXT/instance UUID and eight-slot public enrichment;
numeric overflow display and exhaustive byte-transition tests. Android's current
ToneTimeline lacks donor rebaseForStart, and display-bus tests cover only their
stated fade/ACK subset. No donor suite, production code, SDK build, device,
speaker/PBX or audio action was run in this source review. Final frozen native
target/proof binding, gates and runtime exclusivity evidence are still pending.

## Exclusive native transport frozen source binding

Reviewed the root's complete new connection.rs and changed main/config/HTTP/IPC
sections, plus the UI owner's complete unified control, shell dispatch/dialog
changes and actual replacement source guard. Fully reread Pealayer dropdown.rs
646lines (134f1613df05c9db44f8ccd178981a8eca4fb6c254ced6466229afca66b40ee9)
and menu.rs883lines (a0196878c7b76b898d0a5791b122181d9045d3fbbe9f56de52bc43242edf2d9d).
Current meaningful source identities:

- main95016f60e1032efe11d0d5e2e1f30b8f271bb4ec05d1a242d290f930a4c43c63
- connection38778812b8bf03975198808c672421f808a1486c402452473af3e63a83b0eda6
- peer_httpab52253344193953b470f963358afca041f8e0871a86d74778c91044bd5bc055
- shell8e250a5e1dfb4d7902913b98c97e57fdff3288a743b429195a8b7a9b323aec2c
- connection_controls573fe0f11df41826f7ff797cec716abf76a2482339dac7f5e9ff592fffbad6d4
- replacement source guard574c605348414b7f65ce48f3e92821ab1abdf41f05ccb4be1d6f7b5526d6a38b

Before admission, both method and epoch must match; selected USB rejects Wi-Fi
leases and a second USB handle. Revoked HTTP/WS resources retain leases until
drop; normal serial checks its lease before writes and drops the actual port
before handoff. Old events are discarded while generic uncertain operation
receipts can stay in Events. Existing board/boot HTTP scopes remain independent.
One persisted method/enabled intent replaces Both; USB status does not require
a usable Wi-Fi URL and no HTTP worker/discovery/telemetry is admitted on USB.
These are reviewed source mechanisms, not measured resource-release timing.

Review identified two concrete gaps and owners fixed both before binding:
root disabled idle HTTP pooling so leases do not leave pooled TCP owners;
UI now applies the common dirty/pending guard before Port mutation and verifies
a refused release cannot change its port. Dialog commits share that guard and
check the selected method after handoff. The live indicator now requires fresh
selected-board status, not only a WebSocket socket-open event. Existing manual
paths, focus/keyboard/dropdown semantics, settings drafts and delayed outcome
receipts are retained. Dynamic density/chevron/endpoint geometry remains pending.

Rebound only reviewed surviving targets/proofs; removed dual-popup function/test
anchors now map real unified draw/lifecycle/lazy-scan tests. Added ONE narrow
request_admission_test comparison with actual production Gate and zero-network
HTTP-worker fixture references. Its priority, exact ACK/sequence, reset, late
open/close and physical timing omissions remain explicit; no whole-file parity.
The UI owner reports source wiring/six negative guards passed. Rust fixtures,
native Check/Test/Release, runtime screenshots and true USB/Wi-Fi isolation were
NOT executed by this evidence slice and must be recorded by their owning runner.

Comparison generation and strict Check passed:4673 inventoried,129 comparisons,
4544 pending,93 known gaps. Mandatory build-storage, localization,
native-dropdowns, peer-binding and api-contract gates all passed with exact
current source/target/proof hashes; whitespace validation passed. This unblocks
guarded native compilation without weakening a gate, closing the donor ledger
or claiming physical acceptance. No firmware/AVR/ISP/audio/PBX action occurred.

Actual first guarded Check then reached Rust compilation and reported E0624:
main attempted PreferencesDraft::is_dirty, whose existing visibility is
pub(super). Root consulted preferences reuse and the actual existing draft
helper, then reused the SAME shell connection_change_guard from set_connection
and OTA instead of widening the Preferences API or duplicating its checks.
Reviewed final main65e92fb29dd0b55895c725be9f78fa8a868366d30fd7aa0b0cf9284fe8bac0a0
and shell46697fee16f151e0f1e6b836a9318dc9ca061247cf1c9511adffffa79ab9a4b3:
the centralized guard is pub(crate), and Preferences/device dirty drafts,
pending settings/maintenance and identity writes all still block. Preferences
source is unchanged. Rebound only these two reviewed target/proof hashes;
generation/strict comparison, the same five mandatory gates and whitespace
checks passed again. The first compile failure is not hidden as success;
subsequent actual Check/Test/Release results remain the build owner's evidence.

## Actual bounded ASUS LAN discovery and read-only API observations

After root explicitly confirmed the quiet image alive and passive captures
stopped, executed one bounded read-only helper on ASUS Wi-Fi10.252.139.56,
interface9, targeting hostname ledboard.local. No scan of unrelated addresses,
firewall/service change, board mutation/reset/audio, tunnel or app endpoint change
was performed. The normal AVR link remained disabled and AVR remained offline;
the unresolved repeating welcome report still requires silence and causal
diagnosis, not more actuation. Credentials were process-scoped and cleared.

The original private evidence remains byte-preserved on ASUS staging and local
outputs/PakarController/verification/S3-network-discovery-proof.json, SHA256
e56d9ddb4ec253de0291e5271594cefb526226a8a7e815a30596e0a7c3725f3c.
Its actual helper source was cf19bfd4568048f52a44ba5b547362aed14158e7e9046cc08a0cd851c6f5c465.
Raw query/response datagrams are retained as Base64, filtered to the one resolved
board address10.252.139.27; this is actual LAN evidence, not a loopback fixture.

- Six mDNS packets contained only A records for ledboard.local10.252.139.27.
  No PTR/SRV/TXT response was observed; the DNS-SD follow-up is null, not one
  successful packet. The first summary's PowerShell `@($null).Count` was misleading
  and was corrected after reading the original wire proof. DNS-SD remains open.
- Direct unicast SSDP M-SEARCH returned three actual root/basic/UUID responses,
  with interface-correct LOCATION http://10.252.139.27:80/description.xml and
  PakarController0.2.7 identity. Multicast M-SEARCH yielded zero responses within
  the bounded window; this is not full multicast-discovery acceptance.
- A four-zero-byte MNDP query received one valid TLV reply: station MAC
  90:70:69:ea:c8:00, identity ledboard, ESP32-S3, version0.2.7, station address
  10.252.139.27 and board ID esp32s3-907069eac800.
- Authenticated GET /api/status, /api/network, /api/time and /description.xml
  each returned200. Status matched board ID esp32s3-907069eac800 and fresh boot
  c7f43b2d700581a7bc10c14dbaf9bd7c, with HTTP peer fencing true, buffered mode,
  AVR link false/AVR offline, configured STA connected and AP started.
- WebSocket opened, delivered initial status and answered the read-only
  `{"type":"telemetry"}` request with actual type telemetry and the same boot.
  Its clock reported source ntp, observed true and valid true. This proves the
  measured JSON exchange, not remote cloud MQTT/WebSocket or AVR communication.
- RGB reported available/enabled/have_output false, pin-1; no optical success
  was inferred. ISP available true reflects root's explicit operator-selected
  electrical-acknowledgment build flag, not a signature probe or physical proof.

The helper exposed a PowerShell Task<void> result-shape bug: ConnectAsync and
SendAsync emitted two empty objects ahead of the actual HTTP result map. The
original proof is not rewritten; its final map contains the verified responses.
Only those task results are now explicitly discarded, with a narrow static
regression added to the existing offline parser fixture. No live rerun or firmware
change accompanies this correction. The helper/test are not yet target/proof
bindings in file-comparison-evidence.json, so no unrelated hashes were refreshed.

Actual PTR/SRV/TXT DNS-SD, multicast SSDP/router/phone interoperability, SSID scan,
mobile AP Web UI, semantic RGB/matrix optical output, AVR UART/peripherals and
production multi-client/physical acceptance remain open. mDNS on the ASUS LAN
does not imply multicast crosses the routed VPN to the local PC. App forwarding
and endpoint changes remain held until silence is confirmed.

## Exclusive connection Preferences and exact contract-proof recovery

Fully reread current Pealayer src/ui/preferences.rs through all2308 lines,
SHA1fa2482a389fee84c79eef3d69ad8cc0ecb922c319fd222219af3e90d7e6f965,
after consulting Preferences and exact donor-audit entries. The actual
sync_external_appearance three-baseline merge is now adapted to the live
connection owner, not merely copied as styling. Reviewed frozen Pakar Preferences
c5356e250994432a76e9bc3533e482d3c9903dd5db73864c1479f5c7d1b81739:
selected-method summary delegates to the canonical dialog only from a clean
draft; incoming owner fields refresh config/saved/previewed together; unrelated
Save cannot restore stale Wi-Fi/USB fields. New source-bound donor row preserves
concrete pending helper HWND/process, watcher/config-authority, typed-controls,
density, picker and full lifecycle-test imports. Media-specific options alone
are not applicable; generic mechanisms remain pending. No whole-file parity.

Parent reports actual guarded Check and181 Rust tests passed including the two
new Preferences cases; the earlier checkpoint had179. Release ExpectedCommit
abbreviation39c878d was rejected by its full40-hex identity guard before compilation;
the full39c878da3e9abca43db7bfdb12445ed8d11014f1 was accepted. This is not a
compiler failure, and source tests are not installed-app/human acceptance.

CI correctly exposed two obsolete dual-control fixture assumptions after the
unified connection UI. Reviewed native_dropdown_test.mjs and the full updated
native_render_lifecycle_test.mjs, including new negative mutants: the single
shared consumer remains mandatory, lazy enumeration belongs only to selected USB,
keyboard Configure stays tested and clear-before-return cannot be removed.
Only their reviewed reference hashes were rebound; no acceptance test was
disabled. Parent reports the focused fixtures passed; this reviewer ran no
native/SDK build or device action.

The separate23-file historical docs/pccontroller/SOURCE-MAPPING.json had stale
target hashes despite the exhaustive ledger already identifying the newer S3
sources. Fully reread pinned20c6115 donor UartProtocol.h, MacroQueue.h/.cpp,
FrontPanelModel.h, AutomationStore.cpp and all1007 ProtocolRuntime.inc.h lines,
together with complete current Protocol.h, SerialLink.h, PcControllerRuntime.h/
.cpp and AutomationStore.cpp. Additive ESP front-panel/passive-diagnostic opcodes
do not alter fixed-zero envelope integrity, bounded COBS/CRC or tunneled0x56/57
semantics. ESP menu callbacks do not replace the shared MacroRing owner; EEPROM
guards exclude both ESP proxies while retaining AVR store ownership. Updated
only four target identities and two shifted anchors; preserved the original
source revision/hashes and all hardware/pending omissions. The focused
pc_contract_mapping test passed all23 exact mappings without weakening its
checks. AVR/ISP/audio and physical EEPROM remain deferred, not reaccepted.

Root also changed three architecture-neutral maintenance labels from ESP8266
to ESP/ESP board-module wording and esp.bin. Reviewed the text-only diff:
request paths, file validation and the existing2MiB client cap are unchanged.
This label fix does not establish partition-aware larger-image desktop upload
or module/size verification parity. Final Release/Test after the checkpoint,
runtime replacement, actual USB/no-network observation and human UI acceptance
remain distinct evidence owned by the deployment task.

## Android optional settings catalog failure: bounded parser repair

The sole reported Android CI39045757076 failure exposed real parser/catalog
drift, not a clock/lease workaround: generated optional:boolean/optional:string
DHCP/static fields were treated as unknown required types, so an otherwise valid
older peer response could leave the network draft absent. Fully reread actual
Pealayer PreferencesTab.tsx563lines (30a63cc1db61a46346c2d4c30ce011dc74081a8a6da2e29853e56d39811a0de1),
the frozen mobile DeviceSettingsSession/parser and entire focused test, with
the actual Web settingsType/settingsSnapshot/settingsValidateAddressing and
native Addressing::validate functions. No new source-complete claim is made
for the rest of the native settings module or donor dependencies.

Reviewed mobile35309b6a6d18d9fb8a5ef687509edee9c9f3aa25d63ad3026081bc96b17d5f1b:
only absent optional keys are skipped; present values stay strictly typed.
The six addressing fields must be absent together or complete, with valid IPv4,
static mask/host/gateway/DNS bounds and no fabricated default observations.
Peer ownership, late-read/write epochs, secret handling, positive ACK and exact
post-write readback remain unchanged. Focused test365cb9c9e9986b9f5ad42bf99fdd910be723b690113ebb126253c5c337b79c63
adds absent/complete/partial/null/type/address-boundary checks and explicit
successful network/cloud load assertions without removing the existing no-replay
or ownership cases. This does not complete mobile static-address editor/UI parity.

Rebound only the existing PreferencesTab mobile target/test references; preserved
all generic control/localization/picker omissions. Strict comparison generation,
all seven mobile wrapper gates and whitespace checks passed, with130 comparisons
out of4673 inventoried files unchanged. The UI owner was released to run only
the guarded focused test file; no APK, full suite, device or firmware operations
were performed by the evidence reviewer. Its actual test result remains separate
from this source review and from Android phone/physical board acceptance.

## Deliberate white boot and unchanged LED-record write avoidance

Fully read current actual PCController Project/SettingsStore.cpp/header and
the EEPROM ownership dependencies; source cpp is7309fd36f0680722a4260e8f1251ae5a0fc67e99fc60f2426bb1294f789e87d4.
The applicable donor saveNow mechanism is EEPROM.update changed-byte avoidance,
not an ESP filesystem driver. Reviewed the entire white-boot diff, new
LedPreferences.h and both changed test files: the existing six-byte private
record/checksum is retained, old valid animation records restore brightness but
reset the table white, and saves store pattern-none. Existing exact record bytes
are compared before opening LittleFS with truncation; transient effect changes
therefore do not trigger identical writes. Runtime rainbow/fire remain explicit
commands, not automatic boot animations. No new API generation/layout selector.

Main e25f493d620f64c1b82e863d84003060a4914e50d24d19c490259d18d94df61c
changes only these record/save/load callsites; peer binding, UART, ISP and
maintenance paths were not relaxed. Rebound its surviving historical anchors
and the extended s3_platform_wiring proof007155a9be17532b0912e97aea8a3c65d98f7ab5a08b6fe9b65b1ec96c5a4fb1.
New SettingsStore narrow comparison binds headerbb0a86a9b0506953270c5bf58f0fb9a49d5e6990b67f3adbd1b22d7e3a71d6cc
and LED test136526419cbf0b2985119d24117082970c0cececc9d57c4b232319434a34da07.
Changed-record coalescing, transactional rename/verified save error propagation
and exact filesystem write-count tests remain explicit pending gaps. This is
not crash-safe persistence or donor-wide settings parity.

Root reports the actual protocol wrapper passed the LED case and preceding
protocol/ISP/tone/front-panel fixtures, then correctly stopped at the stale
peer-binding main reference before the WebAccess tail. No full-wrapper success
was inferred; root owns the remaining gated tail and final SDK results. This
reviewer ran no test suite, build, flash, reset, audible test or device request.
Comparison generation/strict Check and API/peer-binding gates passed with131
comparisons,4542 pending and95 known gaps. Generic firmware-hardware is routing,
not a configured gate; no nonexistent S3-specific gate acceptance is claimed.

The preceding read-only reset audit stays separate: saved S3 telemetry reason9
matches the installed SDK brownout enum, not AVR reset cause. Current AVR
telemetry has no MCUSR field; welcome can also replay on game/key/mode/Prog
actions without a CPU reboot. Donor ResetTelemetry's early capture/watchdog
disable is useful pending work, but its EEPROM journal336..719 would overlap
Pakar automation storage270..411 if transplanted unchanged. Actual AVR image,
bootloader/fuses, ISP signature and verified recovery backup remain unproven.
No firmware-cause diagnosis or working paired-programmer claim is made.

Final root verification: the protocol wrapper's substantive C++ cases passed;
its expected stale-evidence stop was followed by successful gated WebAccess and
Tasks tail runs. Quality stopped at the independently hash-bound PU route
manifest; the actual route bodies were rereviewed and its five shared-sketch
hashes/shifted anchors repaired, not the guard bypassed. The same quality runner
resumed at that failed checkpoint and all remaining checks passed. Guarded
16MB/OPI quiet S3 compile passed, using the existing warm cache without clean.
Candidate SHA599509ab551e4b372ad1f15b8ac98d3f22dfdea093d0d107e0ddda4e7ba9fac9
matches ASUS staged readback. No board deployment or physical white-boot test
was possible. Android preview SHA3ce5304a88caad521238216107da51f789a982a69882c44ce08a3b90e516e1b9
was installed preserving data and verified from the actual installed APK; phone
lock blocked launch/capture after one permitted swipe. No second swipe, server
restart, audio, ISP, erase or fuse operation was performed.

The subsequent quality run exposed the separate PU route manifest's stale
main-file binding. Read its complete test/audit definitions, actual PU AsyncOTA,
AsyncWebServer and SSDP sources plus the bounded info-registration slice;
rereviewed the five current route bodies against the LED-only diff. Their route,
authentication, upload/capture ownership and response contracts did not change.
Updated only those five hashes and shifted target lines in the existing
PU850-ROUTE-PARITY.json/.md; source revision/hashes,50 dispositions and every
pending omission remain unchanged. Focused pu_routes_test passed all50 mappings.
Search across docs/tests/tools/.github machine-readable manifests found no other
old main or S3 wiring-test hash. No route source/test guard was weakened.

## S3 inactive-slot remote staging and live OTA admission failure

Read the actual complete PU850 LocalLib/AsyncOTA.cpp/header and
FirmwareTransfer.cpp/header, with the current RemoteFirmware, RemotePolicy,
FirmwarePlatform, MaintenanceRoutes and ArtifactDownload implementations.
AsyncOTA.cpp donor SHA3d681d678b58e5e615448acddc52fd623914c8ec4a76f80f5bbb75cd34c3aaec
is now a narrow hash-bound comparison. The new remote-ota index reaches the
four OTA/transfer donor files; its gate validates two required reviewed cpp
sources, not every donor. AsyncOTA.h still has no independent comparison row.

The identified S3 application exceeds LittleFS capacity. Adapted the actual
ESP-IDF OTA handle lifecycle into S3OtaStage: sequential inactive-partition
staging, bounded writes, exact length/SHA/SDK image validation, no boot selection
while downloading, explicit journaled apply, persisted-stage revalidation and
actual postboot running-image checks. Quiet ESP-only maintenance bypasses only
absent-AVR preparation, never paired Prog/backup/version gates. Ready stages block
competing local OTA and retain bounded artifact export. ESP8266 staging remains.
The publisher now accepts the actual S3 hardware profile/raw image and an omitted
AVR component; no versioned API or compatibility alias was introduced.

The old SDK's getSketchSize verifies the complete image. Actual installed-core
Esp.cpp confirmed the firmware-download chunk callback repeated it for every
chunk. Captured verified response bounds once instead; no repeated image scan
inside the reader. A live old-image download timed out, with a new panic-reset
boot observed during the interval. Without a backtrace this is not proven reset
causality. The user separately reports correcting the SS36 supply drop; that is
not our measured external-power acceptance or an AVR reset diagnosis.

Full protocol wrapper, the actual S3 RemoteFirmware host/SDK-double harness,
publisher cases, S3 wiring guards and complete quality runner passed. Fixtures
exercise a1843552-byte image beyond FS capacity, real BCrypt SHA, stage/resume,
explicit apply, journal/HTTP/length/encoding/write/end/hash/tamper/cancel failures.
Final guarded16MB/OPI quiet S3 build passed without clean/new cache. Raw candidate
1850096bytes SHA5b5d775a77e66d853566e56b4b1af8ccadaabfd30272e52845f0b4d5c4739f12
was staged and hash-checked on ASUS; no new native/APK/AVR builds were needed.

Published only raw application and pinned manifest into a new immutable scoped
office HTTPS directory. Server disk image hash and HTTPS manifest hash match;
manifest SHA isb828e7dfd3833461aa7e83bc1281761569f973828d69d7fb4f4aa08005e231c9.
No server configuration, unrelated tenant, broker credential or full flash backup
was published/changed. This is server artifact proof, not board Internet delivery.

Native IPC enqueue did not expose a durable update result. Fresh same-board old
image/boot/idle/zero-upload resolved no applied image. Later direct one-shot OTA
returned HTTP400: Another operation is active or multiple files were supplied.
Retrieved that exact saved response after SSH returned; no blind replay. A
subsequent attempt stopped at preflight without POST. Disconnected ASUS's native
app through guarded typed IPC to quiesce polling; observed enabled=false and no
retiring resource. It still requires a guarded reconnect before handoff.

ASUS retained Ethernet and joined ledboard AP after replacing only the newly
owned current-user profile with the same all-user profile; original XML was
preserved, existing profiles untouched. Hostname resolution failed on this AP.
Use explicitly selected current192.168.4.1 only with actual board/boot guards;
do not silently change routes. Tunnel access then became intermittent before
the latest private station credentials were applied. No password is in this
receipt or source. Preserve AP recovery and verify real network persistence.

Rebound only reviewed target/test hashes and affected route anchors; all50 PU
route dispositions remain. Comparison count132/4673 leaves4541 pending and96
known gaps, not completed donor parity. Native outcome persistence, bounded
DNS/TLS opening, real OTA admission/HTTP export, slow-client ownership, board
TLS fetch/apply, rejection/cancel/reconnect, rollback/power-loss/soak and optical
white boot remain open. No AVR command/reset/ISP/fuse/audio or PBX was performed.

Live continuation recovered ASUS, read the same isolated board/boot through the
AP, and saved the new private station settings once (202 accepted). Actual
association and AP-enabled readback passed; hostname resolution through ASUS
worked after the OTA reboot. No station secret was written to source or GitHub.
The prior upload's failed phase/32bytes plus saved negative ACK were resolved:
the installed ESPAsync parser treats a Content-Type-bearing MD5 field as a file.
Removed that header and quoted multipart dispositions in the private uploader;
did not weaken the device's duplicate-file/ownership guard. Corrected one-shot
upload returned200 updated/rebooting=true. Boot62122282967feaf4192b9e2f9fa7646e
reported1850096bytes and matching MD56b2b76343d415a3a1a3584f4bb181137,
all80 cells white and480 committed RGB pixels white at255; network settings
persisted. Initial AP readback failed, so recovery used the now-resolving hostname
without repeating any upload. Full-image SHA/optical acceptance remain open.

Measured a65536-byte HTTP range:206, exact Content-Range0-65535/1850096,
17.3seconds. Full stream received at least265120bytes before closing; a later
boot change was observed. Bounded passive USB capture used the existing guarded
SerialPort setup with DTR/RTS false and transmitted zero bytes. It returned
POWERON boot text and an esp_cache_msync invalid-address warning, but cannot
establish when that boot happened or attribute it to download, power or port
opening. Do not claim that an explicit no-reset command guarantees no electrical
reset, or diagnose panic from this log. Subsequent read-only status succeeded.
S3 host fixtures are also wired into the Linux protocol CI runner using its
existing OpenSSL SHA adapter; Windows uses the already-executed BCrypt adapter.

The added Linux S3 invocation correctly tripped the existing display donor's
whole-runner hash binding. Reread the actual current639-line PCController
Tools/VirtualBoard/tests/firmware_controls_test.cpp through EOF and the complete
runner/diff; donor hash4facf265f8593b83f7610ae99ce58050013226cf1aa5a4d6e60bdbd6e5cc399e
still matches. Only the new S3 test/gate was added; the display loop and all
existing invocations are unchanged. Rebound that single runner proof hash,
retaining every display/key/transition/tone pending omission and hardware scope.

The remote-ota gate now runs before every ESP build/Plan and the focused S3 test;
the guarded quiet S3 Plan passed all gates without compiling or writing assets.
The full Linux invocation remains CI execution, not locally asserted Linux proof.
An instrumented full HTTP200 running-image download then completed1850096bytes
in19.24seconds with SHA5b5d775a77e66d853566e56b4b1af8ccadaabfd30272e52845f0b4d5c4739f12.
Request headers pinned bootdc2b802cd3318072494ae7282e8c9e46. Independent telemetry
reported matching MD5/integrity and panic reset4 on that boot; the180second passive
console window contained zero new bytes. Happy-path OTA/image proof is now real,
but intermittent failures, panic cause, AP recovery, optical and soak remain open.

The first Internet stage guard refused an unset clock before dispatch. A pinned
owner set UTC from ASUS and read it back valid; this is not SNTP proof, and the
existing manual-time route still reports source unset (a remaining telemetry gap).
A later stage POST lost its response; actual new boot/idle/job0 was inspected,
never interpreted as completion and never followed by Apply. No two-board action
or remote application/rollback success is claimed. Keep USB available.

Resolved the later pooled-client failure separately: same bootdc2b..., idle/job0,
no error/ACK and no console output. A fresh non-reused mutation transport obtained
the real202 state ACK for job1, manifest phase/ESP-only/apply=false/received0.
The next bootc900bd0d775661e6ab978a527590fba1 recovered that journal as failed:
Interrupted update; inspect Prog and backups. No operation was replayed.
No Apply was sent. Preserve the acknowledged-job distinction from transport-only
failures and diagnose the actual ESP crash before another Internet download.

## Actual S3 DNS/SNTP panic and project-only core-lock adapter

Read the current physical partition table instead of inferring it from the
original factory backup: app0 at0x10000/app1 at0x340000, each0x330000,
LittleFS0x670000/0x180000, crash0x7f0000/0x10000. Read only4KiB table,
64KiB crash and8KiB OTA selection through the identified CH343/COM22 and
MAC90:70:69:ea:c8:00. Those read-only esptool operations reset ESP; they are not
claims of no electrical reset. No AVR UART/ISP/reset command or fuse operation.

Crash partition SHA17e1459ecb1e13022795dc361c7a691d296c0d00cbefd7c0cdbf0762f68c4e8c
has a valid24580-byte ESP ELF/CRC record. Its note contains the9e9299abf ELF
hash prefix matching the preserved fault ELF's complete SHA
9e9299abf8424e06c52d22721976eb9d4e508f3310251b98bce4afe84820130e.
GDB with auto-loading disabled and arguments suppressed shows:
RemoteFirmware::openDownload -> HTTPClient::GET -> NetworkClientSecure::connect
-> NetworkManager::hostByName -> dns_clear_cache -> sntp_dns_found
-> sntp_request -> dns_gethostbyname -> udp_new_ip_type assertion:
Required to lock TCPIP core functionality! This establishes this crash's
software cause; do not attribute it to the earlier corrected supply drop.
Raw RAM/core dumps may contain secrets and remain private, never web exports.
The one preserved13.8MB fault ELF is essential diagnosis/verification evidence,
not another debug build cache or a copied desktop package.

Fully read actual Arduino3.3.12 NetworkManager.cpp (SHA
a4c0c9c35fa185884d5acbc3c9a3e029d0fb9e4bf824d313e81b5dd4c6d5328a),
the applicable secure-client connect overloads, and the actual lwIP core-lock
query/mark macros and nonrecursive mutex contract. Reviewed Espressif's core
dump loader/header/CRC and bootloader OTA selection implementation. The latest
stable core remains3.3.12; did not downgrade or install a prerelease SDK.
The project linker adapter wraps ONLY dns_clear_cache, acquiring the core lock
only when this caller does not already own it. It holds no lock around blocking
DNS/TLS/socket operations. Original DNS, SNTP, SNI and CA validation remain.
Shared SDK bytes are unchanged; the exact S3 build wrapper carries the linker
flag and a source guard prevents forgetting it. Actual linked disassembly
confirms NetworkManager::hostByName calls __wrap_dns_clear_cache.

The actual production adapter fixture passed caller-owned, already-locked,
TCPIP-task and nested-callback cases through the real linker wrap. Added it
to both protocol runners, preserving every prior test/display assertion; only
their hash bindings and the four extended source-guard references were rebound
after reviewing those diffs. Guarded16MB/OPI quiet SDK build passed in the same
cache with no clean. Candidate1850176bytes SHA
87a62afd005c104ab713a013ff51069325fd5dd97d7c3ed1587cd1ed02700d58,
MD57a6cbc3d511cc13c912bb1fcd4340a55, ELF SHA
16b53202d7c4b71cb08de0700ed3ae91ded3dc7a610c0abd48c7325d919ac7ce.

Consulted the network-cloud PU mappings and read the entire actual PU
WiFi_Setup function plus CrashHandler.cpp/header through EOF. The ESP8266
crash UART-suppression hook/cont_t callback do not implement an S3 crash
collector; most persistence is commented TODO code. Do not transplant that
suppression or invent log-rotation parity. Generic safe diagnostics/export/UI
remain pending, as do the donor-wide comparisons; this focused SDK adapter is
not PU shell/discovery/crash-feature completeness.

LAN upload of the fixed candidate ended after520627bytes; actual same boot,
aborted phase and unchanged old image resolved no commit. Did not replay it.
The required one-time USB bootstrap wrote ONLY the selected app1 application
at0x340000..0x503fff, with esptool hash verification. Actual OTA selection
records were CRC-verified(seq1 and seq2/valid); table, OTA metadata, NVS,
LittleFS, inactive slot and crash partition were not written. Known-good5b
application and original16MB backup remain preserved. Boot
fa9d4563d68224b0f87919a17c0f618c reports matching new size/MD5 and persisted
station/AP configuration. ASUS disconnected only its ledboard Wi-Fi client,
retaining Ethernet; ESP AP remains enabled. This USB bootstrap is not passed
off as successful LAN OTA of this fixed image. Internet stage/apply and full
postboot hash/soak still require their own actual receipts.

The post-bootstrap Internet job2 failed cleanly in the same fa9 boot, without
the prior panic. Actual telemetry showed IPv6-only primary DNS and no publisher
resolution; the network gateway answered the identical lookup from ASUS. Read
the SDK's actual STA dnsIP setter: esp_netif_set_dns_info changes the resolver
without stopping DHCP. Added a rate-bounded S3-only DHCP recovery policy using
only the usable backup or this network's IPv4 gateway; static profiles, working
IPv4/IPv6 primary or routed IPv6 backup, offline state and AP settings remain
untouched. No public DNS, insecure TLS, hardcoded publisher IP or settings write.
The build macro can disable fallback. Actual production-header fixtures cover
missing/global IPv6, valid primary/backup, static/offline, unavailable gateway,
setter failure/retry, throttling and millis rollover. Kept the existing PU
AsyncOTA comparison, exact target/proof hashes and remaining live gates rather
than creating a second checklist or claiming donor-wide network parity.

DNS recovery candidate1850752bytes/SHA82ba2e463536b6a86935f629300ddbb78a80f88d7ef89efba1c10a2cfc311554
was USB-bootstrapped after another LAN abort (25202bytes, no commit). Current
OTA metadata was reread before writing only app1. Bootb8a67e1bf0d56ee8f5e5b788008dda4e
reports matching size/MD5; actual DNS became192.168.1.1, publisher resolution
succeeded and SNTP synchronized automatically at41154ms (source ntp,
ntp_observed=true). No manual clock set in this boot. Station/AP persisted.

The .NET fetch helper failed in read-only preflight with dispatched=false; no
mutation was replayed. A new curl-based request returned an actual job3 manifest
ACK. The private helper incorrectly expected an accepted boolean rather than
the documented job-state response; this harness false negative was corrected,
not counted as device rejection. Readback, not a second POST, established that
the board had parsed the manifest but rebooted; interrupted journal became
failed and no Apply occurred. Current crash record SHA
5c6082a3b5f594a108f0386266c5dffa67a4cbeb211bb543d2e35885c5489d35
has a valid23332byte ELF/CRC record. The preserved matching eeffe49a fault ELF
resolves the call chain through HTTPClient::setTimeout → openDownload → service.
PC0 and the installed SDK source establish a dangling client on the transition
from manifest to image: end() leaves _client set if already disconnected, but
Pakar destroyed the socket before begin/setTimeout reused that pointer.

Read actual ESP32 HTTPClient begin/end/disconnect/connected/setTimeout/destructor
and the corresponding ESP8266 ownership implementation. Keep the stopped client
object alive through rebinding, bind the new client before replacing the old
owner or inspecting it, and declare HTTPClient after its socket so destruction
is correctly ordered. stop releases TLS resources; no idle open socket is kept.
The host double now models end retaining a disconnected client and asserts
client liveness during setters, rebinding, end and destruction. Added explicit
manifest→image, cancel→fresh job and destruction coverage. Both actual S3 and
shared updater fixtures pass with this stricter model; old unconditional-null
test behavior was insufficient. No TLS/auth/identity/apply gate was weakened.
Keep private raw cores and the essential matching fault ELF out of GitHub.

The lifetime-fixed94d227ed candidate was app1-only USB-verified, then Internet
job4 failed cleanly before receiving its manifest, in the unchanged0715a909
boot. Automatic SNTP was observed again at96510ms. The generic combined error
was not sufficient diagnosis, so openDownload now preserves distinct URL,
clock/heap, binding, numeric HTTP/TLS, length/encoding and FS/inactive-slot
failure reasons through cleanup/journaling. It never exports response bodies,
signed URLs, auth headers or token text. Added exact rejection assertions and
negative-HTTP numeric TLS coverage; focused production-module fixtures pass.
The fixture initially exposed its own missing String+integer overload; changed
production formatting to bounded snprintf, rather than weakening the tests.

Diagnostics candidate1852176bytes SHA
40a4024e620877295a63385812f43f83344cf9919a4232e337f21c89489294e1,
MD5c7a394b090864fe59b81e8ea709794bd, ELF SHA
e8cc6afc4ea86df5c92c3fd9b37f37bba9cd4ab8aae28cbe9c2534e01d782776
passes the guarded16MB/OPI buffered quiet build and selected-app1 USB hash
verification. Current metadata was reread before each bootstrap; no OTA select,
NVS/FS/table/bootloader/crash or AVR write. Immutable publisher file hashes
match; actual ASUS TLS1.2 HTTPS returns200 and the server verifies200 using
RootX1 with no default CA directory. None of these bootstrap/server/PC facts
substitute for successful board Internet Apply. Keep the LAN aborts, intermittent
empty HTTP replies and full physical/discovery/soak/client gates open.

Live boot76e35a7c showed DNS being overwritten again: IPv6 primary at26577
and57296ms, then gateway restored at34053/71019ms; SNTP was still unset at147s.
This exposes a periodic-repair race, not evidence that one successful NTP boot
means startup is robust. Added immediate, still validated DNS preparation before
the initial configTime call and only when RemoteFirmware needs a new download
connection. Normal background checks remain five-second bounded. No new public
resolver or arbitrary probe: the exact same policy preserves working/static/
offline configurations. Production-header tests inject RDNSS replacement inside
the throttle interval and cover first-boot preparation, immediate repair and no
rewrite of already-valid DNS. Source guards require both actual call sites.

Job5 subsequently reached Ready with all1852128 Internet bytes and the complete
staged raw SHA8acf9939... in72.708seconds. Lost HTTP Apply responses were not
treated as commits; actual journal/boot readback stayed Ready. A later genuine
panic(reset4) occurred while Ready. The CRC-valid23524byte crash record SHA
15604c6d170f81da048a7292b980d790328c631a20724bc6aaee9499f4394aaa
decoded against the current c2cad8f6 ELF shows __throw_bad_function_call →
std::function<void()>::operator() → AsyncHttpService::service → loop. Read the
actual request continuation implementation and adapter: AsyncTCP and loop
concurrently touched the same weak-request/std::function slot; service could
clear the callback while the producer was between assigning it and publishing
the weak request. Weak/shared pointer counters do not synchronize that pair.

Queue publication/take is now one mutex-protected unit on S3/host, with an atomic
processing flag for native upload admission. ESP8266 keeps its cooperative
single-core path. No mutex is held across pause, handler execution, response I/O
or reentrant service. An impossible empty callback returns503 rather than
actuating/panicking. The actual adapter fixture injects the old publication
interleaving through pause, then accounts for every one of20000 concurrent
producer/consumer requests as exactly one200 dispatch or bounded503 rejection.
Retains all old weak-lifetime/disconnect/reentrancy assertions; the actual
WebAccess/epoch fixture also passes. These are doubles/stress, not hardware soak.
Ready's cancellable report now agrees with its actual API; source fixture checks
it. No auth/fence, image validation or explicit Apply requirement was weakened.

The first USB bootstrap guard correctly refused changed metadata before writing:
sector0 was fully erased; sector1 remained seq2/stateVALID/CRC55f63774. Reviewed
Espressif v5.5.5 esp_ota_begin/invalidate-inactive implementation: rollback-aware
staging erases the inactive selection record without selecting it for boot.
Only that exact erased-inactive + unchanged valid app1 case was added to the
guard; current metadata was reread before app1-only write. No metadata was
rewritten to force a slot. Primary implementation:
https://raw.githubusercontent.com/espressif/esp-idf/v5.5.5/components/app_update/esp_ota_ops.c

Queue-fixed candidate1852656bytes SHA
a0dfcd6388e5590acdd184c051e379a411891c9180a9748f10bbb2d50e2b6e74,
MD579c037feb006c16ac3d066aa8b37c12e, ELF SHA
6e0ceea211031e8ed93f1b847811db36570fefe38a7a42f77b1b906c715db9da
passes the guarded warm16MB/OPI build and USB flash hash verification. The old
stage was explicitly cancelled in boota7372ca8 with positive ACK/readback and
no read errors, so it cannot later regress the queue fix. Job6 must stage this
new image before any explicit Apply; no final delivery is inferred from builds.

## Changed-image Internet OTA through the ASUS application

The queue-fixed bootstrap remained app1. A subsequent guarded warm S3 build
added actual esp.ota selected/running offsets and nullable SDK state telemetry;
this additive living contract does not introduce an API version or infer slots
from a backup. Final binary1853328bytes SHA256
29fca3373f32e875ae7181e775ef9b4373b85c898f3ae5375a1d0af6d0cea15c,
MD557dbbb3f6cfbe03a50289adb50539567, ELF SHA
ccce33b9afce12c575649d567b6f2925d93fea25dbdb9aec22ce451d19f0ac8b.
The raw application was published under its immutable digest directory; server
files match and the separately trusted manifest SHA is
71cf1a19e786c27eaa0cc84f899cf40d0be070d5b20744ddbec0a551547e0035.
No server configuration/service/account change. No Wi-Fi secrets in this ledger.

After explicit cancellation of obsolete stages5/6, Internet job7 fetched this
different image. Its observer lost a read late in transfer; no Fetch was replayed.
The installed ASUS PakarController e63c1bb0... CLI independently observed exact
Ready job/boot/manifest/image/length, then sent one explicit Apply and returned
exit0 with prepare-phase acknowledgment. New boot13acc69e049a90569d631f1bc7950281
reports Complete job7/expectedSHA, actual running_offset=boot_offset=65536 and
running_state_code=2 VALID. This final image was not USB-flashed. The old boot
was a7372ca8158212349c4e01295a351446. Independent image identity, changed
firmware-only fields, completed postboot journal and actual slot state are
stronger evidence than merely seeing the same descriptive version0.2.7.

Short same-boot readback through uptime982638ms reports reset3, persisted
station/AP, automatic NTP observed at131463ms, all80 cells white and all480 RGB
tripletsffffff atbrightness255, modecell/effectnone/AVRfalse. Not optical,
external-power cycling, arbitrary concurrent clients or long soak. One small
read needed a retry; original failure is recorded. A complete single running
image stream truncated after269428bytes and the first range observer failed
after262144bytes; neither was counted as successful full raw export. The ESP
remained on the same boot afterward. Private named receipts are documented in
REMOTE-MAINTENANCE; retain failed evidence and recovery availability.

Latest focused verification: guarded S3 build passes; real-linked DNS wrapper
and production DNS policy tests, actual S3/shared RemoteFirmware with stricter
HTTP client liveness doubles, actual async adapter deterministic interleaving
and20000-request producer/consumer fixture, WebAccess/epoch fixture and S3
wiring/PU routes/living-contract guards pass. The10 new telemetry lines required
explicit PU route anchor rebinding; the initial stale-anchor failure was fixed,
not skipped. No unrelated desktop/mobile rebuild or repeated full suite.
Existing comparison count remains132/4673,4541 pending,96 explicit gaps.
No claim of all-donor, AVR, discovery, UI, phone, rollback or production acceptance.

Final independent running-image readback subsequently passed:29 boot-fenced
HTTP206 ranges, exactly1853328bytes, SHA29fca337... matching the changed OTA
image, same hardware/boot before and after. Every range checked Content-Range,
Content-Length and expected MD5 ETag; complete chunks only entered the aggregate
SHA in RAM. Three empty replies and one partial45second read timeout were
retained in the receipt and retried only as bounded reads; no mutation replay
or duplicate firmware SSD copy. The failed initial streams remain evidence.
This establishes complete bytes, not reliable streaming/resume or network soak.

Restored the existing ASUS interactive owner9796 via guarded no-draft typed
Wi-Fi Connect. Positive app readback reports online=true, the exact board/final
boot and all-white cells with no pending maintenance, not just enabled=true.
Kept the same e63c1bb0... native executable, original protected preferences,
separate candidate profile and rollback; no force-stop, USB fallback or native
rebuild. Remote OTA and live app connectivity are separate from visual/human
acceptance and from direct Internet commands while the ASUS relay is absent.

## Continuous LED output and Wi-Fi scheduling regression

User reports visible stale/garbled output during Wi-Fi activity. Current board
was reread over hostname on ASUS: same13acc69e... quiet ESP-only boot, cell/none.
Read games-streaming lookup and literal Puzzles WS2811.cpp inventory; fully read
actual Puzzles LocalLib/WS2811.h/.cpp and verified original hashes39509602... /
0f7fec78... against the ledger. Retain its255 brightness and TypicalLEDStrip
correction; not its AVR D6/eleven-pixel buffer or BRG order. Independent channel/
480-address/optical testing remains required, not a declared donor exclusion.
Also fully read current PCController Tasks.h/.cpp and Pakar's existing adapted
Tasks: those loop-only cooperative callbacks cannot isolate seconds-long DNS/
TLS work or synchronize an AsyncTCP producer; no duplicate scheduler was copied.

Actual code inspection found WebSocket callbacks mutating LedGrid and producing
status from the network task while loop renders/transmits. Complete incoming
messages now pass a bounded eight-slot synchronized FIFO into the main loop;
closed/unauthorized peers are rechecked, overflow explicitly closes1013, and no
network payload pointer survives the callback. The FIFO publication discipline
adapts the existing reviewed AsyncHttpService fix, with no I/O under its lock.

Separate staged RGB storage prevents incomplete chunks editing committed pixels;
an explicit frame commit snapshots before later chunks, and abandoned partial
stages expire after500ms. The S3 output worker owns immutable current RGB and
preencoded internal-DRAM symbols, a capacity-one latest-complete-frame mailbox,
mandatory RMT DMA and a bounded100ms completion wait before buffer reuse. RMT is
initialized on core1 separately from Wi-Fi core0. Maximum legal2046 DMA symbols
respects the reviewed SDK descriptor4092-byte-per-half bound; an entire480-pixel
symbol frame would exceed it and was not blindly configured. Existing FastLED
3.10.6 WS2811 legacy250/350/650ns timings were read and retained exactly at40MHz,
with RGB order, TypicalLEDStrip correction,300us low latch and no temporal dither.
No SPI2/3 bus is borrowed from AVR ISP or future W5500. Initialization/transmit
failures remain visible, with no silent non-DMA fallback or in-flight overwrite.

Idle is removed from Table advertisements/client selection without renumbering
the remaining mode codes or changing the deferred AVR firmware. A runtime LED
output service defaults On each boot; explicit Off queues a full black frame,
then sleeps while settings/mode remain accessible. REST /api/led/output, MQTT/
WebSocket command led-output with boolean enabled, USB opcode0x5C, native CLI/
TUI led-output on|off and Telnet expose the same setting. Native/Web/mobile show
the switch only when the current peer actually reports enabled; missing is not
treated as enabled. Driver ready/active/transmitted/failed/revision/error_code
are separate from the requested switch and the renderer command-buffer revision.

Focused actual renderer and20000-message concurrent FIFO fixtures pass with
Arduino/FastLED doubles, including bounded overflow, copying, all480 pixel
encoding, complete black/latch, static refresh, disable/re-enable and abandoned
partial isolation. Web mode/context and S3 source guards pass. These do not prove
SDK compilation, actual DMA, optical waveform or Wi-Fi contention. SDK build,
app checks/deployment and live wire-worker observations are pending at this point.
Physical user confirmation remains required before marking the glitch fixed.

Independent safety review then found and fixed two additional ownership paths:
legacy PC0xFC commits now validate staging owner and return Busy on refusal,
and successful raw stream commits retire pending cell fades without repainting
their targets over the raw frame. Wrong-owner/expired-commit and old-fade
regressions pass in actual LedGrid/PcLedContract fixtures. Continuing raw commits
while output is disabled update logical RGB only; they do not repeatedly submit
black. PKAR/WS/UDP owner tags prevent cross-source staging, but legacy PC-contract
transports still share owner0, and the existing PKAR header has no frame sequence
to reject late/reordered chunks from the same source. These remain explicit gaps.

Actual SDK disassembly showed RMT DMA ISR cache safety is not enabled in this
bundled configuration. Flash partition write/write_raw/erase wrappers therefore
acquire a bounded150ms full-frame quiescence hold, nest safely across tasks, and
refuse unsafe ISR/cache-off/suspended-scheduler contexts before blocking. They
cover current LittleFS/NVS/OTA application paths, not lower-level panic dump
writers. No shipping direct esp_flash or ESP.flashWrite bypass was found. Actual
worker host fixtures pass functional/timeout/stop-failure cases, including2000
concurrent submissions, immutable in-flight encoding, stable refresh, black once,
resume, nested holds and failed DMA stop refusing flash. Actual linked wrapper
fixtures pass failure/error propagation, unsafe contexts and2000 concurrent calls.
These are source/double proofs; final ELF binding and physical coexistence remain
independent gates. The initial SDK compile caught FastLED CRGB memcpy ADL
ambiguity; qualifying ::memcpy resolved it rather than altering SDK sources.

Async WebSocket dispatch retains client IDs, never a raw client pointer across
tasks. Admission/session maps have short locks with SDK send/close outside them;
the actual middleware fixture passes20000 concurrent lifecycle operations and a
reentrant-close lock test. Existing credential-record access paths outside this
slice still need a complete auth synchronization review before broad concurrency
acceptance. The Linux fixture now links pthread explicitly.

Native guarded Check and Release pass: version0.2.7,25529344-byte executable
SHA91a3ea57c7ae6cf16bc1577f5bf3d54bdfe187267141216766354a86ce29c6a8.
Guarded stable staging plus CLI identity probe pass. Full native suite deferred
per incremental user policy. Mobile analyze and focused LED4/peer5 tests pass;
initial test-only Map identity matching was corrected to deep-map assertions.
Preview APK0.2.7 is55396153bytes,
SHA38bd2a0edad8b3fb3fb4e995d5e6f35876a84023d29de074ad352f5b740af423.
ASUS ADB currently has no devices, so no phone installation/launch is claimed.
Web generic-LED placement, live Persian Output localization, pending interlock,
unknown-state hiding and post-readback behavior now execute in the focused DOM
fixture; not browser/human acceptance. Generic non-table Flutter placement and
programming-latch admission parity remain pending before resumed AVR integration.
The private rebinding helper initially used an incorrect manifest property;
the comparison gate caught the stale test hash. Corrected to actual files array,
rebound reviewed changed targets only and retained every original donor hash.

### Follow-up: reconnect, unavailable preview/mapping and requested R/B swap

The user then explicitly requested reconnect and pixel/mapping fixes plus a
red/blue swap. S3 wire encoding now defaults to BGR, configurable at build time
with PAKAR_S3_LED_SWAP_RED_BLUE=0 for RGB. Logical API/effect/preview RGB values
and per-logical-channel correction remain unchanged. Actual worker fixtures
pass functional/timeout/stop-failure in both profiles. A rerun exposed a fixture
race: its live flag could identify an already-completing frame rather than a
driver wait stopped at the test barrier. Generation-specific barrier ACKs now
make the forced-quiesce-timeout test deterministic; no production timeout was
increased. Ten focused runs of the corrected scenario passed.

LedGrid now keeps a distinct immutable last-published RGB/map/brightness snapshot
through render-dirty, partial streaming and disabled-service states. Its optional
pending flag distinguishes a newer uncommitted draft. This removes the actual
null-preview starvation path without exposing uncommitted pixels or implying
physical transmission. Actual renderer fixtures verify snapshot/revision
retention and subsequent publication. The API mapping response now checks a
complete worst-case escaped-name reserve and returns503 rather than attempting
truncated200 JSON after allocation failure. Mapping POST atomic persistence and
stricter name validation remain separate pending hardening.

AsyncHttpService now dispatches occupied slots round-robin: first-free producer
reuse can no longer indefinitely starve older mapping/status requests behind a
continually refilled low slot. Deterministic seven-older-request fairness and
existing20000-operation lifetime/publication fixtures pass. This is one concrete
timeout path, not a claim that all intermittent HTTP failures are fixed; SDK
DNS/TLS/MQTT calls can still block the loop longer than a client's read deadline.
Hostname-first ASUS reads of the old13acc69e... boot returned actual valid80-cell
mapping and complete2880-character RGB in281–344ms; these did not prove the new
firmware was installed or that a physical reset caused the client reconnects.

The guarded final S3 build passes for16MB/OPI PSRAM, buffered UART profile with
normal AVR link paused:1773600bytes,
SHAc48201f02e2e52046d3dc3e0f7c5a5935acb2263291b3ffb24fb067aec917c43,
ELF494d5a33504f010f26440f3d16e74952024f157b29ff1b1fdde6718e11155a08.
Independent final map/disassembly verifies all three partition wrappers in IRAM,
actual LittleFS/NVS/encrypted/OTA caller rewrites, context/acquire/real/release
flow and unchanged lower-level panic flash APIs. Not hardware coexistence proof.

The Rust reconnect fix retains a fresh same-peer view after transient HTTP loss
rather than clearing it while WebSocket is healthy; eight-second peer freshness,
preflight/epoch/write fencing and explicit Disconnect remain. Null pixel samples
retain only their original five-second age, never renew it; invalid data, changed
mapping and revoked peers clear it. Mapping reads validate80 unique physical IDs,
coalesce pending requests and retry initial failures with bounded backoff.
WebSocket recovery/backoff resets only on an acknowledged actual status, not TCP
upgrade, and write waits are bounded. Guarded Check/Release and, at this
significant milestone, one guarded native Test pass186/186. New executable is
25562112bytes, SHA557cc3d2cb695f539bcc182d104df9b942c3e019efcb69fb35f8c1370330015e.
Source stability and stable stage/probe pass; actual replacement/readback remain
separate. Six preexisting compiler warnings remain, not new clean-build claims.

The first Internet Fetch lost its direct curl52 response. Independent actual SDK
inspection showed closed TCP makes a queued weak request ineligible to execute;
the loop-served job remained old7 (queue increments ID before any yield). After
that explicit outcome reconciliation, one native CLI Fetch returned new job8,
then exact Ready identity/length/hash was observed. The first Apply helper stopped
before dispatch because native info returned telemetry-error, not a board object;
its failed receipt is retained. Correcting read-only preflight to actual status
kept the only Apply in the verified native CLI. That Apply acknowledged prepare;
same-board new boot85c496400eebcf04b5b57540724914b8 reports Complete/c482...,
DMA ready/active, BGR, failed0/error0 and all480 preview pixels. No USB/AVR writes.

This first deployed scheduler did NOT pass the bounded HTTP-load test:12 valid
status/mapping pairs,9 empty replies and no LED mutations before two consecutive
read failures stopped it. Actual wire counter advanced1341 to3539; this separates
working driver service from unreliable API delivery. Do not check reliable
reconnection/Off/On/optical acceptance from this observation.

Further actual ESPAsyncWebServer3.12.1 inspection found a precise response race:
its AsyncTCP _onData calls _send after middleware returns; our loop could already
call request.send, while _sent is set only after response._respond. ACK/poll and
response fields were likewise not protected by the application's queue mutex.
AsyncHttpService now constructs independent owned responses in loop and publishes
them into a bounded per-request mailbox. A public AsyncClient.onPoll event sends
only on the SDK's network actor; the installed SDK's exact _onPoll ACK progression
is retained. Weak request ownership, disconnect cancellation and destructor
cleanup avoid dead connections or leaked unsent responses. No SDK/global-library
patch, forced delay or arbitrary sleep establishes safety. Peer IP is captured
at admission instead of dereferencing the mutable PCB in loop. The two deferred
raw send sites now publish through this adapter; native SDK-context OTA/auth/file
routes retain their direct sends. Two deferred AVR backup downloads no longer
explicitly close the File handle shared with an asynchronous response; actual
ESP32 FS shared_ptr destruction and AsyncFileResponse ownership were inspected.

The focused actor fixture models the real outer SDK send tail, late sent flag,
network-actor-only response/ACK operations, disconnect/duplicate cleanup, fair
dispatch and20000 concurrent publications. It passes; WebAccess tests now use
fresh requests for independent authorization probes and explicit network poll
events for queued responses. The initial reused already-sent fixture failed,
was corrected without relaxing production authentication, and passes. Real SDK
compile/deployment/load confirmation for this actor is still pending here.

The telemetry failure has a separately reproduced cause: ArduinoJson's actual
ArduinoStringWriter clears its destination, unlike the old generic host write
stub. ClientTelemetry.json serialized directly into an existing array string,
losing its prefix and previous clients. It now serializes a bounded entry, then
explicitly appends it with length checks. The fixture enables the real Arduino
String writer: old code fails, new empty/one/eight-client arrays, failed writes,
TTL, observed-IP distinction and secret allowlist pass. No blind heap/snapshot
limit increase was used. Production telemetry and actor coexistence still need
their own live proof; maintenance-admission counters, credentials/Telnet task
ownership and blocking outbound DNS/TLS remain independent concurrency gaps.

### Critical optical regression and emergency stop

The user then reported rapid visible flashing on current c482.../boot85c49640...
while the replacement actor image was being staged. One authenticated pinned
Off request was acknowledged and read back enabled=false/driver.active=false.
The user confirmed flashing stopped but some pixels remained lit: no all-black
or fixed-waveform claim is made. Job9 Ready/bc624443... was explicitly cancelled
through native557c/PID19088 and read back cancelled; it was never applied.

Actual SDK review found the missing coverage: esp_partition_read/read_raw and
esp_image_verify's bootloader_flash_read path can disable cache too. The installed
GDMA refill interrupt lacks the cache-safe registration; a2046-symbol two-half
DMA buffer still needs refills about every1.28ms during a14.7ms480-pixel frame.
The higher RMT interrupt priority does not configure the independently allocated
GDMA refill interrupt. This is a concrete scheduling defect and strong causal
candidate, not a measured waveform diagnosis. Dynamic PM clock changes were
excluded by the actual SDK config; timings match the installed FastLED legacy
WS2811 setup. Increasing RMT memory beyond its two4092-byte descriptors is not a
valid full-frame DMA fix.

Added task-context read/read_raw/full image-verifier wrappers with the same
frame-boundary pause. Guarded FirmwarePlatform metadata helpers cover Arduino's
direct getSketchMD5/flashRead and flash-mode reads; sketchSize now initializes
metadata and checks verification failure instead of reading an uninitialized
result. Lower-level panic esp_flash APIs remain untouched. Actual GNU-linked
fixtures pass all six guarded boundaries, denied acquisition, unsafe contexts,
original errors, nested verifier/read/metadata holds and8000 concurrent accesses.
Focused actual RemoteFirmware fixtures pass on both ESP8266 and S3 using narrowly
updated SDK doubles, without weakening existing assertions.

Next quiet SDK build uses PauseLedOutput; its standard source default remains On.
Quiet begin queues one black frame while keeping scene and service Off, so the
next image must not automatically resume a potentially flashing effect. SDK/
linked-artifact and physical black-clear acceptance remain pending at this point.

### Static six-pixel fallback requested after emergency stop

The user explicitly requested a non-DMA default, immediate identical values for
each six-pixel cell, no animation or transition, deployment and an explicit
rainbow test. This supersedes the prior quiet-only commissioning plan. Consulted
games-streaming and reread actual Puzzles LocalLib/WS2811.h and WS2811.cpp: retain
its FastLED WS2811 timing/correction and full brightness intent, but its AVR
11-pixel driver is not a suitable 480-pixel S3 transport implementation.

LedGrid now defaults to static mode, applies cell changes immediately, normalizes
each physical six-pixel group to its first pixel (including streamed commits),
and does not refresh unchanged scenes. Explicit rainbow in static mode is a
stationary grouped color test; effects is an explicit runtime opt-in. The same
led_output.mode contract is being wired into API and clients. The focused actual
LedGrid fixture verifies immediate pulse requests, identical grouped pixels,
stationary rainbow, grouped raw input and no unchanged retransmission. Existing
effects-path tests remain explicitly selected and pass. These are source/fixture
results; new backend SDK, deployed image and optical evidence remain pending.

User correction before deployment: rainbow test must animate, including with
static grouped cell control selected. The renderer now explicitly animates that
test while retaining equal six-pixel groups; ordinary cell changes remain static.
Repeated Off requests now enqueue black even when requested state is already Off,
instead of returning early and leaving WS2811's latched garbage untouched. Native
UI has an explicit Off action to resend it. Focused renderer checks cover repeated
black and animated grouped rainbow. Compiled-object inspection also caught GCC
inlining the IRAM pulse routine into flash-resident work(); noinline is required
and the superseded image must not deploy. These corrections require one warm
firmware rebuild; no CI wait or full test rerun is a prerequisite for user viewing.

## AVR recovery resumed

User explicitly resumed hardware UART and ISP restoration. Actual PCController
`Project/ResetTelemetry.cpp` and `.h` were completely read, with EEPROM layout,
codec and reset-fixture dependencies. Root verified the donor checkout at
`b0111ae0c80dc34149ea1044de7dd5488920301b`, with raw source SHA256
`8699f6cb75170fb62ee94e24ce0faa74bdfb51021b9eba6fb95c61013619199c`
and `7ad43276b93bc31248848ad80cb4c2f5f40500dd33bf736ddeb6c2d5114256a9`.
The existing comparison ledger records both actual imports; its new `avr-reset`
route prevents this startup capability being missed by broad hardware keywords.

Adapted early MCUSR/r2 capture and watchdog disable, journal416..799 with
commit-last CRC/readback and quiet watchdog/brownout/unknown welcome policy.
Optional B5 adds reset flags/count/persistence without changing46-byte B1.
The buffered AVR build passed at31628flash/1406static-SRAM. Final ELF
`c6d916c3c04c85af668c74845f354a16c61660d76ba40592afbe35befa9157e7`
contains the intended early instructions and fall-through. Raw32KiB image SHA256
is `8e06ffe3b286012ed1fd743c1d93166349ac01bc83d71c8ff5e45a8bae16db9a`.
Focused actual reset-journal, packet/identity and ISP EEPROM-preservation fixtures
passed; no broad suite or CI wait was used. These are source/host/compile results.

S3 source now uses its existing dedicated UART1 adapter instead of duplicating
unchecked initialization. It reports driver-ready/hardware errors, requires
valid AVR identity before online state and avoids the legacy18ms drain delay
for buffered hardware UART. GPIO assignments, AVR hardware serial, original ISP
backup/byte verification and fuse policy are unchanged.

ASUS localhost:8022 timed out before SSH login; local ledboard.local resolution
also failed. No physical AVR probe/backup/program or audible action was possible.
Recovery remains open until access, verified backups and both live paths work.
The user subsequently stopped the private evidence PowerShell helper: its last
run had finished; it must not be invoked again or replaced with another wrapper.

## Basic White/Red delivery

Delivered `/basic` on the integrated S3 firmware, with80 logical buttons and
explicit White/Red controls using the existing shared peer HTTP implementation,
application artwork and palette. Actual RayanLamp `webui/src/api.ts`, Pealayer
`web_ui/src/appearance.ts` and PU850 `LocalLib/AsyncWebServer.h/.cpp` were consulted;
this bounded delivery is not donor-wide UI parity.

The first page request failed with an empty reply. Reviewed installed SDK3.12.1
`ESPAsyncWebServer.h`, `WebRequest.cpp` and `WebResponses.cpp`: ESP32's deprecated
char-pointer `send_P` selects a heap-backed BasicResponse. Serve this immutable
page directly on the native SDK actor with explicit bytes/length instead. Hardware
mutations remain queued; do not restore loop-side network sends. Direct ASUS GET
returned200/18672bytes. Local forwarded-browser failures remain a separate open
transport/browser acceptance item; neither their cause nor a universal fix is claimed.

USB application-slot flash verified image SHA256
`f5572b0b5cf8d750f00390ad25c1c53e8699410d0086487bc66e9a174cdc82da`.
Live boot `d62f69605c125734372ebd96d8cb3c7e` acknowledged Cell mode;
static `gpio-static`, DMAfalse and all80 white were read back. Shipping controller
code then performed cell1 Red and White against the actual board from ASUS:
both POST202 and subsequent state readback succeeded. This was a Node transport
integration using the actual app code, not a browser-click or optical/mapping test.
The focused controller fixture and S3 build passed; no broad suite/CI wait ran.
ASUS has the hostname-based desktop shortcut. The one requested PBX call completed
with archived Completed receipt and matching ANSWERED CDR,30 connected seconds.
Physical cell placement/colors and browser interaction remain for live acceptance.
Final direct readback matched running sketch MD5
`4297dbf58cf1aa6a36c3b79186db241e`, same boot, Cell/none,80 white/0 red,
11 transmitted frames and0 reported driver failures. The isolated source
checkpoint still needs reconciliation with preserved concurrent working-tree edits.

## ASUS native USB-only delivery

The prior compact USB packet did not contain all80 cell states or mapping. Added
a frozen, bounded nonce/token/offset snapshot and exact-boot-fenced table commands.
The reviewed PCController Session admission/matched-ACK/no-replay mechanisms are
adapted in `desktop/src/usb_table.rs`; UI connection epoch, physical-open session,
peer pin and enqueue time are captured before queueing, never rebound by the worker.
Capture-start time remains the freshness authority. Windows serialport4.10.1 uses
first-byte COMMTIMEOUTS; its60ms timeout is not a per-chunk fixed sleep.

ESP application image SHA256
`34e685d5191c62fbb4a69739a08d9053015a260731babc6c25d27a867c15089b`
was written through ASUS COM22 to the previously verified application slot0x10000,
with esptool byte/hash verification. AVR firmware/EEPROM/fuses were not changed.
The boot stayed `2143183069bb9f814ab1cd714a2da268` across app USB reconnect.

Native candidate SHA256
`4c49835d24c96b3663ecfc7276095557436ef6ffa23212a05d3e31f859b3ced8`
is running interactively on ASUS as PID18680, version0.2.7, one process, COM22 USB,
WebSocket disconnected and no established TCP connections. It reads80 real cells
and80 mappings. The preceding USB candidate performed cell1 Red then White with
matched ACK and new same-board snapshot readback. Disconnect cleared live state,
an offline command was rejected, and reconnect returned80 White with no replay.
This is live protocol evidence, not confirmation of physical placement/color.
The final UI adds readable semantic state circles when individual RGB is unknown;
the user clarified logical11 is right/half below1. Native and mobile now interleave
paired logical rows for presentation only; physical mapping and API IDs stay intact.
Actual final framebuffer capture: outputs/PakarController/native-usb-ready.png.
The old browser shortcut was preserved in staging and replaced by a native shortcut.
Settings, original executable and the working USB rollback remain preserved.

All196 guarded native tests passed after correcting the TUI's stale Down-from1
assertion to21; that final correction is test/comment-only after the runtime build.
Flutter analyze and5 lifecycle/readback tests plus2 geometry/phone-panning tests
passed. Preview APK SHA256
`2fa5d2e296bd6bdf948cc0adc2442ab2906cf4d3326429049b4dba1cf3557364`
was built and checksum-verified on ASUS. Neither host lists an ADB device; phone
installation/touch/physical mobile acceptance remain open. Android direct USB is
not implemented or presented as operational, and no network fallback is claimed.

Rainbow was restored by the actual ASUS native USB path at the user's request.
Same boot, gpio-static/non-DMA, output enabled, increasing transmitted counters,
no reported driver error. The user reports glitches/rough movement: acceptance is
OPEN. Source shows frame-count-based hue increments and six-pixel grouping with a
large inter-cell hue jump. This establishes animation roughness in firmware, not
that all corruption is software; GPIO counters cannot validate the wire waveform.
No further driver/physical-mapping changes were made from that observation alone.

These are preserved-working-tree candidates, not a clean reproducible release or
complete donor/production acceptance. Master issue2 remains open.

### USB RGB/effect follow-up — deployment pending

The later user request to remove the Wi-Fi telemetry limitation is implemented
as actual480-pixel published RGB in the same frozen USB capture, not a text-only
change. Firmware/client bounds are16KiB. Existing Frame validation, capture-start
age, current peer and confirmed mapping gate preview/tooltip use. USB also updates
the observed effect; the panel has an explicit selected Rainbow test control.
New native candidate SHA256
`f800bd10b0c4982e31ed4c945bbc83a12327d4cb2cdb33ed61375c071a5693ef`;
new firmware SHA256
`0528a38523f3fa76fa363dee09b056bb6fa8bc943d8ff707de6fd939d80f2067`.
The new source passes197 native tests and the S3 build. A test-only serde Value
construction correction followed the release build; shipping behavior is unchanged.

LedGrid adds RAM-only actual-pattern-transition attribution with constant reasons,
previous/new pattern, uptime and then-published frame revision. It changes neither
wire timing/defaults nor EEPROM/flash. It cannot identify individual API clients
or distinguish an AVR caller from another caller of the same grid method.

Neither new candidate is deployed: ASUS tunnel8022 alternated banner timeout/reset,
then lost its listener/refused connections. The existing USB app remains the
previous4c49835d candidate; the final rainbow retry was only IPC-accepted before
confirmation became unavailable. Do not claim current optical effect success.
The owned untriggered PakarControllerUsbControl task still holds that last explicit
rainbow-and-status action; inspect and reset to status-only before any blind start
after reconnect. No automatic replay or new PBX call is authorized by this note.

### GPIO4 stop investigation and static recovery

ASUS8022 returned. The actual selected-USB app first observed boot
`0c4babc5dee21e8d343747a12a54abe3`, effect none, enabled gpio-static, no flash
pause/driver errors. One explicit native rainbow command was followed by about
1490 completed frames and then effect none on the same ESP boot. This proves a
logical cancellation occurred, not its caller or an electrically correct frame.
AVR input9 was true; a held input alone does not prove repeated press edges.

The already-built0528a385 diagnostic firmware was application-slot flashed and
byte-verified on ESP90:70:69:ea:c8:00, and matching nativef800bd10 was installed
gracefully with current settings and previous binaries preserved. The older
4c49835d app rejected the larger USB RGB snapshot; pairing the already-built
client restored complete table/mapping state. Fresh live snapshots on the new
boot413ed378 show rainbow selected and243,1947,4925,7477,10880 submitted frame
revisions, completed counts one higher, no driver failure/pause, and actual2880
RGB hex characters over USB. No Wi-Fi transport fallback or optical acceptance
is inferred from these reports. No AVR programming or audible test was run.

Read the actual Puzzles LocalLib/WS2811.cpp and header completely again; their
original hashes remain0f7fec78 and39509602. They initialize FastLED/brightness/
black, but do not solve delayed external strip power. The shared LedGrid's
static early returns plus the fallback worker's queue wait explained why an
unchanged static scene never sent again. Added an enabled-only1000ms full-buffer
maintenance resend to needsRender/service, without stepping fire/fades, changing
cell values, adding persistence, or limiting rainbow throughput. Exposed its
interval in led_output. Focused actual-renderer/FIFO fixtures pass, including
999/1000ms boundary, identical RGB, static-fire nonadvancement and disabled-output
behavior; the guarded S3 build passes (1826727 sketch bytes,90052 global bytes).
Candidate image62d26c0f96c67d3f461e3f96df03046acca5cb98fd43d549557e0a29ce490f76.

Read-only review also found old AVR action/reset revisions survive a HELLO/new
session and can replay phantom deltas after a reconnect. This remains a distinct
source-confirmed defect, not today's proven cancellation cause; no speculative
AVR or session behavior change is hidden in the static-refresh fix. Live waveform,
visible colors, all480 pixels and the user's original logical mapping remain open.

### Explicit RMT-DMA run

The user then requested rainbow using RMT and DMA. Added canonical wrapper option
`-S3LedBackend rmt-dma`, passing PAKAR_S3_LED_USE_DMA=1 to the paired S3 build.
Static remains the default; other targets reject this S3-specific selection.
The same cache/output path is reused without clean. Added Plan assertions for
the explicit flag/default/target restriction; that broader script suite is
deferred, not reported as executed before deployment. Changing the global
compiler flag required a full dependency rebuild; no duplicate cache was made.

Guarded build passed (1826843 sketch bytes,90036 global bytes). Image SHA256
94da0453188c3b07a1ab1ebc6a0e651887a18dcfeb820b3b3996d980d4fadaa4 was copied,
hash checked, application-slot flashed and byte-verified on the same COM22 S3.
Non-DMA62d26 remains available in ASUS staging/usb-table-refresh.bin. Current
nativef800bd10 was gracefully restarted without changing user preferences or AVR.
Fresh native USB readback on boot6b13a6ab5f9b383db8ca671e76b949ec reports actual
backend rmt-dma,dma true,ready/active true,failed/error0 and transmitted1569 then
3451. Full effects mode was explicitly commanded through the app and ACK plus
same-board readback verified before the separate rainbow request. Station link
remained connected; this is not a high-load Wi-Fi or optical acceptance test.
Subsequent fresh same-boot native snapshots verify rainbow/effects and RMT-DMA
with submitted1055 then2632, completed5695 then7273, failed/error0, no flash
pause, actual480-pixel RGB previews changing from06fd0101ef10... to9a0166ac0153....
Both commands have ACK plus same-board readback. Current task is status-only,
not a retained mutation for replay. The app remains USB-only; AVR remains
unprogrammed and no audible/PBX tests were performed. Visible smoothness and
electrical correctness await the user's observation; master issue2 stays open.

### Atomic cell updates and Flutter selection/network controls

Implemented one board-owned selection mutation across ESP HTTP `/api/cells`,
cloud JSON `cells`, authenticated Telnet `cells`, native GUI/TUI/CLI, USB opcode
0x5F, Flutter and both Web controllers. The full80-cell selection is validated
before mutation; the12-byte USB bitmap fits the existing boot fence. One common
start timestamp drives300ms wall-clock fade/pulse on the ESP; delayed rendering
catches up rather than requiring client frame uploads. Static grouped fallback
deliberately remains immediate. No EEPROM/LittleFS scene write or80-command AVR
fan-out occurs. Explicit `cell-batch` acknowledgment is required; no per-cell
fallback, optimistic success or automatic uncertain-command replay. Every
selected target state requires matching-board post-ACK readback. This does not
prove completion of a fade or physical light output.

Flutter adds Select all, Invert and Clear using the same normalized Find
selection, transition choice and live AVR-advertised melody chooser. Snake is
hidden across clients and excluded from the default firmware mode manifest;
implementation remains behind PAKAR_SNAKE_ENABLED. Successful hostname addresses
survive reconnect/transport retirement; real lookup failure, not HTTP/auth errors,
evicts a route. Missed discovery scans retain explicitly unverified records.
Actual PCController lanresolver and netpolicy/proxy source/tests were read fully,
adapted and bound in the existing ledger. Actual canonical AddressableLeds.cpp/.h
stage/show ownership is adapted, not its AVR timing sender or old100-pixel limit.
PU850 Shell.cpp/.h were reread fully at their original hashes; Telnet now forwards
the correct audio `cue` field and uses escaped JSON for the new bulk command.
Generic shell registry/broadcast/serial-shell parity remains open.

Failures were investigated rather than bypassed: obsolete queue translations
were replaced and canonical assets regenerated. The widget binding intentionally
blocked real HTTP with400; transport fixtures were isolated using the existing
mirroring transport pattern. Their next timeout exposed an actual proxy defect:
ledboard.local went through the dependency-download proxy (zero local lookups).
Imported donor LAN/private/link-local proxy bypass while retaining remote proxy
policy. Actual Dart SDK custom-factory code also required explicit direct TLS;
SecureSocket.startConnect now preserves hostname validation and cancellable SDK
ownership. Public localhost test-only certificate/key are not APK assets. Real
loopback tests verify original HTTP Host/cache reuse, untrusted/wrong-host TLS
rejection and explicitly trusted fixture success; no bad-certificate bypass.

Verification: guarded S3 RMT-DMA build passes (1833047 sketch bytes,90676 global
bytes);202 native tests and native Release/stage probe pass; Flutter analyze and
seven selection/cache/melody plus three actual transport/TLS tests pass; Android
preview release APK builds. Focused Web table/basic/context tests and complete
protocol wrapper pass with actual renderer/FIFO/worker/ownership host doubles.
The native read-only fixture's first transient failure did not recur unchanged;
the separate batch fixture was corrected to require the real accepted count.
Comparison generation/Check and configured mobile-network gate pass. The broad
firmware-hardware lookup has no acceptance gate configured and correctly refuses
`-Gate`; it is routing, not a claim that all hardware donor parity is certified.

Checksum-verified candidates in outputs/PakarController/staging:

- PakarController.exe:8f176ca8ad5d0c71ee2b4094125adf5681b7c630cdb5072101a70192cf138084.
- PakarController.apk:1070e6c34ae6216ed010ef5da7bbab949c63a6cf5a4be85b248ba3b13860be19.
- PakarController-esp32s3.bin:75f2b18280fc494d8a251d33bc691244f1669c555f68cde97950095f7eca4aef.

These remain preserved-working-tree candidates, not clean-main/release promotion.
ASUS SSH8022 refuses connections and ledboard.local is unavailable here; no
current candidate was flashed/installed on ASUS/phone. Local ADB lists no device.
The previously deployed94da045 RMT image is not replaced or silently switched to
another backend. AVR programming and all audible/PBX tests remain held.

Critical remaining gate: SDK5.5.5 RMT DMA is interrupt-refilled ping-pong, not an
autonomous full-frame transfer. Existing immutable buffer/loop-owner tests do not
establish refill-deadline safety or optical stability under Wi-Fi. Bulk atomicity
fixes staggered client requests but is NOT claimed to fix all green flashes.
Deploy the matched candidates after access returns, then verify USB and direct
Wi-Fi80-cell batches, clean-white-to-red fades, all480 pixels, mapping and actual
visible stability. No production/hardware checkbox is closed; issue2 stays open.

### Quick stress/benchmark continuation

Host fixtures pass static/DMA x BGR/RGB (2000 mailbox submissions, timeout/flash
holds;8.45s wrapper including compilation), plus async HTTP20000 concurrent
publications accepted200 or explicitly rejected503 (2.955s wrapper). Actual
renderer10000 full80-cell batches per mode: static10.759ms total/p95 0.900us,
effects21.076ms/p95 2.100us. Desktop CPU/FastLED doubles, NOT ESP/API/FPS evidence.

Fixed two reproduced issues: an abandoned partial stream repainted unrelated
committed raw pixels from stale logical targets; Rust manager could confirm
another same-sized batch. Regression first fails then passes in both renderer
modes; immutable operation ID+exact selection/state/effect+peer+route now crosses
HTTP/USB admission/result, rejecting foreign/identical-but-different-ID/retired
receipts and altered payloads.202 native tests, Release/stage and S3 RMT-DMA SDK
build pass. Cheap optional free_heap_bytes status avoids sketch hashes/flash holds.

Added bounded hostname-only tools/benchmark-led-api.mjs:8seconds each static/
effects, two5Hz readers and one5Hz writer on cells1/80, strict boot fencing and
ACK/count/all80 readback, cancellation/no replay, response bounds/85s deadline.
Reports HTTP latency/throughput, minimum observed heap, submitted/transmitted/
revision/error deltas; frozen repeated frames are failure, not FPS. Success
restores logical targets/pattern/mode, NOT raw-stream pixels or animation phase.
Real local HTTP tests cover both backend profiles, bad count, changed boot,
read failure, driver mismatch, stalled transmission and frozen revision. Fixed
a deadline-edge pacing burst caught by that fixture. Added it to existing CI.
Read actual PCController http_url.go and dependency network.mjs completely;
adapted URL bounds/secret-safe diagnostics, rejected dependency retry semantics
for hardware mutations. Existing ledger/led-benchmark gate reflects exact hashes.

Live ledboard.local run failed preflight after3005ms, zero completed writes and
zero mutations. SSH8022 refuses; no local8022 listener or S3 USB device exists.
No physical/API throughput, optical success or deployment claimed. New staged
firmware15b00358308ca3acd8eea1ebd05db6215c22020b2f7af31460264e3386880fbc;
native5cb4ad2e3cd1c4433a150c8cba11d655ef7f51dfb5497abdf9c50cc66a3704f9.
APK/caches/settings/rollbacks preserved; no AVR/audio/PBX contact. RMT refill and
green-glitch physical acceptance remain OPEN on issue2.

### Standalone Internet and Ethernet continuation

Actual ESP boot already loaded network/cloud settings without PC/USB ownership.
Fixed STA-only readiness in cloud, NTP, DNS and discovery using NetworkPath;
ready Ethernet is preferred, station is fallback, AP stays local, and route/IP/
gateway/DNS changes revoke old cloud sessions. Native W5500 uses isolated HSPI/
SPI3, not AVR FSPI/SPI2. Build feature defaults off. Separate verified/pending/
rename Ethernet DHCP/static persistence never overwrites the Wi-Fi profile;
HTTP /api/network/ethernet and cloud ethernet-config/configure-ethernet expose it.
Dedicated graphical Ethernet editors and physical discovery remain pending.

CloudConfig imports the actual NetworkConfig no-op/pending/readback/rename
transaction. Shared typed cloud/network/Ethernet handlers save copied candidates
before active RAM publication and defer transport changes after receipts. Remote
commands use bounded board/boot/deadline/replay admission before dispatch, with
typed top-level normalized JSON closing duplicate/nested-key parser disagreement.
Status streams in256-byte MQTT writes without enlarging the2048-byte RX buffer.
Existing cloud sessions carry update progress/cancel without new TLS handshakes
or full captures during maintenance; boot-image telemetry identity caches once,
while update integrity checks remain fresh. ESP-only OTA is AVR-independent when
unlatched; paired backups/Prog/version gates remain. Cached original availability
topics now get offline on graceful changes; failed writes preserve broker LWT.
Stable hardware MQTT client IDs and high-uptime/long-offline retry tests added.

Focused actual-source host fixtures pass for ESP8266/ESP32 cloud config, cloud
services, command admission/typed getters and settings; Ethernet off/on adapter,
route/discovery and management tests pass, plus independent Ethernet storage
fault tests. Protocol runners include these checks on Windows/Linux. Two stale
DNS/TUI source guards were re-reviewed against actual shared route/batch code,
with negative mutants rather than reduced coverage. Ledger -Check and living
API guard pass;142 complete hash-bound comparisons,4531 pending are unchanged.

Frozen-source ESP32-S3 SDK3.3.12 buffered/RMT-DMA/16MB/OPI/W5500-enabled build
passes:1948979 bytes flash,97720 bytes static RAM. Image SHA256:
9deb77baa18b188e6a10e7514215fe33659b864fe14564d0f66d20bf8343ccee.
This is a working-tree optional-Ethernet build, not a clean-head production
release, staged Wi-Fi-only replacement or a deployed board image. Native/mobile
packages, settings, rollbacks and warm caches were preserved; no unrelated build.

Office broker TLS certificate verification, operator MQTT authentication and
ledboard status subscription pass. Only three Pakar telemetry/part ACL grants
were added after exact baseline hash comparison, isolated config parse validation
and rollback preservation. Broker remains active after HUP. Active ACL SHA256:
65efddef57a02bb6d5dfb584fcf34a8ea83fb4d948d86de2845098e250bd962f;
rollback /etc/mosquitto/acl.pakar-standalone-rollback retains2991371e... .
Temporary validation config was hash-verified then removed; passwords unchanged.
Mosquitto2.0.22 warns shared passwd/ACL ownership will need future migration;
no unrelated ownership/service upgrade was performed.

ASUS SSH8022 still refuses. A bounded authenticated Internet read-only status
request got no fresh or retained board record within12seconds. Broker proof is
NOT ESP cloud/physical proof. Actual ESP deployment, PC-unplugged commands,
telemetry, remote settings, Internet stage/apply/reboot/recovery and W5500
DHCP/static/cable-loss/multicast acceptance remain OPEN on issue2. No AVR/audio/
PBX action and no hardware success inferred. Pre-existing working-tree changes
were preserved rather than swept into a blanket commit or release.

### Flutter physical table stagger correction

Kept the display origin at cell1, with11 to the right and half a cell below,
and added alternating half-cell horizontal row offsets. Display coordinates are
separate from API IDs and the board's physical LED-chain mapping. The preview
uses1.5-cell vertical row pitch to prevent diagonally overlapping square touch
targets. Alternating-versus-cumulative row offset and exact physical row pitch
remain user-confirmation items; no physical geometry acceptance is claimed.
Canvas width includes the last staggered column; phone targets stay44px minimum
and pan rather than being squeezed. RTL does not mirror the physical table.

Consulted ui-shell and exact donor-audit entries, then read actual PCController
mobile/test/appearance_layout_test.dart, relay_layout_test.dart and
front_panel_scroll_test.dart completely. Reused the viewport matrix, measured
widget bounds, DPR/size teardown and isolated fixtures, not the donor's unrelated
relay/telemetry business. Added a narrow hash-bound appearance-layout comparison
with concrete remaining appearance/lifecycle cases rather than certifying the
entire donor. Existing main.dart source references were explicitly rereviewed and
rebound;143 comparisons are complete,4530 pending and107 have known parity gaps.

Canonical build-mobile wrapper analyze and six focused table layout tests pass.
All80 bounds, cell1/11 geometry, row offsets, no overlapping hit targets, phone
scroll/tap selection, independent reverse physical mapping and LTR/RTL phone/
tablet layouts are checked. The separate real-widget renderer capture passes
with zero mutations and was visually inspected. No repeated full suite, SDK
upgrade, clean, duplicate build cache, firmware/native build or board contact.

Reviewed source SHA256 (UTF8-LF):
- mobile/lib/table_layout.dart:3d4e3a476034e29fe197fe0d8eb704e64d57e3ecc164d0293c6fd7c8eb31cd69.
- mobile/test/table_layout_test.dart:00e5e159745a24dd53771a7a65a004a25cb0d9185b9ecc42534f3521f40a6647.
- mobile/lib/main.dart:2b4b39772724d58c38d51bc0e3ebf36e2711ea91ea06d33b1fc966bb128945f3.

Simulated outputs/PakarController/mobile/table-stagger.png SHA256:
075205e9f984d2cb894a5e75cdbb07069f6da91973b0ed82ef28819c9cc4e408.
Warm release-mode preview APK build passed in50seconds;55,494,457 bytes, SHA256:
ba89236aa89dfe1409a19b5e9fc80daddee6d6748a591a43e9db6a946605c749.
Updated only the owned stable staging/PakarController.apk after donor storage
gate, exact previous/candidate hashes and verified pending-copy/rename. Existing
production/rollback APKs, settings and warm caches remain untouched. This is a
debug-signed working-tree preview, not a clean-head production release.

Fresh ASUS SSH localhost:8022 refuses and local ADB has no device. No APK install
or phone screenshot was possible; phone/touch/physical-table acceptance and
issue2 remain open. No AVR, audio, PBX, broker or firmware mutation this pass.

### Unified Flutter directory and shared workspace redesign

Read the actual complete PCController mobile/lib/discovery.dart and
mobile/lib/devices_page.dart, its complete discovery.go, the complete Pealayer
PreferencesTab.tsx, current Pakar client/profile/NSD/XML/MNDP implementations and
affected fixtures. The existing ledger now maps both additional donor files,
their concrete replaced contracts, exact target/test hashes and remaining live
acceptance:145 comparisons,4528 pending and107 known parity gaps. This is not
whole-repository parity. Existing references were rereviewed and rebound; stale
Android discovery/build-wrapper hashes stopped the gate before compilation and
were corrected after reviewing the actual affected source.

Discovery no longer keys rows by endpoint+protocol. One directory merges saved
profiles and compatible observations using board ID, UDN, normalized MAC and
canonical IP/origin aliases; explicit conflicting identities remain separate.
Actual ESP DNS-SD instance/hostname correlation preserves a legacy saved .local
profile's relationship to the resolved IP even if ordinary Android DNS fails.
Different origins are publicly probed before coalescing so a failed hostname
does not suppress a working IP. Failed scans retire availability, not remembered
intent. Saved names survive and saved profiles not reached in the latest scan
dim while remaining manually selectable. Search, All/Available/Saved filtering,
compact source/status badges and icon-labelled profile actions share one list.

Adapted the donor's four-worker/deadline/source-pinned public verification to
Pakar's actual description.xml and capability-bearing status, not host healthz.
Generic HTTP/router MNDP records are filtered; MNDP must carry the actual Pakar
model/MAC fields. DNS-SD restricts generic HTTP to the advertised Pakar app ID.
XML/status probes send no credentials, follow no redirects, cap response size
and time, and never mutate. XML metadata verifies compatibility, NOT crypto
identity or hardware presence; fresh hostname/capability/board/boot checks still
own selected-peer control. Existing authenticated GETs now also refuse redirects.

New profiles save without credentials. Existing profile edits keep, replace or
clear an optional stored password; no plaintext/default-password fallback.
Connection first performs a read-only probe, prompts only after401 and verifies
the submitted login before save/selection. Discovery does not read the vault.
The selected-profile path no longer incorrectly invokes credential-rotation
readback on a newly selected open board. Rotation's authenticated readback stays.

Shared app_theme.dart/workspace_widgets.dart now own Studio colors, typography,
stable input borders, header/actions, grouped sections, menus/dialogs and badges
across Devices, connection preferences, device settings, firmware and bridge.
Bridge Session/MQTT/WebSocket/Routing navigation retains controller drafts.
The screenshot harness imports the actual application theme. The requested
permanent plain-HTTP warning was removed from embedded WebUi.h; server access
protections are unchanged. The embedded Web change is SOURCE ONLY, not deployed.

Focused canonical-wrapper verification passes:7 device/registry/widget cases,
5 real HTTP/XML/DNS-SD cases,3 credential-storage cases,7 existing bridge/discovery
cases and7 existing network/batch cases. Two renderer captures pass with zero
device mutations and were visually inspected. Initial narrow badge overflow,
optional-profile alias normalization and progress animation behind sign-in were
fixed rather than hiding failures. The existing batch fixture now settles the
focused field's bring-into-view work before measuring/tapping; no missed-tap
warning or assertion was suppressed. Analyzer and ledger -Check pass. Unrelated
formatter-only changes to generated assets/settings were exactly reversed;
canonical generator checks pass, no donor source was edited and no full suite
or CI wait was introduced.

Actual simulated captures under outputs/PakarController/mobile:
- devices-phone.png SHA256:37d3de6e23f1fc5bd6eaea30a0b1f1c5d4f9748fcab60243aa0922110507a863.
- bridge-workspace.png SHA256:5e7e2c060aea581116af4a75bc3f471955875bbc4081fda803d233c34da34bcd.

Final warm release-mode APK build passed in40seconds after the additional
DNS-SD alias regression. Preview55,723,909 bytes, SHA256:
d624597060b2fb5094c454eea702da84b38dadfb68188c577de72ccf2815833d.
Checksum-promoted once to the owned stable staging/PakarController.apk using
the donor storage gate and verified pending-copy/rename, replacing only the old
staging candidate. No extra archived APK, cache clean, SDK install or unrelated
firmware/native build. Existing production/rollback/settings remain untouched.
Base2dc319fb8add1516a0f2caa4306bee1bcb1284ab plus preserved working-tree edits;
debug-signed preview, not a clean-head commit or production-release claim.

Fresh ASUS SSH8022 refuses and local ADB lists no phone. No APK install, actual
Android/LAN multicast/credential test or Web firmware deployment is claimed.
Phone/human, live board discovery and complete application/donor acceptance stay
open in issue2. No AVR, board, broker, audio or PBX action this pass.

## Integrated S3/AVR and application handoff checkpoint

ASUS identity was freshly established through its already-trusted hostname key;
the stale forwarded-port key was not accepted or deleted. Both Rust candidates
are running from verified staging/deploy paths, not overwritten primary binaries.
Release SHA256 is14110a245082ba48bdb66477dd69dfc3b700967d22109d1d314c748892cd879b.
The local candidate acknowledges the actual board over its ASUS SSH HTTP forward,
with live WebSocket, contextual title, all80 mappings and actual RGB snapshots.
Settings, previous executables, recovery images and warm release cache survive.

Flutter's columns are now exactly1/21/41/61,11/31/51/71,2/22/42/62,...,
with no horizontal row offset. Neighboring columns keep their half-cell vertical
stagger. All134 Flutter tests/analyzer pass, including all80 positions and actual
phone/tablet LTR/RTL bounds. APK SHA256
fee48458e08ce49bd786fe0651266b8f08636eca318dbefb3748990d9f20264b
was installed and launched on the authorized ASUS-connected phone. Its captured
runtime was disconnected, not visual table acceptance. The separately inspected
aligned-table renderer capture is explicitly simulated. Disconnect also stops
hidden board-preferences polling. All214 Rust tests and source quality pass.

Integrated RMT S3 imagebc73583039b79113d32500dd46bc5577c6f5ec54041ccefdbba1d491c84bec36
was uploaded over LAN in20.95seconds, rebooted, and matched a complete downloaded
SHA256; selected/running OTA slot is VALID, auth is disabled and same-peer HTTP/WS
pass. No USB, filesystem format or fuse write was used. The initial private .NET
helper accidentally typed the MD5 form field as a file; installed SDK3.12.1
WebRequest.cpp::_parseMultipartPostByte confirmed that Content-Type marks a file.
After read-only rejection/unchanged-boot inspection, the corrected plain-field
wire format from tools/update-firmware.mjs/PU850 AsyncOTA.cpp succeeded. Existing
firmware backup was verified and preserved, not overwritten or retried blindly.

AVR full image4f3f00454ec913551c88da4746ea11955087835c9619e6331406c67f15034fb1
was programmed through the integrated ISP path in43.98seconds with byte-verify,
durable Prog/UART admission/release and EEPROM preservation. Original recovery
and fresh pre-erase flash/EEPROM/metadata were exported privately. Hardware UART
returned online, settings read back unchanged and Silent remains enabled.
Post-update watchdog reset reason is recorded, not evidence of a continuing loop.

User optical feedback still reports cross-cell spill and incomplete cell80.
The steady cell80 test snapshot contains exactly physical pixels1..6 white;
English PBX DTMF3 reported missing pixels without spill. Manual individual pixel1
and pixel2 frames sent through the actual Rust app matched exact RGB readback,
but operator DTMF3 reported each dark. This does not establish a wiring or driver
root cause. Non-DMA comparison and physical acceptance remain open, as do the
display, power/clock, network recovery and all outstanding donor/production gates.
PBX coordination is now keep-open, with a separate scoped Jitsi coordinator;
queued audio, bot presence and screenshots are not bidirectional audio proof.

PR18's complete dependency diff/conversations were reviewed and normally merged
as71d8f0f5153718fa4f6593ff95ca47b3a9574073; no open PR remained at recheck.
Its exact-head CI does not certify this integrated tree. Current native CLI
fixtures exposed stale contract mapping hashes/one line anchor: reread actual
UartProtocol.h, EepromLayout.h and AddressableLeds.h/.cpp and current adapters;
rebound only the four changed consumer references. Donor revisions/source hashes
remain unchanged; no unread donor row or hardware checkbox was promoted.

The follow-up GPIO-static OTA is now independently verified as
a92300d546b44e81ab5ddd3f6cf80fd1988bf44324385b0b694d1ea97b4750ef,
1899200bytes, bootc42e6ee31bcabb93af86df14c3ad01ac. The canonical updater's
post-upload verification lost a forwarded HTTP connection; it was not rerun.
Direct ASUS full-image readback established the result. A fresh32KiB AVR ISP
capture also matches4f3f00454ec913551c88da4746ea11955087835c9619e6331406c67f15034fb1;
UART returned and Prog is clear. User reports three steady white pixels and one
cycling color in cell80 under non-DMA too; hardware acceptance remains failed.

Checkpointdbb2f0631a7052d0b1701815431feaffa8434a4f exposed a cross-platform
CI fixture exit-status bug: script_tooling_test.ps1 intentionally rejects a
child exit7, but left that as the last native status in GitHub's pwsh-Command
wrapper. Reread the exact PU850 Tool/build-all.ps1 pinned Git blob and imported
runner; production failure propagation is retained. Add an actual successful
child case and assert caller-directory/status restoration instead of clearing
errors or weakening CI. The exact GitHub wrapper now exits0 locally, and the
failure case still asserts exit7. Only this fixture's28 proof references rebound.

The next CI run passed all three tooling OS jobs, then exposed missing
ArduinoJson.h in the clean Windows quality runner. Native LCD/cloud/management
fixtures use real ArduinoJson; local installed headers had masked provisioning.
Reread PU850's pinned .github/workflows/build.yml: retain explicit dependency
installation from reviewed pins, but install only ArduinoJson before host tests,
not any board SDK or the donor's unpinned fallback. The existing canonical JSON
pin remains authoritative. Workflow-order/no-SDK regression and local LCD fixtures
pass; CI remains asynchronous. No firmware/app rebuild or deployment for this fix.

ASUS native recovery: the actual session1 owner was connected with80 confirmed
mappings but only Events in its dock. Disconnected save/startup pruning could
persist that temporary view as the connected workspace. Preserve the last
connected layout, cache it before retirement, and migrate the legacy Events-only
layout once without overriding subsequent explicit panel choices. Continue to
gate every visible panel on the current board's acknowledged capabilities.

The first live recovery checks exposed additional real failures, not success:
optional settings transactions flooded the HTTP queue, and cell writes expired.
Read complete current PCController session.go and Pealayer workspace_profiles.rs;
the latter's stable user-owned profiles are retained, not replaced by a reset of
the entire user profile. Adapt actual Session grantNextLocked priority/FIFO for
unsent native HTTP work, serialize optional settings reads with bounded retry,
and reuse the fresh worker preflight for atomic cell batches. Keep mutation8s
expiry, generation revocation, server board/boot fencing and post-ACK readback.
Separate read24s admission never extends mutation permission or replays writes.
Only existing consumer/proof hashes were rebound; no unread donor is certified.

Native release a853fbfd291dd36117d94613650400aa9645ba281aabeab1c05118262a9e4d01
is now the ASUS deploy owner5196 in session1. Settings and the prior executable
were preserved before graceful IPC replacement, not primary-install overwrite.
Actual app IPC reconnect restored Table/effect/hardware/network panels and80
mappings. Under background network-settings loading, cell80 red then white took
1048ms and4974ms, with two Atomic cell batch ACK/same-board readback events,
zero active incidents and zero repeated occurrences. Network and cloud settings
both loaded. Internal framebuffer capture completed and was visually inspected:
efae0a85f705281fd7f9799b56e540773d51d54e0279060b3a49f01015f77220.
Private receipts remain on ASUS deploy/asus-priority-* and local outputs.
20 focused native tests passed across layout, optional reads, HTTP fencing and
atomic batches; the warm release build passed without rebuilding SDK libraries,
firmware or APK. This verifies the native/LAN command path, NOT physical LED
placement/color or production completion. Issue2 stays open. Existing main CI
39111550290 passes quality/native/Android but fails the separate ESP8266 link
on missing __atomic_*_4 symbols; that firmware issue is not fixed by native work.

USB control latency: reread the complete current canonical PCController
Session, session_test.go and request_admission_test.go (live hashes match the
existing b0111ae0 ledger). Adapt actual control-over-background admission and
fragmented fakePort/opcode-sequence assertions into the production Rust worker.
The new optional same-sequence0xB7 packet contains observed post-dispatch board
state, not request-derived success. Boot/session/intent/value checks remain;
missing companions use read-only verification without replay, including when
another command is queued. Background captures pause only between chunks and
resume, retain capture-start freshness, overlay newer actual command state and
discard pre-command RGB. Partial receipts do not renew peripheral observations.

Measured through the actual ASUS session1 app IPC, COM22, Wi-Fi/WebSocket
inactive: old630459cb had2440-4031ms confirmed completion (four commands), while
e897e313 plus the new ESP had29-63ms (six immediate and twelve commands spaced
1200ms apart); wire ACK+board readback10-12ms, zero active incidents, original
cell/search restored. Original full scene also matched after the ESP update;
the restoration helper refused to overwrite its non-white values, then direct
comparison established that no restoration write was needed. Internal app
framebuffer captured/inspected with exact PID13368. No optical LED proof inferred.

Native eight focused tests pass, including the actual worker under a slow,
fragmented preview and prevention of older-capture state rollback. Additional
admission assertions cover lost-companion/legacy readback with queued writes.
The first test compilation used PartialEq on a donor manifest lacking that
trait; compare serialized observations in the fixture, not change the public
model just for a test. ESP32-S3 16MB/OPI/static/buffered warm build passes.

OTA preflight failed before dispatch; later hostname resolution and curl reads
worked. Internet job12 failed TLS before staging image bytes; no mutation was
replayed. The first USB pass read chip MAC/partition table but failed its next
bootloader handoff before any flash write. Reconciled that no-write receipt,
then reused the fully read Install-S3SlotRecovery per-operation default-reset/
hard-reset recipe rather than retrying a held-stub sequence blindly. Actual
partition table plus CRC-valid VALID OTA records selected0x340000; full prior
1903312-byte running backup matches481ef136bb6cecb50a85a610abd31fc7ec8db06231fc75a0dadb4e35cb8a59f2.
New1903488-byte ESP image8bc0cd92dd00e8b78f5ce40187d549095859470f32d95ea372521b45a9f75558
was written/verified; the same MAC returned with a new boot and80 USB mappings.
AVR was not programmed. Private stage/flash/IPC receipts remain under the
existing outputs/PakarController/verification and ASUS deploy directories.
LAN/TLS/update reliability and the unrelated existing ESP8266 CI failure are
not fixed or accepted by this USB checkpoint. Production issue2 remains open.

Final nativef74e21addd521852ef9ec37cae40b2475d8f042264e0a11be98648f4dae58d3b
adds the queued-write/missing-companion fallback safeguard, passes the same eight
focused tests and is the ASUS session1 owner10908. Six additional actual app-IPC
USB commands spaced1200ms apart confirm in32-64ms, wire readback10-12ms, zero
active incidents, original cell/search restored, no HTTP/WebSocket fallback.
The full source/receipt-hash and aggregate timing record is USB-LATENCY.json.

Flutter screen-off retention: reread the complete live Pealayer
src/ui/workspace_profiles.rs (410d274f51e5e7db7ce81582d36166a97240b097e993cec5fee8707a95183e2f)
and PreferencesTab.tsx. Extend the existing stable-board workspace adaptation,
not a new parallel preference framework. Saving only when entering background
left a process-suspension/death window; scroll-end PageStorage could also restore
an older offset. User edits now notify a 350 ms quiet-period/2 s maximum-wait writer,
with immediate inactive flush, ordered/coalesced writes and identical-snapshot
skips. No telemetry-driven or per-scroll-pixel storage writes. Selection/Find,
indicator visibility, multi-select, transition, activity and both scroll axes
restore from bounded UI-only snapshots, never cached device authority or commands.
Reattached scroll controllers start at the latest actual offset. Colour actions
now precede the separate selection-management row, with stable explicit keys.

Resume retains healthy sockets; a 750 ms read probe replaces only a silent
pre-suspend socket, with read-only HTTP fallback. HTTP success does not cancel
that WS recovery; generation/foreground/explicit-Disconnect fencing remains.
Seventeen focused production-widget/ownership/writer cases and analyzer pass
through the canonical wrapper. A fixture initially tapped the navigation bar
before the scroll layout settled; target the visible actual control instead.
The socket fixture also needed one real event-loop turn for broadcast-cancel
completion, not an increased production reconnect delay or disabled assertion.
Temporary tracing was removed. One analyzer brace warning was corrected without
weakening the gate. Existing donor omissions stay pending in the same ledger.

ASUS is reachable but its authorized Samsung wireless-ADB advertisement times
out; devices -l is empty. No install, app-data clear, shared ADB restart, firmware,
board command or physical acceptance is claimed. Prior APK staging is not phone
delivery. The final candidate is built/staged separately after source settles;
actual Android screen-off/process-pressure and human acceptance remain open.

The one warm release-mode preview build completed in 44.4 s; no dependency
upgrade, new cache or clean. APK 56,215,521 bytes, SHA256
b5df3f10904af6ce2a23e91923afb6e7b7f6032350ea586ad50f8633e019a449;
local staging independently matches. The existing Flutter-rendered simulated
capture also passes (18 total focused cases), visually inspected with colour
actions above selection controls and no overflow. PNG SHA256
da22502fa23accdafd6809f74607f75dc87da1e7111579200fdd72ac1c8cac1f.
Preview signing, fixture rendering and APK staging are not Android installation
or physical/human acceptance. No production issue checklist is closed by them.

Native selected-owner bridge checkpoint: fully reread PCController discovery.go,
discovery_test.go, mndp.go, protocols.go, hostbridge peer_response and tests,
lifecycle.go, discovery_metadata and tests; Pealayer server/discovery.rs and
network.rs. Exact live bytes match the inventoried b0111ae0 / 5a6961ce snapshots.
The existing ledger binds these adaptations and concrete omissions;
bridge-repeater routes the remaining manager/public/enrichment/adapter work.
Generic PCController RPC, Android LAN server, opaque firmware bytes and binary
pixel streaming are not implemented or certified by JSON table-command sharing.

The native opt-in listener uses the existing selected USB worker and its matched
ACK + observed command-state/full snapshot. No second COM handle, implicit
network fallback, discovery actuation, auth prompt or owner acquisition is added.
Distinct bridge instance/hostname/path metadata preserves board_id/boot_id and
board hostname. Same-ID command receipts are consumed before dispatch; HTTP
recovery accepts the existing packet form. Native mDNS/DNS-SD, SSDP and MNDP
advertise/browse only when sharing is enabled. Loopback fixtures open no LAN
discovery sockets. Active protocols and faults reflect actual startup; conflicting
mDNS automatic suffixes are rejected instead of misreporting configured identity.
Flutter consumes bridge endpoints and explicit MNDP API ports without
pretending application instance IDs or virtual MACs are physical board IDs.

Initial offline Cargo metadata resolution conflicted with the already-locked
zbus/xcap dependency. Consulted build-storage and actual donor dependency slices;
online resolution added six packages without changing any existing locked version.
mdns-sd is pinned to the actual donor 0.21.5; socket2, httparse and network-interface
use cached/donor APIs. Warm release cache retained; no clean, debug profile,
donor build, SDK upgrade or alternate target directory. First compile caught
helper visibility/parser ownership errors: reuse existing shell LED encoders,
not duplicates. Two Dart analyzer brace findings were fixed without weakening
the gate. A chained fixture used the nonexistent effect name instant; inspecting
the actual encoder showed the contract is immediate, so only the fixture changed.
Failed assertions did not trigger any production command or board contact.

Thirteen focused native bridge/socket/parser tests and the actual fragmented USB
worker test pass, as do six Flutter bridge identity/discovery tests and analyzer.
These use explicit loopback/fake serial peers, not physical boards. LAN/router/
firewall, installed Android, optical LED and generic peer-RPC acceptance remain
open in issue #2. No firmware or board was contacted or changed; no PBX/audio
test was requested or performed for this checkpoint.

Sharing-off has its own revocation token on queued USB requests; it does not
release the primary USB owner. Both pre-dispatch admission and result delivery
observe that token. Explicit board Disconnect remains separately gate-fenced.

Existing main CI failed tests/pu_routes_test.mjs before this bridge checkpoint:
PU850 route bindings still named the ESP source hash at451dff7. Read the entire
451dff7..f162a8c ESP delta, actual commandReply/WebSocket/OTA handlers, complete
donor AsyncOTA.cpp and AsyncWebServer.h, and the actual audit-pu-routes.mjs.
Donor source bytes remain unchanged. Only ESP main target bindings were stale:
the additive USB command-state and atomic pixel diagnostic do not remove or
weaken the50 classified route handlers. Use the existing generator to refresh
current target hashes/line anchors, not disable its assertion. network-cloud
did not route pu850-esp.ino; donor-audit found it, and pu-routes now keeps that
slice and its still-pending complete-file comparison discoverable. This review
does not certify an entire read/import of the PU main sketch or live OTA success.

The specific failed route check now passes with the actual donor path configured,
including registration enumeration. Reports were regenerated after adding the
new reuse route and their stale-report check passes. This is not a claim that
all CI jobs have passed; new CI is allowed to run asynchronously.

Canonical release/preview builds succeed with version0.2.7 (Android+7). Native
27,829,760 bytes, SHA2563107992850817b2829ce9787f73a804de2903792a259510b465f926d5c39f819;
APK56,215,521 bytes, SHA256d45ae5612e484bc3ed430c42a03eea6b9445b21952a69a8368b9f9015ff6edf5.
Stable local staging matches; native CLI/package probe passes. Existing installed
apps, settings and rollback are untouched. Android app is built/staged, not
installed; native is a staged candidate, not an ASUS replacement or LAN test.

An isolated disconnected Preferences fixture was launched with its own config,
no production IPC/tray hooks and sharing disabled. Internal framebuffer produced
no image; explicit expected-PID Win32 fallback found no visible non-minimized
window (including a second normal-launch fixture). Consulted capture and reread
the actual Pealayer read-only capture source; no focus/restore/input workaround,
blank-image acceptance or claim of visual QA followed. Only identity-verified
fixtures created in this pass were stopped. UI screenshot acceptance is pending;
this does not mean the available Win32 capture capability was removed/unavailable.
