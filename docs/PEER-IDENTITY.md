# Peer identity and supported releases

Release admission is separate from the single living API. All paths stay
unversioned; additive optional fields are tolerated, and capabilities still
determine what can be controlled. No adapter is chosen for an app/firmware build.

`interfaces/peer-policy.json` declares default enforcement and the inclusive
minimum app/board releases, currently `0.2.8`. Maximums are optional exclusive
bounds and currently null: future releases are not arbitrarily rejected.
Strict SemVer compares prerelease identifiers, ignores build metadata, rejects
malformed/oversized values, and never requires equal app/board patch versions.

## HTTP, WebSocket and discovery

- Native, mobile and tools send their actual package `User-Agent`,
  `X-Pakar-Client`, `X-Pakar-Version`, client ID and build. Browser User-Agent
  stays the genuine browser value; custom product headers identify the Web app.
- Firmware responds with `Server: PakarController/<firmware release>`,
  `X-Pakar-Application`, `X-Pakar-Version`, `X-Pakar-Board-Version`, board/boot
  identity and support-policy headers, including errors and downloads.
- A bridge's Server/version describes the bridge app, never its upstream board.
  The upstream firmware release is separately named `X-Pakar-Board-Version`.
- `/api/peer/info` is a read-only bootstrap. Status, identity, telemetry and
  `/info` include release/policy metadata. `GET /api/clients` lists bounded peer
  reports; `POST /api/clients/report` introduces or refreshes a peer.
- Missing-version status bootstrap remains readable. Unsupported supplied
  versions, unidentified control requests and unsupported WebSocket upgrades
  receive HTTP 426 before control dispatch. Auth/origin/board/boot fences remain.
- Browser WebSocket upgrade uses nonsecret `client`, `version`, `client_id` and
  `build` query fields because browsers cannot set custom upgrade headers.
  Native/mobile use HTTP headers. Peer introduction uses an additive
  `peer-introduction` event; no protocol-generation negotiation occurs.
- The pinned embedded WebSocket library owns its genuine
  `arduino-WebSocket-Client` User-Agent. Firmware adds its actual product/version
  in `X-Pakar-*`, not a second conflicting User-Agent or a dependency rewrite.
  Firmware HTTP downloads do use `PakarController/<release>` User-Agent.
- SSDP Server/modelNumber, DNS-SD TXT and MNDP advertise release identity.
  Discovery is a candidate hint, never authentication or command ownership.

USB introduces a caller using HELLO `[role,major,minor,patch]`; empty HELLO is
read-only bootstrap. The ESP accepts sequential opcode `0x61` report chunks
`[totalLE16,offsetLE16,<=44 JSON bytes]` up to 1536 bytes, with an ACK per chunk.
Full report/identity versions retain SemVer detail; no control is replayed after
read-only reintroduction following a reboot. AVR peers use the same role/release
HELLO with hardware UART; ISP recovery remains independent of API admission.
AVR's fixed numeric UART version fields carry stable release triples; its
compile-time minimum/maximum overrides must therefore also be stable triples.
Full string SemVer and prerelease bounds apply to ESP and application metadata.
BLE uses the same API policy through its existing framed requests. Cloud
MQTT/outbound-WebSocket mutations include `client_version` in their existing
board/boot/request/deadline envelope; broker authorization and replay gates remain.

In a native build with version checks explicitly disabled, an exactly correlated
USB `Unsupported` response to peer-reporting may mark that optional reporting
capability absent for the physical session. It does not admit a failed HELLO,
timeout, authentication error, stale identity or missing board/boot validation,
and never selects behavior by a particular release number. The default build
rejects that unsupported introduction instead.

## Reports and privacy

Apps report installation/session identity, actual app version/build, available
OS/architecture/device/hardware data, capabilities and allowlisted public
presentation/transport preferences. Unknown hardware is omitted, not invented.
The receiver distinguishes observed connection addresses from self-reported IPs;
USB/BLE reports expose their observed transport and a null observed IP, not an
invented network address. Native bridges retain their downstream clients in
their own registry; relaying every indirect client's report to the MCU remains
an explicit follow-up, rather than pretending that the bridge is those clients.
No passwords, tokens, arbitrary settings, files or documents are retained.
UTF-8 model/hostname text is validated; long observed User-Agent is explicitly
marked truncated, not grounds to reject a valid app. The firmware stores eight
bounded recent peers; reports expire for slot reuse after two minutes. A session
switching USB/Wi-Fi can update its own metadata without creating duplicate peers.
Self-reported IDs/versions are diagnostics/admission, not authentication.

## Build controls

- Firmware: `tools/build-firmware.ps1 -DisablePeerVersionChecks`, or
  `PAKAR_ENFORCE_PEER_VERSIONS=0`. Optional `PAKAR_MIN_APP_VERSION`,
  `PAKAR_MIN_BOARD_VERSION`, `PAKAR_MAX_APP_VERSION`, `PAKAR_MAX_BOARD_VERSION`
  override firmware support ranges.
- Native: `tools/build-desktop.ps1 -DisablePeerVersionChecks`; the wrapper
  scopes `PAKAR_ENFORCE_PEER_VERSIONS=0` to that build, restoring the environment.
- Flutter APK: `tools/build-mobile.ps1 -Mode ApkPreview -BuildOnly
  -DisablePeerVersionChecks`, forwarding the equivalent Dart compile define.
- Web default is generated from the policy file; regenerate its existing assets
  after changing the built default. Node/PowerShell tools can explicitly disable
  their own check with `PAKAR_ENFORCE_PEER_VERSIONS=0` or `false`.

Each party enforces its own policy independently. Turning off a client check
does not override firmware policy, and turning off firmware policy does not
disable an app's check. Identity, authorization and hardware/capability checks
are never turned off by this flag. Source/fixture/build evidence is not live
deployment, physical board, multicast or human acceptance.

For example, an identified read of the peer registry is:

```powershell
curl.exe -H 'User-Agent: PakarController/0.2.8 (manual)' -H 'X-Pakar-Version: 0.2.8' http://ledboard.local/api/clients
```

`GET /api/peer/info` is readable without those headers to diagnose a release
rejection. The version header is a release claim, not a password or authentication.

## Coordinated rollout

The release floor intentionally excludes the previous `0.2.7` products: they do
not implement this admission/reporting contract. Build all paired products as
`0.2.8` before replacement. A new default-enforcing client will not control the
old running board. For deliberate migration, the update tool accepts an explicit
`--recovery-board ACTUAL_BOARD_ID` (`-RecoveryBoard` in PowerShell). This admits
only identity-pinned maintenance and verified backup reads of that exact board;
it does not change the global policy or authorize ordinary controls. Missing
producer headers may be tolerated only together, with pinned address, whole-image
integrity and before/after status/boot checks. Conflicting/partial identities
remain errors. The NEW boot must pass ordinary supported-release admission.
Authentication, image/capacity/digest/latch/job gates remain. Optional
`--upload-rate BYTES_PER_SECOND` (`-UploadRate`) adds explicit pacing to the
backpressure-aware uploader;0 has no rate cap. USB ROM/ISP recovery is also
independent of application release admission. No tool automatically disables
policy on rejection or replays an uncertain update.

Read-only app-package information/download and release-feed endpoints remain
available for recovery from old apps; enrollment, commands and upgrade mutations
still enforce their receiver's policy and existing authorization rules. This
source checkpoint has not replaced any installed app, board or remote service.
