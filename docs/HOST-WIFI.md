# Client host Wi-Fi (#31)

This feature joins the phone/tablet/PC to a board AP or its router. It does not
reconfigure the ESP, change its credentials, or treat a matching SSID as identity.
Normal discovery and board identity/version/authentication checks still apply.

## Android

Devices → Wi-Fi icon, or a device's menu → **Join Wi-Fi…**. A remembered name is
scoped to that entry, and a verified current board's advertised station/AP SSIDs
can be selected as hints. Hostnames are never guessed to be SSIDs.

- **Join for this app**: Android 10+ `WifiNetworkSpecifier`, explicit OS consent,
  45-second acquisition bound. This app's networking uses the selected local
  Wi-Fi until released/lost/activity destroyed. This includes cloud/update traffic;
  an ESP AP without Internet is not an Internet route. Other apps retain Android's
  normal network routing. Existing sockets may need a deliberate reconnect.
- **Save in Android**: Android 11+ `ACTION_WIFI_ADD_NETWORKS`. Check individual
  result codes; saving is not proof of joining. Use Wi-Fi settings to select it.
- **Wi-Fi settings**: OS-managed joins, saved profiles and WPA3/enterprise support.
- **Release app-only Wi-Fi**: retires only this app's callback/binding. Does not
  disconnect the system's Wi-Fi. Requests can be cancelled and no board commands
  or uncertain requests are replayed.

Permission is requested only when joining: nearby Wi-Fi on Android 13+, fine
location on Android 10–12 (some devices also require Location enabled). The direct
form supports open and WPA2-Personal networks. Other security modes use settings.
Only non-secret SSID hints persist; Wi-Fi credentials never enter API login storage.

## Windows Rust

Connection settings → Network → **This PC's Wi-Fi**. Refresh reads adapter-specific
saved profiles through Native Wi-Fi (`WlanEnumInterfaces` / `WlanGetProfileList`),
without parsing localized `netsh` output or exporting passwords. Select a profile,
Join, then explicitly confirm switching. The exact adapter/profile is revalidated
before `WlanConnect`; work runs off the UI thread. OS acceptance is reported as a
request, not successful board connectivity. Use Connect/Discover for that check.
**Wi-Fi settings** adds/edits new networks with OS-owned credentials. Non-Windows
Rust builds provide a truthful system-settings instruction rather than a fake join.

## Verification boundary

Unit/widget tests cover validation, secret omission, no implicit network changes,
hint semantics and platform dispatch. Build success does not establish OS consent,
AP reachability or correct routing on an installed device. Live network switching
requires a deliberate operator action; do not disrupt production tunnels to test.

Platform contracts: [Android local Wi-Fi requests](https://developer.android.com/develop/connectivity/wifi/wifi-bootstrap),
[Android saved-network consent](https://developer.android.com/develop/connectivity/wifi/wifi-save-network-passpoint-config),
[Windows Native Wi-Fi connection requests](https://learn.microsoft.com/en-us/windows/win32/api/wlanapi/nf-wlanapi-wlanconnect).

### 2026-10-27 candidate verification

- Flutter analysis: clean; `host_wifi_test.dart`: 5 passing tests, including
  small-screen layout, explicit dispatch and saved-name/no-password persistence.
- Rust canonical Check, filtered `host_wifi` test (1 passing), and Release: passed.
- Hash-bound donor comparison check and `git diff --check`: passed.
- Android APK SHA256: `72cd52693b7d794cf35091162a75ab9aa196a7bb75d1376d7b30529f1c8d13d4`.
- Windows executable SHA256: `436328c9d901c4327abd2e84ea7a1e642714ed353073de5cf0041c5e5c037f57`.

Candidates are built, not installed. No board or host network was switched.
Installed-device consent, AP routing, denial/recovery and live peer handshake
remain open on #31. The owned, unused 2.782 GiB `desktop/target/debug` cache was
removed through the existing guarded cleanup after plan/owner checks; release,
running applications, settings and rollback data were preserved. Debug cache
contents are rebuildable, not user data.
