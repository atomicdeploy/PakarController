# ESP32-S3 migration and commissioning

**Current priority:** fix the partial/glitchy RMT-DMA rainbow and periodic pause,
retaining the integrated services, saved scene and AVR UART/ISP. Physical matrix
feedback remains required. Historical observations below do not establish the
current board's health.

The ESP32-S3 target builds the **same integrated application** as the ESP8266:
Web/API/WebSocket, table effects, cloud clients, storage, self-OTA and AVR ISP.
`firmware/esp32s3/esp32s3.ino` includes the shared sketch; its individual source
wrappers retain separate translation units. The ESP8266 target remains available
for a later, separately configured device. Do not attach two controllers to the
same AVR UART/ISP signals at once.

## Current rainbow and LAN recovery

### Current ESP0.2.8 Internet update and remote panel

User authorized the prepared0.2.8 image and temporary panel access change.
Actual ESP esp32s3-907069eac800 now runs2239504B image SHA256
`23e660ec3dd4d7a89ba6370c7c6eb1075cb0ed26e86a8d77f2b75f119c40e731`,
MD5`3ac8bff19cb935008dcc61dc81c78583`, boot
`a193c8d230f0bd24efb97f4c57cb35fb`. Independent35-range running-image readback,
boot integrity and selected/running VALID app1/0x340000 passed. Profile remains
16MB/OPI/buffered/RMT-DMA/verified-ISP/BLE-disabled/optional-Ethernet/status-GPIO48.
The existing prepared build is deployed; newer PU reuse source is not in it.

Internet manifest SHA256
`ff49acbf038a48d9e3be015b198c784a4ac34888abf284cebe9c67a530ad6597` at
`https://office.digitalogic.ir/pakarcontroller/firmware/esp32s3/23e660ec3dd4d7a89ba6370c7c6eb1075cb0ed26e86a8d77f2b75f119c40e731/manifest.json`
was received/checked by actual stage22, then separately applied. Stage21 failed
TLS allocation; two local uploads independently aborted737292/1188264B without
reboot. Verified old2193760B backup SHA cb1f18f16c62e369e52120ec0986bd30d1b5d38e71204216c47aded721a6de0f
is preserved on ASUS. Temporarily closing MQTT freed43KB for HTTPS; MQTT was
restored before Apply and is again operational. Automatic memory coordination
and reliable LAN OTA are still unresolved, not fixed by retrying blindly.

HTTPS https://ledboard.digitalogic.ir and Internet WSS received actual same-boot
state. A remote same-state cell1/white command was correlated, acknowledged and
read back in294ms with unchanged scene hash. Mode cell/effect none/brightness255,
all80 saved states and output configuration survived. This is not optical output
or all480 success. The command is accepted by0.2.8 default policy; no downgrade.
The canonical tool retained final LAN-WS interruption/exit1 after hash validation;
independent full hash and Internet WSS verification are separate real receipts.
Job22 phase complete still has completion_verified:false: reporting fix remains.

AVR stays0.2.7, untouched and no longer admitted by0.2.8 policy. Companion update,
AVR peripherals, physical RMT defects, autonomous power restart and soak remain
open. This is remote LED control handoff, NOT full two-board production release.

### Unresolved partial rainbow: timing candidate and pad capture

The user still reports only the first23 cells working. This is NOT fixed or
physically accepted. Fresh hostname-first ASUS readback still identifies the
same0.2.7/ff6da73... boot and advancing RMT-DMA counters, not later-pixel success.
Whether "first23" means cable order or logical IDs was explicitly asked.

Two fresh `status?pixels=1` snapshots contained all480 non-black RGB values,
including changing pixels139 and480, with80 cell mappings. Thus the published
RAM frame has no missing/black tail; this is not evidence of correct wire output.
Internal heap was67532..68228B free but its largest block only31732B.

The complete eight-page WS2811V1.4 donor datasheet was extracted AND visually
reviewed. Current source moves RMT from250/1000ns zero and600/650ns one to
300/950ns and625/625ns respectively; both retain1250ns bits/300us reset.
This removes exact/near timing-limit choices, not a proven root-cause diagnosis.
The separately selectable static backend keeps its existing cycle profile.

`POST /api/led/output` with only `capture=true` arms one on-demand digital GPIO4
RX-DMA capture of the next existing immutable matrix frame. No extra wire,
pattern change, output toggle, mapping change, persistent write or automatic
capture loop. Admission still uses the ordinary auth/release/board/boot gates.
Read `led_output.driver.signal_capture` in ordinary status: state0 idle/1 pending/
2 capturing/3 complete/4 failed quiesce; match remains null on pending/error.
Compare all11520 data bits, first mismatch bit (zero-based), pulse ranges and
data-LOW gaps to the actual TX payload. Initial idle/final reset LOW are excluded
from data timing checks; this does NOT verify280us reset duration, analogue
voltage at strip DIN, downstream reshaping, power, physical controller count or
optical output. SDK encoded-symbol totals alone remain insufficient.

