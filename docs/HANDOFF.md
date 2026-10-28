# Source handoff

The consolidated source includes the ESP32-S3/shared ESP application migration,
AVR maintenance and radio work, runtime LED backends, pixel corrections/custom
groups, semantic state/effects, Rust and Flutter controls, and the single canonical
Web application. Existing donor and production acceptance ledgers remain open;
publishing source is not a new hardware deployment or complete feature acceptance.

## Verified GitHub integration

### Completed main consolidation (2026-10-28)

PR27 merged normally as `e47b230d1791d3bf4186f3b48d5690436b5b4f98`;
PR34 merged normally as `da12fde72257d98d7952230b9474760356629b1e`.
The latter has the identical source tree of complete-stack head
`ccc061d1b79907c00547a0ea963fc1ab5c6b4a5c`, which passed every build/package/
contract/resource job in [Build40753870815](https://github.com/atomicdeploy/PakarController/actions/runs/40753870815)
and [CodeQL40753922292](https://github.com/atomicdeploy/PakarController/actions/runs/40753922292),
including the standalone finding check. Parent8102f04 independently passed
[Build40753861517](https://github.com/atomicdeploy/PakarController/actions/runs/40753861517)
and [CodeQL40753861444](https://github.com/atomicdeploy/PakarController/actions/runs/40753861444).
Draft publishing was skipped; no release or tag was created.

All paginated conversations/reviews/threads, including outdated findings, and
both complete PR diffs were reviewed. All four CodeQL threads are resolved;
no completed Copilot review is claimed. Original preservation/source heads are
verified main ancestors. Root main is fast-forwarded; only its verified-merged
feature branch and obsolete remote-tracking refs were removed. The other clean,
fully merged PortableShell worktree remains owned by its original chat: preserve
it rather than archiving/deleting it. No stash hides source or drafts.

Windows495 tests/package, Android analysis/tests/APK, both S3 roles, AVR link
profiles, optional transfers, Web/ledger/tooling and absolute resource budgets
pass. No retained resource baseline existed, so a historical growth comparison
is not claimed. The subsequent handoff/audit commit changes metadata only;
post-merge main CI is separate from the verified application-code runs above.

Issue2's three GitHub/source/merge boxes and issue29's integration-merge box are
checked with evidence; all hardware and product acceptance remains open. The
host/palette filename collision, AVR role compile guard, flaky RX fixture and
review/contract drift are repaired. Issue17 records the remaining Linux tray
dependency security investigation, not a claimed fix. The Arduino setup action's
Node20 declaration warning under forced Node24 is nonblocking tooling follow-up.
No board, installed client, saved scene/rules, host network or credentials changed.
Private settings, backups and build outputs are preserved locally, not published.

### Consolidation review and repair history (PR #27)

The consolidation proceeded through PR #27, rather than the historical
PR21-24 checkpoint below. Its published reconciliation commits are
`378016eeb91dffea64b38d29d7a64bfb18270668` and
`89b4a6f8ae36cdf7c9e27ba181931aa7fa831e8c`. All previously uncommitted native
taskbar/notification and Flutter signal-display work is included, together with
the reviewed provenance records. A separate active PortableShell integration
worktree owns stacked PR #34; preserve it and merge in dependency order. No stash
is used to hide work.

Local verification: all495 native tests, Flutter analyzer and869 Flutter tests
(three explicitly skipped), full `node tools/ci-checks.mjs quality`, source-version agreement and the
generated comparison report pass. The signal menu's discovered overflow is
fixed. Four CodeQL findings were fixed without suppressions: descriptor-owned
documentation reads and forgiving trusted-script syntax extraction. Current
PR-head security checks pass; SDK/Windows/Android CI is recorded separately on
the PR. Do not infer a green final build from this local-test receipt.

The subsequent minigame configuration fix acquires the common LED flash-write
guard before saving; refusal preserves live state. CI's Ethernet fixture now
declares/tests the shared artifact-reader lease and runs in the earlier quality
phase too. Core/integration, enabled/disabled Ethernet fixtures and guarded S3
build Plan pass. Final firmware linking remains a CI gate, not a Plan claim.

Further complete-store review reproduced a shared-filename data-loss risk:
host presentations and normal appearance both used `/presentation.json`.
Host layers now use `/host-presentation.json` and their own staging file. Actual
combined-store tests verify independent save/reboot and unchanged scene,
corrections and palette staging bytes. No automatic migration or device write.
The next CI failure was a Linux 10ms RX-fixture preflight race, not a production
retry: deterministic transport-loss fixtures retain exact one-START/one-STOP
checks and verify the failure phase. Current-head CI must be refreshed after
these changes; previous-head results do not attest the new firmware.

CI40750230610 then built both S3 key configurations, Windows (495 tests and
package) and Android successfully, but caught a table-role AVR callback using
the first-wins-only espGameOwned symbol. The callback now guards that branch
with the same role flag as its declaration. An actual-source compile/dispatch
fixture reproduces the old failure and passes for both roles after the fix.
All three table AVR link profiles and RF-enabled keycontroller now build locally,
with boot-chain verification. Final-head CI remains required after this repair.

Merge only after every required check succeeds and the complete diff and every
conversation/review-thread page are reviewed. The final synchronization receipt
is on PR27/issue2; inspect GitHub's current main SHA before pulling. No release,
tag, installed app replacement, firmware flash/reset or production output action
is part of this source handoff. Private backups/settings/caches remain local.

### Subsequent dependency checkpoint (PR35)

Dependabot opened PR35 after both consolidation merges. The reviewed update
changes only rustls0.23.45 to0.23.46 and its verified crates.io checksum;
incidental tempfile/getrandom resolver churn is removed. The stale Cargo.lock
donor target binding is refreshed after rereading the actual PuzzleBoard lock
and checking the bounded dependency diff. Final-head CI and normal merge remain
required; the preceding successful runs do not certify this new dependency.
No production deployment is included.

### Remaining issues, not forgotten work

Use GitHub's open-issue list as authority, not historical counts in old receipts.
Issue2 is the production umbrella; source/fixture/build evidence never closes
its physical gates. Current work is grouped as follows:

| Scope | Tracking and next acceptance |
|---|---|
| Controls/corrections/host presentation | #26: installed-client and acoustic latency acceptance; native correction USB parity remains unfinished. |
| Keycontroller games | #32: all ten source modes are included by default, but each mode still needs David's LED/audio/display/output feedback on the approved running build. |
| Shell and file transfer | #28/#29/#33: managed public files, shared shell and opt-in SSH/SCP/SFTP/FTP exist; encrypted ESP sessions, host-key persistence, memory/TLS/LED coexistence and optional storage hardware are not accepted from host fixtures. Services remain excluded/default-disabled unless explicitly opted in. |
| Network/peers/cloud | #7/#10/#31: clock/async scan/diagnostics and verified-IP relay recovery exist; automatic peer discovery, continuous subscriptions and fully selectable bridged devices remain incomplete. Actual LAN/Internet/OS join tests are separate. |
| Hidden Snake | #30: external host app and hidden launchers exist; board engine remains opt-in/excluded from current builds and keycontroller. Preserve the normal scene on exit; actual cross-client sound/control acceptance remains open. |
| Optional hardware | #19/#20: W5500/TFT/BLE coexistence and exact electrical/module/runtime acceptance remain open. Do not infer another board's ISP wiring from ledcontroller's verified build profile. |
| Native/mobile/Web polish | #4/#5/#8: current source is not full visual, Explorer, background/reconnect or hardware acceptance. Keep one canonical Web source/entrypoint and explicit size-only build flags. |
| Provenance/build/repository | #3/#9/#11/#16: 434 hash-bound comparisons,4302 pending and310 known gaps. Continue actual donor reads; no blanket parity claim. CI/package/API policy evidence stays separate from deployment. |
| Dependency security | #17: glib0.18.5 remains in the default Linux AppIndicator dependency chain. Upstream tray-icon0.26.1 now has a GTK-free KSNI/menu-snapshot backend worth evaluating with actual Linux compilation and runtime coverage; no unsafe forced glib upgrade or tray removal was made. |
| Preserved legacy | #25: ESP8266 source is retained but its full image still exceeds physical budgets; S3+AVR remain the release targets. |

ASUS access was deferred by David pending his next instructions. Do not repeat
tunnel prompts, switch host networks or contact/actuate production boards merely
to finish this Git handoff. Production approval rules in AGENTS.md still apply.

### Earlier main integration (historical)

PRs #21–#24 are merged. Normal merge
`f4259db186464aae8d2aeb080e3368acf084ad0c` has the exact source tree of
`c1d2dd17d479dc4ab259d8785b995ddd7db5de41`, which passed every required build,
package, contract and security check in [Build 40508742014](https://github.com/atomicdeploy/PakarController/actions/runs/40508742014)
and [CodeQL 40508742015](https://github.com/atomicdeploy/PakarController/actions/runs/40508742015).
Draft publication was skipped; no release or tag was created. The original
preservation checkpoint, dependency heads and temporary AVR repair are verified
ancestors of main. Only verified-merged branches were removed; their commits
remain recoverable from main history. No stash or second worktree held source.

The follow-up handoff commit changes documentation/audit metadata only; its
post-merge CI is separate from the verified code-bearing checkpoint above.
Production issue #2 and donor gaps remain open. No board was flashed/reset or
actuated, and no installed desktop/Android app was replaced for this Git handoff.

Dependency reconciliation retains mdns-sd 0.21.6 and archive 4/MQTT/XML 7. The
eframe/egui 0.37 migration is explicitly deferred because current dock/icon/theme
dependencies still require egui 0.36 types; no version-specific adapter was added.
Original PR #22 history is preserved, not a claim that 0.37 is enabled.

## Release targets

The owner selected **ESP32-S3 + ATmega328P** as the release targets. The primary
workflow builds S3 buffered UART plus all three AVR link profiles, packages the
matched S3/AVR buffered pair, and retains native/Android and security checks.

ESP8266 source, pins, direct compiler commands, optional compression/reporting
and explicitly requested legacy packaging are preserved, not silently removed.
Its full current polling build does not fit: the measured `.bss` end is
`0x3fffdd20`, beyond the `0x3fffc000` DRAM boundary by 7,456 bytes; `.irom0.text`
is `0x1054c4` bytes, beyond its instruction mapping. This remains an unfinished
support/resource optimization item, not a successful build or hardware release.
No physical memory, OTA-slot or Urboot limits were enlarged to hide a failure.
The unfinished legacy checklist is [issue #25](https://github.com/atomicdeploy/PakarController/issues/25).

## Continue on another computer

Pull the repository's `main` branch only after the synchronization receipt confirms
its final commit. Use the canonical build wrappers and existing dependency pins;
do not copy another machine's build caches, settings or executable into source.
Review `AGENTS.md`, `docs/REUSE-PREFLIGHT.md`, `docs/WEB-PARITY.json` and issue 2
before continuing. PR27 wires the correction-preview leases into the real
clients; this is no longer an unwired helper. See #26 for transport and physical
acceptance limits, and preserve the board's existing saved correction addresses.

## Local-only data deliberately not published

`outputs/`, `build/`, `.build/`, native/Flutter caches, installed apps and device
settings are not source. They can contain private Wi-Fi profiles, firmware/EEPROM
backups and host-specific receipts. They are preserved on the original machine,
not deleted or treated as remotely available. Provision secrets privately on the
next machine. Production firmware/settings backups must remain local unless a
separate approved private transfer is arranged.

Historical one-off `outputs/PakarController/staging/keycontroller4` UART/ISP and
four-box scripts are preserved locally rather than promoted to runnable tooling:
they reference obsolete host ports, relative staging dependencies or fixed board
profiles. Continue with canonical `tools/capture-avr-rx.ps1`, `tools/serial-wire.ps1`,
`tools/invoke-usb-api.ps1`, `tools/update-firmware.mjs` and the firmware build
profiles instead. Generated cache sketches are not independent authored source.

No firmware update, reset, LED actuation or installed-client replacement is
authorized merely by pulling this checkpoint. The production hold remains.
