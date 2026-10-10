# Donor audit and release gate

Tracking: https://github.com/atomicdeploy/PakarController/issues/2

`legacy-source-ledger.csv` inventories canonical Pealayer/PCController, the
legacy ControllerBoardMini tree (excluding duplicate Puzzles and generated
build/cache directories), Puzzles, every PuzzleBoard variant including 40LED,
and PU850. Every tracked Git path is recorded even when absent in a sparse
checkout. Missing rows must be read from their exact Git blob or materialized
before review; they are not silently skipped. Non-Git donors use file hashes.

Historical path-only decisions in `reuse-decisions.csv` are routing hints.
They now produce `legacy-unverified`, not a current manual-read claim.
`reuse-review-evidence.csv` is the manual decision authority: a matching
SHA256 is mandatory, changed content becomes `stale-review`, and an entire
read can remain `reviewed-open` when integration or a scoped exclusion has
not been established. Compound historical paths need individual evidence.
Do not generate manual evidence from keyword screening.

The entire Puzzles sketch and all 14 LocalLib files have been read. Their
individual decisions are bound to hashes; remaining input/menu/scheduling
and hardware gaps remain open. The Puzzles folder has **no**
`LocalLib/SevenSegments.cpp`: its header uses an external library. A previous
path-only row naming that nonexistent file cannot serve as import proof.
The PU850 Telnet and Shell headers and implementations have also been read
in full, with the actual adapted parser/editor tested separately.

Run the inventory on a machine containing the donor trees:

```powershell
./tools/generate-reuse-ledger.ps1
./tools/check-reuse-ledger.ps1
./tools/check-reuse-ledger.ps1 -RequireComplete
```

Normal CI validates ledger integrity without pretending the unresolved
scope is complete. The strict command must pass before a production release;
it currently fails intentionally on unresolved rows. Physical acceptance is
an additional independent gate in PRODUCTION-ACCEPTANCE.md, not inferred
from any ledger status or CI success. Fresh donor revisions must regenerate
the ledger and revalidate manual hashes.
