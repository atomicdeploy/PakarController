# Web streaming policy

The embedded Web controller no longer uses a fixed 40 ms (25 FPS) timer.
Target FPS defaults to 60, accepts 1–240, and accepts 0 for uncapped pacing.
Uncapped means no application interval or display/vsync cap; browser timer
clamping, background throttling, Wi-Fi, socket buffering and LED wire timing
still apply. This does not promise 60 FPS physical refresh.
New labels, hints and dynamic receipts use the shared Persian/English catalog;
changing language refreshes a receipt without translating board-provided values.

The browser stores this client policy in `pakar-stream` only when it changes.
It is not an ESP device parameter. Native streaming already has 60/default,
0/uncapped pacing; Android streaming and complete cross-interface preferences
parity remain separate acceptance work. Do not present this as all-client parity.

Buffered frames defaults to 2, accepts 1–16, and bounds the socket send window
in whole 1,460-byte table frames. Each frame has two ordered 730-byte chunks;
only the second requests commit. When the next complete frame would exceed the
window, skip it rather than accumulating animation history. Animation hue uses
elapsed time, so slow callbacks neither slow the animation clock nor replay a
catch-up burst. Frame work counts toward the target interval.

Start requires an authenticated, fresh, acknowledged Table + Streaming peer,
an open current socket and a supported board/boot identity. A fresh HTTP status
check pins ownership before starting. Each send revalidates observed peer/socket
ownership. Capability loss, reboot observed in status, disconnect, explicit stop,
hidden document and page departure stop the stream. An uncertain partial send
is not replayed. Unknown socket buffering stops rather than sending unboundedly.
Late cancelled callbacks cannot disarm a replacement generation's timer.

The status line measures browser sends, skipped attempts and socket queued bytes,
not board acknowledgements or LED refresh. Its rate is frozen after stop. Already
queued packets cannot be recalled. Server WebSocket boot fencing, multi-client
arbitration, frame ownership on reconnect and physical buffer/refresh stress are
still open; HTTP fencing does not establish any of those automatically.

## Actual donor reuse

Read PCController `Tools/Controller/web/src/status-frame-batcher.ts` and its
`.test.ts` fully. Adapted the actual injected-clock, generation admission,
single scheduled callback, cancellation and disposal structure into
`assets/web/frame-stream.js`. The existing generator embeds that exact source;
tests compare the embedded bytes. The donor fake clock deliberately retains
cancelled callbacks; adapted its stale-generation/disposal scenarios and extended
them with pacing, backpressure, peer loss and partial-send cases.

The donor retains all status arrivals for metric history. LED commands instead
use latest-only frames to avoid replaying stale actuations. Generic metric history,
`controllerSnapshotIdentity`, `applyStatusFrameBatch` and their error/history
tests remain pending, not excluded as business logic. I2C sensor metrics are
hardware-gated while no such sensors are connected; they remain useful for future
boards. Donor React/TypeScript setup is not added to the standalone embedded Web
page. The stronger existing board/boot fence replaces build-name identity for
this bounded stream slice, without claiming the whole donor module was imported.

New `web-streaming` donor gate participates in quality and ESP build preflight.
The authoritative file-comparison ledger records both hashes, actual consumers,
tests and omissions. A successful compile is not a flashed image or physical test.
