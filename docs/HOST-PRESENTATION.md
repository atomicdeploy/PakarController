# Host-owned presentations

Check acknowledged `host-presentation` before using this living, unversioned API.
A presentation is a separate display layer: it does not own game state, saved
cell assignments, topology, or correction rules.

## Shared transports

| Surface | Read | Write |
| --- | --- | --- |
| HTTP | `GET /api/presentation` | JSON `POST /api/presentation` |
| WebSocket | normal status/events | `type:command`, `command:presentation` |
| MQTT | normal status/events | existing authenticated command topic, same envelope |
| USB | generic `0x6A` API GET | generic POST `/api/presentation` or `/api/command` |

All writes require existing `board_id`, `boot_id`, 32-lowercase-hex `request_id`
and `expires_at_ms`. Deadlines use the observed board `command_clock_ms`, not the
host wall clock. Same-intent fallback must keep the identical envelope. An unknown
result is not permission to retry with a new ID. Existing auth/maintenance apply.

```json
{
  "type":"command", "command":"presentation",
  "board_id":"<fresh board id>", "boot_id":"<fresh boot id>",
  "request_id":"<32 lowercase hexadecimal characters>", "expires_at_ms":123456,
  "revision":7, "group_layout_revision":3, "owner":"scoreboard",
  "lifetime":"temporary", "ttl_ms":3000, "space":"cells",
  "composite":"overlay", "replace":true,
  "entries":[
    {"id":1,"rgb":65280,"effect":"ring","period_ms":1000},
    {"id":2,"rgb":16711680,"effect":"pulse","period_ms":700}
  ]
}
```

- `revision` is current presentation revision. Edits, expiry and local-control
  retirement increment it; stale edits are rejected atomically.
- `group_layout_revision` fences actual prefix spans. Read `/api/pixels/groups`;
  never infer offsets by multiplying every group by six or twelve.
- `owner` is `[A-Za-z0-9_.-]`, 1–32 characters. It is a label, not a credential.
- `space:cells` means 1-based logical IDs through the current map;
  `space:groups` means 1-based physical groups. One entry or a batch uses the same
  path. `replace:true` starts a fresh layer; otherwise entries patch it.
  Changing space requires replace. `{id:4,enabled:false}` removes one override.
  Duplicate/invalid IDs reject the complete operation.
- `composite:overlay` masks enabled entries onto the underlying frame;
  `composite:replace` clears unselected pixels in this layer.
- Effects: solid, pulse, ring, chase, blink, breathe, rainbow, fade. Motion runs from
  the board clock using shared effects, not per-pixel network updates.
  `period_ms` is 100–10000, default1000; `neighbors` defaults true and `invert`
  false. Ring period must be at least the largest physical group's count so a
  later logical-map change remains valid; 255 ms is sufficient for any topology.
  Fade is a one-shot ramp across `period_ms` from the underlying live frame
  (overlay) or black (replace) to the target. Its exact terminal frame is sent;
  it does not request perpetual animation once finished.
- Maximum request4096 bytes including envelope. For80 fully parameterized styles,
  use bounded patches. GET exposes limits; no hidden transport-size increase.

## Lifetime and recovery

`lifetime:persistent` explicitly saves an independent layer through staged
close/readback/atomic rename. Identical saves skip writes. `saved_revision` is
the durable revision; `revision` also counts volatile changes. Failed saves leave
the old live/durable layer intact. No scene or correction file is rewritten.

`lifetime:temporary` requires TTL250–10000 ms. Acquisition returns `lease_id`;
renew/edit/release must use that lease and owner. A different client cannot steal
it. Renew with `entries:[]`, `replace:false` to retain effect phase or the complete
raw frame. Explicit
replacement restarts its animation. Expiry runs even with output/network off,
restores the durable layer/base scene, and never writes flash.

Release using current revision/layout/owner/lease plus
`action:clear,lifetime:temporary`. Clear the saved layer with
`action:clear,lifetime:persistent`; an active lease must be owned too.

Direct local controls take priority. Assignments, pattern/mode changes, reset,
winner, diagnostics, legacy preview, raw stream and outputOff retire the lease
and suspend the durable layer in RAM. Status shows `suspended:true`. Saved layer
data remains intact for explicit reapplication or reboot restoration. A new
presentation never silently enables a disabled LED output service.

Maintenance visuals are higher priority. Disabled key boxes/unused tail stay
black. Corrections remain the final outgoing transform, applied exactly once.
Saved correction indices and values are never migrated.

## Atomic complete-frame fallback

For a host-rendered mini-app, use `frame_rgb` containing exactly
`used_pixels * 6` RGB hex characters, `replace:true`, `composite:replace`, and
`lifetime:temporary`; omit `entries`. Invalid length or hex rejects the whole
frame. No frame is persisted. Driver, brightness, tail clear and corrections
remain shared. Use receipt/backpressure and wire timing; do not queue obsolete
frames or issue480 individual HTTP requests.

