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
