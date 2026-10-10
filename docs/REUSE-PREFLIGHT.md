# Donor-first continuation and regression prevention

## Failure acknowledged

The file inventory existed but was treated as documentation rather than a required
lookup. Before the storage incident, only 11 of 493 Pealayer files had completed
comparisons. Six generic tooling/hygiene sources were merely screened; the recent
build workflow was introduced without consulting the actual Pealayer resolver,
runner, package probe and source-stability code. That was a process failure, not
a lack of available source or a reason to drop useful functionality.

## Authorities, not another disconnected checklist

- legacy-source-ledger.csv: all six donor inventories, revision, original-byte
  hash, availability and read state. Selectively refresh Pealayer with
  tools/generate-reuse-ledger.ps1 -OnlyOrigin Pealayer; other donors stay unchanged.
  The generator skips identical CSV writes. Missing sparse files remain rows.
- file-comparison-evidence.json / file-comparison.csv / FILE-COMPARISON.md:
  complete-read decisions, exact source/target/test hashes and line anchors,
  retained code, concrete omissions and unfinished work. Screening is NOT reading.
- reuse-capabilities.json: capability-oriented index into those authorities:
  build-storage, capture, ui-shell, preferences, windows-desktop, network-cloud,
  firmware-hardware, updates-telemetry and games-streaming. It does not resolve
  unread files just because a matching keyword exists.
- consult-reuse.ps1: read-only lookup and gate. Outputs all matched donor paths,
  exact mapped targets, pending decisions and gap actions. Unknown concerns fail.
  -Gate verifies required complete-read source hashes, target/test hashes and
  anchors. Locally available donor bytes are rechecked. -RequireLive fails if
  any matched source is unavailable/changed; without it CI can use the committed
  snapshot, explicitly marked live-unavailable. No cached result is presented
  as fresh physical or live-host acceptance.
- Exhaustive lookup: `-Concern donor-audit` reaches EVERY inventoried path,
  including assets, tests, scripts and absent sparse blobs. `-Origin Pealayer`
  and literal `-SourcePath scripts/run-windows.ps1` narrow it without treating
  unmatched keywords as omissions. Rows expose read/availability, specialist
  routes, owning issues, preserved behavior, exact omission code/reason/disposition,
  proof references and remaining actions. Stale snapshot comparisons are labeled
  stale even outside a gate. `AuditOnly` counts paths without a specialist route;
  those need routing/review, not an accepted exclusion. The existing comparison
  CSV carries the same routes and issue links; there is no second decision ledger.

## Mandatory sequence on every task and unexpected issue

1. Read AGENTS.md, this document and the owning GitHub issue. Identify the user's
   applicable requirements and the affected capability; do not rely on a summary
   or promise that a previous pass brought everything over.
2. Run tools/consult-reuse.ps1 -Concern CAPABILITY. Inspect Rows, Remaining,
   Pending and Gaps, then read the exact donor and relevant dependencies in full.
   If the donor is available, check its live hash/revision; a stale mapping must
   be reread, not copied forward as proof.
   Also consult `-Concern donor-audit -Origin DONOR` for inventory coverage;
   specialist regexes alone miss unnamed implementation files. Use exact-file
   lookup when a dependency/helper is found. Inspect omissions and proof meaning,
   not just the verdict/count. Build a short next-file worklist from these rows.
3. Before writing a replacement or claiming a platform/tool is unavailable,
   inspect relevant Pealayer/PCController/PU implementations and tests. Search
   additional inventoried files with rg when the index has insufficient coverage.
   Add the missing route to the index. Do not blindly retry a failed approach.
4. Import actual reusable code, or adapt it with function-level provenance and
   concrete reasons. Every useful omitted behavior gets a pending item and next
   acceptance step. Only explicitly excluded media/PU business or conflicting
   hardware is not applicable; generic build/UI/diagnostic code is not business.
5. Update the SAME evidence ledger and issue checklist with source/target/test
   hashes. Run relevant tests and record whether proof is static, fixture, native
   runtime, LAN or physical hardware. Do not claim all-feature parity from a
   consulted slice or passing source guards.
