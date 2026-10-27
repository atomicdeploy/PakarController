# Shared interface contract and current acceptance

Owner: [issue #4](https://github.com/atomicdeploy/PakarController/issues/4).
Donor decisions remain in `file-comparison-evidence.json` / `file-comparison.csv`,
not a second exclusion ledger. This document records the interface contract and
runtime acceptance boundaries. It does not certify complete production parity.

## One catalog, distinct transport and platform adapters

`assets/contracts/device-settings.json` owns stable page IDs, labels, ordering,
search keywords, capability requirements, native Network/Cloud DTO fields and
build-only classifications. `tools/generate-interface-contract.mjs` embeds the
actual `assets/web/device-settings.js` into `WebUi.h` and generates Rust/Dart
catalogs. `--check` rejects stale outputs without writes; ESP, desktop and Android
build/CI paths run it. The contract test compares exact native DTO fields too.

Build flags are not runtime parameters. UART busy-wire/polling selection has no
dedicated dashboard tile or mode selector. Web Overview places only explicitly
reported build information in a collapsed read-only disclosure, without a
fabricated polling fallback. Native build-only explanations remain read-only.

The catalog is not a claim of shared execution: Rust owns native draft and
transport state; embedded Web uses its actual HTTP adapter; Dart uses the page
catalog together with `DeviceSettingsSession` and real network/cloud/identity
editors. CLI commands use the same firmware endpoints; they are not automatically
generated from this UI catalog.

## Implemented Web adapters

- Searchable, grouped Device settings dialog; only fresh individually ACKed
  capabilities permit relevant pages and workspace links.
- Existing board identity, Wi-Fi, device access and firmware controls moved into
  the dialog, preserving the actual controls/handlers rather than cloning them.
- Separate MQTT and outbound WebSocket pages, labeled full-width inputs,
  write-only secret Keep/Replace/Clear semantics, observed secret presence, shared
  telemetry interval and explicit reconnect action. Replacement rows hide unless
  selected; no secret is fetched or included in a Keep write.
- Wi-Fi scanned or manually entered SSID, station/recovery-AP policy, explicit
  open-network choice and password re-entry. No default Wi-Fi password is shown.
- Actual native Draft/SecretEdit/Write contracts adapted: strict required-field
  reads, validation, dirty draft preservation, external-change conflict, frozen
  inputs during writes, one POST, positive ACK and exact public-state readback.
  Old-session/older-read replies are ignored; uncertain writes are never replayed.
- Panel show/hide/collapse/reset and panel-heading context menu use the same
  Panels dropdown; capability loss overrides a saved visibility preference.
- Actionable WebSocket reconnect/disconnect dropdown; HTTP refresh remains
  independent. Obsolete socket callbacks cannot resurrect a paused connection.
- Ctrl+Shift+, Device settings; Ctrl+, Appearance; Ctrl+F Find cells; F5 refresh
  outside inputs. Responsive rail and forms tested in the actual embedded page.

## Known useful parity gaps, not accepted exclusions

| Contract | Native | Embedded Web | Mobile / CLI / TUI next acceptance |
|---|---|---|---|
| Device page catalog and ACK eligibility | Generated catalog used by editor | Generated catalog used by dialog | Mobile `DeviceSettingsPage` uses contextual catalog, owner/freshness checks and retained drafts. Cross-device interaction acceptance remains open. |
| Network/cloud strict draft/secret/ACK/readback | Existing actual contract | Adapted and HTTP-double tested | Mobile `DeviceSettingsSession` implements typed public fields, Keep/Replace/Clear secrets, conflicts, one write and readback. Compare remaining CLI/TUI behavior and physical connectivity handoffs. |
| Identity and device access | Existing editors | Existing handlers retained in dialog | Mobile has identity edits and hostname handoff. The current auth refinement adds an enabled toggle, conditional credentials, exact ACK/public readback and conflict checks; installed acceptance is separate. Web identity/access dirty-close parity remains open. |
| Firmware maintenance | Probe/local-file/remote/vault controls | Existing remote/vault actions retained | Mobile has local/URL/manifest/feed delivery and acknowledged ISP/Urboot maintenance selection. Native already observes ISP/Urboot progress, but initiation still uses legacy ISP operations: typed admission/receipt handling is missing, not just a dropdown. Both clients also need explicit Urboot EEPROM-operation controls/parser coverage for the newer firmware contract. Physical update acceptance remains open. |
| Workspace shell | Docking, layouts, preferences, native dialogs | Visibility/collapse/reset, grouped settings and transport actions | Mobile retains per-board activity/workspace state and reorderable Output cards; native key-card grid/drag behavior is not yet mirrored in Flutter. Profiles/logs and full native-dialog parity remain open. |
| Platform shell | Native tray/single-instance/toast code exists in source | Appropriate browser capabilities still pending | Install/update/share/fullscreen/notifications/haptics/audio/wake-lock require permission/lifecycle tests; not media-business exclusions |
| Table, front panel and diagnostics | Actual native panels | Existing live panels revealed rather than duplicated | Mobile has retained capability inventory, contextual Table/Output, atomic diagnostics/corrections, numeric/text display, custom tones and acknowledged output writes. Remaining streaming and platform differences are itemized below; Web screen-sampler parity remains open. |

## Rust / Flutter panel audit

This is a function-level source inventory, not a declaration that every panel
has passed visual, installed-device or hardware acceptance. The authoritative
donor decisions remain in the existing file-comparison ledger and issue #3.
Named controls below were read in their actual implementations; a route name or
keyword match alone is not parity evidence.

| Native panel / actual implementation | Flutter counterpart / actual implementation | Coverage and explicit remaining acceptance |
|---|---|---|
| Table: `ui/shell.rs::draw_table`, `cell_table.rs` | `main.dart::TablePage`, `table_workspace.dart`, `cell_controls.dart` | Both retain selection, semantic assignments, mapping and bulk actions. Native per-pixel shape/background preferences are not identical mobile controls; check equivalent semantic actions against configured palettes, not hardcoded red/white assumptions. |
| Effect controls: `draw_effects`, `draw_led_settings`, `ui/effect_controls.rs`, `state_appearance.rs::controls` | `output_panel.dart::OutputPanel`, `state_appearance_page.dart::StateAppearancePage` | Both expose state colors, motion/timings, neighbors/invert/diagonal, temporary preview and brightness/none/rainbow/fire controls. Both brightness sliders retain a gesture draft and commit once on release; native now reads live USB/network observations outside the draft and revokes it on peer/route/epoch loss. Explicit Off can reassert black while already disabled. All output actions require the current acknowledged LED capability, and Rainbow requires observed enabled output. Native removes the duplicate single-cell palette; Table retains shared semantic actions. Native mode picker allocates no card or spacer with fewer than two advertised choices; first-wins reset stays separate. Native appearance/preview observations remain visible on USB, but their unsupported command path is disabled; that transport parity remains open. |
| Keys: `draw_hardware(true)`, `key_boxes.rs::show` | `OutputPanel::card('scene')/gameSettings`, `GamePresentationEditor` | Both provide remote press/reset, masks, cues, auto-reset, palette and group presentation. Native press now requires explicit applied-mask acknowledgement, online AVR and released programming latch, matching the live command contract. Current USB snapshots carry that acknowledgement; legacy binary-only status stays unknown. Native supplies snapped collision-safe placement, hover-responsive grid and labeled group previews; Flutter's audited free-position Stack permits overlaps and lacks equivalent grid feedback. |
| Pixel diagnostics: `draw_pixel_diagnostic` | `pixel_diagnostic_page.dart::PixelDiagnosticPage` | Single/group probes, index step, RGB, all-off and correlated observations exist in both. Verify 12-pixel rows at narrow native widths and mobile tab/overflow-menu reachability. Source state is not optical LED proof. |
| Pixel corrections: `draw_pixel_corrections`, `ui/panel_details.rs::correction_rules` | `pixel_corrections_page.dart::PixelCorrectionsPage` | Both expose ordered blackout/remove/shift rules, reorder, enable, save/readback and mapping. Native uses styled wrapping rules, square row actions and a virtualized applied mapping, and loads on first open. Its editor is still restricted to Wi-Fi; no USB parity is inferred. |
| Front panel: `draw_front_panel` | `main.dart::FrontPanelPage`, `table_workspace.dart::FrontPanelWorkspace` | Both expose acknowledged display/text/cues, numeric display, custom frequency/duration tone and configurable hold/repeat timing. Mobile retains drafts and its custom-tone expansion owns separate child scroll keys. Both keep Stop text allocated and disabled when inactive, preventing card motion. Mobile commands lead in a tablet two-column / phone one-column grid; local mirror controls follow and exceed the corresponding native panel. Installed timing/audio acceptance remains separate. |
| Hardware monitor: `draw_hardware(false)` | `main.dart::HardwarePage`, `HardwareIoSection` | Both include writable shift-register outputs. Mobile writes require observed 8-bit mask, output capability, online AVR, explicitly released programming latch and correlated command readback; rendering never mutates the mask. Native observed capability chips reserve measured single-line slots and expose full accessible labels/tooltips; Controller actions wrap as a complete row before sizing. Both retain contextual details/actions rather than inventing missing peripherals. Real output actuation remains unverified by widget tests. |
| Network & cloud: `draw_network`, `ui/panel_details.rs::network`, `ui/device_settings.rs` | `DeviceSettingsPage`, `HardwarePage` | Real contextual editors exist in both. Native dock now groups observed interfaces, discovery/time and remote-service state in detailed actionable cards using the existing settings editor. Mobile retains detailed sheets. LED settings now route first-wins to Output and table-selection to Table; inactive or owner-conflicted edits remain guarded. |
| Streaming: `draw_streaming` | No equivalent dedicated mobile workspace | Native Stream source/Frame delivery cards expose host-rainbow/screen-sampler, target rate (0 means uncapped), and measured completed-write FPS, not physical LED refresh. Stop remains reachable after WebSocket loss. Mobile board-rainbow and bridge telemetry are not host streaming parity. Android screen capture is platform-specific: it needs MediaProjection consent, foreground-service/lifecycle and mapping work; host-rainbow/source-rate controls are an ordinary remaining UI/transport gap. |
| Events: `draw_events`, `diagnostics::EventLog` | Status/error detail and support logs only | Native retains deduplicated events/counts/clear. Audited mobile navigation has no equivalent event-history workspace. App status/error text is not feature parity. |

Native shared UI building blocks are `dialog::section/setting_row/action_button`,
the styled `ui::dropdown`, bounded scrolling and fixed-geometry key/cell painting.
Flutter counterparts are `WorkspaceHeader/WorkspaceSection/WorkspaceBody`,
`WorkspaceGrid/WorkspaceActionCard/WorkspaceValueRow`, `WorkspaceBadge`, semantic
`CellColorAction` and retained workspace models. The action card adapts the fully
read PCController `workspace_chrome.dart` Material/InkWell composition, semantic
icon background, reserved outline and directional chevron; it is not a claim
that all donor chrome behavior has been imported.
Use these existing controls before introducing another local button/card system.

Device-access behavior follows the actual `AccessConfig::configure` and
`WebAccess::registerRoutes` contract: disabled-to-enabled requires a valid new
username/password but no old password; changing protected access requires the
current password; disabling does not require replacement credentials. The
mobile editor uses the current connection secret when no manual current password
is supplied, never displays that secret, verifies `updated`, `enabled` and
`reauthenticate`, then reads the public configuration before handoff. A changed
owner or conflicting baseline prevents replay. The API has no atomic CAS, so
preflight/readback cannot eliminate an intervening multi-client race.

Server configuration has no compare-and-swap revision. Web preflight catches a
change observed before submission, but cannot make two concurrent clients' writes
atomic. Public readback and secret-presence flags do not verify the exact secret
bytes or authenticate a firmware image. These limits remain explicit work.

Native AVR maintenance follow-up must reuse `firmware_progress::observed` and
the existing pinned HTTP owner rather than add a second worker. The missing
request path needs the actual `UrbootRoutes.h` admission fields: advertised
method, operation, unique request ID, unchanged boot, fresh board-clock deadline
and exact staged SHA-256 for programming. `maintenance::Operation` currently
dispatches legacy `/api/avr/program`; adding a method dropdown alone would be
misleading. Flutter `FirmwareController::maintainAvr` already pins the ordinary
probe/backup/program admission, but its receipt parser currently rejects the
newer `eeprom-write` / `eeprom-initialize` operations. No EEPROM parity is claimed.

## Proof and blockers

`tests/interface_settings_test.mjs` executes the actual embedded module with
HTTP/DOM doubles: required fields, native DTO equality, secret semantics,
conflicts, ACK/readback, single-write/no-retry and late-read revocation. Existing
Web access/context/cell/appearance/identity/remote/display tests remain relevant.
`mobile/test/settings_catalog_test.dart` and
`mobile/test/device_settings_page_test.dart` cover the catalog and real mobile
editor behavior. The auth/navigation additions include enabled/disabled flows,
password requirements, peer changes, conflict preservation and contextual LED
navigation. The canonical mobile wrapper has now completed a clean analyzer,
`panel_refinement_test.dart` (12/12), `device_settings_page_test.dart` (13/13),
`overview_hardware_test.dart` (3/3) and `output_panel_test.dart` (9/9), including
the final Off/readback, current-capability admission and hidden-Scene cases. The
panel suite covers controls-first tablet/phone hierarchy, narrow/wide/RTL and
enlarged-text geometry, numeric/custom-tone actions, stable Stop-text placement
and one brightness write.
These are source/widget results, not installed tablet, AVR sound or LED proof.
The first native UI run passed 131 tests and exposed one narrow-panel overflow.
Final canonical focused reruns now pass `panel_details` (3/3), including strict
220/300/600px cards in both themes, actual Ethernet `link_up` observations and
responsive action rows; `state_appearance` (2/2), including unchanged strict
220/280/380/640px bounds and no command during rendering; and `workspace_layout`
(10/10). The final canonical native `ui::` suite passes 139/139, including the
three USB/network brightness pointer/keyboard/ownership regressions, explicit
key-mask admission and whole-slot Hardware chips. `state_appearance::` passes
3/3 at unchanged source, including real pointer gating for disabled controls.
The final preview-signed APK was built with production hold enabled, SHA256
`5db85ea054539076e6daa08d80d56001eef5e1adb0330665b94f932116760431`.
Authorized tablet `R52T60C6V9B` was updated with `install -r`; installed `base.apk`
matched that SHA256, version0.2.8/build23, PID7497. It automatically reconnected
to ledcontroller.local, restored Output, and the viewed final screenshot confirms
the empty fixed-mode Scene wrapper is gone. The new nine-test Output suite
preserves its saved position and the first-wins game card. Actual Hardware and
Front-panel captures from the immediately preceding package (unchanged pages)
show 22 acknowledged capabilities, AVR online and NTP synchronized. These
establish installed connected observation, not physical output or command
execution; a bounded log query found no Flutter/AndroidRuntime errors but is
not a soak or a claim that no possible bug remains.
The native post-brightness Release and canonical staging probe passed, executable
SHA256 `69f8c41c8f450d99c66af1e833b60a367d1bdcb32cf523f622d74536a11245d2`;
the canonical ASUS replacement then reported `replaced=true`, `ready=true` and
no error, moving PID2452 to PID5592 in session2 while preserving rollback and
settings. That startup receipt alone does not prove a connected board.
The subsequent actual native framebuffer and status receipt confirm PID5592
on the same executable, Wi-Fi/WS live to ledcontroller.local and AVR live,
while preserving the user's multi-panel layout. Hardware chips below the
viewport are not falsely treated as visually checked by that capture.
Separate final native framebuffer fixtures from that exact executable were
opened for Hardware and Effects: the acknowledged feature pills fit as complete
single-line slots, and fixed-role Effects has no Board modes card while Reset
stays accessible. These clearly labeled simulated views are layout evidence,
not additional physical-board acceptance.

Browser inspection uses the actual `WebUi.h` through
`tests/web_appearance_fixture.mjs`, visibly SIMULATED and mutation-disabled.
Navigation preserves the edited MQTT username; unacknowledged front-panel/AVR/
firmware pages are absent. Narrow-width inspection caught an intrinsic-size
overflow; shrinkable rail/grid correction was retested with dialog scrollWidth
equal to clientWidth. This is not physical or production deployment proof.

Build and replacement use the canonical guarded wrappers and shared caches;
historical cache or SSH outages are not assumed current blockers. Refresh actual
host/owner/draft state before replacement. Board firmware and disruptive hardware
tests remain subject to the current production hold. Do not present loopback
screenshots, source tests or ESP builds as installed/physical proof.
