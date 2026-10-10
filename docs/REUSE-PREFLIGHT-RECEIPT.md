# Donor-first preflight receipt

Tracking: issues #3 (all donor comparisons), #9 (storage tooling), #8 (capture).

Pealayer inventory refreshed file by file from clean canonical source
a54933aa7d0c05a058045dc9cd21b1b7fbc7378e: 493 tracked paths, none missing,
original-byte hashes/sizes and screening state retained. Other five inventories
were preserved. Automated screening is not a complete manual read. Six actual
tooling/hygiene files were read fully and added to the existing authoritative
comparison ledger: package probe, libmpv/shared resolver, runner, packager,
crate-boundary guard and workspace hygiene. Each has exact target/test anchors,
hashes and individual omitted-code reasons.

Pealayer comparisons increased from 11 to 17; 476 remain pending and nine reviewed
files still have gaps. Across all donors: 1,868 inventoried, 39 complete comparisons,
1,829 pending and 22 known gap files. Five newly reviewed tooling files correctly
remain parity-gap, not accepted exclusions. Only the complete package-probe helper
is byte-identical imported code (SHA-256
f4516884990ad76c020fbc6534d902c4949abae0b48da8ea40c008afa44dfd4f).

Implemented prevention: nine capability routes into the same file ledger;
read-only lookup with unknown-concern and unreviewed-gate rejection; exact required
source/target/test hash checks; live donor byte and tracked-path verification;
mandatory gates before native build/Plan, staging and screenshot capture; first-step
instructions in AGENTS.md for every task, failure and missing capability.

Executed locally:

- tests/reuse_preflight_test.ps1: all nine routes nonempty; cached build/capture
  gates; rejects unavailable live sources, unconfigured gate, unknown concern,
  screened rather than complete read, empty target mapping, stale source/test hash
  and escaping target. Actual imported Process helper returns short-lived stdout
  and rejects exit 7. Actual adapted resolver expands relative cache override.
  Source-stability helper rejects wrong commit.
- Live build-storage lookup with Gate and RequireLive: ten matching files,
  all live hashes match refreshed inventory, four pending comparisons and five
  reviewed gaps remain visible. Consultation does not certify them complete.
- Imported Process helper runs existing Pakar --help successfully. Optional stage
  -Probe on identical current artifact returns Changed=false/Written=false.
- Build storage plans/parser tests, reuse/ledger comparison adversarial tests,
  screenshot source guards and version checks pass. No compiler or firmware
  flasher was run, no app replacement and no private settings write occurred.
- Repeated unchanged inventory/comparison generation leaves write timestamps
  unchanged for legacy-source-ledger.csv, file-comparison.csv and FILE-COMPARISON.md.

The capture wrapper's only functional addition is the preflight gate; its previous
PID/backend/no-overwrite behavior is unchanged and both donor target hashes were
rereviewed. This pass does not claim a new runtime capture of that wrapper.

Limitations remain explicit: full package manifest/embedded build identity,
saved host/toolchain provenance, generic runner fallback, crate-boundary adaptation,
font/Web/branding/resource packaging and full screenshot/interaction matrix.
The original Process probe does not terminate its child on timeout. Native CLI
help is not full renderer/DLL smoke. Arbitrary direct shell commands are not
sandbox-enforced; continuation instructions prohibit bypassing these gates.

Earlier cleanup is still blocked; no old cache/SDK/source/app backup was removed.
The running native app remains the prior cell-surface executable. Source/CI
checks are not board, LAN, Android or whole-project production acceptance.

## Exhaustive consultation and actual guard follow-up

The earlier counts above describe the previous checkpoint, not current closure.
This follow-up retains the SAME inventory and comparison authorities:
1,868 files, 39 complete comparisons, 1,829 pending, now 21 reviewed gap files.
Pealayer remains 493 files / 17 compared / 476 pending; eight reviewed gaps.
Live canonical Pealayer is still clean at a54933aa7d0c05a058045dc9cd21b1b7fbc7378e;
all 493 inventoried bytes match. No new complete-file read is claimed from lookup.

