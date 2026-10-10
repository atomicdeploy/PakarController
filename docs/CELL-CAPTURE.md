# Cell styling and screenshot capture

## Current tint-free selection and Find cell contract

Selection, Find and highlighting are now one domain concept, not parallel
sets with matching colors. Native `Selection.ids` and Web `selectedCells` are
the authoritative per-client set. Rendering accepts only membership in that
set; the native `matched` argument, Web `matched` class and redundant Select
matches buttons are removed. Navigation, counts, managers, menus and bulk writes
all read the same set. Keyboard cursor/focus remains separate, not a selected
or highlighted set, and navigating within the selection never shrinks it.

Parsing remains the validation/normalization input boundary. Valid input is
committed to that set; invalid draft text changes neither selection nor highlights.
On valid blur/Enter, the editor is normalized from the model as sorted,
deduplicated compact ranges. Status `cell_search` is canonical; `cell_search_draft`
is explicitly editor text. `selected_cells`, `highlighted_cells` and the legacy
`search_matches` are read-only projections of the identical authoritative set,
including while an invalid draft exists. There is no query reparsing to render
highlights, drive navigation or manufacture status membership.

`assets/cells/selection-contract.json` is shared by native and Web transition
tests, covering normalized IDs, duplicates, invalid prefixes/incomplete ranges,
click/toggle/extend/row, next and clear. Web executes this against shipped code and
checks painted membership/ARIA/counts after every step. Native runs the same
fixture against `Selection`; its execution still awaits the guarded build/CI.
Selection remains client-local; this does not silently broadcast a client's
temporary selection to other connected users or turn selection into an LED write.

Supersedes the earlier tinted search/LED-state descriptions below. Native and Web
cells retain neutral alternating-column backgrounds regardless of red/white/off
LED state. Selection and search highlighting share one amber inside outline;
neither hover, focus nor pressed state adds a competing accent fill or ring.
Optional LED-state dots remain default-off; tooltips still report actual state.
Hover/press on unselected cells uses only neutral surface colors. Existing
animation, half-row stagger and hit geometry are preserved.

Find cell is two-way: valid input selects its exact logical cells without hardware
writes; clicks, Ctrl toggles, Shift ranges, keyboard selection, row/all/clear,
manager and marquee gestures replace it with the exact sorted range summary.
F3/previous/next navigate within that selection without shrinking its query.
Invalid partial drafts remain editable with validation; a selection gesture,
including reselecting the same cell or clearing an already-empty selection,
replaces the draft. Native selection revisions prevent stale drafts and repeated
idle rewrites; the UI requests repaint after synchronization.

Consulted actual Pealayer controls.rs neutral/fixed-geometry interaction helpers
and its timecode selection-before-paint handling (bounded slices, not a complete
file review). Existing imported dialog, palette and icons remain the reusable
implementation. Its transport intent tint deliberately does not apply to this
request to remove tint; timecode/playback mutations do not apply to cell selection.
Generic remaining controls.rs capabilities stay pending in the authoritative
ledger; no whole-file parity or new donor import is claimed by this domain fix.

Actual embedded Web DOM/HTTP-doubles tests exercise selection/query agreement,
invalid drafts, same-cell reselection, rows, all/clear, keyboard and marquee.
Native tests cover range roundtrips, draft/revision sync, neutral LED-state fill
and selection/highlight/focus equality. Native execution/deployed screenshots
remain pending: the canonical Test wrapper refuses the cache above 8 GiB. No
guard bypass, cleanup reroute, duplicate cache, app replacement or board flash.

LED-state corner dots are configurable and hidden by default, including existing preferences that omit the setting. Native: Preferences -> Workspace -> Show cell LED-state indicators. Web: Table -> LED-state indicators, persisted per browser.

Search owns one amber fill/border regardless of selection or keyboard focus. The competing green inner cursor stroke is removed. Web matching-cell CSS overrides selected/hover/active/focus layers; native light/dark painter tests compare the actual fill/stroke of selected and focused matches. Geometry remains fixed.

## Internal harness

Help -> Save workspace screenshot captures the actual egui framebuffer, independently of Windows.Graphics.Capture. Typed single-instance CLI/IPC:

```text
PakarController.exe --screenshot C:\path\workspace.png
PakarController.exe --app-status
```

An absolute PNG path, existing parent directory and absent output file are required. Status.screenshot reports pending/result/backend; IPC acceptance does not mean a saved image. Root-viewport callback IDs, dimension validation, create-new output and a 10-second timeout prevent wrong callbacks, overwrites and indefinite pending state. Capture does not change focus/panels/visibility.

For startup capture, set PAKAR_CONFIG_FILE to an isolated profile with single_instance=false and PAKAR_SCREENSHOT_PATH to a new absolute PNG path before launch. The harness skips production IPC/tray/Jump List hooks and window activation; no environment is changed underneath native threads. An explicit simulated read-only peer is available:

```text
node tests/remote_ui_fixture.mjs PATH_TO_EXE ISOLATED_PROFILE highlight OUTPUT_PNG
```

The fixture also supports table, cells and firmware surfaces. It refuses hardware mutations and WebSocket upgrades; fixture logs are not live-device evidence.

## Restored Win32 capability

