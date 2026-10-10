# Front-panel transport dropdowns

The former WebSocket and USB badges are real native popup controls, available
even while no board is acknowledged. Device offline/connected remains factual.

- WebSocket: current endpoint and live state; select Wi-Fi, USB or automatic
  command routing; reconnect; open endpoint/authentication settings.
- USB: current port and live state; the same routing choices; lazily enumerate
  actual OS ports, select a port, disconnect, reconnect, or open settings.
- Port changes persist only on an actual choice. Selecting the current value
  is not another write. Enumeration failure is displayed rather than concealed
  as an empty scan. Missing configured ports are labeled unavailable.
- Identity writes prevent port replacement/disconnection/reconnect until complete.
  No dropdown fabricates device capability ACKs. Automatic routing keeps both
  connections and picks one destination; it never duplicates a mutation.

Actual donor comparison: Pealayer `src/ui/menu.rs`'s connection cluster uses
`selectable_value`, persisted endpoint changes and a custom OS-path fallback.
Pakar adapts those real mechanisms in `ui/connection_controls.rs` and the existing
connection dialog. It uses the imported Phosphor catalog and egui menu styling,
not its media controls. The donor's single coordinator worker cannot be copied
as two independent connection workers. The same-endpoint WebSocket reconnect
request now exits the old socket loop; previously that request was ignored.

Pealayer `src/ui/status_bar.rs` was reread fully. Its per-item visibility/context
menus are retained in Pakar's status bar; the requested controls live in the
front-panel header. Timeline/media timing and E-stop-specific dependencies are
not part of this change. Useful status lifetime, physical RGB, readiness and
localization differences remain explicit in the existing comparison ledger.

Verification: Node source wiring guard and Rust parsing are not a compiled app.
The Rust popup/pointer regression test exercises offline menus, separate popup
identities, lazy/deduplicated enumeration, and actual port selection. Execute it
through the guarded native Test wrapper before native/runtime acceptance.
