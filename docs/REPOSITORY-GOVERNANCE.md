# Repository governance and Pealayer comparison

This is a repository-infrastructure adaptation, not whole-project superiority.
Pealayer's actual files were read in full, not reconstructed from their names.
Original-byte hashes, target hashes and exact omissions are maintained in the
canonical `file-comparison-evidence.json` / generated `file-comparison.csv` on the
application branch. The owning checklist is issue #3; production remains issue #2.

| Actual Pealayer source | PakarController mapping | Retained / adapted | Omitted and why |
|---|---|---|---|
| `README.md` | `README.md`, canonical `docs/assets/app-icon.png` | Centered brand, workflow/status badges, real preview, section navigation, architecture, build/docs/contribution links | mpv, media timeline/cues, cinema controls and claimed OS support: not this application's hardware or product. Preview is explicitly simulated; no fabricated release badge. |
| `.github/workflows/ci-cd.yml` | `.github/workflows/build.yml`, `tools/ci-checks.mjs`, `tools/prepare-release.mjs` | Pinned actions, no checkout credentials, health gate, timeouts, concurrency, test-before-build, packaging, curated notes, source/run manifest, checksums, attestation, consolidated fail-closed summary | libmpv downloads, MinGW linker/DLL, standalone downloader, macOS/Linux native packages and UPX compression: this app is mpv-free and these targets/package smoke checks are not accepted. No fake platform matrix. Native runtime/physical acceptance remains open. |
| `.github/workflows/codeql.yml` | `.github/workflows/codeql.yml` | Actions + JavaScript extraction without native build, extended queries, scheduled/PR/push checks, consolidated result | React-specific coverage names replaced by embedded Web/tools. Rust and embedded C++ are explicitly not analyzed by this configuration. |
| `.github/dependabot.yml` | `.github/dependabot.yml` | Weekly grouped Actions/Cargo updates | npm/Vite project: absent here; Cargo root changed to `/desktop`, Flutter `pub` added at `/mobile`. No dependency update is silently installed/deployed. |
| Remote Pealayer labels/settings | `.github/labels.json`, `.github/repository-settings.json`, `tools/sync-github-repository.mjs` | Acceptance, integration, lighting, profiles, performance, TUI, WebUI, dependency and release taxonomy; clear About/topics and cleanup/merge policy | Playback, media macros and motion labels don't describe current Pakar hardware. Existing labels/rules/secrets are preserved, not deleted. |

## CI/CD policy

Every child command is checked immediately. Quality tests do not compile an
extra native cache. `.gitattributes` normalizes text to LF so a Windows checkout
cannot corrupt hash-bound comparisons. Each native runner uses one canonical
`desktop/target`; locked builds and existing disk-budget/donor gates remain intact.
Hosted native cache reuse is separate from, and does not duplicate, the local cache.
Superseded branch/PR runs cancel; main/tag runs remain preserved.
Build pushes are restricted to main/tags; PR updates run once instead of compiling
the same feature commit in both push and PR workflows. Other branches can dispatch
explicitly. This adapts Pealayer's all-branch push trigger to avoid duplicate work.
Run artifacts expire after seven days. Local binaries are not rebuilt for README/governance edits.

Build stages: workflow lint → repository/interface checks → ESP32-S3 and AVR
(polling/busy-wire/buffered profiles), Windows test/release, Android preview. One aggregate check fails for a
missing, cancelled or failed required build. CodeQL has a separate aggregate check.

`vX.Y.Z` or `vX.Y.Z-preview` tags must match the shared firmware source version.
Owner dispatch derives `vX.Y.Z-preview`. Publishing is restricted to this repository
and requires all build jobs; PRs never get a write-token publishing step. Packages
have stable names (no random/time suffixes) inside the version's release. A release
is always created as a **draft prerelease**, never overwritten or automatically
flashed. The remote firmware manifests include raw ESP/AVR URLs, SHA-256, model,
signature, full 32-KiB Urboot image and matched UART mode. Draft asset URLs become
public only after an owner publishes the reviewed release. Checksums alone are
not authentication: independently trust the manifest hash and verify provenance.
Default firmware packages are the buffered S3+AVR pair. ESP8266 source, direct
build commands, optional raw/gzip reports (`firmware-report.mjs --legacy`) and
explicit legacy packaging (`prepare-release.mjs ... --legacy`) remain available,
but the current full image overflows real hardware budgets and is not required
or advertised as a release artifact. Legacy packaging still validates actual
images; opting in is not proof that today's source fits or boots.

No production-signing key is configured here. Android remains preview-signed.
The complete build/release upgrade is on the application PR. Main's repository-only
checkpoint adds presentation, CodeQL and governance without claiming the expanded
build/publisher is already active on that older application source.
No new release or tag is created just to make a badge green. Curated version notes
may replace `docs/releases/next.md` using `docs/releases/vX.Y.Z.md`.

## Repository settings and labels

```powershell
# Read-only diff, using the currently authenticated account
node tools/sync-github-repository.mjs
# Apply only to PakarController, with an owner-scoped GH_TOKEN
node tools/sync-github-repository.mjs --apply
```

The script never reads or persists credentials. It preserves existing labels,
topics, rulesets, branch protections and secret-scanning settings. Desired settings
include About/homepage/topics, Issues/Projects/Wiki/Discussions, squash/merge
support, no rebase merge, merged-branch cleanup and Web commit signoff. Actions
retain read-only default permissions and cannot approve PRs. Private vulnerability
reporting, Dependabot alerts and security fixes are enabled. No missing secret,
signing credential or completed acceptance is invented.

Issue forms collect device/client versions, acknowledged capabilities, transport
and proof scope. The PR checklist records actual donor paths and omissions,
generated-contract tests, screenshots, privacy and low-write verification. Release
categories use the same labels. Security disclosure stays private.
The donor's emoji Bug alias is not duplicated: the existing canonical `bug` label
is preserved and used by the forms. Useful domain labels are adapted, not removed.

## Default branch versus application review

The previous default branch `0428360` contained the initial firmware/client and
basic build workflow. Expanded native/Web/mobile work remains in PR #1 until its
required checks and outstanding acceptance are honestly reconciled. Repository
presentation/governance can be published independently; that does not deploy or
certify the app. Do not merge known failing application code merely to refresh
the GitHub landing page. Source, CI execution, installed app and physical board
versions must be reported separately.

## Remaining release gates

- [ ] Full native compile/test/package and runtime proof at the published commit.
- [ ] Complete hardware, UI/mobile parity and deployment acceptance from issue #2.
- [ ] Configure production release approval/signing after those gates pass.
- [ ] Promote the expanded application to main with verified checks; no forced merge.

These are real open requirements, not accepted omissions or a production label.
