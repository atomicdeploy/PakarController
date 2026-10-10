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
