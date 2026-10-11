# File-by-file donor comparison and omission decisions

Subtask of production acceptance #2. **Whole-repository superiority is not
established.** A complete hash inventory is not a complete code comparison.

The current comparative report accounts for 1,868 files. There are 26 complete
hash-bound comparisons: four byte-identical imports, two behavior-equivalent
files, four scoped improvements and sixteen files with known parity gaps.
Header/implementation pairs can describe the same missing behavior. All other
1,842 paths are pending; none is silently treated as an approved exclusion.

Evidence: `docs/FILE-COMPARISON.md`, `docs/file-comparison.csv`,
`docs/file-comparison-evidence.json`, `docs/FILE-COMPARISON-RECEIPTS.md`.

- [x] Inventory all six donor roots, including 63 absent sparse PU850 files.
- [x] Bind reviewed source bytes and target/proof content to hashes and exact line anchors.
- [x] Record exact omitted/replaced code and a concrete reason per reviewed file.
- [x] Restore Pealayer's actual eleven icon/picker tests; 42 native tests pass.
- [x] Check byte equality of MessageHub, theme catalog and both shift-register files; retain all 90 Puzzles pitch/REST definitions.
- [x] Execute ten C++ suites and Web/boot/version/audit checks locally.
- [x] Add CI checks rejecting stale/unbound comparison evidence and a strict completion gate.
- [ ] Compare the remaining 1,842 inventoried paths; obtain/read all sparse source blobs.
- [ ] Complete reviewed TM1637 recovery: import current PCController ACK sampling, detected state, cached-frame reconnect and periodic keepalive onto A4/A5; wire loop/telemetry; test actual reconnect.
- [ ] Resolve Puzzles 50 ms versus Pakar 25 ms debounce, physical bit order and reusable key callback/state contract with tests and measurements.
- [ ] Implement a bounded generic task scheduler preserving add/cancel behavior, fixing sentinel/rollover pitfalls.
- [ ] Share Pealayer's actual appearance/parameter contract across native/Web/mobile; port donor contract tests rather than dropping them.
- [ ] Make WS2811 chipset/channel order configurable; test separate R/G/B and all 480 pixels.
- [ ] Reconcile optional encoder versus RC pin ownership; do not silently omit useful encoder support.
- [ ] Import useful PU850 shell parameter registry, readonly handling, prompt-safe events and utilities; replace unsafe numeric parsing with a strict tested implementation.
- [ ] Configure Telnet enable/port/network/discovery lifecycle; provide an explicit framed/negotiated serial shell without corrupting COBS.
- [ ] Complete file-level Pealayer dialogs/panels/actions/events/preferences/tray/single-instance/UI asset mappings with invocation, tests and real screenshots; do not infer application parity from imported helpers.
- [ ] Complete individual PuzzleBoard/40LED/ControllerBoardMini variants, scripts/dependencies/CI and remaining canonical PCController operations; justify exclusions with exact source scope.
- [ ] Pass comparison completion gate, all-surface/runtime/physical acceptance and final CI before making a superiority/production-release claim.

No board flash or live hardware acceptance was performed for this audit.
The master issue and draft PR remain open. Future comparisons must continue from
this manifest and rereview stale targets/sources instead of blanket-promoting
the older keyword-screening ledger.
