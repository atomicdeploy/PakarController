# PakarController mobile

The Flutter control surface targets Android phones and tablets with the same
truthful board-state contract as the native and embedded Web interfaces.

It provides the row-major 80-cell table with the physical/logical identity
editor and half-cell column stagger, game modes, front-panel marquee and live
seven-segment preview, hardware/discovery state, and persistent authenticated
connection preferences. The user-facing endpoint defaults to
`http://ledboard.local`; any valid renamed `.local` hostname is supported. An SSDP origin is used only as a matching identity-checked
transport fallback when the Android resolver cannot resolve mDNS.

The Phosphor font is vendored in `assets/fonts/Phosphor.ttf`. Only used glyphs
are retained in release builds. The visual colors follow Pealayer's Studio
palette roles. Logical-ID search supports exact numbers and ranges with
highlighting; odd/even column backgrounds alternate and odd columns use +0.5.
Unknown board state is not displayed as zero or a confirmed off cell.

Passwords use `flutter_secure_storage` 11.2.0 (Android platform encrypted
storage), not SharedPreferences. A legacy plaintext password is removed only
after secure migration succeeds; no plaintext fallback is permitted.
Android app backup is disabled. HTTP Basic/Telnet over the ESP's plain HTTP
LAN services still require a trusted network; encryption at rest is not
transport encryption. Discovery is not cryptographic device identity.

Mutations are sent once, after a read-only identity check. A lost response
reports an unknown outcome and is never replayed by discovery. Marquee text
defaults to Once without forced scrolling; Loop and Interval are explicit.

```powershell
C:\development\flutter\bin\flutter.bat analyze
$env:NO_PROXY='localhost,127.0.0.1,::1'
C:\development\flutter\bin\flutter.bat test
C:\development\flutter\bin\flutter.bat build apk --release
```

The release APK is written to
`build/app/outputs/flutter-apk/app-release.apk`.

The current release-mode APK uses the template's debug signing identity and
is a **preview**, not a production-signed Android release. A durable signing
key, physical phone/tablet verification, full preference/menu parity and
actual board round-trips remain open in GitHub master issue #2. No keystore or
device credentials are committed. Flutter/Dart versions used for this
checkpoint are 3.47.3/3.13.3; all product surfaces advertise version 0.2.4.
