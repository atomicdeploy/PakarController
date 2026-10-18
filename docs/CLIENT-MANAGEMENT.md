# Client updates and management

Temporary service: [office.digitalogic.ir/pakar](https://office.digitalogic.ir/pakar/).
The endpoint is configurable; it is independent of ESP/AVR firmware updates.
APIs remain unversioned. The dashboard requires the private operator token;
public catalogs/package downloads do not grant management access.

## Controls and limits

- Windows: Preferences → App updates. Automatic checks/download/cleanup are
  enabled by default. Automatic apply requires an explicitly initialized
  current-user managed installation, verified package, closed editors and no
  active device/firmware/stream operation. The app exits through normal Quit;
  its verified helper preserves the prior executable and profile and checks
  the replacement's startup receipt. It never force-kills an unhealthy process.
- Android: context menu → App updates. Automatic foreground checks default on;
  download, installation requests, cleanup and enrollment are independently
  opt-in. Installation still requires Android permission/confirmation, exact
  package/signers, newer build, SDK/ABI, length and SHA256 validation.
- Android automation runs while the app is foregrounded, not as an installed
  background service. Disabled/closed clients cannot report or act. Desktop
  automation runs while the application is running.
- Cleanup only removes owned obsolete staged packages. Current app, active
  transfer, user settings, journals, rollback and shared SDK/build caches are
  preserved. Server retention keeps at least the newest three packages per
  platform and removes additional verified packages only after seven days.
- Self-uninstall of the running Windows package remains unavailable pending an
  approved removal inventory. Android has an explicitly confirmed local
  self-uninstall request through the OS. Remote self-uninstall is disabled;
  it is never silently interpreted as removing arbitrary applications.

## Enrollment and telemetry

Root-local office commands (operator credentials are never command arguments):

```sh
node /opt/pakarcontroller-management/operator.mjs enroll windows-x64
node /opt/pakarcontroller-management/operator.mjs enroll android
node /opt/pakarcontroller-management/operator.mjs clients
node /opt/pakarcontroller-management/operator.mjs command CLIENT_ID check-update
node /opt/pakarcontroller-management/operator.mjs command CLIENT_ID cleanup
node /opt/pakarcontroller-management/operator.mjs commands
node /opt/pakarcontroller-management/operator.mjs revoke CLIENT_ID
```

Enrollment output is a secret, single-use, platform-bound token valid ten
minutes. Enter it in the client settings; Android stores the returned credential
in secure storage and Windows uses Credential Manager. Public settings/exported
IPC never contain it. Revocation stops subsequent reports/claims immediately.
Automatic telemetry is opt-in and contains application/platform/build, uptime,
update state/measured bytes, architecture/OS and local automation policy, not
passwords, board credentials, files, user documents or screenshots. The dashboard
shows latest reports and online/offline age; it is not an indefinite history.

Remote commands have a short expiry and durable single-use server claim plus
client write-ahead receipt. An uncertain claim/action is never replayed. Only
check-update and policy-approved owned cleanup currently execute. No arbitrary
shell, URL, package name or filesystem command is accepted.

## HTTP and WebSocket contract

Under the configured service base (`https://office.digitalogic.ir/pakar`):

| Method/path | Access / result |
|---|---|
| GET /api/health | Public readiness and product identity |
| GET /api/releases?platform=windows-x64 or android | Public newest package descriptors |
| GET /api/app-updates | Android `X-Pakar-App-Session` and `X-Pakar-App-Package`; session/deadline-bound offers |
| WS /api/events | Public Android release feed; `subscribe` with exact self-package/session |
| GET/HEAD /packages/SHA256/PakarController.exe or .apk | Immutable exact package; bytes/ranges/checksum headers |
| POST /api/clients/enroll | Single-use enrollment; returns protected client credential |
| POST /api/clients/report | Enrolled Bearer; allowlisted identity-bound report |
| GET /api/client/commands | Enrolled Bearer; short-lived allowlisted commands |
| POST /api/client/commands/ID/claim or /result | Enrolled Bearer; one claim/idempotent matching result |
| /api/admin/enrollments, /clients, /commands, /releases, /cleanup | Private operator Bearer only |

Release descriptors contain product/platform/version/build/bytes/sha256/url.
Android additionally has package/signers/version_code/version_name. No redirect
or embedded credential is accepted by client downloaders. HTTP success is not
installation success; measured transfer bytes and verified startup/OS receipts
are distinct states. Firmware versions are descriptive, not API generations.

## Deployment and publication

```powershell
node tools/deploy-client-service.mjs --plan
node tools/deploy-client-service.mjs --run
node tools/publish-client-release.mjs --platform windows-x64 --file PATH --token-file PRIVATE_FILE --plan
```

Publisher requires `--run` to write and Android requires the actual verified
signer digest. `interfaces/client-release.json` is the shared native/mobile
build identity; version guard also checks `mobile/pubspec.yaml`. Publication is
immutable: do not rewrite a consumed platform/build. Build newer source with the
next build number, verify/sign it and publish a new package. Publishing source
or a CI artifact is not proof of installed client acceptance.

Office uses a dedicated low-privilege systemd service, loopback port18743,
SQLite WAL and private content store at `/var/lib/pakarcontroller-management`.
Its root-only operator token is `/etc/pakarcontroller-management/admin-token`.
Apache has a scoped `/pakar/` include; existing PBX/MQTT configuration is untouched.
To migrate, preserve the service database, packages and private credentials and
change client service/catalog/feed origins explicitly. Never publish these secrets.

## Current checkpoint

Build9 source: Windows28,985,344bytes,
SHA256 `d366a33ba19f39235f04c8d6a398f7cc8089366691cbeee8ad23427528981271`;
Android57,542,761bytes,
SHA256 `acce672d7fac7a40c67216ce1e4fe55f5638c85041938a2816cc73a605ddebd7`.
APK package/version/signature independently checked:0.2.7+9, existing Android
Debug preview signer `cb2620ad0c885fffd5e1ee7df0bbef8ff3262729e4e880888e9001fbbfb83e26`.
Preview signing is not a production signing-key rollout.

Actual HTTPS service and both package publication receipts are verified. Focused
service9, native7 and Flutter13 cases plus analyzer and real Kotlin/APK build
pass. Actual client replacement/enrollment and Android installed two-version
consent, background uptime and human acceptance are recorded separately; no
board firmware, fuse, EEPROM, LED, buzzer or PBX action is needed for this slice.

Actual DAVID-ASUS rollout is verified: interactive session1, old PID22880
gracefully quit, new PID20244 ready/build9, new executable hash above, preserved
old executable hash `72270d79867da8fd50baecac8b8c3b659f2dc6c75a63dba5464d0d96edddb6fc`
and settings backup. Managed EXE/profile/current-user marker initialized; private
single-use enrollment consumed and Credential Manager-backed telemetry received.
The server observed periodic build9 reports and both remotely queued
check-update and cleanup actions with durable `claimed=1` and completed results.
Cleanup found no obsolete pending package to delete. This is actual client/service
acceptance, not proof of the future automatic two-new-version helper exchange.

The exact new APK is staged on ASUS at
`C:\Users\David\Documents\PakarController\staging\PakarController.client-management.apk`;
fresh ADB inventory is empty. No phone installation is claimed. The local native
EXE is staged at `C:\Users\David\AppData\Local\Programs\PakarController\PakarController.exe`;
host policy rejected initialization/launch before execution. Its preexisting
2024-byte profile remains unchanged and no alternate launch route was attempted.
