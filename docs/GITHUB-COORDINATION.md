# Conversation-aware integration

Adapted from the fully read Pealayer `docs/REVIEW-COMMENT-AUDIT.md`, original
SHA-256 `7a91d9fa8778383ee37e33d861ceebc5ab210e46bedab6af8be82d56c27523cc`.
The procedure is now executable in `tools/audit-github-conversations.mjs`.

Before merging, read the entire PR diff, issue comments, review summaries and
paginated review threads, including outdated and unresolved threads. A merged or
closed PR does not imply that its findings were resolved. Quota notices are not
findings, and requesting Copilot is not proof that Copilot performed a review.
Reply with specific implementation/test evidence; resolve only findings actually
fixed. Do not bypass required protections or weaken checks to obtain a merge.

Run the read-only auditor against all relevant repositories, or only Pakar for
routine PR work. It follows every issue/PR, comment, review and nested thread page,
rejects incomplete responses, and keeps one stable ignored snapshot under
`.build/github-audit/`. Identical data is not rewritten. The committed
`github-conversation-inventory.json` records content hashes and pending review,
not a claim that collected bodies were read or imported. `--cached` reprojects
the local snapshot; it is not a live refresh.

The existing source-file ledger remains authoritative for actual code imports.
Conversation decisions are a separate, hash-bound requirements audit, not another
code-completion ledger. Never promote all pending source rows after fetching a
repository, or reinterpret useful generic behavior as excluded business logic.

## Current integration and discovered risks

- Pakar PR #14 merged as `e9125c766d105b784b1d4a44adcaa5cdde5a44e7`.
  Complete diff/conversations read; zero review threads and prior exact-head CI
  passed. The new icon font dependency does not replace the shared launcher icon.
- Pakar PR #15 merged as `78b6128`. Current main was merged normally into its
  branch; original donor hashes were retained, and exactly two changed workflow
  target hashes rebound after rereading Pealayer CI/CD and CodeQL plus both Pakar
  workflows. Focused governance/ledger acceptance passed. Current-head CI remained
  asynchronous: this is not an all-green claim. No admin override was used.
- Copilot review requested on both PRs; no completed Copilot review was observed.
  All remote Pakar branches except main are gone; the merged local branch and
  obsolete remote-tracking names were removed with normal verified Git operations.
- Pealayer PR #55 retains two unresolved findings: authentication/origin safety,
  and correlated command errors/persistence completion. Pakar already checks
  origin on authenticated HTTP/WS paths and preserves settings drafts through
  accepted-write/readback mismatch. These source checks are not a full DNS-rebinding
  audit or physical durable-write proof. Do not import permissive donor CORS.
- PCController PR #212 retains two CodeQL origin findings for service workers.
  Pakar has no service worker today; future PWA imports must validate message
  origin and sending WindowClient before cache/lifecycle mutation. Do not mark
  those donor threads resolved from this repository.
- PCController #552 distinguishes ACK from authoritative output state. Preserve
  this distinction in every Pakar actuator surface; live output/readback parity
  remains pending, not inferred from mock HTTP acceptance.
- PCController #595 distinguishes segment authority from complete front-panel
  authority. Existing individual peripheral ACK gates remain; generic event
  reduction, sequence/gap recovery and multi-client physical acceptance are still
  useful imports, not excluded because full snapshots currently work.
- PCController #291 requires scroll/cancel-safe touch activation; retain this
  open acceptance for future instant/hold controls, including mobile. Current
  click controls are not proof of the complete shared gesture primitive.
- PCController #357 requires reachable/current/rollback-aware storage retention.
  Pakar's protected single release cache and verified debug-only pruning do not
  constitute the full artifact-store CLI/TUI/Web/automatic housekeeping contract.
- Pealayer #27 requires contextual identity and OS-discovered ports, never runtime
  examples. Pealayer #28's shaping discussion separates font, bidi, translation
  and pixels. Rayan BidiLtr and Persian-calendar date functions now serve actual
  Pakar Web protocol/NTP consumers; VM acceptance is not browser/Android/native
  screenshot acceptance.

## Snapshot coverage and unfinished review

The first complete enumeration fetched 792 records: Pakar 11 issues/5 PRs,
Pealayer 31 issues/111 PRs, PCController 226 issues/408 PRs. Standalone comments:
120/623/2534; reviews: 7/40/242; threads: 6/36/106. Unresolved thread counts:
0/2/2. Full bodies are in the stable local snapshot; fetching is not semantic
review of all 792 records. The decisions file identifies the exact reviewed
records/comment scope. Remaining records stay pending.

RayanLamp's verified source is a local filesystem donor without Git metadata.
The referenced Build RayanLamp Go app chat was read. Its last white-border and
settings-geometry work was interrupted, not accepted. No GitHub repository has
been verified; the user was asked for its URL. Never substitute rayanpbx or invent
a repository. Local Rayan source importing continues independently.

Source/CI/package/live proof stay separate. No installed desktop/Android
replacement, board flash, donor-repository edits or physical acceptance occurred.
