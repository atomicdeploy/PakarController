# Resource usage: builds and running boards

## CI and existing build artifacts

The firmware job publishes **PakarController-resources / resource-report.json**
for 90 days and adds a resource table to its GitHub Actions summary. It reports:

- ESP32-S3 raw application bytes against the smaller OTA slot read from the
  actual partition binary, not total flash capacity.
- AVR application bytes against the packaged Urboot boundary. The padded
  32 KiB recovery image is not the application's flash usage.
- AVR ELF `.data`, `.bss` and `.noinit` allocation against 2,048 bytes SRAM.
  Remaining static space must accommodate stack/heap; it is not live free SRAM.
- Artifact hashes and source-checkout identity. Newly compiled artifacts also
  carry FQBN, firmware version and numeric feature flags. Existing artifacts
  without build metadata explicitly say unavailable; no identity is invented.

After a firmware build, run without compiling again:

```sh
node tools/firmware-report.mjs --require --write
node tools/resource-report.mjs --write
node tools/resource-report.mjs --baseline previous/resource-report.json --write
```

The workflow tries the last successful `main` build's retained resource artifact.
The first run, expired artifact, or changed hardware profile explicitly has no
comparable baseline. It does not manufacture a zero baseline. Comparable clean
builds show byte deltas. History remains in the per-run artifacts, not a fabricated
runtime chart. Local old outputs are not proof they were built from today's checkout.

Reviewed limits live in [resource-budgets.json](../interfaces/resource-budgets.json):
S3 OTA 95%, AVR application flash 98%, AVR static SRAM 90%. Per-comparable-build
growth limits are 256 KiB, 2 KiB and 128 bytes respectively. Exceeding either the
capacity/utilization or growth limit fails the resource step; adjust policy only
through review with justification. These limits are guardrails, not proof that
runtime memory, interrupt timing or electrical behavior is safe.

## Read-only live observation, including remote access

```sh
node tools/resource-report.mjs --origin http://ledcontroller.local
node tools/resource-report.mjs --origin https://ledcontroller.digitalogic.ir --output live-resources.json
```

Run the same command on ASUS over SSH when only ASUS can resolve/reach the board.
Remote URLs must already route to the authorized board's `/api/telemetry`; this
command does not enable cloud access or bypass authentication. If authentication
is enabled, set both `PAKAR_RESOURCE_USERNAME` and `PAKAR_RESOURCE_PASSWORD` in the
process environment, not command arguments or repository files. Requests are
bounded GETs, use the shared client identity headers, never follow redirects,
and never refresh the filesystem, flash firmware or change a scene.

Results identify the board, boot, hostname, version, observation UTC time and
request duration. Consumers must expire `observed` after `stale_after_ms`
(15 seconds); `observationStatus` implements this. Failed requests produce
`unavailable` with an attempt timestamp, not old values labelled current.

Live fields include internal free/minimum/largest-block heap, PSRAM total/free,
running-image identity, uptime and UART errors. AVR measurements are suppressed
when offline or its firmware marks telemetry stale. LittleFS retains its own
**cached observation** label and capture age even when the HTTP response is fresh.
`ota_free_bytes` is not relabelled as the running slot's capacity.

CPU utilization and flash-write wear are unmeasured. CPU MHz is not utilization.
Compile-time allocation is not runtime pressure. The historical chat figures
(2.14 MiB, 74 KiB heap, 293 bytes AVR free, etc.) are dated observations, not
defaults, baselines, or current readings. Cloud MQTT/WebSocket delivery requires
explicit provisioning and independent reception evidence; endpoint availability
does not establish Internet reachability.

The current work adds reporting/tooling and CI. Full equivalent resource cards
and history views across Rust, Flutter and Web still require separate UI review.
