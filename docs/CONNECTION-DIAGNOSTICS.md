# Connection diagnostics and repeat handling

Native events previously compared only `self.message != e`. Alternating HTTP
and WebSocket failures therefore defeated suppression and displaced distinct
history. Browser suppression keyed only the request category, dropping distinct
HTTP/close codes inside its three-second window, then flooding again later.

Native now keeps bounded, memory-only incidents by source and full cause
fingerprint plus displayed cause. Interleaved repeats increment the original
row's occurrence count and last-seen tooltip, not a new row. Different causes,
HTTP routes and USB ports remain separate. Explicit command/settings/maintenance
outcomes are never suppressed: two requested writes are two outcomes even when
their error text matches. History256/incidents128; eviction safely rearms reporting.
Clearing events rearms diagnostics. Changing peer/auth/port closes affected
incidents without falsely claiming recovery. Late peer/auth/port observations
are still rejected before updating the ledger.

Matching successful HTTP reads, WebSocket handshakes and USB opens close only
that source and emit one recovery summary with its occurrence count. Transport
recovery is not peripheral ACK or fresh board status. Actual WebSocket/USB
online-to-offline transitions remain separate from retries; device/AVR freshness
still owns capability availability and its existing connection notice policy.
Streaming/sampling stop on WebSocket loss and do not automatically resume.

HTTP status has at most one queued/in-flight request for the selected endpoint,
with2..30second failure backoff and reset on successful read. WebSocket retries
back off600ms..15seconds; commands interrupt the wait. No device mutation is
replayed by either mechanism. All incident state is transient: no log SSD writes.

Web adapts the actual imported RayanLamp keyed logger rather than adding a
second logger. Incident keys now include severity/event/sanitized detail/error
code; GET failures stay quiet until that route succeeds, and WebSocket failures
until its handshake succeeds. New error codes remain visible. Recovery includes
the coalesced occurrence count. Explicit POST failure outcomes stay visible.
Existing redaction, metadata allowlist, bounded maps and stale socket ownership
remain. Browser error-code-only diagnostics deliberately do not log raw bodies,
credentials or server messages; indistinguishable browser network failures cannot
be reclassified as different causes without actual metadata.

Donors consulted: full Pealayer messaging.rs (actual existing unchanged import),
ui/error.rs and network.rs; actual RayanLamp browser-console.ts and its full tests.
Pealayer four_d/engine.rs transport-rejection/mark-disconnected slice confirms
that operation rejection and connection lifecycle are different concerns; it is
not a complete-file comparison or an imported media engine. Native incident
tracking/backoff is the controller-specific adaptation, not a claimed byte-copy.
The same authoritative file ledger retains all remaining donor-wide imports.

Proof: native diagnostic unit tests, typed-source/late-peer tests and actual
shipped Web logger/API/socket VM tests. The first new browser count assertion
expected20 but correctly observed21 including the prior401; corrected the count
with its explicit explanation, not by removing the recovery assertion. Native
build/live deployment and real board reconnect acceptance must be reported
separately. Guarded native Test passed138/0/0 in the existing release cache
(2m41compile,0.35stest). Browser production-module/API/socket VM assertions
passed. Firmware and mobile were not rebuilt for this host/Web change. Release
replacement and live offline counters are the next acceptance, not inferred from
unit tests. The separate tray initialization error remains unresolved.

## Completed host and source acceptance

Native source2af7358 passed138 guarded native tests. Root-cause refinement
67948be passed all6 focused pure diagnostic tests (829440-byte stable harness,
no repeated GUI test relink) and the guarded release build/stage/CLI probe.
HTTP errors now expose bounded underlying DNS/TLS/socket causes before incident
comparison, rather than only reqwest's generic outer request description.

Production67948be is running locally as PID58400, version0.2.7, executable SHA256
b5133dc91bddb3a70b186f4e179514739a1981e9c604ed5ca39dd49e1b38aebb.
Replaced PID77020 through graceful IPC, with no input or force kill. Preferences
remained byte-identical (22f8aeed4bf0cebc3116c670f681d90a3d1b1b833ece3de23a491d0d896a8711).
Both prior packages were moved into verified rollback paths; five original
rollback executables were preserved. Actual inspected1999x1453 internal capture
shows the underlying DNS error and repeat badge, not generic HTTP-only text.
The earlier2af7358 live snapshots retained7 rows while repeat counts rose5→8;
final-package offline observation is recorded in the output receipt separately.

Final67948be live snapshots retain8 event rows/7 active incidents while repeated
occurrences rise6→11; status backoff reaches30seconds. Final inspected framebuffer
shows DNS and timeout on the same status route as distinct causes, each with its
own repeat count. That extra timeout is intentionally retained, not misreported
as a deduplication failure. Five original rollback hashes were rechecked unchanged.

The full local repository-quality phase passes after correcting two exposed
fixture/certificate issues: reconnect VM now loads the actual shared logger;
two stale PU Web target hashes were rebound after verifying unchanged line244
route controls. All existing reconnect/session/no-replay and50 route assertions
remain. Added continuing503, distinct500 and one handshake-recovery assertions.
Reconnect preparation now uses the WebSocket incident owner too, so no timed
quiet-window flood remains on that path. CI run38107886274 failed before those
repairs; no all-green CI claim or CI wait. New source CI proceeds asynchronously.

No firmware/APK build or board write. Web is source/VM verified, not deployed
to the offline board. Real device loss/reconnect/USB and Android acceptance
remain separate, as do the Windows tray error and glib advisory.
