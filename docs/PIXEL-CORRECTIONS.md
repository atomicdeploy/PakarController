# Pixel corrections and diagnostic workbench

ESP32-S3 build option: `-PixelCorrections enabled|disabled` (enabled by default
in the canonical S3 builder). The compiled capability is `pixel-corrections`.
Rules are owned by each board; deploying support does not invent or enable a rule.

Rules run in listed order over the intermediate output address map:

Each rule has an optional `enabled` boolean (default `true` for existing records).
Disabled rows keep their values and order but do not change the output map.
The `pixel-correction-rule-toggle` capability advertises this behavior. Clients
must not silently drop disabled rows when talking to older firmware.

- `black`: retain the slot, emit black there.
- `remove`: discard that slot's value, compact subsequent values toward the
  beginning, and emit black at the tail.
- `shift`: move the suffix by a signed pixel offset. Positive moves toward the
  physical end; negative toward the beginning. Fill vacated suffix slots black,
  clip out-of-range values, and preserve the prefix except where moved values
  overwrite it. Zero is a no-op.

The frame always contains the board's full configured pixel count. Rules do not
change logical cell mapping, saved scenes, raw pixel telemetry, or LED protocol
timing. They run after the static six/twelve-pixel grouping stage and before the
S3 output worker copies its complete frame. Explicit Off remains complete black.
This cannot repair missing power, a broken data path, or a failed LED that does
not forward data.

`GET /api/pixels/corrections` returns enabled, revision, rules, pixel count and
the applied destination-to-source map (`-1` is black). Addresses in the API are
zero-based; editors display one-based pixel numbers. `POST` takes that revision,
enabled and rules as JSON, or a JSON string in the `configuration` form field.
At most 32 rules are accepted; invalid indices/offsets and stale revisions reject
the entire transaction. Storage uses a private checksum, pending-file exact
readback and atomic rename. The saved map changes only after durable commit;
unchanged saves and animation frames do not write flash. Each board owns its
`/pixel-corrections.json` on LittleFS.

## Temporary preview and diagnostics

`pixel-corrections-preview` advertises temporary mapping previews. POST
`/api/pixels/corrections/preview` accepts the complete configuration and saved
revision, plus `ttl_ms` (250–10000, default 3000). The first request returns a
nonzero `lease_id`; updates and clearing require that lease and base revision.
Clear with `clear:true`. GET corrections keeps saved rules unchanged and reports
`preview` and `active_sources`. Expiry restores the saved map without a flash
write. Another editor cannot overwrite an active lease or stale revision.

Flutter previews locally as a rule is adjusted. Live preview is opt-in, debounced
and verified by readback; it expires after ten seconds without another adjustment.
Leaving the tab or backgrounding requests cancellation; expiry is the recovery
path if connectivity is lost. Save is the explicit persistent operation.

Diagnostics use the same `/api/led/diagnostic` command and board render clock.
`pixel-diagnostic-effects` adds `solid`, `blink`, `breathe` and `rainbow`, with
bounded `period_ms`, `duty_percent`, `min_brightness`, `max_brightness` parameters.
`pixel-diagnostic-overlay` adds `composite:overlay`; `clear` temporarily masks
the other pixels. Neither replaces the saved scene. `enabled:false` restores the
underlying scene. Effects and overlay RGB previews require published frames,
not invented colors from the requested configuration.

Flutter links Diagnostics/Corrections tabs and the held-cell physical-pixel
inspector. Both reorder arrows, per-rule checkboxes, bounded +/- fields,
diagnostic-address shortcuts and address diagrams are available. Rust retains
its existing linked editor; parity for these new workbench controls remains
pending. Flutter correction configuration is network-only; USB/BLE correction
editor parity remains unfinished. Advanced diagnostics use the shared WebSocket,
HTTP or capability-gated generic USB API. USB returns a bounded same-peer partial
receipt, preserving diagnostic identity/revision without truncating JSON.

Reuse reviewed: PCController `Project/SettingsStore.cpp` and `.h` (complete
reads), plus the existing adapted `SceneStore.h`. Retained MCU ownership,
validated records, unchanged-write avoidance and safe defaults. Replaced AVR
EEPROM addressing with the existing LittleFS staged transaction pattern. Pixel
remapping is new project-specific code, not claimed as a donor import.

The current workbench source, package and deployment evidence is recorded in
`REUSE-PREFLIGHT-RECEIPT.md`; earlier candidate receipts are historical, not proof
of the current installed image. Source tests are not physical LED proof.
