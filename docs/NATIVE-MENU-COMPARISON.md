# Native menu comparison: actual code, not visual inference

Pealayer read snapshot: clean `3d5f7ae324d1d8fa16752e6cbe2ecbe661becc77`.
Live inventory refreshed to clean `8d9bc1d86988987354e373a049ef953f9cf3034d`;
the two menu/appearance source hashes are unchanged. Two newly added donor
verification/doc files remain inventoried and pending, not silently skipped.
Read `src/ui/menu.rs` (through EOF), `src/ui/mod.rs` (through EOF), and the
relevant egui 0.36.2 Button/menu/Spacing implementation. Both clients use egui
0.36.2; this is not a framework-version mismatch. The attached image is the
user's running native menu, not a newly captured post-fix screenshot.

## Why bindings and heights differed

Pakar's shell placed shortcuts inside the primary label using runs of spaces.
That gives proportional-font text neither a common right edge nor a muted
shortcut color. Pealayer uses `Button::shortcut_text`: egui adds a grow spacer,
right-side atoms, and weak text formatting. Pakar now uses that actual idiom
for all six existing shortcuts; the matching keyboard dispatch is unchanged.

Pakar's `config::apply` separately imposed minimum control height 28 points,
vertical item spacing 6 points and a 13-point font cap. Pealayer's
`configure_main_window_style` caps fonts at 12 and retains egui's defaults:
18-point minimum control height and 3-point vertical spacing. Popup
`menu_style` replaces button padding with (2, 0), but does NOT replace those
minimum heights, spacing or fonts. Thus blaming the global button padding
alone would be incorrect. Display pixels also depend on zoom/DPI.

No documented requirement justifies the divergent native menu metrics. This
was an incomplete import, not evidence that the user wanted different heights.
**No height/font/spacing/zoom setting is changed in this checkpoint**, as asked.

## Exact donor menu mapping and remaining decisions

| Actual donor code | Pakar target / concrete disposition |
|---|---|
| `top_menu_button` | Actual function body in `desktop/src/ui/menu.rs`; only state ID and public visibility changed. All four root headings in `shell.rs` use it. Idle hover does not open a menu; active sibling hover switches it. |
| `draw` Preferences / Edit configuration / media shortcuts, Undo/Redo `shortcut_text` calls | Actual shortcut idiom now used by Device settings, Connect, Cell mapping, Preferences, Fullscreen and Refresh. Binding values match current dispatch. Persisted rebinding and Edit configuration remain pending, not certified by this styling fix. |
| `draw` Panels Phosphor submenu | Existing actual `icons::submenu` calls retained. No second submenu engine introduced. |
| `draw` ordered workspace profile selection and reset persistence | Still pending: Pakar exposes its manager but lacks the donor's inline active-profile identity and ordered selection contract. Reset has no immediate saved-layout acknowledgment. Requires profile state/persistence tests, not just copying labels. |
| `draw` Language, Direction and Keyboard Shortcuts menus | Still pending: language preference exists, but System language, Auto/LTR/RTL direction and validated configurable binding contracts are incomplete. Do not display nonfunctional entries or silently count Preferences as equivalent. |
| `estop_button`, `draw_estop_release_dialog`, `draw` E-STOP availability/hide cluster | Still pending hardware adaptation: vector octagon, authoritative shared latch, guarded release/cancel/skip preference and synchronized remote release are useful generic safety features. Media pause must be removed, not used to exclude the hardware contract. |
| `draw` connection controls/status | Connection dialog/typed actions exist; donor requested/connecting/connected distinction, manual/discovery controls and advertised readiness must still be compared with actual transport ownership. No fabricated connected state. |
| `draw` remote connection/browser/download actions | Pending generic endpoint/browse/download lifecycle adaptation with existing firmware tools and authentication, not import of the Pealayer media browser wholesale. |
| `draw` video open/recent/close/info/folder, timeline open/save, Audio/Subtitles/Chapters selectors; `TrackMenuState`, `track_menu_state`, `format_track_label` | Media/timeline business expressly excluded. No video tracks, chapters or media undo stack exist here. Their disabled Undo/Redo placeholders are not useful functional controller actions. Generic information/config-folder navigation remains a separate pending action, not excluded with media. |
| `track_menu_is_contextual_to_media_and_discovered_tracks`, `test_format_track_label_variations` | Media-only tests not copied: their assertions require tracks/media IDs, not controller menu behavior. New real egui paint/pointer tests cover the imported generic helper instead. |
| `src/ui/mod.rs` interaction, appearance, accent/contrast and tests | Read completely for this diagnosis, but full function mapping remains open. Existing partial appearance imports do not certify the entire module. The 12-point desktop metrics are deliberately left unmodified in this height-diagnosis request. |

## Proof boundaries

Later source 8849ab8 Windows job 114334311601 completes 114 passed/1 failed. The
texture destructor panics and abort are gone; actual shortcut muted-ink/right-edge
checks get past their prior failure, but the label's primary-ink expectation fails
at menu.rs:125. Native package/deployed acceptance is not complete. Inspect actual
widget text-color policy before any production change or test-oracle correction;
no weakened assertion or passing-runtime claim for this menu.

The restored serial/uncaptured native harness identifies the actual failure in
0c723f1 Windows job 114331734735: egui caches PLACEHOLDER layout colors and paints
TextShape fallback/override colors. The shortcut test now checks actual tessellated
glyph vertex colors, retaining distinct muted/primary ink, right alignment, six
separate atoms and unchanged geometry. Headless outputs drain texture deltas before
assertions; unused heading frames explicitly discard them. This restores Pealayer's
resource ownership pattern and prevents the observed double-panic path without
changing production styling or weakening assertions. Native re-execution and
deployed visual acceptance remain pending; source guards are not runtime proof.

`tests/native_menu_test.mjs` binds the actual donor helper body after only
ID/visibility/whitespace adaptation, checks all six production shortcut
call sites and ensures the user's unchanged height policy. The three Rust tests
in `ui/menu.rs` exercise actual painted shortcut atoms/colors/right edges in
light/dark themes, actual popup hover state/closure/context isolation and
production call sites. Source checks are not native execution or screenshots.

The local canonical Cargo cache guard remains enforced; no duplicate cache,
donor build, height change, cleanup reroute, firmware build/flash or running-app
replacement is needed to publish this source checkpoint. Native CI execution
and runtime acceptance must be reported separately. All useful omissions above
remain in the authoritative hash-bound comparison and issue #3; this is not
an all-Pealayer parity claim.