## Readback

Receipt/status `presentation` is a bounded summary. Immutable pixel frames add
`presentation_revision`; wait read-only until it matches the accepted revision,
topology matches and `pending:false`. Submission is not optical verification.
Cached retries retain the original revision/lease. If that lineage has expired
or been superseded, the receipt returns `accepted:false,original_accepted:true`,
`outcome:unknown,error:readback_required`; current state is not misrepresented as
the original ACK. Read state without replaying the mutation.
USB can return explicit `status_partial:true`; merge only after exact board/boot
match, without inventing missing state.

GET additionally returns `saved` and `live` with compact `styles` rows:
`[id,rgb,effectIndex,period_ms,flags]`. `limits.effects` maps effect indices; flag
bit0=neighbors, bit1=invert. Omitted IDs have no override. Both full80-cell layers
fit the existing USB response budget. Write entries remain readable objects.

Developer workflow:

1. Discover, connect, check capability, GET presentation and groups.
2. Send a fresh fenced request over WS, temporary by default.
3. Retain correlated revision/lease. On uncertainty, read state; do not resend
   a mutation under a new ID.
4. Observe matching published frame, or GET `/api/pixels/frame` on demand.
5. Renew only while intentionally active; release on close. TTL covers lost hosts.

A durable scoreboard uses persistent entries only on explicit Save. A highlight
uses a short overlay lease. Host Snake/screensampling uses temporary full frames;
game logic remains outside firmware. Example JS calls through the shared client:

```js
// `send` is the connected client's correlated common-command dispatcher.
const layer = await read('/api/presentation');
const groups = await read('/api/pixels/groups');
const accepted = await send('presentation', {
  revision: layer.revision, group_layout_revision: groups.revision,
  owner: 'scoreboard', lifetime: 'temporary', ttl_ms: 3000,
  space: 'cells', composite: 'overlay', replace: true,
  entries: [{id: 1, rgb: 0x00ff00, effect: 'ring', period_ms: 900}],
});
await send('presentation', {
  revision: accepted.presentation.revision, group_layout_revision: groups.revision,
  owner: 'scoreboard', lease_id: accepted.presentation.lease_id,
  lifetime: 'temporary', action: 'clear',
});
```

## Correction-preview and AVR command additions

Capability `pixel-corrections-preview-command` enables command
`pixel-corrections-preview`, with `configuration` containing existing revision,
enabled/rules/ttl_ms/lease_id/clear fields. Receipt summary
`pixel_corrections_preview`, `corrections_revision`, `corrections_active_revision`
is followed by GET corrections to verify exact `active_sources`. Original HTTP
preview remains supported; transient edits never write flash.

Capability `avr-command-ack` means common commands `audio` (`cue`), `buzzer`
(`frequency`, `duration_ms`) and `outputs` (`mask:0..255`) wait up to150 ms for the
actual AVR handler ACK. Success: `state:acknowledged`, `avr_acknowledged:true`,
`physical_verified:false`. Missing confirmation: `avr_outcome_unknown`,
`outcome:unknown`; never blindly replay it. Legacy HTTP routes share this ACK
helper. The new AVR starts notes and latches output masks before replying.
Outputs additionally return `acknowledged_mask`; `status.active_outputs` remains
the separately observed AVR snapshot and may lag that handler ACK.

Streaming audio keeps the existing silent-prefill/start/circular-buffer
protocol and local musical clock. S3 dispatches queued local input before
optional outbound network housekeeping. Wire time, network scheduling and
hardware load remain real: host fixtures cannot prove zero or acoustic latency.

## Runnable host examples

Only run while authorized to change the table's visible output. These Node.js
examples use the repository's bounded hostname-first HTTP fallback and peer
fencing. They do not install games into firmware, modify saved assignments,
corrections or durable presentation, or replay unknown commands. Existing host
presentations cause refusal; normal scene colors return after release/3s TTL.

```powershell
node examples/host-mini-apps.mjs --origin http://ledcontroller.local --mode snake --steps 40 --run
node examples/host-mini-apps.mjs --origin http://ledcontroller.local --mode memory --run
```

Snake is an autonomous five-cell trail demonstrating host animation over all80
logical cells; it is not an interactive game UI. Memory uses a monotonic ten-second
host deadline after the initial ACK, including renewal network time, then hides
the cells for recall and prints the answer. Initial ACK delay and final hide
request latency mean an exact ten-second optical interval is not guaranteed.
Ctrl+C requests a
safe release only if the previous receipt established ownership. An uncertain
write is left to its TTL, never blindly cleared. Optional authentication uses
`PAKAR_USERNAME` and `PAKAR_PASSWORD`, never command-line secrets. The examples
export pure frame functions and an injectable transport for offline tests.