6. Publish source/checkpoint and remaining work. Do not rebuild unrelated boards,
   APKs or donors, duplicate whole packages, or claim blocked cleanup completed.

At an unexpected failure, stop retries and record the failed operation, exact
donor files consulted, reusable mechanism selected, adaptation/rejection reason,
test and remaining acceptance in REUSE-PREFLIGHT-RECEIPT.md and the owning issue.
This applies to storage, screenshots, toolchain, UI behavior and deployment alike.
Do not make the user rediscover a pending capability to get it onto the ledger.

Build, stage and screenshot wrappers invoke their reviewed-slice gate before
action, including build Plan. Other capability lookups expose unfinished work;
using -Gate for a concern without required reviews fails instead of faking
acceptance. CI validates every indexed source and the preflight tests. This
enforces the important tooling paths, not every possible arbitrary shell command
or future agent behavior: AGENTS.md makes donor lookup mandatory for those too.

## Imports made from actual source

| Exact Pealayer file | Actual Pakar code / disposition |
|---|---|
| scripts/package-probe-windows.ps1 | tools/package-probe-windows.ps1: actual Process handle, redirected async stdout, bounded wait, exit code and Dispose. Used by optional stage -Probe. |
| scripts/libmpv-windows.ps1 | tools/windows-build-host.ps1: adapted host paths, Get-PakarCargoTargetDirectory and Get-PakarFileIdentity. Resolver called by native wrapper; identity called by staging. |
| scripts/package-windows.ps1 | Assert-PakarPackageSourceStable copied/adapted into shared helper; optional -ExpectedCommit checks before/after build or stage. No destructive Web restore copied. Full manifest/embedded identity still pending. |
| scripts/run-windows.ps1 | Native wrapper now uses the actual adapted shared resolver and locked builds. Toolchain fallback selection and host-triple provenance remain pending. |
| docs/verification/LOCAL-WORKSPACE-HYGIENE.md | Canonical cache, stable staging, owner checks, separate runtime acceptance and no blocked-operation rerouting become mandatory instructions. Historical donor paths/deployments are not Pakar facts. |
| scripts/check-crate-boundary.mjs | tools/check-crate-boundary.mjs imports actual module extraction/intersection, thin-entry and resource guards. Adapts to today's single binary and automatically enables library ownership checks when lib.rs exists. Native wrapper and CI execute it; six negative fixture cases prevent regression. |

mpv resolution/import conversion, DLL aliases, LIBMPV_DIR/user-PATH persistence
and media downloader packaging are not applicable to the requested mpv-free
controller. Generic branding/font/Web packing, UPX/resource verification, build
metadata, source-provenance manifest, saved host configuration and full screenshot
matrix remain useful unfinished imports. They are not accepted omissions.

Package source stability attests the Git source, not the already-built exe's
embedded source identity. -Probe currently tests CLI identity/exit/stdout, not
GL startup, all runtime DLLs or full package smoke. Original probe timeout throws
but does not terminate its child; that donor limitation remains explicit.

CI runs comparison generation with `-Check`: stale committed reports fail instead
of silently regenerating. Normal generation skips identical writes. Tiny comparison
fixtures now reuse an owned stable build directory, not a new GUID directory each run.

Example:

```powershell
$reuse = ./tools/consult-reuse.ps1 -Concern build-storage -Gate
$reuse.Rows | Format-Table Key,Comparison,Live
$reuse.Rows | Where-Object Comparison -eq parity-gap | Select-Object Key,Remaining
./tools/consult-reuse.ps1 -Concern donor-audit -Origin Pealayer
./tools/consult-reuse.ps1 -Concern donor-audit -Origin Pealayer -SourcePath scripts/run-windows.ps1
./tools/consult-reuse.ps1 -Concern preferences
./tools/generate-reuse-ledger.ps1 -OnlyOrigin Pealayer
./tools/generate-file-comparison.ps1
```
