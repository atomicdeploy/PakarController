# PakarController guides

One firmware/API contract, one canonical Web UI, and contextual Rust and Flutter
clients. Start with the guide for your task; production acceptance remains in
[issue #2](https://github.com/atomicdeploy/PakarController/issues/2).

## Operate and recover

| Task | Guide |
|---|---|
| Identify the board and wire it safely | [Hardware](HARDWARE.md), [verification](HARDWARE-VERIFICATION.md) |
| Connect, authenticate or recover access | [Access](ACCESS.md), [host Wi-Fi](HOST-WIFI.md), [peer identity](PEER-IDENTITY.md) |
| Configure cells and their states | [Cell assignments](CELL-ASSIGNMENTS.md) |
| Use the terminal | [Terminal UI and shell](TERMINAL-UI.md) |
| Browse or transfer files | [User files and protected paths](USER-FILES.md) |
| Update boards | [Remote maintenance](REMOTE-MAINTENANCE.md), [tooling](SCRIPT-TOOLING.md) |
| Update and monitor client applications | [Client management](CLIENT-MANAGEMENT.md) |

## Develop and integrate

| Task | Guide |
|---|---|
| Build only what changed | [Root build instructions](../README.md#build), [toolchain](TOOLCHAIN.md), [low-write storage](BUILD-STORAGE.md) |
| Find the authoritative component | [Architecture and gap register](ARCHITECTURE.md) |
| Implement an integration | [API](API.md), [living API policy](API-POLICY.md), [duplex control](DUPLEX-CONTROL.md) |
| Build a host-owned game/presentation | [Host presentation](HOST-PRESENTATION.md), [removable Snake example](../mini-apps/snake/README.md) |
| Contribute without losing another owner's work | [Contribution rules](../CONTRIBUTING.md), [GitHub coordination](GITHUB-COORDINATION.md) |
| Reuse prior projects | [Mandatory donor preflight](REUSE-PREFLIGHT.md), [file comparison](FILE-COMPARISON.md) |
| Verify CI packages and release scope | [Repository governance](REPOSITORY-GOVERNANCE.md), [handoff](HANDOFF.md) |

## Before reporting a failure

Include the board role/hostname, app and firmware versions, selected transport,
exact error, and whether the failure survives reconnection. Include the relevant
CI run or commit for build failures. Do not paste credentials, private settings
or whole device backups into public issues.

| Symptom | Safe first check |
|---|---|
| Missing tool or compiler | Run `pakar doctor`; use the existing SDK location and locked wrapper. Do not clean caches or reinstall everything. |
| Build-storage guard refuses a build | Inspect `tools/storage-hygiene.ps1` read-only output and the [storage policy](BUILD-STORAGE.md). Release caches and rollback are protected. |
| Cannot find the board | Check the client network/USB selection and host Wi-Fi. A discovered name is not an authenticated/current connection. |
| Firmware update response is lost | Inspect the board's running identity and maintenance job first. Do not blindly repeat an ambiguous upload. |
| Donor report is stale | Read the affected source/target before regenerating evidence; generation is not manual review. |

CI verifies source/build contracts. Installation, live board behavior, optical
output, audio and human acceptance are separate gates. Historical receipts are
dated evidence, not a permanent claim about today's connected devices.
