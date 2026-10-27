# DRS Snake — external table mini-app

Tracked in [PakarController #30](https://github.com/atomicdeploy/PakarController/issues/30).
This directory is removable. It is not part of the board Web bundle, Flutter
workspace, or Rust panels. Normal board menus contain no Snake entry.

## Run and open (no LED changes)

From the repository root, with Node.js installed:

```powershell
node mini-apps/snake/server.mjs --origin http://ledcontroller.local
```

Open `http://127.0.0.1:18490/snake`. Opening creates no device connection.
Start defaults to **host display only**. Explicitly check **Present on LED table**
before Start to acquire a temporary board presentation. Respect any production
approval hold before doing so. Authentication, when required, uses the existing
`PAKAR_USERNAME` / `PAKAR_PASSWORD` environment variables, never URL credentials.

The server is loopback-only, with an exact Host/origin check and random local
RPC token. Do not expose it publicly by removing those checks. Up to eight browser
controllers can observe the same game. Node owns simulation; browsers render it.
Android can control that host, but does not yet run a standalone local Node host.

### Hidden entry points

* Rust source: `pakarcontroller.exe --snake`; existing typed IPC accepts
  `{"command":"snake"}`. These open the external URL, not a game round.
* Android source: `pakarcontroller://mini-app/snake` opens the same external UI.
  With an explicitly selected authorized device and the server on its ADB host:

  ```powershell
  adb -s DEVICE reverse tcp:18490 tcp:18490
  adb -s DEVICE shell am start -a android.intent.action.VIEW -d pakarcontroller://mini-app/snake ir.digitalogic.pakar.pakarcontroller
  ```

* Web: `/snake` on the external server; no hidden alternate board Web application.
* CLI/RPC: `node mini-apps/snake/cli.mjs status|start|pause|resume|restart|stop`;
  `direction up|right|down|left`. Start/restart are host-only unless `--board` is
  explicitly supplied. `open` prints the URL without starting anything.

Rust/Android entry points require updated builds. Merely adding source does not
update an already running native app or installed tablet APK.

## Mechanics and editor

Wraparound in both axes; one queued turn per tick prevents immediate reversal.
Configure speed, seeds, growth, no-game-over and autoplay. Edit stopped maps with
obstacles, two mirror types, reverse tiles, paired portals, obstacle switches and
autoplay-stop potions. JSON import is validated as data; never executable code.
Start, pause, resume, restart, directions and stop are shared across controllers.
Keyboard arrows/WASD, Space, touch swipes and visible direction buttons work.

The physical table has 20 staggered columns × 4 rows. Logical IDs follow
`1 + floor(x/2) + (x%2)*10 + y*20`: 1/21/41/61 are vertically aligned;
11 is immediately right and half a cell lower. Firmware owns physical pixel
spans, wiring mapping and corrections. The game never multiplies IDs by six.

## Board protocol and failure behavior

Uses the existing [host presentation contract](../../docs/HOST-PRESENTATION.md),
not a second game transport. Require acknowledged `host-presentation` and
`table-selection`, an 80-group layout, board/boot identity, revision and clock.
Send temporary logical-cell frames through WebSocket first, HTTP if no socket
is available before a send. An uncertain command is **not** replayed through a
second transport. One in-flight frame, no growing frame queue.

Frames carry a 3-second lease, owner, request ID, expiry and layout revision.
Pause renews the lease; Stop releases it. Disconnect/no observers terminates the
round. Unknown outcomes expire naturally rather than risking another owner's
scene. Saved colors, mappings and correction records are untouched. Actual LED
output still needs a separately authorized live acceptance test.

## Optional board-owned engine (OFF in current builds)

`PAKAR_BOARD_SNAKE=0` is the default. The canonical firmware build accepts an
explicit `-BoardSnake` only for ESP32-S3 table selection; keycontroller rejects it.
No current production candidate is built with that switch.

When compiled, the independent engine additionally requires an explicit runtime
`snake` command with `action:configure, enabled:true`; that opt-in is saved in
`/snake-enabled.txt`. The common peer-fenced `/api/command` / WebSocket / framed
USB command paths carry `command:snake` and actions `start`, `pause`, `resume`,
`restart`, `direction` (0 up, 1 right, 2 down, 3 left), `stop`, `status`.
Optional `interval_ms` is 80–2000. Every mutation uses normal board/boot/request/
deadline admission. Status contains `snake`; it is not a discoverable game mode.
The engine uses the shared temporary presentation layer and stops if ownership
or topology changes. Its current gameplay is the basic one-seed engine; richer
map mechanics belong to the external host implementation.

## Donor provenance and honest remaining work

Adapted from David Refoua's local DRS-jSnake sources, including the primary
`DRS-JSnake.html`, demo/map variants, orientation, two-player and network variants.
Original attribution: **DRS David Soft / David Refoua**, all rights reserved;
the supplied Readme explicitly invites modification. No broader license is invented.
Exact original-byte hashes and file-specific decisions are in the existing
`docs/legacy-source-ledger.csv` and `docs/file-comparison-evidence.json`.

Reused mechanics: body/tail movement, growth/seed settings, toroidal wrapping,
pause, directional input, map designer, mirror/reverse/portal/switch/potion tiles,
autoplay and no-game-over. Replaced per-pixel DOM dragging with responsive Canvas;
replaced eval-based maps, unauthenticated PHP shared files and polling with bounded
JSON, a single authoritative simulation and event-stream observers.

Still open in #30: two independently controlled snakes, motion/tilt input with
modern permission handling, lossless larger donor maps in a host-only arena,
archive-versus-loose variant review, standalone Android-host execution, packaged
native/mobile launcher deployment and physical LED acceptance. These useful
features are **not** declared excluded or complete.

Verification: `tools/test-snake.ps1` compiles the portable board core and runs
host/HTTP/WebSocket loopback tests. Screenshots are host UI evidence, not board
or production acceptance.
