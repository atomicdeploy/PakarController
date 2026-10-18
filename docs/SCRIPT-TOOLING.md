# Shared script tooling

Windows: `pakar.cmd …`. Linux/macOS or Git Bash: `bash pakar.sh …`.
Both launch the same Node owner and existing guarded PowerShell wrappers;
Node 20.19+ and PowerShell 7 (`pwsh`) are required. No npm install is needed.
Use the existing Arduino/Rust/Flutter SDKs, not duplicate installations.

```text
pakar --help
pakar doctor
pakar --json doctor
pakar all                              # plans only; no compilation
pakar all --only Firmware,Desktop       # same component-selection contract
pakar firmware --target esp32s3 --plan
pakar desktop --mode Check --plan
pakar mobile --mode ApkPreview --build-only --plan
pakar dependencies --plan               # offline locked SDK command plan
pakar dependencies --check              # reads registry metadata; no pin edits
pakar dependencies --apply              # explicitly edit only newer stable pins
pakar dependencies --install            # explicitly install locked dependencies
pakar artifacts                        # report existing bytes; no build
pakar artifacts --write --require       # stable metadata/checksums for six images
pakar update --origin http://ledboard.local   # plan only
pakar update --origin http://ledboard.local --run
pakar scripts                          # inventory of existing utility wrappers
```

On Windows use `pakar.cmd` in the examples; on other platforms substitute
`bash pakar.sh`. Add `--run` to `all` only to execute the selected components.
`all --release` plans native Release / Android preview packaging; even with
`--run` it does not install applications or flash hardware. `build.cmd` and
`bash build.sh` preserve the positional firmware interface, for example
`bash build.sh buffered esp32s3 --s3-flash-mb 16 --s3-psram opi`.
Legacy `upgrade.cmd` / `bash upgrade.sh` now delegate to the same dependency
owner and check by default. Protocol tests are explicit, not a hidden build step.

## Output, failure and storage policy

- Shared staged messages, actionable missing-tool errors and elapsed outcomes.
  `--json`, `--quiet`, `--no-color` precede the command. JSON emits newline-delimited
  events, including captured child diagnostics. Quiet suppresses wrapper chatter,
  not raw compiler errors/output; compiler carets and native exits are retained.
  `NO_COLOR` is honored; redirected Node output has no ANSI coloring.
- Arguments are arrays, never `eval`, shell interpolation or `Invoke-Expression`.
  Windows child windows are hidden. PowerShell native calls restore the caller's
  directory even after failure. The first failed selected component stops `all`.
- Plans validate policy, donor hashes and generated-asset freshness; they do not
  invoke SDK compilers. Builds reuse canonical warm caches. No default clean,
  debug/symbol build, cache relocation, SDK installation, artifact removal,
  audible cue or deployment was added. Existing capacity guards remain active.
- Registry checks keep one bounded in-memory copy per official index, compare
  numeric stable versions (including two-component library versions), never
  downgrade, and detect concurrent pin edits before atomic replacement. A failed
  or interrupted pin replacement can leave a `.pending` file for inspection;
  it is never overwritten automatically. `HTTPS_PROXY` / `ALL_PROXY` (and lowercase
  equivalents) use curl's bounded HTTPS path, including HTTP/SOCKS proxies and
  `NO_PROXY`; credentials are inherited, not placed in argv or logs.
- Artifact reports distinguish raw images from optional, separately named
  ESP8266 `.gz` files. S3 application bytes remain raw. Full AVR+Urboot images
  must be 32768 bytes. Reports contain sizes, SHA-256, MD5 and current source/dirty
  identity; unchanged reports are not rewritten. Metadata is not installation proof.

## Platform scope

The shared dispatcher, dependency checker, artifact verifier and firmware/native/
mobile **plan paths** have Windows/Linux/macOS CI coverage configured. Actual
SDK builds still require the matching platform SDK/toolchain and the existing
component guards. Windows window capture, native runtime staging and Windows
package probing are explicitly Windows-only, not silently emulated. Routing an
older utility through `pakar run NAME` preserves that utility's own platform and
hardware requirements; it does not certify every legacy helper as portable.
`PAKAR_FLUTTER` or `--flutter-path` selects an existing Flutter installation;
PATH is preferred to the legacy Windows fallback.

