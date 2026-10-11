# Cell workspace refinement

Historical ec80d5d receipt. The current indicator/highlight and screenshot follow-up is in CELL-CAPTURE.md; hashes/counts below identify the original checkpoint, not the newer source.

This is a scoped desktop/Web improvement, not a declaration that the entire donor backlog or physical production acceptance is complete.

## Delivered behavior

- Dark neutral cells, small physical-state indicators, alternating column bands and the existing half-cell stagger. Stable hit rectangles, inside-painted borders and fixed Web outlines do not move content when interaction styling changes.
- Animated hover, focus, pressed, selected and search states; persisted animation/reduced-motion controls.
- Click selection without a hardware write; Ctrl toggles, Shift ranges, arrows navigate, Home/End operate within a row, Ctrl+Home/End span the table, Ctrl+A selects all, Space selects, Enter manages, Escape clears, Shift+F10 opens the compact icon menu.
- Marquee selection, row selection, explicit selection of search matches, and configurable additive selection. Native: Preferences -> Workspace; manager's Toggle without Ctrl control. Web: table selection preferences.
- Manage cells dialog with all 80 cells, eight row selectors, state/effect controls, mapping entry and unsent cancellation.
- Bulk writes paced at 50 ms using the existing selected transport. Target/transport changes stop the batch; uncertain writes are not replayed. Authoritative readback, not an accepted POST, confirms the final states. These are sequential operations, not an atomic transaction.
- Panels submenu in Workspace and toolbar; checkbox visibility, close/close-others and empty header right-click selector. Actual Pealayer tab-rectangle bookkeeping and disclosure-icon painting are adapted.
- Status-bar item menus plus a persistent whole-bar menu to restore hidden items. Connection, telemetry and message visibility survive restart; navigation and refresh/connect actions are provided.

## Exact donor mapping

The JSON companion embeds the reviewed original excerpts, full original-file snapshot hashes and target-file hashes. These are bounded code reviews, NOT complete-file reviews. The canonical Pealayer workspace was dirty and left untouched; its base HEAD was 78563ff7501e13cb8e40a60dd181f69f127c550d. Snapshot hashes, not that HEAD, identify the reviewed bytes.

| Reviewed Pealayer source | PakarController target | Imported/adapted implementation |
| --- | --- | --- |
| src/app.rs:1897-1978 | desktop/src/ui/shell.rs | Tab-button recording lifecycle, empty-header popup, deferred docking edits, transparent legacy carets and application icon overlay. Empty-header detection uses actual leaf header rectangles, avoiding blank workspace false positives. |
| src/ui/layout.rs:11986-12011 | desktop/src/ui/layout.rs:108 | Actual checkbox panel visibility selector, eye icons and close/open actions adapted to Pakar's seven panels. |
| src/ui/layout.rs:12121-12130 | desktop/src/ui/layout.rs | Actual on_tab_button rectangle collection. |
| src/ui/layout.rs::paint_dock_disclosure_icons | desktop/src/ui/layout.rs:139 | Actual Phosphor caret painter with hover color; existing egui_dock hit targets retained. |
| src/ui/menu.rs:485-491 | desktop/src/ui/shell.rs | Actual nested Panels selector pattern, with Pakar panel vocabulary. |
| src/ui/status_bar.rs:24,169-189 and whole-bar menu | desktop/src/ui/status_bar.rs:57 | Item-level hide pattern and whole-bar checkbox restoration adapted to controller status. |
| Existing imported ui/dialog.rs, ui/icons.rs, ui/palette.rs | desktop/src/ui/cell_table.rs | Reuses real dialog-section, Phosphor icon and app palette modules; domain selection/painter/batch code is new, not a copied media timeline. |

Concrete omissions from these slices: media/frame-rate status is not imported because this application intentionally excludes mpv/media playback and timeline/cues. Physical status-RGB and donor hardware-warning widgets cannot advertise Pealayer board capabilities that the ESP/AVR peer does not provide; Pakar retains its own authoritative connection/telemetry/message surfaces. Media panel enum entries are replaced by Table, Effect controls, Front panel, Hardware monitor, Network & cloud, Streaming and Events. Donor translation calls are not represented as full translation parity: remaining locale coverage stays in issue #5. Donor background/context media actions are excluded from these controller headers, not from the ledger silently.

The cell interaction state machine is controller-specific. Selection is not itself a command, and queued cell writes require peer readback. Copying cue/timeline mutation behavior would violate these contracts. Useful unreviewed donor code remains pending rather than being declared unnecessary.

## Verification and limits

- 84 native Rust tests pass, including fixed inside-stroke painter geometry, dark white-state appearance, selection/ranges/rows, bounded navigation, marquee and the real egui Space/repeated-key activation regression.
- Locked optimized Windows release build succeeds.
- Actual embedded production JavaScript passes selection/rows/search/keyboard/marquee/context icons/dialog, paced bulk readback, cancellation and uncertain-write tests using DOM/HTTP doubles. Existing Web access, remote-maintenance and display tests also pass.
- 50 PU route registrations and 23 focused PCController contracts retain their exact hash/anchor audits. Version checks agree at 0.2.7. Ledger tests and diff whitespace checks pass.
- Both busy-wire and polling ESP/AVR profiles compile; AVR+Urboot images are 32768 bytes. ESP IRAM is 63055/65536 bytes (96%); that is a remaining capacity risk, not a measured runtime failure.
- cell-manager.png is an actual final-binary native capture against an explicitly SIMULATED, isolated read-only peer; that fixture refuses hardware mutations and WebSocket upgrades. Its red WebSocket log is a fixture limitation, not live hardware evidence.
- running-table.png is the actual replaced production-profile app. It correctly reports the configured http://ledboard.local peer as offline. Captures use the existing xcap helper constrained to the expected PID. The normal Windows capture service failed twice and provided no accessibility tree; native interactive gesture acceptance is still pending, not claimed from model tests.
- User-authorized graceful replacement: old PID 40100 acknowledged quit; new PID 74824 reports 0.2.7, single-instance/tray available and the same endpoint/panels. Old executable saved as PakarController.previous.exe; preferences backed up before replacement. The candidate SHA-256 is 563678af67f969e855264928dfbf807cfb4d3e1e6dccec7e01202beb30d180d0.
- No firmware/EEPROM/device settings were written. ASUS SSH localhost:1022 refuses connection and ledboard.local does not resolve from this PC. Firmware packages are candidates awaiting reachable physical hardware.

The global file ledger remains honest: 1868 files inventoried, 31 complete-file comparisons, 1837 pending, 16 reviewed files with gaps. This UI pass does not complete those 1837 reviews. Remaining work stays in #2, #3 and #5; PR #1 remains draft.

## Reproduce

```text
cargo test --locked --manifest-path desktop/Cargo.toml --bin pakarcontroller
cargo build --locked --release --manifest-path desktop/Cargo.toml --bin pakarcontroller
node tests/web_cell_table_test.mjs
node tests/web_access_test.mjs
node tests/web_remote_test.mjs
node tests/web_display_test.mjs
node tests/pu_routes_test.mjs
node tests/pc_contract_mapping_test.mjs
powershell -File tools/verify-versions.ps1
powershell -File tests/file_comparison_test.ps1
powershell -File tests/reuse_ledger_test.ps1
```
