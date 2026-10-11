# Native Windows notifications and remaining desktop parity

Owner: #5. Parent production tracker: #2. File-by-file tracker: #3. Existing draft PR: #1.

The old foundation imported in-app toasts, but omitted native system notices.
Actual Pealayer Shell notification code and the controller result/connection
call sites are restored. No full Pealayer/production-complete claim is made.

- [x] Native Shell banners on the existing tray HWND/GUID, severity, quiet-time and optional sound.
- [x] Actual shown/hidden/timeout/click counters; OS acceptance is separate from visibility.
- [x] Privacy-safe native messages, bounded priority queue, cooldown and flood protection.
- [x] Matching settings/maintenance results, explicit failures and fresh device/AVR connection transitions.
- [x] Native/embedded notification preferences, persistence, test action and CLI/IPC deep-link.
- [x] Icon-bearing tray entries, Device Settings and Refresh actions, status tooltip and safe no-tray fallback.
- [x] Generic fullscreen/F11 and typed eight-action Jump List/resource catalog.
- [x] Native caption palette adaptation and indeterminate activity from real pending requests; balanced COM.
- [x] Seventy native tests and locked local Windows release build.
- [ ] Final exact-commit GitHub CI verification.
- [x] Final packaged local runtime, shown callback and inspected preferences screenshot; repeatable receipt script included.
- [ ] Actual banner-click restoration, visible tray/Jump List action and hidden-window acceptance.
- [ ] Root taskbar initialization/retry and Explorer-restart acceptance.
- [ ] Restore configurable/local/global shortcuts with the actual donor parser/matcher/ownership.
- [ ] Shared native/Web/TUI notification and preference contract.
- [ ] Desktop peer updater, custom/state icon editor, generic window settings and remaining diagnostics.
- [ ] Full translation/workspace state and complete remaining donor-file comparisons.
- [ ] ASUS installation and real device-triggered notifications after the laptop/device returns.

Exact source/omission mapping: `docs/WINDOWS-DESKTOP-AUDIT.md`,
`docs/FILE-COMPARISON.md`, `docs/file-comparison-evidence.json`.
The large `windows.rs`/`app.rs` donors are not promoted to complete-file review
by the focused native-notification mapping. Useful unported code remains open;
only identified media/frame-step/chapter/volume behavior is excluded.

ASUS localhost:1022 currently refuses SSH, and the real profile's ledboard.local
is offline. No firmware, EEPROM or device-setting writes occurred in this pass.
