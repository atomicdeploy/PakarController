# One living API contract

Tracking: [PakarController issue #16](https://github.com/atomicdeploy/PakarController/issues/16).

PakarController follows the user's unversioned policy established in the
PCController chats, not the older API names retained in this alpha project.

- Routes are `/api/status`, `/api/device/name`, `/api/firmware/fetch`, etc.
  Every numbered root namespace (`/api/v1`, `/api/v2/...`) returns **404**,
  before authentication or handlers. There are no aliases, redirects or
  version-negotiation fallbacks.
- JSON capabilities, identity, telemetry, client reports, firmware jobs and
  manifests have no product-generation selector. The firmware hardware profile
  is `pakar-nodemcu-atmega328p`; the outbound WebSocket subprotocol is
  `pakarcontroller`. Inbound WebSocket remains `/ws`.
- Features and modes are explicitly acknowledged by the peer. Optional additive
  fields may be ignored; missing/malformed critical identity, geometry, capability,
  authentication, ownership, replay, deadline and integrity fields still reject
  the affected operation. Removing a selector does not authorize absent hardware.
- USB/AVR COBS+CRC8 framing retains its size and field offsets. The former
  envelope/HELLO/context/telemetry selector slots are fixed reserved zero.
  Nonzero reserved bytes are rejected, not interpreted as another generation.
  Streaming uses stable `PKAR` magic, reserved zero bytes 4/5, and checked flags,
  exact size and pixel range. There are no readers for old numbered envelopes.
- Firmware/application `version`, component build versions and monotonic
  identity `revision` remain: these are diagnostic identity/update-safety data,
  not selectable API contracts. Standard protocol versions such as JSON-RPC 2.0,
  HTTP, MQTT and UPnP remain where their standards require them.

Audio availability now uses the AVR HELLO's explicit cue count and buzzer ACK,
not a comparison against the AVR software version. Locale BroadcastChannel
identity is also the stable unversioned `pakar-locale`.

## Evidence and exact adaptation

Read live Codex chat **PCController — Host Runtime, Networking & Automation**
(`019fb4b1-3642-7d21-a49e-218d33582eab`), including turn
`01a0e523-b880-79f0-94f4-86df9763a910`: the owner clarification was alpha,
no legacy contract, aliases/readers/fixtures removed. Also consulted
**PCController · WebUI, TUI & Client UX**, **PCController — Firmware, Board
Engine & Protocol**, **Merge PCController PRs for Pealayer**, and
**Inventory Pealayer task state**. These are historical context, not authority
to mutate those chats or donor projects.

[PCController issue #46](https://github.com/atomicdeploy/PCController/issues/46)
specifies the living IPC/REST/JSON-RPC/WebSocket/bridge API and distinguishes
standards-required wire markers from product API versions.

Actual donor `Tools/Controller/docs/Go-Embedding-API.md` was read completely.
`internal/ipcjson/rest_routes_test.go` was read completely; its
`TestVersionedCoreAPIRoutesAreRejected` cases are adapted into
`tests/protocol_test.cpp`. The top-level 404 guard in `ipcjson/ipc.go` was
read at function scope and adapted into `ApiPolicy.h` plus the async middleware.
**The large ipc.go file was not read completely and remains pending in the
whole-file ledger.** Pakar generalizes its guard to all numbered namespaces.

## Deliberate preservation, not API compatibility

Existing checksum-protected LittleFS/EEPROM binary layout bytes and the existing
identity/network settings migration are preserved. Changing byte offsets or
erasing those records would destroy saved Wi-Fi, credentials, board identity,
cell mapping, safety latches or AVR settings. They are private storage validation,
never routes or API negotiation; AVR telemetry labels the field `storage_layout`.
Existing remote journals are read by their actual job/phase/target/hash fields,
not a generation gate; no interrupted operation is replayed.

Imported PCController hardware-specific binary payload layouts are distinct
opcode/profile contracts, not selectable REST generations; this change does not
silently rewrite donor EEPROM, fuse or hardware semantics.
Repository-only donor-ledger/report format checks are internal audit tooling,
not board/client API fields. They are intentionally unchanged.

## Prevention and acceptance

`tools/check-living-contract.mjs` inspects every tracked shipping source in its
declared scope. CI also runs negative guard fixtures, all native/Android contract
tests and both firmware link variants. Actual C++ route rejection/reserved-byte
tests exercise the helper used by the middleware; Web tests execute shipped JS.
Add capability IDs and optional fields to this single contract, not another
generation. Record local/CI/build/device evidence separately.

Deploy both firmware peers and clients as a coordinated update. Installing only
a new client against old firmware is not an accepted migration strategy.
