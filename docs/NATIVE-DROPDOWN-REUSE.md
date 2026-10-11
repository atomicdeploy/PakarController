# Actual Pealayer dropdown and color imports

Donor checkout: Pealayer at 3265bf80b6f7543831e096a05b04c3b58b770584,
observed clean. Original-byte hashes and exact target/proof hashes are in the
authoritative file-comparison ledger, not inferred from similar styling.

| Donor source, read through EOF | Actual retained code and consumers |
|---|---|
| src/ui/dropdown.rs, all 631 lines | Full focus owner, search-field registration, disabled-choice skipping, Arrow/Home/End/Enter navigation, full-width rows, inside focus stroke, nested-frame restoration, popup cleanup and wrapper. All three actual donor tests. |
| src/ui/color_picker.rs, all 446 lines | All generic helpers and four tests: 12-entry session MRU, deduplication, recent/shared swatches, inset HEX swatch, Custom row and independently anchored palette with temporary validated draft. |
| src/ui/icons.rs, complete reread | Actual register_search, dropdown_choice, register_choice, shared ComboBox and compact-popup lifecycle hooks added to existing full icon vocabulary and eleven tests. |

Preferences, device settings, shell selectors and both connection dropdowns
use that shared implementation. The actual Pealayer Preferences Accent branch
was adapted to Pakar's existing persisted RGB-string draft; it is NOT a completed
comparison of the entire Preferences file. Recent colors are temporary egui
state and do not write the SSD.

## Concrete adaptations and boundaries

- Product-only context IDs, palette path and colors heading adapted to Pakar.
  No generic source helper was excluded from the two new modules.
- Labeled ComboBoxes retain the locked egui trigger ID via its actual from_label
  factory. Donor and Pakar both use egui 0.36.2; no dependency/cache churn.
- Shared selectors keep donor 32px choice rows. This does not change top-menu
  heights, accelerator policy or claim that every menu consumer was migrated.
- WebSocket/USB headings use the actual Phosphor CARET_DOWN, replacing an
  unsupported Unicode glyph. Actual port enumeration remains lazy and factual.
- Busy USB identity writes disable port/reconnect choices. Keyboard traversal
  must skip them but can still reach Configure. Both controls clear stale state.
- Color Custom row opens a separate palette without changing the value just by
  opening it; pointer and palette behavior have a new light/dark fixture.
  Its eleven choices use a bounded 420px popup, retaining 32px fixed rows and
  egui viewport constraints. The fixture targets the actual popup, not the same
  Custom label on the closed control.
- Current donor icon reopen tests explicitly use Folder within the scrolled
  viewport, not the old offscreen Seat assertion; those actual assertions are
  imported without dropping filter/select/reopen checks.
- Dynamic System accent and universal native/Web/mobile Preferences contracts,
  full board EEPROM editors, workspace/language/FPS menu ownership, offline
  error coalescing and Windows-shell initialization remain useful pending work.

Native test definitions are not execution proof. The source wiring guard is
explicitly static; a guarded native test run and PID-bound production capture
must be recorded separately. No board flash, electrical or LAN acceptance is
implied by these UI imports.

## Executed checkpoint

Guarded native Test: 128 passed, zero failures, no ignored tests, using the
existing release cache and isolated configuration. Includes all three donor
dropdown tests, all four color-helper tests, eleven icon tests, busy USB keyboard
rejection and Custom palette interaction in light/dark themes. Existing menu
paint/pointer, selection, settings-draft and capture tests pass in the same run.
The static quality phase also passed; post-correction wiring, lifecycle, menu,
connection and exact-hash checks passed separately. Five compiler warnings
remain (unused bindings/helper and two duplicate translations).

Release packaging, new source CI, running-process visual acceptance and all
physical-board paths are independent gates; no result is borrowed from Pealayer.
