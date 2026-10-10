# Cell styling and screenshot capture

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
