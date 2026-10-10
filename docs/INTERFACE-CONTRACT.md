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
transport state; embedded Web uses its actual HTTP adapter; Dart currently
consumes page eligibility only. CLI commands use the same firmware endpoints;
they are not automatically generated from this UI catalog.

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
| Device page catalog and ACK eligibility | Generated catalog used by editor | Generated catalog used by dialog | Dart eligibility DTO added; mobile screen still needs contextual navigation/freshness binding |
| Network/cloud strict draft/secret/ACK/readback | Existing actual contract | Adapted and HTTP-double tested | Mobile editors not ported; compare command validation and conflict policy in CLI/TUI |
| Identity and device access | Existing editors | Existing handlers retained in dialog | Mobile lacks full editors; Web identity/access drafts do not yet share the network/cloud dirty-close guard |
| Firmware maintenance | Probe/local-file/remote/vault controls | Existing remote/vault actions retained | Compare/export full probe/recovery surface and uncertainty handling on Web/mobile; physical update acceptance open |
| Workspace shell | Docking, layouts, preferences, native dialogs | Visibility/collapse/reset, grouped settings and transport actions | Web panel order/edit handles, profiles, logs/notification surfaces and native dialog equivalents remain pending |
| Platform shell | Native tray/single-instance/toast code exists in source | Appropriate browser capabilities still pending | Install/update/share/fullscreen/notifications/haptics/audio/wake-lock require permission/lifecycle tests; not media-business exclusions |
| Table, front panel and diagnostics | Actual native panels | Existing live panels revealed rather than duplicated | Mobile hardcoded navigation/default state still requires ACK-only contextual migration; Web screen-sampler parity pending |

Server configuration has no compare-and-swap revision. Web preflight catches a
change observed before submission, but cannot make two concurrent clients' writes
atomic. Public readback and secret-presence flags do not verify the exact secret
bytes or authenticate a firmware image. These limits remain explicit work.

## Proof and blockers

`tests/interface_settings_test.mjs` executes the actual embedded module with
HTTP/DOM doubles: required fields, native DTO equality, secret semantics,
conflicts, ACK/readback, single-write/no-retry and late-read revocation. Existing
Web access/context/cell/appearance/identity/remote/display tests remain relevant.
`mobile/test/settings_catalog_test.dart` is added, not locally executed yet.

Browser inspection uses the actual `WebUi.h` through
`tests/web_appearance_fixture.mjs`, visibly SIMULATED and mutation-disabled.
Navigation preserves the edited MQTT username; unacknowledged front-panel/AVR/
firmware pages are absent. Narrow-width inspection caught an intrinsic-size
overflow; shrinkable rail/grid correction was retested with dialog scrollWidth
equal to clientWidth. This is not physical or production deployment proof.

Native compilation/replacement requires resolving the existing canonical cache
budget block without a second cache, guard bypass or repeated clean/rebuild.
The intended ASUS SSH and `ledboard.local` must be reachable for live firmware
deployment. Do not present loopback screenshots or ESP builds as deployed proof.
