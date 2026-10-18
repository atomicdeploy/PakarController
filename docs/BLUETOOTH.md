# Bluetooth LE

ESP32-S3 supports BLE, not Bluetooth Classic/SPP or Bluetooth audio. The S3
firmware uses the Arduino-ESP32 platform's bundled NimBLE stack; Android uses
BluetoothGatt through the existing Flutter platform-channel boundary. No
second LED engine, Wi-Fi loopback server or new Bluetooth dependency is needed.
ESP8266 has no Bluetooth radio and does not advertise this capability.

## Connection and identity

Select Bluetooth as the mobile connection method, grant Nearby devices access,
scan and choose the board. Older Android releases also require Location scan
permission. Advertised names are candidates, not proof: the app reads product,
stable board ID and boot ID from INFO before accepting the connection. The
advertised name follows the configured hostname (shortened only for the BLE
scan-response length); the full identity is available through INFO/status.

Bluetooth is a separate selected board transport. Board commands, status and
configuration do not require DNS, Wi-Fi, HTTP or WebSocket, and do not silently
fall back to them. Independently enabled app updates/remote-management services
remain independent: their failure must not block offline board controls.

One GATT owner is supported. Extra clients cannot receive the owner's replies.
Disconnect, boot/identity changes, disabled BLE and maintenance revoke queued
authority. Missing command receipts report uncertainty, not automatic replay.

## Living GATT contract

| Attribute | UUID | Operation |
|---|---|---|
| Service | `a4b68d10-5c3f-4c5a-9f12-50616b617243` | Pakar-compatible candidate discovery |
| RX | `a4b68d11-5c3f-4c5a-9f12-50616b617243` | Write with response |
| TX | `a4b68d12-5c3f-4c5a-9f12-50616b617243` | Notifications |
| INFO | `a4b68d13-5c3f-4c5a-9f12-50616b617243` | Read verified identity/capabilities |

Each value has a seven-byte header: kind (`0` data, `1` ACK), little-endian
16-bit message ID, offset and total length. Data carries the following bytes;
ACK carries the next expected offset. Values are bounded by `min(MTU-3,512)`.
Only exact outstanding fragments may be retransmitted, never a new logical
command. Requests are at most 8192 bytes and responses 65535 bytes. Partial assembly,
queued callbacks, retries and teardown are bounded; conflicting/out-of-order
fragments are rejected. ATT write success is not device-command acceptance.

JSON RPC uses `type: ble-request`, random `request_id`, `method`, living `/api/...`
`path`, string-valued `form`, `board_id`, `boot_id` and `expires_at_ms`. Responses
carry `type: ble-response`, the same request ID, current identity, HTTP-style
`status` and object `body`. Writes pin the actual board clock, not phone wall
time, and consume the existing request receipt before shared command dispatch.
`GET /api/ble/clock` provides a compact clock without waiting for a full status.

`GET /api/ble` reports the exact supported routes and limits. `POST /api/ble`
with `enabled=true|false` persists the setting through the existing HTTP peer
fences or the selected BLE session. Disable replies have a finite completion
window before link teardown. `PAKAR_S3_BLE_ENABLED=0` is the build-time exclusion.

## Supported operations and boundaries

The adapter reuses existing status, table/mapping, LED command/bulk/pixel
diagnostic, device identity, network/cloud settings, remote firmware jobs and
hardware-acknowledged peripheral contracts. Live state, update progress and
peripheral events use TX; Android feeds the existing state/mirroring consumers.
The board's advertised route/capability set, not the transport name, enables UI.

Direct binary firmware upload/download, raw high-rate pixel streaming and Wi-Fi
SSID scan are not advertised over this BLE transport. Remote firmware fetch/
apply/cancel commands use the existing board updater; they still need an actual
board Internet/LAN route. Unsupported operations must not silently use HTTP.
BLE is a control path, not a promise of Wi-Fi-equivalent throughput.

Board authentication is currently disabled by user policy. If authentication is
enabled or its configuration is damaged, BLE RPC fails closed: credentials are
not sent over this unencrypted GATT service. Bonded/encrypted authenticated BLE
is a separate pending acceptance item, not an implicit password bypass.

