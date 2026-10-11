# HTTP board and boot ownership

The living `/api/status` contract adds `board_id`, `boot_id` and
`http_peer_fencing`. ESP8266 board identity uses the full station MAC, not its
editable hostname. Boot identity reuses the existing 128-bit SDK random value;
entropy failure, all-zero boot or invalid MAC disables the advertised fence.
No new API generation, route alias, hostname suffix or persistent write exists.

The TUI requires a valid, acknowledged fence before sending mutations. Old
firmware remains readable and selectable but cannot dispatch TUI actions. Each
cell still gets a fresh capability/latch preflight. Mutating commands inherit
`X-Pakar-Board` and `X-Pakar-Boot` on every nested shared-parser request. Firmware
rejects a partial, malformed or mismatched supplied pair with HTTP409
`peer_changed` before handler dispatch. Both upload callbacks check it before
writing too: normal async middleware executes after upload callbacks.

A reboot between GET and POST therefore does not dispatch that old POST.
No automatic replay occurs. A timeout after dispatch is still an unknown
outcome; accepted prior writes are not rolled back. A board/boot/manifest change
clears selection, find and command drafts, and previous-session receipts stay
in Events rather than becoming the new peer's confirmed output.

## Deliberate boundaries and unfinished work

- The optional header pair preserves existing unfenced HTTP clients. Missing
  headers do NOT mean ownership protection. TUI and embedded Web now require
  and pin it; native GUI HTTP now adopts it too. Mobile/standalone CLI still need
  adoption, and native USB/WebSocket messages are not boot-fenced.
- MAC/random correlation is not authentication, encryption, attestation or
  protection against a malicious server spoofing identity. Existing auth and
  same-origin guards remain separate. Login is exempt from mutation fencing.
- The fence binds ESP HTTP dispatch, not the AVR boot epoch. AVR-only reset,
  downstream ACK identity, USB/UART, WebSocket/MQTT, three-node ownership and
  integrated ESP32-S3 firmware remain open acceptance items.
- Initial connection is trust-on-contact, not remembered/pinned board pairing.
  Supplied pairs are checked on GET/HEAD readback as well. Header-free reads
  remain explicitly unfenced. A reboot/rename during an in-flight command cannot
  retract an already dispatched write. Shared multi-step commands have only an
  initial quit/admission cancellation check, but all HTTP writes keep one epoch.
- Tests bind actual production C++ validation and native HTTP request headers;
  fixtures/source checks and firmware compile are not physical acceptance.

Donor session.go's before/after admission closing checks and session_test.go's
matching/mismatched identity/request tests were read in full and adapted to
terminal cancellation and peer matching. Its HELLO/build stamp is not a boot
epoch. Go serial acquisition/quarantine, ACK sequence/opcode routing, DTR policy
and unsolicited-event transport ownership remain useful pending imports; see
the exact omission ledger, not a claim of full session parity.

## Embedded Web adoption

`assets/web/peer-http.js` is mechanically embedded, not a second implementation.
It adapts RayanLamp api.ts's typed error, Headers, no-store/same-origin fetch,
AbortController deadline and finally cleanup. All mutating Web HTTP actions,
including uploads and credential rotation, preflight `/api/status` then carry
the original board/boot headers. Login/logout remain local-auth exceptions.
One pin spans an entire cell batch or settings save; an observed new boot stops
unsent edits rather than silently rebinding the remaining commands. False cell
ACKs stop the batch. No POST or upload is replayed after 401, timeout or loss.

Late responses from a revoked generation cannot populate settings or mapping.
Observed identity loss/change clears selection/Find, mapping drafts, pixel sample,
text/identity/secret edits and remote-job state; it retires the old WebSocket.
Public settings/name verification includes a final same-boot status observation.
Pinned GETs now reject a mismatched supplied epoch at the server too; header-free
reads remain unfenced. This does not prove durability or rule out a later reboot
during the readback window. Accepted is not physical output.

The request deadline is 12 seconds (explicit bounded overrides up to 60 seconds),
and cancellation releases timers immediately. A sent write may still have applied.
The sign-in epoch also prevents an old bootstrap from dismissing a newer login.
Existing distinct-cause incident aggregation and secret-redacted logs are retained;
blank typed codes no longer mask HTTP status codes. Tests execute the actual
shared helper, embedded sign-in app, batch and settings paths with HTTP/DOM doubles.
They are not real LAN, multi-client, reboot, downstream AVR or WebSocket fencing
acceptance. Flutter and non-HTTP native transport adoption, remembered pairing and
integrated ESP32-S3/DMA remain pending. Embedded Web streaming now uses the shared
configurable frame scheduler; see WEB-STREAMING.md. No all-interface FPS parity claim.

## Native desktop HTTP queue adoption

`desktop/src/peer_http.rs` shares strict fence validation and no-redirect pinned
clients with the TUI. The GUI's HTTP queue captures the original board/boot pin
and a revocable generation. Endpoint/credentials changes, observed boot changes
and a current HTTP status failure revoke queued work and device drafts. An
eight-second admission lease prevents old queued requests from later dispatching;
it does not expire an already-dispatched maintenance receipt.

Mutations require fresh status, the same pin and individually acknowledged
capabilities, followed by a second admission check. Every nested settings,
identity, upload and maintenance HTTP request retains one pin. Mapping/public
settings readback gets a final same-boot observation before applying. Unfenced
firmware remains readable; HTTP settings/identity mutation controls are disabled
and the domain POST handler refuses writes. The settings page explains this in
English/Persian. Bootstrap reads may be dropped until the first
identity is adopted. Credential/hostname/self-update operations may intentionally
retire their endpoint and cannot require old-credential same-host readback.

Preflight failures preserve settings/maintenance request IDs so editors do not
remain stuck waiting. Revoked operation receipts remain generic Events entries,
not new-peer drafts/credentials; their bodies are not exposed there. Diagnostic
recovery cannot be inferred from a correlated failed settings result. Client
telemetry POSTs use the actual status pin, not a separate unfenced client.

This adapts the already completely read PCController `session.go` before/after
admission checks, with actual loopback tests for reboot/capability loss, pinned
nested requests, 409 without replay, readback mismatch and cancellation before/
after dispatch. It does not import serial priority/ACK routing/reset/quarantine.
Cancellation inside multi-step maintenance, native WebSocket/USB boot fencing,
AVR-only epochs, remembered pairing and Flutter/standalone CLI remain open.
An old WebSocket status can still temporarily present an old identity; fresh HTTP
preflight/server fences protect HTTP writes, not whole-native transport truth.
ISP probe is a hardware-affecting GET. WebAccess now validates supplied headers
independently of HTTP method, before async queue admission; authenticate() also
rechecks the queued request at loop dispatch before the probe touches SPI/reset.
The actual middleware and async queue are tested with transport/auth/SDK doubles
for stale/malformed/partial pairs, all HTTP methods, auth/origin ordering and
retirement between admission and dispatch. Optional header-free compatibility
still has no ownership protection. This binds the ESP epoch, not an AVR-only boot.
These fixture checks are not hardware, LAN, multi-client or durability acceptance.

Actual Windows framebuffer inspection of the first candidate exposed offline
bootstrap settings preflights adding separate expired/read errors. Configuration
and remote-job reads now wait for fresh selected Wi-Fi status. Offline startup
queues only status discovery, not four redundant settings preflights. This does
not merge distinct transport/cause incidents or erase a Windows Shell failure.
