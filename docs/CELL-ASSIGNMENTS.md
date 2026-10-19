# Board roles and cell assignments

Each firmware build selects one business program:

```powershell
./tools/build-firmware.ps1 -Target esp32s3 -GameMode table-selection
./tools/build-firmware.ps1 -Target avr -GameMode table-selection
```

`-GameMode first-wins` selects the other program. Both processors in a pair
must use the same role. Direct builds use `PAKAR_GAME_MODE=1` (table selection,
default) or `2` (first wins); other values fail compilation. The wrapper's normal
flash/PSRAM/pin/link/backend parameters still apply. Changing role requires a new
firmware build, not a runtime mode switch. Rainbow, streaming and pixel
diagnostics remain presentation services rather than additional game programs.

The board reports `capabilities.role` and its single baked mode in `modes`.
Clients filter controls by that role and the individual acknowledged features.
An unknown explicit role cannot acquire table-selection controls. Older
role-absent manifests use their advertised mode/capabilities, not a hostname or
firmware-version adapter. Connection loss never grants command authority.

## Red-cell lock

`red_lock` is a persisted board setting. With it enabled, changing an assigned
red cell directly to white is rejected, including a bulk import/reset containing
that change. The entire requested batch is rejected before any cell changes.
Off remains available; this is an assignment lock, not an electrical interlock
or a prohibition on diagnostic/effect pixels. Unlocking does not recolor cells.

```http
POST /api/cells/lock
Content-Type: application/json

{"enabled":true}
```

Read `red_lock` from `/api/status` or `/api/cells/assignments`. UI controls use
the board's observed value, not optimistic checkbox state.

## Import and export

The ESP Web UI and the Rust app's local bridge expose the same unversioned API:
For the Rust bridge, enable **Preferences → Connection → Share connection**;
sharing remains opt-in and does not make USB depend on Wi-Fi.

```http
GET /api/cells/assignments
```

```http
POST /api/cells/assignments
Content-Type: application/json

{"assignments":[{"cell":1,"state":"red"},{"cell":80,"state":"off"}]}
```

Normal configured peer authentication, board/boot fencing, client introduction
and command ownership still apply. URL-encoded `assignments=<JSON array>` and
`enabled=true|false` are supported too. WebSocket commands are
`cell-assignments` (typed `assignments` array) and `cell-lock` (`enabled` bool),
using the existing correlated command envelope and readback. USB uses a single
boot-fenced atomic bitmap/packed-state packet, not per-cell fan-out.

Imports contain 1–80 unique logical IDs; omitted cells are unchanged. States
are `white`, `red`, or `off`. File codecs also accept their exact hex equivalents
`#FFFFFF`, `#FF0000`, and `#000000`; arbitrary RGB and physical remapping are not
silently interpreted as assignments. Invalid IDs, duplicate assignments,
formulas and unsupported colors reject the whole file before dispatch.

The common JSON file is:

```json
{"role":"table-selection","assignments":[{"cell":1,"state":"red"}]}
```

CSV and Excel use columns `cell,state`. Rust and Flutter import/export genuine
`.xlsx` workbooks, plus JSON and CSV. The embedded Web UI imports/exports JSON
and CSV without a CDN or external parser. Exports require all 80 known observed
states; unknown data is never exported as invented Off/white. Importing opens
a preview and requires Apply; picking a file does not actuate the board. A
changed board/boot revokes the preview. Uncertain writes are not auto-replayed.

Source/fixture/build checks are not deployment or optical proof. These changes
remain subject to the current production hold and their own live acceptance.

## Verified source checkpoint

- Both ESP32-S3 roles and both AVR roles compiled through the actual SDKs.
  Final canonical outputs are table-selection. S3 profile: 16MB/OPI/buffered,
  static backend, BLE disabled, verified ISP, optional Ethernet.
- Final S3 BIN: 2,282,608 bytes, SHA256
  `be7481027e50739c45c92c2bb423b315cf2f2123f772a8a31dea4023d88a1c59`.
- AVR table-selection HEX SHA256:
  `a0fc89bf0e11854f66494b09c19b24825acd643afaf6c6bc1df39f2cb09b398d`.
  Only 258 bytes of AVR application flash headroom remain.
- 47 distinct native checks and 24 Flutter checks passed; Flutter analysis is
  clean. Web codec/atomic-handler, context, batch, basic-controller, peer-fence,
  compression and localization fixtures passed. Firmware fixtures cover lock
  persistence, atomic rejection, chase exclusion, role migration, queue
  re-entry/backpressure, full 80-cell MQTT and discovery metadata.
- The legacy ESP8266 build links after its in-place queue correction, but size
  reporting is 80,232/80,192 nominal DRAM bytes and 96% IRAM. This is a **legacy
  reuse/deployment blocker**, not a healthy-runtime or OTA acceptance result.
- No board, running app, tablet package or automatic update feed was changed.
  Installed-client, actual HTTP/USB/WS board round trips, physical keypad/LED
  behavior and reboot/persistence acceptance require a separately approved
  production update window. Flutter's new labels currently remain English;
  a shared mobile localization layer is still a separate open parity item.