Capture allocates an8128B internal RX-DMA ring only on request and compares
recycled1016-symbol chunks incrementally without copying/allocating in the ISR.
A contiguous46112B capture was rejected against the actual31732B largest block;
PSRAM is not substituted for unsupported RX-DMA storage. TX refill retains its
witnessed priority3; RX uses the SDK GDMA allocator without that TX-only override
(`intr_priority` is not used by the SDK's DMA RX allocation). Capture adds IRQ load:
an incomplete/mismatched observation can include capture overrun, so it is NOT
automatically a TX-fault diagnosis. Buffer bounds/partial EOF are validated.
The ring is freed after bounded capture/stop. Allocation/receive failure is reported without stopping
healthy output; failure to quiesce RX retains memory, stops output and refuses
flash rather than risk DMA use-after-free. Worker fixtures exercise full capture,
corruption after23 six-pixel groups, malformed chunks, timeout, no-memory and
failed-stop behavior; the independent decoder tests recycled chunks, short
frames, internal gaps, LOW-first symbols and idle handling.
Host worker/static/RMT/RGB/BGR, exact-IDF ring and renderer/persistence checks
pass. Final canonical16MB/OPI/RMT-DMA/verified-ISP/BLE-disabled/optional-Ethernet/
status-GPIO48 SDK build passed:2231680B application, sketch2231535/3342336B,
globals116436/327680B; SHA256
`95cc8740d8c5289dd4553438c8cf9560d3b046eb549ba542808a03c445d2b8fa`,
MD5`ee0cf2d7daa40596c8dc624cdb900742`. The actual image contains
`partial-rx-dma`, `gpio-digital-not-optical` and the capture admission error;
it is not the superseded contiguous-buffer candidate or a failed link output.
Deployment and real capture/optical acceptance remain separate and unperformed.
No ESP, AVR, app or wiring was changed during these read-only observations.

### Onboard status pixel source checkpoint

This board's existing persisted configuration is GPIO48, confirmed/enabled,
brightness64; it was freshly read through trusted David-ASUS and ledboard.local.
No external wiring is needed for that onboard pixel. GPIO4 remains exclusively
the matrix data output; no AVR, ISP or matrix settings were changed.

The new semantic profiles retain the actual PCController renderer arithmetic
and Pealayer direct-output mirroring (no client double dimming):

| Observation | Onboard indication |
|---|---|
| Startup | Blue breathing, eased state transition |
| Ready on a network | Steady green |
| Intentionally offline / USB | Steady teal; not a network warning |
| AP only | Cyan breathing |
| AP plus an available network path | Slow green/cyan color fade |
| Station connection pending | Amber pulse |
| Required AVR unavailable | Amber double blink |
| Update/programming | Purple pulse; measured progress raises the brightness floor |
| Verified update success | Brief green pulse, bounded by the existing lifecycle |
| Shutdown/reboot preparation | One-shot purple fade to held black |
| Configuration warning | Slow amber blink |
| Access/update fault | Red double blink, highest priority |
| User disables status output | Driver-accepted black |

Priority is fault, shutdown, active update, startup, warning, required-peer loss,
verified success, then connectivity. Static/breathing/blinking/color-fade/
fade-out/pulse/double-blink effect IDs are 0..6; `effect_name` is additive in
status/API/WebSocket observations. Existing settings remain `/api/status-rgb`
(enable, brightness, confirmed pin), independently of matrix brightness/output.
Exact repeated states preserve phase; failed driver writes do not advance output
revision and are retried no more often than250ms. No animation writes settings.

Engine/config host tests and native Check pass, not physical light/load proof.
The live ESP and AVR remain0.2.7. The candidate0.2.8 requires coordinated peers
under the default-on admission policy; rollout confirmation was requested rather
than disabling that policy or silently making the AVR incompatible.

Source20a242bf38c8a5d2b70dfbde6c9460574a2bc845 is actually installed:
2193760B, SHA256`cb1f18f16c62e369e52120ec0986bd30d1b5d38e71204216c47aded721a6de0f`,
MD5`a3c0e540730958c561d4da6ac8dd686b`, boot`ff6da73b47d6fd790fc1eb36c491d6ef`.
Actual ROM MAC/CRC-bound seq23 VALID app0 were reread before a33.8s USB write
of application0x10000 only. Saved settings, rollback app1, partition table,
bootloader, otadata and AVR flash/EEPROM/fuses were not written by recovery.
The subsequent hostname-first Wi-Fi check verified all34 exact running-image
ranges/SHA, VALID selected/running app0 and same-peer WebSocket49ms.

The preceding ec2d497... candidate WAS delivered over LAN OTA22.779s, but failed
acceptance when MQTT exposed repeated loopTask stack-canary panics. Read-only
COM22 panic capture plus exact-ELF address decoding identified nested status/
full-telemetry stack exhaustion. This failure is retained in the private original
network-delivery receipt and cloud-panic capture; do not call it a successful stable
network deployment. The corrected stack-readback verifies image/WS but separately
fails MQTT readiness before host clock setting; the final timing receipt establishes
the later connected observation. Recovery keeps the same renderer and queue1, with producer-
owned serialized storage and isolated scratch lifetimes instead of raising SDK
stack limits. Actual Xtensa frames: loop2976->592B, LED submit1488->48B,
publishTelemetry1408->80B; fragment/stream/message helpers own only their respective
1392/1376/1360B frames. Focused worker and both-platform cloud fixtures pass.

Actual refill receipt is source71/core1/vector27/level3. In the final16.418s
read-only observation all26 samples retained the same boot and connected MQTT;
publishes10->13,775 transmitted-frame increments, error/failed0, no zero-frame
sample intervals, longest inter-sample gap516ms (previous1086ms). Read median244ms;
the3651ms first read includes hostname resolution and is NOT an LED pause. Global
cloud maximum2166ms includes initial TLS connection; individual blocking socket
calls are still a limitation, not a completed nonblocking-worker claim. No
two-second counter pause appeared in this bounded window; optical waveform/
all80-cell/long-soak acceptance remains open.

MQTT is restored/enabled and was actually connected during the final observation.
NTP had not synchronized after the cold USB restart, so the existing UTC setter
was explicitly used with ASUS host time to exercise TLS. This is NOT autonomous
NTP acceptance; investigate resolver/UDP123 before a standalone handoff.

Actual ASUS LAN wire replies verified mDNS A, DNS-SD PTR/SRV/TXT, SSDP root/basic
responses, MNDP identity/MAC/IP, NB name and full121B NBSTAT. Windows nbtstat also
lists LEDBOARD<00>/90-70-69-EA-C8-00 on ASUS Ethernet and Wi-Fi interfaces. ESP
itself remains on Wi-Fi+AP; optional W5500 is built/configured but DISABLED,
uninitialized, with no claimed wired/LLDP acceptance. Router probes found no IGD
descriptor; no router login/mapping/firewall changes were made. TFT stays backlog.
Native/Flutter addressing packages were built, not deployed this pass. Read-only
native IPC from SSH session0 was unavailable; that is not an authoritative owner
health result and no GUI fallback/replacement was attempted.

## Historical Wi-Fi-first checkpoint

### Earlier image and short runtime checks

Follow-up source`cc8e71112053e227810cc14f154b2db78b8d05c2` is now deployed:
2074112bytes, SHA256`2909fdb06411d3ea935a29d16889a13cd1b1fa6065061da66741cf965cd49e36`,
MD5`2bddfa05605eaee7f9028befa4aedba3`, boot`647bbf1e7608295ec5ce6de2e97ba20e`.
The canonical S3 build retains buffered UART/16MB/OPI/rmt-dma/verified ISP/BLE-off;
sketch2073971/3342336B, globals104016/327680B. One new local Async OTA transferred
in19515ms; full actual running SHA/VALID selected-running slot/new boot/auth-off/
WebSocket verification completed in60022ms. Failed Internet job20 was explicitly
acknowledged against its ORIGINAL053233... image, not overwritten or applied.
MQTT alone was temporarily paused/restored, credentials and settings preserved;
cells unchanged/output disabled/AVR memory untouched. Private delivery receipt
SHA256`98547aaf9d9d1e44dccba8846e24d4c7cb36849e659b20e14d5f3abc5b62cdca`.

Actual concurrent Telnet queries now return `rmt-dma` and `ledboard`, with ANSI
prompt and negotiation, after the stale-clock lease fix. Seven bounded HTTP reads
passed: status387/523/489ms, telemetry497ms, shell480ms, full223213B WebUI732ms,
description XML684ms; same-peer WebSocket53ms. NTP is now observed synced, MQTT
connected, station+AP active, AVR online and TM1637 ACKtrue. RMT admission/accounting
is healthy11521/11521/failed0, but output is disabled: this is not an LED-load soak
or a physical waveform/color result. Service receipt SHA256
`1b0e5c3598fa6b983b2132d610a041185fcc9a3a9d864a48093f7057632466fa`.

The unchanged ASUS native owner20976 recovered onto THIS final boot: Wi-Fi live,
80 mappings confirmed, AVR/TM acknowledged, zero active incidents and no pending
mapping. Final status receipt SHA256
`93f630410b5625e963617979e544cb6eff6f10af9d3dc30dbdbe9d1c5dbb46bb`.
An earlier same-app online checkpoint has an inspected1920x991 actual egui PNG
SHA256`80cdaa2ccc2b1e2fcdfca1be0ca6e2ea3bca5131841571f56a1dca0ef782f4b5`.
The private capture runner was corrected using the actual Pealayer retained-
Process/async-output/bounded-wait mechanism; earlier ampersand-launch failures
were not proven production IPC outages. No GUI input, forced restart or reconnect
mutation was needed. Settings, selection, saved layout and rollbacks remain.

The final raw image and manifest are published atomically without replacing any
prior release at `https://office.digitalogic.ir/pakarcontroller/firmware/esp32s3/2909fdb06411d3ea935a29d16889a13cd1b1fa6065061da66741cf965cd49e36/manifest.json`.
Public HTTPS independently verifies419 manifest bytes/SHA256
`228c7cba39527aec134849f6412020057e90d05f910e2690626d8e47356a4ea5`
and the complete image SHA. Publication is not a new ESP Internet fetch/apply
test. TLS allocation/cloud coexistence and power-removed restart remain open.
Android APK is staged, not phone-installed; no new physical LED confirmation or
AVR flash/fuse test was inferred. Production issue2 remains open.

### Guarded LAN deployment checkpoint

The user authorized latest deployment and short checks. The corrected RMT image
from f6d3fce is now actually running, not merely compiled:2073872bytes,
SHA256`0532332175ea496216ab8a125e21a6ea442f98acc2e316cbc1379ddc9d190fc8`,
boot`37992b7973b2501b3bccde7292b7c96d`. Guarded LAN Async OTA took23158ms;
total48719ms includes new-boot/full ranged SHA256/VALID selected-running slot,
auth-off and same-peer WebSocket verification. Saved80 cell states remain;
output stays disabled pending physical all-dark feedback. Actual driver reports
250/600/1250ns, cache-safe refill admitted,11521/11521 accounted symbols and
failed0; those are NOT scope or optical measurements. AVR memory/fuses untouched.

Old job19 was explicitly cancelled rather than applied. Its replacement Internet
job20 failed before image bytes with TLS-32512 (installed SDK SSL allocation
failure). That failed diagnostic is preserved. For the separate LAN recovery,
MQTT alone was temporarily paused, internal free heap rose to114236B, then its
original setting and TLS connection were restored; no credentials/network fields
changed. Fresh independent Internet telemetry and a correlated accepted reply
passed. Current Internet firmware-fetch coexistence remains a production gate.

Actual ASUS wire replies verify mDNS/DNS-SD A/PTR/SRV/TXT, multicast and unicast
SSDP, and MNDP for the same MAC/hostname. Native PID20976/hash23ac25... recovered
itself onto this new boot with live WebSocket,80 confirmed mappings, no active
incidents/toasts or pending writes. Web/API reads were observed, not a long soak.
NTP was not yet observed after this reboot. New real Telnet connections closed
before answering; the stale-clock lease edge is fixed and regression-tested in
the next checkpoint, with actual deployment/readback recorded separately below.

Android preview0.2.7/build14 SHA`f1013c0e08716393fbd5974046c63245f8b76d2b3b98729137944f3153013257`
is built and staged on both PCs. No ADB device was available, so it is NOT newly
phone-installed or production-signed. External-power restart, physical all-dark/
white/red/mapping and coexistence remain open. Do not yet declare safe USB/laptop
removal from this bounded checkpoint alone.

### Superseded Internet recovery and terminal preparation

The subsequent user-reported DMA/RMT glitches take priority over applying job19.
Do not apply that old recovery image just to prove a reboot. Fresh LAN and
Internet observations agree on80 semantic white cells and480 publishedffffff
pixels, with ready/active RMT-DMA and failed0/error0. Physical glitches despite
that stable published buffer are not an app-side optimistic-state success.
The internal heap minimum84bytes is a separate serious headroom gate.

The new candidate corrects d46's ad-hoc350/700ns highs to classic WS2811
high-speed250/600ns (40MHz ticks10/24, full bit50ticks). The Worldsemi datasheet
halves its low-speed500/1200ns highs in high-speed mode; making these pulses
longer is not a safe timing margin. The1250ns bit/300us low latch, channel order,
full480-pixel immutable mailbox and static backend remain. Requested values
are reported explicitly, not presented as measured GPIO timings.

The SDK DMA buffer is already at its2046-symbol maximum: two4092-byte
descriptors still require refills for11520 data symbols. Increasing it past
that limit is not a valid fix. Installed3.3.12/IDF5.5.5 has the normal refill
graph in IRAM/internal objects but omits the channel's cache-safe allocation
flag. `LedDmaGuard` wraps only this task's single TX allocation, requests that
flag and requires witnessed admission plus SDK callback validation. Unrelated
DMA callers are unchanged; unsupported prerequisites fail closed. Partition
flash guards remain and the shared RMT completion IRQ is not globally changed.
This is not a refill-free driver or a proof that arbitrary critical sections
cannot starve it. SDK files and dependencies remain unmodified.

The driver now checks11521 SDK-accounted symbols including EOF after completion;
missing/short accounting stops without reusing an uncertain buffer. These are
SDK accounting values, not waveform readback. Ten actual-worker host cases
(RGB/BGR, static/DMA, immutable buffers, black Off, holds, timeout/failed stop and
short completion) pass, as do supported/unsupported GNU-linked guard fixtures.
The existing renderer pipeline passes, including elapsed-time rainbow cadence,
no imposed FPS cap, clock rollover, unchanged static selection and atomic batches.
Real S3 compilation, corrected-image deployment and optical/RF/cache-coexistence
acceptance must be recorded separately; job19 stays Ready, unapplied.

Canonical corrected S3 build passed at source
`f6d3fce29005bb218ae6b9bb476d25edd6270b00`, using the same16MB/OPI,
buffered/rmt-dma, electrically verified ISP and BLE-disabled profile. Image size
2073872bytes, SHA256`0532332175ea496216ab8a125e21a6ea442f98acc2e316cbc1379ddc9d190fc8`,
MD5`070edbac497b46c65265be7d5cccbfb3`. Sketch storage2073727/3342336,
static globals104016/327680; linker/static free space is NOT live heap headroom.
Actual new objects and final map contain the witnessed allocation path; the
completion callback is an18-byte IRAM load/store/barrier/return function at
0x4037a174 with no call/flash dependency. No SDK file/dependency was modified.
This is compiled-candidate evidence, not deployment or optical acceptance.
The corrected-image deployment question remains pending; do not substitute
the old Ready job19 for this image or start new color/chase tests.

Independent office-host TLS MQTT observations received fresh, nonretained status
and telemetry from ledboard and a correlated read-only firmware-status reply.
The station uses its router, not ASUS as its gateway. Its Internet download of
the immutable recovery manifest completed: job19 Ready,2046320 ESP bytes with
SHA256`d46b74b74d3471215fff3ac85cc4777287a7f9718f42cfd504804e195628f88f`;
manifest SHA256`b68a7ad22aa19dd2f8fa654d72342c523fede2109be93e7bd63487efade79ab5`.
The image/manifest are published under the image SHA at
`https://office.digitalogic.ir/pakarcontroller/firmware/esp32s3/`.
No AVR image, credential, automatic retry or apply request was published.

The apply preflight deliberately stopped: a fresh status showed output enabled
and a pixel-diagnostic trace, different from the earlier disabled/all-Off scene.
Preserve that scene and the Ready job until the user approves reboot. Internet
download is not update completion, continued access after reboot, external-power
survival after USB removal, or physical all-dark acceptance. Do not yet declare
the laptop safe to disconnect. Disconnecting ASUS also removes its native UI and
SSH/LAN proxy; the broker path is independent. Never expose auth-disabled HTTP
or Telnet directly to the Internet.

The terminal candidate adapts actual PU850 Shell/Telnet/Commands descriptors,
session routing and IAC/NVT editor, with RayanLamp form-owned keys and terminal
sanitization. The exact donor/target/test hashes are in the existing file ledger.
Default: two independent LAN sessions, TCP23, colored hostname prompt, eight
editable nonsecret history entries per session, separate watch state and a
device/network/LED/pixel TUI. Property edits use the same validated configuration
owners; LED actions use the existing common command router. `get led.backend`,
`get led.rmt` and `get led.dma` distinguish output mode from the actual driver.
The TUI cell list is explicitly logical, not a claim to reproduce physical table
geometry. Entering a page never actuates a pixel. ESP terminal text is bounded
ASCII; host/mobile Persian typography is not certified by this renderer.

The USB terminal is COBS-encapsulated on the existing binary port, not a raw
Ctrl-C escape that would interrupt recovery. `tools/device-shell.ps1 -Port COM22`
opens an interactive terminal; `-Execute 'get hostname'` is a bounded query and
`-Tui` opens the TUI. These commands require the new boot-fenced shell firmware;
do not use a port concurrently owned by the native app. A physical USB session
still obeys configured authentication. Current device auth remains disabled.
Input packets cannot be replayed as commands and output remains pending until
explicitly acknowledged; reopen before input/output sequence reuse.

Configuration: GET `/api/shell`; POST form fields `enabled`, `port`, `color`;
or `set shell.port 2323`, `set shell.enabled false`, `set shell.color true`.
The listener applies after the response, closing existing shells. JSON command
surfaces use `shell-config` and `configure-shell` with a `settings` object.
Only a successfully bound listener advertises its actual DNS-SD Telnet port.
Build controls: `-ShellSessions 1..4`, `-DisableTelnet`, `-DisableSerialShell`
on the S3 application-only profile; no SDK-wide flag/cache rebuild is needed.
The source candidate is not yet a delivered/physically accepted shell. Real
multi-client terminals, USB framing and LED/network coexistence remain gates.

Two applicable project-owned workarounds were submitted upstream rather than
silently modifying the installed SDK: ESP32Async/ESPAsyncWebServer PR497
(`4e0f94ae35e7f8541367534800cce89798173aca`) for TCP response backpressure,
and espressif/arduino-esp32 PR12999
(`2f2359a4cd1cc7a212650b3eec6d31da2aafcf83`) for DNS cache TCP/IP-task context
and retry-safe failure. Extracted actual-method host fixtures passed; this is
not upstream maintainer or SDK/hardware acceptance. Installed dependency pins
and files are unchanged. Espressif requires the user's own CLA signature;
no agent has signed on the user's behalf.

### Current all-off signal recovery

The user reported that Rust showed C001 OFF fade but the matrix did not clear.
One explicit same-peer80-cell immediate Off batch was accepted; published RGB
contained1440 zero bytes and gpio-static reported completed transmissions, yet
the user confirmed pixels remained lit. This is failed physical acceptance, not
an app receipt or driver-counter success. Output was then explicitly disabled;
no subsequent color/effect tests were sent.

The integrated recovery code builds against Arduino-ESP32 3.3.12/IDF5.5.5 with
the explicit rmt-dma, buffered,16MB/OPI, electrically verified ISP, BLE-disabled
profile. Static remains selectable; this does not change generic defaults. The
RMT worker uses an immutable1440-byte internal source and SDK MSB-first bytes
encoder/ring instead of a46084-byte expanded-symbol allocation. The recovery
profile requests350/700ns highs at1250ns/bit and300us LOW latch. These are code
parameters, not measured scope values or an underrun-immunity claim. Startup
always emits a complete black frame, including when output is disabled.

Fresh post-UART time and buffered-UART admission remove the obsolete12ms
SoftwareSerial lease from S3 rendering. Raw segment cloud snapshots coalesce
outside AVR receive while local WS/USB events remain immediate. Non-display
cloud I/O and reconnect still block the loop; the display drain's timestamp
currently precedes the blocking publish, so it is not a guaranteed inter-attempt
quiet interval on a stalled link. A single-owner asynchronous cloud worker and
full coexistence/timeout acceptance remain open.

One canonical local OTA attempt aborted at42460/2046320bytes after5556ms with
ECONNRESET. Independent ASUS and forwarded reads confirmed unchanged old boot
ca1acf3d423c6a8c0256ef6d9d7d3863 and old2045120-byte image before USB recovery.
No upload replay occurred. Fresh USB partition/OTA reads validated sequence19/20
CRCs, selected app1/0x340000, bridge COM22/5CE4108809 and MAC90:70:69:ea:c8:00.
Only app1 was written,31.7seconds/460800baud, and esptool verified the data hash.
No bootloader, partition table, OTA selection record, filesystem or AVR write
occurred in this ESP recovery. Old images and settings remain preserved.

Actual running image:2046320bytes, SHA256
`d46b74b74d3471215fff3ac85cc4777287a7f9718f42cfd504804e195628f88f`,
MD5`b66aca0895d8d3ad7e52232c93c54e94`; boot
`730642a9a16509512ec9365ec42b1f0b`. Full ranged readback matches SHA; selected
and running app1 are VALID. One firmware-export read needed the existing bounded
read-only retry. Auth remains disabled, BLE unstarted, Wi-Fi awake. Actual driver
reports rmt-dma/ready, submitted2/transmitted3/failed0/error0; service disabled
and80 saved Off states. This still awaits the user's physical all-black verdict.

Actual native PID15928/build14 reconnected over Wi-Fi/WS: mapping80,80Off,
AVR online/TM ACKtrue, resting masks[63,113,113,0] (OFF), no pending maintenance
or active incidents. Recovery timeouts remain in inactive history. Its older
historical progress message is not a verified update receipt. Four subsequent
cached-address reads returned complete status529ms/panel479ms/Web223213bytes
in1336ms/SSDP XML661ms; WS upgrade110ms plus requested status36ms. This is not
a soak or proof OTA ingress is repaired. Cloud service still reached2044ms.
Internal heap at the image-verification sample:71000free/31732largest/55024min.

Source progression:174da58 adds the semantic AVR/ESP/native/mobile events;
fae96a6 fixes custom notice receipts and updater event compatibility. Current
recovery code is the subsequent LED/scheduler checkpoint. AVR app-only Urboot
delivery and build14 native deployment precede this incident; their exact
receipts are in REUSE-PREFLIGHT-RECEIPT.md. Android build14 is staged on ASUS,
not phone-installed. Focused panel/LED pipeline and8 worker-double cases pass;
real S3 compile and readback are distinct from electrical/optical acceptance.

Private local receipts: verification/led-signal-recovery-verified.json
(SHAacc55175e2f6d1a512d9b94a38536f1d336ac019b9799558b29e4d7fdf98d48b),
led-signal-recovery-service.json and rmt-recovery-native-final.json
(SHA98d850c48d9a8294af76ded96907207a0a0b1b9cac691dd33e81c31d295923e2)
under outputs/PakarController. Do not publish the boot-metadata dump: it also
contains private NVS. USB-disconnected operation and current all-off/color
optical acceptance remain unchecked. The older checkpoints below are history,
not current shipping identity.

### Web-service recovery update

Shipping firmware source802512cb6f6a24b3ae977a7f164298c913fb5016 replaces the
earlier image below:2036080bytes, SHA256
`f81237f48d51cff5dd94c34257134c3d243dfece824a05640736cdfff0205b16`,
MD5`6612324b5cf9cc91c6bbe8b91d798c46`. Fresh USB metadata validated OTA
sequence19/20 CRCs and selected app1 at0x340000. Only that app range was written;
esptool verified its hash. Bootloader, partition table, OTA selection data,
filesystem and AVR image/EEPROM/fuses were not written. The previous fa1a7a9a
image remains preserved as rollback. Current boot is
`35bed89db160d5b4c9028808df5929e4`.

This stable profile uses `-S3Bluetooth disabled` and explicitly awake Wi-Fi.
Actual status confirms BLE configured:false/started:false/startup_attempted:false
and wifi_modem_sleep:false. The runtime-capable alternative remains available;
missing/corrupt BLE settings default Off, first enabling requires reboot, and
excluded builds reject On. Do not enable BLE coexistence before issue20's gate.

The async response adapter now retains short writes/failed sends and waits for
all admitted header/body bytes to be ACKed. Actual postboot verification read
the entire running image and matched SHA256, confirmed VALID selected/running
app1, authentication disabled and same-board/boot WebSocket status. All12 bounded
cached-address HTTP reads completed, including the complete223213-byte Web UI
in877ms. This ran concurrently with image export and native transport checks;
median434ms is not claimed faster than the earlier unloaded308ms measurement.
One TCP connection still took3.2s. ASUS DNS lookup separately takes roughly2s;
hostname-first clients keep the verified current-address hint until failure.
The primary-PC forwarded Web UI also returned all223213bytes in2.586s.

Guarded native PID23376/build13 verified actual USB COM22 first, then Wi-Fi/WS,
same new boot,80 mappings/80 acknowledged white states, AVR online, TM1637 ACK,
GPIO48 semantic green and zero active incidents. Preferences hash was unchanged;
the app remains on Wi-Fi. USB serial errors and AVR UART errors were0 at final
readback. AP+STA, NTP synchronization and MQTT connected were observed. Internal
heap was85164free/38900largest/59980minimum after concurrent traffic.

Evidence: outputs/PakarController/verification/service-recovery-verified.json,
service-probe-after-cached.json, http-adapter-native-final.json and
http-adapter-native-routes.json (private runtime receipts, not shipping assets).
The 46-second app check is not a soak or optical/audio acceptance. Some native
table cells are clipped by the retained dock viewport, and individual-pixel RGB
preview remains unavailable; neither is relabeled complete UI acceptance.

Before USB recovery, one c75edea local OTA attempt aborted at206164/2222176bytes;
independent HTTP and native USB verified unchanged old79db5e boot before any
fallback. It was not replayed. OTA ingress and simultaneous BLE/TLS/OTA remain
open; the new snapshot adapter does not replace the separate firmware-export
response. Synchronous cloud reconnect still reached3.122s in this boot and needs
a single-owner worker/queue redesign. These successful bounded checks do not
claim that every long-duration outage or cloud reconnect delay is solved.

### Earlier successful Wi-Fi OTA checkpoint

Source8a3827a48aec5265adb6dde699c8c2a101d35fd4, version0.2.7; native/Android
client build13. The S3 application is2221280bytes, SHA256
`fa1a7a9aca7427517485d1729393666c3599b2ad88078e0b19f8fc89bdc8831a`,
MD5`467416f0d9f60404c1bf19ac02534831`. A hash-verified app-slot-only USB
bootstrap was followed by a real automatic local Wi-Fi OTA. It changed boot
`67ebb82317aecf582b6ecd0bdcb33115` to`79db5ecd43872d3e64e5dbd5dea65997`;
the whole running image was read back in bounded ranges and its SHA verified.
Running and selected OTA offsets are0x340000, state2 VALID. Preflight+upload
took35185ms; complete reboot/hash/WebSocket verification took63244ms.
No AVR image/EEPROM/fuse, ESP bootloader or partition table was written; the
filesystem was not reflashed or reformatted. Explicit RGB/BLE configuration and
debounced scene settings were saved normally.

The current instance is deliberately Wi-Fi-first: optional BLE is configured
off, not removed from the integrated firmware. Its NimBLE/controller internal
pools reduced initial free heap to8272bytes. Moving BLE application framing and
the WS queue to PSRAM recovered about23KB, but a BLE-on OTA still aborted at
135800bytes with a232-byte internal minimum. That attempt did not reboot/commit
the image and was not blindly replayed. BLE-off persisted across a controlled
reboot:106268bytes free /63476largest /87092minimum initially. After NTP and MQTT
connected, a later snapshot was59264free /31732largest /43624minimum. BLE/TLS/OTA
coexistence remains an explicit issue#20 gate, not an accepted simultaneous mode.

The static GPIO4/BGR matrix profile is enabled and all80 cells are saved white.
An actual same-boot command/readback confirmed logical1 red and79 white; a later
80-cell white batch and AP readback confirmed80white/saved. This is controller
state and driver evidence, not optical proof of physical group alignment.
GPIO48 status RGB is configured/persisted at brightness64 under the user's
common-DevKit assumption; driver-submitted ready bytes are[0,64,10]. Its physical
light and transition appearance still require human confirmation.

Actual AVR hardware-UART telemetry reports0.2.7 online, TM1637 ACK, freeSRAM474,
and no lost events. Boot count1076 and uptime continued through the ESP OTA;
the cached prior POR/brownout flags are not a new measured voltage fault. UART
error count changed1->2, so zero-error contention/long-soak acceptance remains
open. No repeated welcome/reset loop was observed in these snapshots.

Current firmware AP+station policy, hostname, static scene, RGB and BLE-off
configuration survived reboot/OTA. ASUS directly joined the saved ledboard AP
profile, obtained192.168.4.3 and read the same board/boot at recovery gateway
192.168.4.1 with auth disabled, AP+station true and80white/saved. Its original
Wi-Fi profile was restored afterward. This does not certify all AP-only/
station-only transitions or an externally powered restart with USB removed.

Actual Wi-Fi discovery captured mDNS/DNS-SD A/PTR/SRV/TXT and follow-up records,
three multicast SSDP responses, three unicast responses and one MNDP reply with
the same MAC/hostname/board identity. Private receipt s3-final-discovery.json
SHA256`d4b66810bd4e29fa88b2c543a46e482cc929e9675467d86739e95a1bd4202136`.
The latest ESP scan found four networks, not DRS Tether; its requested join test
is therefore still open. NTP callback/source and MQTT connected state are now
observed; broker connection alone is not cloud-command/telemetry round-trip proof.

ASUS had equal Ethernet/Wi-Fi metrics and unreliable Ethernet-bound board
sessions. A bounded comparison received WS101 then no data/reset via Ethernet,
but real frame data via Wi-Fi. An owned device-only ActiveStore/32 route via its
Wi-Fi interface made canonical updater readiness pass. This is a temporary,
current-address ASUS workaround, not a permanent IP in the application or a
W5500 test; cross-adapter routing/reacquisition remains issue#10 acceptance.

Native13 is running on ASUS, with80 confirmed mappings, WS/HTTP online, verified
upgrade state and zero active incidents at the post-OTA checkpoint. Android13
is staged/published, not phone-installed (no authorized ADB device present).
Both public artifacts and release metadata were independently hash-verified;
Android is still a debug-signed preview. Full native299/mobile287 suites passed,
two opt-in mobile captures skipped; exact8a3827a CI succeeded. No open PRs remain.
PBX milestone call17923611908127222 was completed/ANSWERED for34seconds, no retry.
External-power/optical/audio/keypad/shift-register/phone and broader donor parity
gates remain open in the owning issues; do not close the production issue.

## Identified connected hardware

The actual device reported ESP32-S3 revision 0.2, 16 MB quad flash, 8 MB embedded
OPI PSRAM and MAC `90:70:69:ea:c8:00`. This identifies the chip/memory, **not** the
DevKit PCB revision or its onboard RGB GPIO. David-ASUS exposes the CH343 UART
bridge as COM22. Its native USB application previously appeared as COM13.
Use the current UART/COM USB socket for commissioning; no further switch is
required while COM22 remains present.

The complete original 16 MB flash was read before any upload and copied to both
PCs. Both copies have SHA-256
`cd0f30ac9491da499eaf2167f07b5accf23cf6779c506110431aa766f346273e`.
The backup is private and outside Git because it may contain credentials.
Secure boot and flash encryption reported disabled. No eFuse change is needed.

## Wiring contract

| ESP32-S3 GPIO | Controller Board Mini / function |
|---|---|
| 17, UART1 TX | AVR D0 / PD0 / RXD |
| 18, UART1 RX | AVR D1 / PD1 / TXD |
| 11, ISP MOSI | AVR D11 / PB3 / MOSI |
| 13, ISP MISO | AVR D12 / PB4 / MISO |
| 12, ISP clock | AVR D13 / PB5 / SCK |
| 10, reset control | AVR RESET; user reports direct connection and current AVR VCC 3.3 V |
| 4 | External WS2811 matrix DIN, 480 pixels / 80 cells |
| GND | Common signal ground |

The printed TX/RX bridge pins (commonly GPIO43/44) are the independent PC host
path, not the AVR UART. Both UART wires cross TX to RX. UART1 is 38400, 8N1;
the CH343 PC protocol is 115200, 8N1. The AVR retains hardware Serial. The paired
buffered build does not require a busy wire. Old busy-wire AVR firmware must not
be mistaken for the new buffered image; unsolicited events can remain blocked
until that firmware is safely updated.

Current commissioning firmware deliberately pauses the normal AVR UART. The
38400 setting above is the intended paired application profile, not a claim that
the unknown firmware currently on the AVR has acknowledged this protocol.

ESP32-S3 GPIOs are not 5 V tolerant. Reconfirm AVR VCC, clock and any level
translation before driving the connected circuit. GPIO10 must not be connected
to AVR D10 or ESP EN. The integrated ISP implementation defaults to disabled
until `PAKAR_S3_ISP_ELECTRICAL_VERIFIED=1` is deliberately selected after reviewing
the actual RESET circuit and its assertion polarity. A successful S3 build is
not permission to alter AVR fuses, EEPROM or bootloader.

AVR display pins remain PuzzleBoard's A4/PC4 DIO and A5/PC5 CLK. Its buzzer is
D9. ESP settings navigation calls the AVR's actual existing cue owner, rather
than synthesizing a second melody engine on the ESP.

## Reproducible target

From a whitespace-free checkout, use the guarded build:

```powershell
./tools/build-firmware.ps1 -Target esp32s3 -LinkMode buffered `
  -S3FlashMB 16 -S3Psram opi -DeviceName ledboard -Jobs 4
./tools/build-firmware.ps1 -Target avr -LinkMode buffered -Jobs 2
```

The current quiet commissioning image was built with:

```powershell
./tools/build-firmware.ps1 -Target esp32s3 -LinkMode buffered `
  -S3FlashMB 16 -S3Psram opi -DeviceName ledboard `
  -PauseAvrLink -S3IspElectricalVerified -Jobs 4
```

`-PauseAvrLink` leaves normal AVR TX/RX as inputs and disables normal AVR requests;
only explicit physical-USB diagnostic requests can start the separate bounded
RX-only capture. `-S3IspElectricalVerified` is an explicit, S3-only operator
acknowledgment for the reported 3.3 V/direct GPIO10 RESET path; it defaults off.
Its name is not an electrical measurement or successful programmer probe.
Enabling it does not automatically touch RESET, flash, fuses or EEPROM. Never
use an ISP probe to silence the AVR: its scoped operation releases RESET again.

The actual installed SDK is Arduino-ESP32 3.3.12. Dependencies are locked in
`firmware/esp32s3/sketch.yaml`; Async TCP 3.5.0 is the maintained ESP32Async
implementation. The selected known partition layout uses the first 8 MB:
two 0x330000-byte OTA slots and a 0x180000-byte filesystem. The remaining flash
is intentionally unallocated, not claimed as usable filesystem capacity.

Only first commissioning after the full backup may add `-InitializeFilesystem`.
That build attempts filesystem creation at most once, recording the attempt in
NVS **before** formatting. Ordinary builds never auto-format damaged settings.
Do not erase the whole flash to resolve a mount error.

The normal host protocol uses UART0/CH343. Defining
`PAKAR_S3_HOST_NATIVE_USB` selects native USB instead; do not expect the same
physical COM port to carry both implementations. The AVR UART remains separate.

## Network setup and persistent configuration

Use physical USB commissioning with the checked-in helper on David-ASUS:

```powershell
$wifiPassword = Read-Host 'Station password' -AsSecureString
./tools/set-device-network.ps1 -Port COM22 -Ssid 'DRS Tether' `
  -Password $wifiPassword -Mode both
```

The helper confirms ESP identity, correlates every COBS/CRC ACK and never replays
an uncertain mutation. It verifies configured station/AP mode separately from
actual association/AP startup. Credentials are never printed. Prefer
`http://ledboard.local/` after live DNS-SD/mDNS verification; use a current
observed address only for diagnosing discovery failures.

Fresh filesystem Web authentication is `admin` / `pakarcontroller`; change it
through Device settings before untrusted network access. The recovery AP uses
the configured hostname as its SSID. Its current commissioning password is
`pakarcontroller`. Neither value is the private station password.

Web/native Device settings expose persistent station/AP/both and DHCP/static
configuration. Changing mode without providing a password preserves the saved
station credential. DHCP preserves the last validated static profile.
The paired AVR ESP submenu currently supports mode, reconnect, scan request,
DHCP/static-profile selection and a once-scrolling hostname summary. Full text
editing and scan-result selection remain pending; see
[front-panel details](S3-STATUS-FRONTPANEL-INTEGRATION.md).

## Status pixel and discovery

### Standalone WS2811 wiring and USB-free handoff

The ESP owns the matrix renderer; the AVR is not required for rainbow, fire,
cell colors or streamed pixels. The current image uses WS2811/RGB on **GPIO4**,
with 480 addressable RGB pixels arranged as 80 logical cells of six pixels.
Count addressable controller groups, not merely physical LED emitters: some
WS2811 strips drive several emitters from one address. Current per-cell effects
are immediate/fade/pulse with white/red/off; arbitrary named semantic groups,
independent group effect instances and full WLED feature/API parity are not yet
implemented or accepted.

With all power disconnected, connect GPIO4 to the strip's **DIN/input** end,
ESP GND to strip GND and supply negative, and the strip's V+ directly to its
correctly rated external supply. Follow its printed voltage; WS2811 products can
use different supply voltages. Never feed strip voltage into a GPIO or 3V3 pin,
and never power 480 pixels through the DevKit regulator/USB/header traces. Use
appropriate fused distribution/injection and wire ratings. A 3.3-to-5 V AHCT
data buffer and a short data lead are preferred for reliable final wiring.

Removing PC USB still requires an independent ESP power source. For the assumed
common DevKit, use a USB wall supply or regulated 5 V at 5V/GND, not strip 12 V.
Use one selected power-input method; do not parallel external 5 V with PC USB
unless the actual board's isolation is established. AVR stays on its reported
3.3 V supply. Matrix GPIO4 is independent of the onboard status RGB, which remains
disabled until its configured GPIO is enabled and optically checked.

**Current stored LED state was observed as cell mode, all cells white,
brightness255.** Do not treat startup as dark or current-limited. Arrange an
adequate supply before connecting the full matrix, or deliberately select off
before wiring and verify its persisted state.

USB-free update readiness is still OPEN. A fresh ASUS hostname-first read returned
the expected board ID, station/AP active, AVR link disabled and 480 configured
pixels. `/update/info` initially exceeded a seven-second request deadline; a
subsequent independent bounded curl read returned HTTP200 in2.868267 seconds,
OTA phase idle, slot capacity3342336 and MD5
`9ec4ce6a0f2a0d4ad982468acfe46f2e`, matching the exact local quiet application.
An intended same-image Wi-Fi OTA then stopped at the initial hostname preflight:
`ledboard.local` did not resolve. **No upload started and no firmware was written.**
This does not distinguish power-off from a discovery/network fault. Do not
declare OTA broken or successful from these observations.

Before a Wi-Fi-only handoff, verify one authenticated/pinned OTA upload, reconnect
to the same hardware/new boot with the expected image and preserved network
settings, then perform external-power startup and actual WS2811 effects tests.
Retain USB recovery until this milestone is explicitly reported passed. Keep
AVR reset/ISP, buzzer and UART actuation out of that standalone LED test.

Electrical references: [Espressif common DevKit power and headers](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html),
[WLED strip voltage variants](https://kno.wled.ge/basics/compatible-led-strips/) and
[WLED power distribution guidance](https://kno.wled.ge/advanced/wiring/).

The onboard pixel is independent of matrix GPIO4. Its actual GPIO must first
be confirmed (supported DevKit variants: 38 or 48). It then transitions smoothly
between semantic boot/connecting/ready/AP/peer-lost/update/warning/fault states.
Unidentified pins stay disabled. Native/Web previews consume actual reported
driver RGB bytes without applying brightness twice. Driver submission is not
optical acceptance.

The S3 discovery implementation contains mDNS/DNS-SD, the adapted PU850 SSDP
device/search queue, and MNDP on active station/AP interfaces. NetBIOS is
explicitly unavailable on this target rather than falsely advertised. Live
multicast reachability must be tested from each relevant PC and phone; hotspot
client isolation can prevent it even when ordinary HTTP works.

## Acceptance boundaries

### AVR recovery candidate

The buffered AVR candidate includes actual PCController reset capture before
C++ startup, the Urboot r2 fallback, watchdog shutdown and a CRC/commit-last boot
journal at EEPROM416..799. Settings, automation and Prog ownership are preserved.
Automatic welcome playback requires a clean power-on/external reset and is
suppressed for watchdog, brownout and unknown causes. Existing46-byte telemetry
is unchanged; optional B5 reports raw reset flags, boot count and journal validity.

The integrated S3 source now uses its actual UART1 adapter with checked buffer
setup, driver-ready state and hardware error counts. Peer availability requires
an identified AVR and a semantically valid response. Buffered S3 UART draining
does not inherit the ESP8266's18ms per-download-chunk wait. The ISP build keeps
the existing explicit GPIO10 electrical acknowledgment and does not write fuses.

The AVR candidate compiled at31628bytes flash and1406bytes static SRAM. Its
compiled .init3 sequence was inspected: MCUSR/r2 capture, MCUSR clear, watchdog
disable and fall-through into data initialization. The reset journal fixture,
protocol/identity cases, ISP EEPROM-preservation fixture and S3 source guards
passed. These are compile/host results; physical UART and ISP remain open.

David-ASUS SSH on localhost:8022 timed out before login during this recovery
attempt. No AVR signature read, backup, erase, firmware upload or audible test
has been performed. On restored access: identify the live ESP/boot, inspect UART
and reset state, read ISP signature/fuses and export verified flash/EEPROM backup,
then apply the paired candidate as required and verify fresh UART telemetry.
Do not label the AVR healthy until its live reset count, uptime and errors are
observed stable and its actual peripherals respond.

### Observed quiet commissioning checkpoint

The exact shared main is
`e19a7c10fe7e0bcd2ed523a9a011123748ba1640637827bfe5fa72411930b5f2`.
The real S3 SDK build passed: sketch 1,843,223 / 3,342,336 bytes, globals 74,916
bytes. The application binary is 1,843,376 bytes, SHA-256
`6c65e309173c91284605476662c2aad1f26349e5c6fa68bc2431c4482c3789f5`.
Only the application at `0x10000` was subsequently flashed on the same COM22/MAC
and hash-verified by esptool. The original full-flash backup remains unchanged;
bootloader/partition data were not rewritten in this quiet-image update.

Actual USB identified ESP 0.2.7. Its valid 42-byte status acknowledged normal
AVR link disabled, station connected, AP started and NTP synchronized, with
reported protocol errors zero. The two explicit RX-only captures returned:

| Selected baud | Elapsed ms | Received | Stored | Dropped | Hardware RX errors |
|---|---:|---:|---:|---:|---:|
| 38400 | 3059 | 140 | 128 | 12 | 96 |
| 115200 | 3030 | 172 | 128 | 44 | 92 |

Both confirmed pin-ownership checks and stop. GPIO10 was observed high with
output disabled; RX18 was high at stopped snapshots and was also observed low
during the 115200 capture. Stored bytes were mostly `00`, `FF`, `FE`, without
plausible framing. These are raw observations, not valid protocol communication,
the correct baud, electrical safety, or a diagnosis of power/wiring/firmware.
Neither capture transmitted AVR data or drove RESET.

The user reports the annoying welcome sound still repeats with this quiet image.
Its cause is **unknown**; a repeating melody alone does not prove MCU resets.
All further board tests have stopped. The immediate requested action is to
disconnect AVR power, or the whole assembly if it shares the supply, before
continuing diagnosis. No ISP probe, AVR reset-control write, AVR flash/fuse/EEPROM
operation, PBX call or test tone was performed. There is no existing reset-hold
API: the programmer's probe/backup/program operations release RESET on exit.

### Remaining acceptance

Track actual flash, startup, Wi-Fi scan/association/reboot persistence,
API/WebSocket, UART ACK/error counters, keypad/display/audio and matrix output
individually in GitHub issue #2. SDK compilation and host fixtures are not these
physical tests. FastLED uses the S3 backend; actual DMA use and refresh timing
are not inferred from a target macro. Firmware transfer abort/partition checks
do not establish hardware rollback or interrupted-power recovery.

## W5500 Ethernet commissioning

The W5500-enabled S3 candidate builds against Arduino-ESP32 3.3.12 using native
ETH/lwIP and the existing AsyncTCP/ESPAsyncWebServer stack. No second Ethernet
server, updater, authentication policy or API namespace is introduced. This is
source/build readiness, not a physically verified Ethernet delivery.

| W5500 signal | ESP32-S3 GPIO |
|---|---|
| SCK/SCLK | 5 |
| MISO/SO | 6 |
| MOSI/SI | 7 |
| CS/SCS/SS | 8 |
| RST/RSTn | 9 |
| INT/INTn | 14 |
| GND | GND |

Power off while wiring. W5500 signals are3.3V; supply a module's explicit3V3
input with regulated3.3V. VCC/VIN-only carriers need their input requirement
confirmed first; do not guess between3.3V and5V. Do not join regulator outputs.
The private HSPI/SPI3 bus is separate from AVR FSPI/SPI2. LED GPIO4, AVR UART
TX17/RX18 and ISP RESET10/MOSI11/SCK12/MISO13 remain unchanged. Onboard RGB is
not reassigned by this driver. See the [migration wiring notes](ESP32-S3-MIGRATION.md#optional-w5500-ethernet-separate-spi-bus).

Build without a cold clean (retain the confirmed16MB/OPI hardware profile):

```powershell
./tools/build-firmware.ps1 -Target esp32s3 -LinkMode buffered -S3FlashMB 16 -S3Psram opi -S3LedBackend rmt-dma -S3IspElectricalVerified -S3Ethernet -Jobs 16
```

The ISP acknowledgment above retains the existing candidate profile; it does not
prove the live wiring or authorize fuse/EEPROM changes. Ethernet testing does not
need an AVR reset, melody or flash operation.

After deploying through the authorized OTA/USB path, read `/api/status` and
`/api/network/ethernet` on `http://ledboard.local`. Status includes actual
`uplink`, `uplink_ipv4_ready` and `ethernet` initialized/started/carrier/IP/DNS/MAC
state. Disconnected addresses are null, not a stale DHCP address. IP readiness
does not imply an Internet connection. The latter route provides independent
persisted DHCP/static settings, not Wi-Fi settings; POST uses form fields
`dhcp`, `address`, `gateway`, `subnet`, `dns1`, `dns2` with normal access and
board/boot fencing. A same-value `dhcp=true` save is also an explicit driver
recovery attempt if initialization failed at boot. No background reset storm is
used for an absent module or unplugged cable. Read `apply_pending`, `apply_error`
and fresh status after the delayed apply; an accepted save is not a live link ACK.

On an on-LAN PC with Node:

```powershell
node tools/check-ethernet-live.mjs --run --origin http://ledboard.local
```

This checks a compatible board/boot, the real TCP peer against the Ethernet IP,
Web UI, JSON, SSDP description and firmware HEAD/Range serving. It does not
automatically change settings or apply firmware. If auth is enabled, provide
`PAKAR_ETH_USERNAME` and `PAKAR_ETH_PASSWORD` privately in the environment.

To verify Ethernet-only Internet download, first disconnect Wi-Fi station using
the existing network settings while retaining the recovery AP, then add
`--manifest HTTPS_URL --manifest-sha256 SHA256 --image-sha256 SHA256` to the same
command. Use a known-compatible manifest/image and optional private environment
`PAKAR_ETH_FIRMWARE_BEARER`. It sends exactly one ESP-only stage request with
`apply=false`, follows the original fenced job to Ready and compares its image
hash. Unknown dispatch, timeout, reboot, wrong interface or busy firmware state
stops without replay/apply/cancel; inspect the original job. This staging step
writes the inactive OTA slot. Do not use arbitrary images for convenience.

Remaining physical checklist (GitHub issue2 stays open):

- [ ] Confirm module power input and inventory the actual S3/installed image.
- [ ] DHCP and static profile work across reboot; hostname resolves on Ethernet.
- [ ] Same local APIs, WebSocket handshake/control, Web UI and LED bulk commands
  work over the actual wired peer, without duplicated app connection owners.
- [ ] NTP, MQTT/WSS and a pinned Internet image stage work with Wi-Fi station
  disconnected; separate explicit OTA apply/reboot/running-image verification.
- [ ] Cable unplug/replug and Wi-Fi fallback recover without stale state,
  duplicate mutations or AVR resets; AP recovery remains reachable.
- [ ] mDNS/DNS-SD, SSDP and MNDP discovery work on the wired LAN; XML serving
  alone is not multicast proof. Observe LED timing under concurrent traffic.

That earlier source checkpoint had no ASUS SSH access. ASUS SSH8022 now works;
its Ethernet and Wi-Fi interfaces were inventoried without changing either.
Native/Web/mobile Ethernet DHCP/static editors have peer-bound ACK/readback
fixtures and built client packages. Actual W5500 wiring/link, installed-editor
interaction and Ethernet-only Internet acceptance are still unverified. No
optional Ethernet GPIOs are driven by a fresh disabled profile. TFT LCD remains in the separate
[hardware backlog](S3-STATUS-FRONTPANEL-INTEGRATION.md#additional-hardware-backlog).