Specific failure: specialist keyword routes cover only 530 distinct paths.
The other 1,338 (including 309 Pealayer paths) were inventoried, not approved
exclusions, but weren't returned by any specialist lookup. Added donor-audit
to reach every path, plus exact Origin/SourcePath lookup, visible AuditOnly,
read/availability, code/reason/disposition omissions, proof meaning, remaining
actions and owning issues. Existing CSV now includes routes and issue links.
Snapshot-stale comparisons are visibly stale even in ungated lookup.

Actual recovery source consulted fully in this follow-up:

- Pealayer docs/verification/LOCAL-WORKSPACE-HYGIENE.md: preserve the canonical
  cache, stable candidate and owners; do not route around blocked cleanup.
- Pealayer scripts/check-crate-boundary.mjs: adapted into
  tools/check-crate-boundary.mjs, not merely copied into a plan. Its actual
  module ownership/thin executable/resource guards now execute before native
  builds and in CI. The original assumes a library; Pakar detects optional
  lib.rs, applies shared ownership guards when present, and checks duplicate
  declarations/resources today. Exact source/target/test hashes and reasons
  replace this file's previous pending comparison in the same manifest.

Locally executed: exhaustive/filtered lookup, stale-source comparison checks,
all existing routing/gate/adversarial tests; comparison integrity and stale
report rejection; storage plan/parser tests; actual single-binary guard and six
negative crate/resource regression cases. Comparison -Check makes no writes and
refuses stale committed reports; CI uses it. Repeated unchanged generation leaves
CSV/report timestamps unchanged. Comparison fixtures use one owned stable
build/native directory instead of creating a fresh GUID directory each run.