```powershell
./tools/capture-window-windows.ps1 -ExpectedProcessId VERIFIED_PID -OutputPath NEW_PNG -Backend Auto
./tools/capture-window-windows.ps1 -ExpectedProcessId VERIFIED_PID -OutputPath NEW_PNG -Backend Win32
```

Auto checks executable identity, expected PID and advertised internal capability before requesting a framebuffer; it never executes unsupported capture flags on arbitrary programs. Win32 uses imported PrintWindow(PW_RENDERFULLCONTENT), blank/near-uniform rejection and GetWindowDC/BitBlt. Screen copying is last resort only when foreground/sample-point ownership is safe. Per-monitor-v2 coordinates, size/owner validation, resource disposal and no-overwrite writes are retained. It does not activate, resize, move or inject input.

This restores Pakar's capture capability, not the external Computer Use WGC service. AGENTS.md records the working fallback for future passes.

## Actual donor mapping and omissions

- Pealayer scripts/capture-visible-window-windows.ps1 fully read; SHA-256 2876c19687b1a209e777f3d505e4b24b6c4b1403a0f1de99edcedf0a624d9527. Native declarations, bounds/minimized checks, DPI and PNG lifecycle adapted to tools/capture-window-windows.ps1. Name-only selection becomes explicit PID checks; unconditional saving becomes create-new.
- Pealayer scripts/update-screenshots-windows.ps1 fully read; SHA-256 48507f6e9395c8c312c5557db729d05ed8d6a83e980d57a9b01e77ea525db287. Actual Save-WindowScreenshot / Test-NearUniformBlack fallback code is adapted. Input/focus/resize helpers are intentionally absent from this read-only tool. Physical GetWindowRect stays consistent with full window-DC origin instead of mixing DWM crop coordinates.
- Generic locale/theme/icon/width matrix, hashed capture-manifest runner and interactive scroll/right-click scenario matrix remain useful unfinished work. These are explicit pending omissions in file-comparison-evidence.json, not discarded or certified equivalent.

The comparison ledger now has 33 complete source-file reviews, 1835 pending and 17 known gap files. The two scripts have exact donor/target/test hashes and concrete omission reasons; whole-project superiority remains unestablished.

## Verification

88 native tests pass: highlight composition, default-off/persisted indicators, actual PNG encode/decode and no-overwrite protection. Production Web interaction tests and screenshot source guards pass. Both busy-wire/polling firmware profiles compile. Actual internal framebuffer, PrintWindow and Auto-to-Win32 fallback captures are inspected; BitBlt/screen branches are not claimed runtime-exercised just because PrintWindow succeeded.

Physical Web firmware deployment is blocked by unavailable ASUS SSH / unresolved ledboard.local. No firmware, EEPROM or device-setting writes occurred. Runtime captures/package identity are in the local cell-style receipt and GitHub issue #8. Full native gesture and screenshot-matrix acceptance remain separate.

## Alternating cell surfaces

Native alternating column bands are removed. Each individual cell now uses the
imported palette's surface-0 (odd column) or surface-2 (even column) as its base
fill. Row-major numbering, half-row stagger, inside strokes, animation, default-off
indicators and amber search precedence are preserved. Red, selection and hover
treatments compose with the cell's base surface. The embedded Web table already
alternates individual cell surfaces and needs no firmware change for this fix.

89 native tests pass, including actual painter output for cells 1/2/11/12,
column continuity in both palettes and light/dark themes, and absence of the old
band painter. Production Web table/access/remote/display tests, screenshot harness
guards, version consistency and hash-bound comparison integrity pass. The release
executable's SHA-256 is e67c141a5f015f7742c2dadb34ba7a51d718d25c85f256cc6192e5f578951ca5.
The isolated internal-framebuffer capture in the local cell-surfaces receipt was
visually inspected: colors belong to each rounded cell, not full-height bands.
That screenshot is explicitly a simulated peer, not live-board acceptance.

The two shell.rs donor contracts (F11 and icon picker) were rechecked in the
focused diff; only the band painter changed. Their target hashes/line anchors
were regenerated. Ledger scope remains 33 reviewed files and 1835 pending.

## Manage cells: stable hover/focus geometry

The native manager no longer uses stock selectable Frames for cell/row/state
selectors. In locked egui 0.36.2, inactive unselected buttons drop their Frame
stroke, while hovered buttons restore it; Frame total margins include stroke
width and can move the content. Pealayer dialog.rs navigation_control's actual
fixed-allocation/inside-stroke approach is adapted into manager_selector using
the existing shared neutral cell_surface. Slots stay 32/96/104 x 28 points;
text stays centered, and wrapped row headings remain top-aligned. Hover, press,
focus and selected animations cannot change allocation or wrap positions.

Web manager cells use fixed border-box dimensions, constant 2px borders and
inset focus outlines. Shared selected amber emphasis stays neutral on hover,
press and focus; reduced-motion and the animation preference are respected.

The added native painter/input test compares hit rectangles, text positions and
inside strokes at 240/650-point widths with animation on/off. It has not executed:
the guarded native Test build stops at the existing canonical >8 GiB cache.
Web shipped-script DOM-double tests and CSS geometry guards pass, alongside
appearance/access/display/menu/branding checks. No new running binary, screenshot
of this fix or physical firmware deployment is claimed. See issue #8 and the
hash-bound dialog.rs entry for exact donor/target/test mapping and pending work.
