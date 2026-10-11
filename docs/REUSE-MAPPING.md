# Source reuse and UI mapping

This is an evidence map, not a claim that every source file belongs in the new
product. `legacy-source-ledger.csv` contains every tracked Pealayer and
PCController file and every non-build Puzzles file with its source revision,
SHA-256, size, category, source-content signals, and current review state. The
generator reads and hashes every file, screens text sources for the hardware,
transport, shell, update, discovery, presentation, theme, panel, preferences,
actions, and events vocabulary, and never silently drops an unselected file.
`reuse-decisions.csv` records the manually adapted/reviewed subset over that
complete screening ledger.

The claims below are historical integration pointers, not file-completeness
certification. The stricter `FILE-COMPARISON.md`, `file-comparison.csv` and
`file-comparison-evidence.json` take precedence where current comparisons
identify missing behavior. In particular the embedded Web palette is not yet
shared, the generic PU shell registry is missing, and TM1637 ACK/reconnect
recovery from current PCController has not been imported.

The implemented Pealayer-derived desktop foundation, exact imported modules,
native single-instance contract and remaining parity are documented in
`PEALAYER-DESKTOP-FOUNDATION.md`.

## Custom element mapping

| User-visible element | Exact donor source | PakarController implementation | Proof contract |
|---|---|---|---|
| Studio/Native themes | `Pealayer/assets/themes/palettes.json`, `src/ui/palette.rs` | `assets/themes/palettes.json`, `desktop/src/ui/palette.rs` | Same 14 semantic roles and the same four light/dark palettes; unit test requires all roles. |
| Vector icon language | `Pealayer/src/ui/icons.rs` | `desktop/src/ui/icons.rs` | Same `egui-phosphor` 0.14 regular font; toolbar/navigation use glyph constants instead of emoji. |
| Table panel | Pealayer dock/surface rules plus PCController contextual state rules | `desktop/src/ui/layout.rs`, `desktop/src/ui/shell.rs` | Dockable persistent panels; logical IDs are row-major; one-based odd columns render at +0.5 cell; colors come only from device state. |
| Cell identity manager | `Pealayer/src/ui/hardware_control.rs` channel rename/order model | ESP `/api/cells/mapping` and desktop mapping editor | Logical name and physical LED-chain address are separate, persistent, validated, and bijective. |
| Four-digit preview | `Pealayer/web_ui/.../SevenSegmentDisplay.tsx` and `PCController/.../seven-segment-preview.tsx` | `desktop/src/ui/seven_segment.rs` | Exact seven line coordinates; renders only AVR-observed raw segment bytes and brightness. |
| Front-panel marquee | `PCController/internal/native/commands.go`, `Project/Runtime/FrontPanelRuntime.inc.h` | `firmware/avr/FrontPanel.*`, ESP display API | Same 40-byte printable payload, 80–5000 ms speed, once/loop/interval modes, force-scroll bit, raw cells, and terminal blank frame. |
| Welcome animation | `ControllerBoardMini/Puzzles/Puzzles.ino::animation` | `firmware/avr/FrontPanel.cpp` | Exact 12 segment positions at the original 75 ms cadence. |
| Hardware status | `Pealayer/src/ui/status_bar.rs`, `hardware_control.rs`; PCController telemetry components | Desktop live header/front-panel and Web status | Shows only advertised AVR/TM1637/link/discovery/cloud facts; no synthetic “healthy” state. |
| Bottom status surface | `Pealayer/src/ui/status_bar.rs` | `desktop/src/ui/status_bar.rs` | Uses the same persistent bottom-panel composition and truncating message region with controller-specific AVR, TM1637, WebSocket, and USB facts. |
| Typed actions/events | `Pealayer/src/application_shortcuts.rs`, `src/ui/hardware_control.rs` | Desktop `HttpCommand`, `WsCommand`, `SerialCommand`, `Event`, transport router, and tray `TrayAction` | Commands are typed, dispatched once, routed only over selected transports, and acknowledged through factual events instead of optimistic UI state. |
| Preferences | `Pealayer/src/preferences_contract.rs`, `src/ui/preferences.rs` | `desktop/src/ui/preferences.rs`, `desktop/src/config.rs` | Native or embedded searchable category rail, draft preview, Save/Cancel/discard, JSON import/export; the native/Web shared contract remains open. |
| Windows tray | `Pealayer/src/platform/windows.rs`, `src/platform/windows_quick_actions.rs` | `desktop/src/platform/windows_tray.rs` | Real native tray with a stable GUID and Phosphor icon; restore, Preferences, Workspaces, Refresh and Quit use the shared dispatcher. |
| Single instance and shell tasks | Pealayer native mutex/IPC and Windows Jump List publisher | `desktop/src/platform/instance.rs`, `windows_jump_list.rs`, `windows_quick_actions.rs` | Session-scoped GUI owner, typed second-launch forwarding, local request validation and native task-specific icons. |
| Hostname discovery fallback | `PCController/Tools/Controller/internal/discovery/discovery.go` | `desktop/src/platform/discovery.rs`, `mobile/lib/api.dart` | Current native saved endpoint is `http://ledboard.local`; discovery candidates still require peer identity validation. Source parsing is not proof of current LAN advertisement or resolution. |

## Deliberate exclusions

Pealayer timeline, cue editing, and media transport business logic are excluded
by requirement. Existing PU850/Puzzles game business logic is not copied;
hardware transports, discovery, update, presentation, configuration, and
diagnostic infrastructure remain eligible and are tracked individually in the
ledger.