Next-file worklist (all remain open in authoritative rows / issue #3):

- scripts/build-metadata.rs and scripts/package-windows.ps1: embedded source
  identity, package manifest and actual smoke tests, not just CLI --help.
- scripts/libmpv-windows.ps1 and scripts/run-windows.ps1: generic host provenance,
  saved configuration ownership, toolchain fallback and mpv-free runner.
- scripts/new-icon-pack.ps1: inspect actual icon authoring pipeline before
  replacing or claiming tool unavailability; currently only audit-routed.
- scripts/verify-messaging.mjs: inspect actual event/action verification harness,
  then map generic contracts; currently only audit-routed.
- Pending UI/preferences/Web/mobile/PCController/PuzzleBoard/PU sources stay
  open. This is not an import-completion or production claim.

No compiler, donor build, SDK download, firmware flash, app replacement,
cache removal or package copy was performed for this follow-up.

## Live donor drift caught and actual imports continued

The historical counts above remain historical. Canonical Pealayer advanced from
a54933a to clean d3f11b18d3b969398df18c26bc5871b426d4c332 during this pass. The build
preflight rejected the changed tracked inventory **before** the ESP compiler ran.
No failed-build retry bypassed that guard. Read the actual new storage-hygiene
script and all changed libmpv-windows, package-windows and hygiene sources through
EOF, then explicitly refreshed their individual source/mapping/test evidence.

New actual imports: Get-DirectorySummary and Test-PealayerFileContentEqual
(Pakar-named), called by read-only storage inventory and candidate staging.
Delete-destination-first copying is replaced by verified pending-copy/rename;
mutable compiler output is not hardlinked. Cache-inside-checkout rejection needs
an owned nonduplicating migration of the existing sole cache. Prune remains
pending: importing a newly available deletion script is not permission to reroute
blocked cleanup. Read-only scan still measures Cargo 13.49 logical GiB, mobile
build 0.971 GiB; these are not SSD writes or physical recovered-space claims.

Also read actual appearance.ts, appearance.test.mjs, App.tsx and styles.css.
All five appearance functions and four actual donor tests are now adapted into
the shipped Web UI, with the actual palette catalog rather than another theme
engine. Complete generic App/styles mappings and native/Web/mobile preference
transport remain pending. See WEB-APPEARANCE.md for exact uses and browser proof.

Current authoritative comparison: 1,869 files, 42 complete source comparisons,
1,827 pending, 23 reviewed files with gaps. Pealayer: 494 / 20 compared / 474
pending / 10 gap files. Increased gap count exposes additional unfinished useful
behavior; it is not accepted as an exclusion or a whole-repository parity claim.
New storage/appearance/App/styles routes prevent these files remaining invisible
to specialist lookup; exhaustive donor-audit still reaches every inventory row.

Executed: live build-storage Gate/RequireLive (11 paths, zero stale, four pending,
five mapped gap files visible); donor routing/hash/adversarial tests; actual
read-only summary and content-equality tests; comparison integrity and stale-report
rejection; storage Plan/parser tests; ten appearance and existing access/display/
cells/remote/PU-route tests; screenshot source guards and source version 0.2.7.
Comparison Check also correctly rejected the report after its target hash was
changed to reflect readable inventory formatting; regeneration follows explicit
rereview, not a blind build retry. Firmware compile/deployment is separate proof.

Both previous ecf882d CI runs (38059703363 and 38059700392) passed Windows,
both firmware variants/protocol tests and Android preview. They do not certify
the new appearance/storage checkpoint. Current CI is tracked on the existing PR.
No native/Android/donor build, firmware flash, app replacement or cleanup was
performed for these imports; only the affected ESP compile was requested.

## Native menu regression: actual source comparison and guarded recovery

Read full Pealayer src/ui/menu.rs and src/ui/mod.rs at 3d5f7ae. Read egui
0.36.2 shortcut atoms/menu_style/Spacing dependency code; both apps share that
version. Actual top_menu_button helper imported with only state ID/visibility
changes. All six current Pakar accelerators now use the donor shortcut_text
idiom, not spaces in primary labels. Four root headings use active-menu hover
switching. Three native regression tests cover actual painted right edges/weak
colors, popup switching/closure/context isolation and production call sites.

Height diagnosis is documented separately: Pakar minimum 28 / vertical spacing
6 / font cap 13 versus Pealayer default minimum 18 / spacing 3 / font cap 12.
Popup padding is overridden by egui, not the explanation for that difference.
No documented requirement justified the divergence. Height/font/spacing/zoom
remain unchanged as the user requested; no claim of corrected menu heights.

Donor advanced during this pass to clean 8d9bc1d86988987354e373a049ef953f9cf3034d.
Menu/mod source hashes remain unchanged. Selectively refreshed the same
inventory to 496 Pealayer paths; its two new verification/doc paths are pending.
App.tsx changed since its earlier read and is correctly stale-review, not carried
forward as current certification. Current complete comparison count is 43 of
1871; 1828 pending and 24 reviewed parity gaps remain visible. Complete file
reading is not complete import acceptance: the new menu row is parity-gap,
with concrete generic omissions/next tests in NATIVE-MENU-COMPARISON.md.

Executed native_menu_test.mjs source guard: normalized imported helper body
matches donor exactly; four production root headings and six shortcut atoms;
unchanged height policy. Rustfmt parse/check and comparison Check passed.
Attempted the mandatory native Test wrapper: donor/build gate and crate guard
passed, then the canonical-cache budget guard refused compilation (>8 GiB).
No bypass, new cache, deletion reroute, donor build, or package copy attempted.
Native paint/pointer tests must execute on CI; source guards are not native
visual/runtime screenshots. The running app was not replaced in this pass.

## Cross-surface application branding regression

Cause: native build used Phosphor Circuitry; native toolbar/About separately
tinted a font glyph; Web hardcoded a diamond and omitted application favicon.
Read all of actual Pealayer src/branding.rs, build.rs, assets/icons/README.md,
HeaderBar.tsx and webPlatform.ts before adapting the resolver/image binding.
Imported icon_data_from_bytes unchanged and actual seven-frame ICO conversion
with its shell-derivative test adapted to Pakar's PNG-only decoder. One 740-byte
lossless PNG master preserves the verified existing native glyph pixels; native
PE/window/tray/header/About and Web header/login/favicon/touch consume it.
No per-surface recoloring, new drawing, donor build, compiler or icon disk cache.
Existing semantic Phosphor quick-action generator remains unchanged.

New generator first rejected adjacent empty bootstrap markers; fixed the order
guard to explicitly accept an empty block. Appearance stale check caught new
branding CSS placed inside its generated block; moved it outside that owned
block before regeneration. Existing generate-web-appearance.mjs was reread and
used as the no-write/marker-ownership reference. Both current-generation checks
and unchanged-timestamp branding tests now pass; no blind repeat/build bypass.

Live donor inventory then advanced to 498 tracked paths; the exhaustive guard
rejected drift. Refreshed all Pealayer paths without touching donor files.
Reread current src/ui/icons.rs through EOF: its only new change was the STOP
re-export, now imported; old picker functions/eleven tests are retained. New
unreviewed paths stay pending and unrelated changed files stay unreviewed/stale.
Current report: 1873 paths, 47 complete-read comparisons, 1826 pending, 28 gap
files. Branding/HeaderBar/WebPlatform/build rows are explicitly parity-gap,
not claims that custom runtime presets or all browser capabilities are imported.

Executed: actual decoder-body identity guard, exact PNG/native pixel hashes,
four identical embedded Web assets, native production consumer/source checks,
no-write generation, native menu source guard, ten appearance tests, Web access,
Rustfmt parse/check, offline locked dependency metadata, comparison Check and
reuse gate/adversarial tests. Reused already locked base64/image versions as
build dependencies; no new dependency set/download/cache was created.
New native pixel/ICO/texture tests run asynchronously on CI per user instruction;
no blocking wait, running-app replacement or board flash claimed for this patch.

## Tint-free unified cell selection and two-way Find cell

Consulted ui-shell mappings and exhaustive exact donor lookup. An assumed
src/ui/selection.rs path had no inventory row; stopped that lookup, searched
actual files and consulted src/ui/controls.rs instead. Its neutral/fixed-geometry
interaction and selection-before-paint slices were read, not falsely promoted
to a whole-file review. Added controls.rs to ui-shell routing; its generic
remaining capabilities remain pending in issue #3. Existing actual imported
dialog/palette/icons are retained. Tinting transport intent is deliberately not
copied onto cells because the user explicitly requested no tint.

Native/Web selected and matched cells now share an amber outline and neutral
alternating cell surface; red LED state no longer colors the background. Two-way
query/selection binding covers clicks, toggles, ranges, row/all/clear, manager,
keyboard and marquee; invalid drafts remain editable, but repeated selection
gestures replace them. Native revision-based sync requests repaint; F3 navigation
does not collapse multiple selected matches. No selection writes LEDs.

Extended actual shipped Web DOM/HTTP-doubles tests pass, including same-cell
reselection after invalid input and exact range display. The old appearance
test correctly caught its obsolete matched-only selector; updated its expected
contract to unified tint-free emphasis and retained all four donor tests.
Reviewed focused changes at all existing shell/appearance/icon/branding evidence
anchors before rebinding hashes. Native tests added/extended for query
roundtrip, revision/draft handling, neutral LED state and shared painter output.

Native Test wrapper passed donor/crate gates then refused the canonical cache
above the existing 8 GiB budget. Consulted actual recorded build/storage recovery;
no guard bypass, cleanup reroute, new cache or direct compiler retry. Rust tests
and deployed screenshot acceptance remain pending; CI is not waited on.