## CI/CD

- `build.yml`: OS-matrix script fixtures/plans; existing contract/firmware/native/
  Android jobs; centralized locked firmware pins; rich image-integrity summary;
  immutable draft release packages and existing attestations. Source checks do
  not certify a successful current GitHub run or physical peripherals.
- `update-dependencies.yml`: weekly/report-only by default. Draft pin-change PRs
  require repository variable `PAKAR_DEPENDENCY_PULL_REQUESTS=true`. A manual
  dispatch defaults `check_only=true`. No automatic merge, SDK install or flash.
  A dedicated `DEPENDENCY_BOT_TOKEN` can permit normal downstream PR CI; otherwise
  GitHub's default-token workflow-trigger restrictions still apply.
- `upload.yml`: **disabled unless explicitly opted in** with
  `PAKAR_ENABLE_DEVICE_DEPLOY=true`. Requires an authorized self-hosted
  `pakar-deploy` runner, `ledboard` environment and reachable
  `PAKAR_DEVICE_ORIGIN` (default `http://ledboard.local`). Check repository,
  main branch, trusted build workflow/event, exact run/SHA and artifact digest
  before delegating to the existing automatic updater. Unknown writes are not
  replayed. A runner reaching the board does not imply Internet reachability.

## Actual PU850 source import and remaining gaps

Donor `atomicdeploy/pu850-esp` revision
`431104072952f694a35b3a83c19f13fbb098ca5b`, original byte hashes, Git blob identities,
target anchors and test references are recorded in the existing
[file-by-file evidence](file-comparison-evidence.json), not a parallel checklist.

| Fully read donor files | Reused/adapted destination |
|---|---|
| `build.cmd`, `build.sh`, `.vscode/prebuild.cmd`, `.vscode/prebuild.sh` | `pakar.cmd/.sh`, `build.cmd/.sh`, `tools/pakar.mjs`, guarded build wrappers |
| `Tool/build-all.ps1`, C#/C++/Go PowerShell/Bash build helpers | `tools/build-all.ps1`, `tools/script-runtime.ps1`, shared native runner/preflight |
| `Tool/upload.js`, `Tool/package.json`, `Tool/README.md`, `Tool/NODE-UPLOADER.md` | `tools/script-console.mjs`, CLI validation/dispatch and existing updater integration |
| `upgrade.sh`, dependency workflow | `tools/firmware-dependencies.mjs/.json`, `upgrade.cmd/.sh`, guarded dependency PR workflow |
| Build/tooling/upload workflows, Dependabot configuration | OS-matrix tooling checks, `tools/firmware-report.mjs`, `tools/deploy-artifact.mjs`, guarded upload workflow; existing Dependabot/release pipeline retained |
| `.vscode/arduino-cli.yaml`, `sketch.yaml`, `Tool/package-lock.json` | Central firmware pins / existing explicit board flags; no duplicate npm tree or SDK installation |
| C#/C++/Go client READMEs, `Tool/upload.test.js`, `Tool/C++/tests/integration_test.py` | Explicit protocol/platform comparison; actual subprocess offline/no-contact and existing loopback multipart, identity/hash and no-replay fixtures |

This is 28 fully read files, not complete PU client/tool parity. Concrete omissions:
forced clean/no-cache rebuilds conflict with the low-write requirement; beeps and
personal/timestamp headers are unwanted side effects; `eval` is unnecessary;
gzip renamed as `.bin` obscures image identity; bypass switches and first/latest
artifact guesses weaken update integrity. PU UART/business payloads and C#/C++/Go
product-client builds are not substitutes for Pakar's existing Rust/Flutter clients.

Useful watch/resume/export/pre-update-backup flows, full donor test-case parity,
complete Unix native distributables, actual OS CI execution,
bot/runner/environment activation and live firmware acceptance remain open.
They are recorded as gaps, not declared unnecessary or passed by fixtures.
