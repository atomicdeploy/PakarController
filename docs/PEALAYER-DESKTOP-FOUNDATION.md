# PakarController desktop foundation

PakarController now runs a Pealayer-derived desktop shell. The former single
page and tab strip were replaced with a persistent `egui_dock` workspace.
The native program remains independent of mpv, media playback, timeline and
cue logic.

## Source evidence

| Pealayer source | PakarController source | Integration |
|---|---|---|
| `src/ui/dialog.rs` | `desktop/src/ui/dialog.rs` | Production dialog geometry, opaque frames, action bars, rail navigation, responsive setting rows, numeric input and scrolling helpers imported. Upstream tests that depend on Pealayer-only dialogs are excluded. |
| `src/ui/icons.rs` | `desktop/src/ui/icons.rs` | Full production icon vocabulary, searchable list/grid picker, icon names, contextual disclosure controls imported. Used by workspace profiles and shell controls. |
| `src/ui/color_picker.rs` | `desktop/src/ui/color_picker.rs` | Semantic swatches, editable HEX field and popup imported. Used by Appearance preferences. |
| `src/ui/elegance_theme.rs` | `desktop/src/ui/elegance_theme.rs` | Production theme adapter imported with the configuration type mapped to Pakar. Section cards follow the active palette without replacing egui's style. |
| `src/ui/i18n.rs` | `desktop/src/ui/i18n.rs` | Production native font selection, Phosphor registration, bundled Persian fallback and translation helpers imported. Pakar-specific captions still need a full translation pass. |
| `src/messaging.rs` | `desktop/src/messaging.rs` | MessageHub imported with upstream tests: bounded notifications, stable IDs, expiration, dismissal and validation. |
| `src/ui/toasts.rs` | `desktop/src/ui/toasts.rs` | Root overlay imported; only configuration access is adapted. Device commands publish into this hub. |
| `src/ui/layout.rs`, `src/app.rs` | `desktop/src/ui/layout.rs`, `desktop/src/ui/shell.rs` | DockArea/TabViewer composition, finite rectangle sanitization, panel reveal, tab context menus and empty workspace recovery adapted to controller panels. |
| `src/ui/preferences.rs` | `desktop/src/ui/preferences.rs` | Draft/saved/preview lifecycle adapted: native or embedded host, search, category rail, body, footer, Save, Cancel and discard confirmation. Controls use the imported dialog primitives. |
| `src/ui/workspace_profiles.rs` | `desktop/src/ui/shell.rs` | Profile manager actions adapted: save, rename, restore, overwrite and delete; layouts use stable IDs and the imported icon picker. |
| `src/platform/windows.rs`, `src/platform/interop.rs`, `src/startup.rs` | `desktop/src/platform/instance.rs` | Windows named mutex and named-pipe request/response loop adapted. Launch races are serialized; subsequent launches forward typed commands to the owner. Requests reject unknown fields, invalid panels and remote/session-mismatched clients. |
| `src/platform/windows.rs` | `desktop/src/platform/windows_jump_list.rs` | Jump List publisher imported, with Pakar's AppUserModelID and controller actions. Media recents category is excluded. |
| `src/platform/windows_quick_actions.rs` | `desktop/src/platform/windows_quick_actions.rs` | Native task/resource-ID contract adapted to Connect, Preferences, Workspaces, About and Quit. |
| `src/platform/windows_shell_icons.rs`, `build.rs` | `desktop/src/platform/windows_shell_icons.rs`, `desktop/build.rs` | Exact Phosphor glyph rasterizer and multi-resolution ICO generator imported. Used for PE resources, task icons, window icon and tray. |
| `assets/themes/palettes.json`, `ui-colors.json`, `assets/fonts/Vazirmatn-Regular.ttf`, `LICENSE` | Matching assets and `desktop/licenses/Pealayer-MIT.txt` | Original assets and license preserved. |

The source hash and revision inventory is `legacy-source-ledger.csv`. Manual
decisions are `reuse-decisions.csv`. A screened file means it was inventoried,
hashed and searched; it does not mean every function was manually integrated.

## Running application

The initial workspace contains Table, Effect controls, Front panel, Hardware
monitor, Network & cloud, Streaming and Events. Panels can be dragged, split,
closed and detached. Workspace profiles and the last dock layout persist under
`%LOCALAPPDATA%/Programs/PakarController/config`.

Connection, mapping, game reset, About, workspace management and Preferences
are real dialogs. The front-panel preview renders the board's segment bytes.
Cells are logical row-major IDs with a half-cell offset on odd columns; physical
mapping and names come from the device. Old status expires from the live display.
Alternating theme-aware odd/even column bands sit behind the live cell colors.
Ctrl+F opens exact logical-number search: lists and ranges such as `3,12,20–28`
highlight matches without hiding cells or sending LED commands. Amber outlines
mark every match, with the active cell also outlined in the selection accent.
Enter/F3 selects the next match, Shift+F3 the previous; Escape in the search
field or the clear button removes highlighting. Renamed cells retain a logical
ID badge. `--find-cell "3,12,20-28"` forwards lookup to the running owner.
The network editor reads the board's current public settings; secrets remain
write-only. On the current firmware, enter the Wi-Fi password when applying
network changes.

Default single-instance mode uses one owner per Windows session. Tray restore,
launch forwarding and UI actions share the same dispatcher. Useful launch
arguments are `--preferences`, `--workspaces`, `--connect-dialog`, `--about`,
`--panel "Hardware monitor"`, `--app-status` and `--quit`.

## Verification and remaining parity

Sixteen Rust tests cover protocol encoding, packet bounds, discovery parsing,
notification lifecycle, palette completeness, draft preview isolation, dock
serialization/reveal, command validation, Windows owner exclusion and exact
cell search parsing/bounds/navigation.
Live IPC also verified a second launch opened Preferences in the same process.
The app received real device status and WebSocket events, and screenshots were
captured for the workspace, native Preferences and Pealayer reference.

This foundation does not establish every requested Pealayer feature as complete.
Remaining parity includes configurable/global application shortcuts, a common
native/Web preferences contract, full Pakar caption translations, complete
workspace snapshots beyond dock placement, desktop peer updating and richer
diagnostic/telemetry views. The broader firmware, mobile and hardware release
acceptance remains separate from this desktop-shell delivery.
