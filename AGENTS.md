# PakarController continuation

- Preserve existing user/concurrent changes. Donor checkouts remain read-only unless the user requests changes there.
- Use hostname-first device access. A build, loopback fixture or screenshot is not physical acceptance.
- For screenshots use the built-in framebuffer command or tools/capture-window-windows.ps1 with an explicit verified ExpectedProcessId. The tool imports Pealayer's read-only Win32 capture and automatically falls back when the expected owner cannot provide internal capture. Do not declare Windows screenshots unavailable merely because the external WGC helper failed.
- Never guess a target, accept a blank image or overwrite evidence. Wait for the app's screenshot result, verify PID/backend and inspect the image. IPC acceptance is not capture completion.
- Win32 capture does not focus, move, resize or inject input. Use supported computer-use APIs for desktop input; capture availability does not broaden UI authority.
- Fixtures require an isolated PAKAR_CONFIG_FILE and explicit simulated peer. PAKAR_SCREENSHOT_PATH enables startup capture; with single_instance=false the harness skips production IPC/tray/Jump List hooks and window activation. Stop only owned identity-verified fixtures.
- Replace production through graceful IPC after checking open drafts; preserve settings/prior executable. Push source/tests/docs and report exact GitHub/package identities and remaining acceptance.
