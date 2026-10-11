# Physical Android preview

Use the already installed SDK/ADB; do not recreate packages, caches or SDKs.
`tools/android-device.ps1` selects an explicit authorized device and queries
`adb devices -l`, `wm size` and authoritative keyguard state before input.

```powershell
./tools/android-device.ps1 -Serial 'YOUR_ADB_SERIAL' -Wake -DismissSwipeLock -Launch
./tools/android-device.ps1 -Serial 'YOUR_ADB_SERIAL' -Screenshot 'C:\absolute\new-capture.png'
./tools/build-mobile.ps1 -Mode ApkPreview -BuildOnly
```

Wake sends KEYCODE_WAKEUP. Optional dismissal sends one 450-ms swipe at half
the queried width, from 84% to 38% of its height, only when secure=false and
keyguard is showing. Ambiguous state or a PIN/password/pattern lock requires
manual unlock. No credential guesses, settings mutations, global server restart
or repeated swipe loop. Launch is the known Pakar MainActivity, not a guessed
package. Screen capture is binary exec-out, a bounded local client and CreateNew;
existing or partial evidence is never silently overwritten.

Actual donor reuse: the imported Pealayer scripts/package-probe-windows.ps1
process handle, no-shell launch, async stdout, exit status and disposal are used
directly by all text ADB probes. Its fixed timeout is retained. Binary capture
needs a separate stream rather than decoding PNG as text; only that script-owned
local capture process is terminated on timeout. This is not whole donor parity.

Physical Samsung SM-S908E acceptance: 1440x3088 queried, unsecured lock showing,
one explicit authorized wake/swipe dismissed it, Pakar launch reported Status: ok,
foreground MainActivity read back and actual PNG captured/inspected. The APK
launcher manifest and packaged PNG were inspected independently; a launcher-grid
screenshot remains separate from a running-app/header screenshot.

No repeated Flutter/Rust suite was run after the user's defer-tests request.
Prior acceptance results are recorded separately, not represented as new runs.
