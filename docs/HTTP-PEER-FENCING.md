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
  headers do NOT mean ownership protection. GUI/Web/mobile/standalone CLI must
  still adopt this policy; only the TUI currently requires and pins it.
- MAC/random correlation is not authentication, encryption, attestation or
  protection against a malicious server spoofing identity. Existing auth and
  same-origin guards remain separate. Login is exempt from mutation fencing.
- The fence binds ESP HTTP dispatch, not the AVR boot epoch. AVR-only reset,
  downstream ACK identity, USB/UART, WebSocket/MQTT, three-node ownership and
  integrated ESP32-S3 firmware remain open acceptance items.
- Initial connection is trust-on-contact, not remembered/pinned board pairing.
  GETs are unfenced readback. A reboot/rename during an in-flight command cannot
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