Wi-Fi and BLE share the S3's 2.4 GHz radio. Actual phone throughput, Wi-Fi/AP
coexistence, optical LED output and Android suspend/reconnect need physical
acceptance. Codec tests, SDK compilation and a staged APK do not prove these.

## Verification

Source checkpoint `a6ddc991a8d51de236f04941d52ddc90bbe37044`: S3 application 2207808 bytes,
SHA256 `efc7b42fc29c70d47f2aab6a8a7dd3de51a0f6bfc4193b69f77de8e792383ec2`;
SDK reports 2207655 program bytes and 127672 static-RAM bytes. Android 0.2.7+10,
57887073 bytes, SHA256
`b673361d72ac498b7b6d6436ed6084e651156cc18feda7924c4032f47a5e8f87`.
Existing preview signer/package verified with apksigner/aapt; this is not new
production-key signing. The exact APK is hash-verified on ASUS at
`C:\Users\David\Documents\PakarController\staging\PakarController.apk`.
Actual Kotlin/APK compilation, analyzer, 9 focused BLE fixtures and the corrected
device-directory/mirroring files pass. Installed phone/radio acceptance remains
open; the latest ADB inventory was empty. No native build/replacement is needed
for this Android transport change.

The APK is also [published through the configured app-update endpoint](https://office.digitalogic.ir/pakar/packages/b673361d72ac498b7b6d6436ed6084e651156cc18feda7924c4032f47a5e8f87/PakarController.apk).

### Actual board delivery, not phone acceptance

The exact candidate was SHA-verified in the inactive OTA slot by remote job 18,
manifest SHA256 `214ecd90c1542986a609376fbd59467844871deb814063b786b4e70b9b756647`.
After a host status reset, the reviewed updater resumed that existing ready job
and applied once; it did not repeat fetch/upload. Boot changed from
`d1170ed71821eda19075c19d70bad8c0` to
`ce96e4b5f7073315aa4bad5e14e3cd0d`. Read-only USB HELLO/status and token-bound
snapshot prefix identify `esp32s3-907069eac800`, hostname `ledboard`, BLE enabled/
started and Wi-Fi uplink ready. The snapshot prefix is not a complete JSON/
image-integrity attestation. USB status also acknowledged AVR online and station/
AP active; no AVR firmware or wiring changed in this delivery.

Actual ASUS Bluetooth scans receive the expected service from `907069EAC801`
at -42/-44 dBm. This is advertisement evidence only. Windows uncached GATT
service discovery timed out; no INFO identity or RPC acceptance is claimed.
The probe now retains outstanding WinRT operations and their resource owners
until terminal completion, rather than incorrectly closing them at timeout.
The corrected bounded probe exited cleanly; the earlier unsafe teardown crashed
its diagnostic process. No pairing, RX writes or radio settings were attempted.

The postboot LAN verifier reached its deadline while ASUS's Wi-Fi was software-
off; Ethernet could not reach the previously verified board address. No flash
retry followed. Full running-image SHA readback, heap/LED soak, restored LAN
reachability and actual Android installation/offline control/reconnect remain
open in issue #20. The known running-image export short-read defect is tracked
separately in issue #7, not reported as repaired by BLE delivery.

Read-only ASUS diagnostics can use Windows PowerShell:

```powershell
./tools/probe-bluetooth-windows.ps1 -Seconds 5 -ReadInfo -ExpectedBoardId esp32s3-907069eac800
```

Omit `-ReadInfo` for advertisement-only inspection. A wait timeout does not mean
Windows has cancelled its underlying connection operation. The diagnostic sends
no Pakar command and is not a replacement for the Flutter acceptance test.

Use `tools/test-bluetooth.ps1` for bounded C++ frame/receipt and integration
fixtures, and `tools/build-mobile.ps1 -Mode Test -TestPath test/ble_transport_test.dart`
for the fake-GATT client path. Build through the normal firmware/mobile wrappers
and reuse their existing caches. Track source, board and installed-phone proof
separately in [issue20](https://github.com/atomicdeploy/PakarController/issues/20).

Primary references: [Espressif Bluetooth support](https://documentation.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/bt-architecture/overview.html),
[radio coexistence](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/coexist.html),
[Android Bluetooth permissions](https://developer.android.com/develop/connectivity/bluetooth/bt-permissions).
