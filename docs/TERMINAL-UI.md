# Interactive terminal checkpoint

Run `PakarController.exe --tui` in an interactive terminal. It uses the same
`PAKAR_ENDPOINT`, `PAKAR_USER` and private `PAKAR_PASSWORD` as the CLI; prefer
`http://ledboard.local`. Redirected stdin/stdout is rejected before raw mode,
escape sequences or network requests. Scripts should use `--status` or
`--command` instead. Normal/error exits restore the cursor, screen and raw mode.

## Navigation and explicit operations

- Tab / Shift+Tab: Overview, ACK-gated Table, Events, Commands, Help.
- Arrows / Home / End: logical row-major table cursor. Space toggles selection,
  Shift extends a range, Ctrl+A selects all. Selection alone never writes.
- `/`: edit Find with the native shared parser; Enter commits, Escape cancels.
  Persian/Arabic digits and ranges normalize to the same selected IDs. Find is
  not a second highlight set; `1,2,3` displays as `1–3`.
- `w`, `r`, `o`: explicit white, red or off for the selected cells with fade.
  No selection means no write. Key-repeat/busy actions cannot enqueue duplicates.
- `:`: shared CLI command palette; Enter submits, Escape cancels. Examples:
  `device name`, `device rename ledboard`, `mode snake`, `display segments -- Hello`.
  Fresh capability/mode/latch checks precede dispatch. Existing CLI parsing,
  private credentials, update confirmation and readback contracts remain shared.
- Up/Down/PageUp/PageDown scroll captured command output on Commands.
- `g`: read-only refresh; `d`: Events; `?`: Help; `q` / Ctrl+C: quit.

Peer features must be individually ACKed. No offline game/table/display/buzzer
controls or fabricated default cell state. A changed board/boot/hostname/capability
clears selection, Find and command drafts. Failures retain distinct causes/sources, coalesce only
repeats and use bounded backoff. Terminal output strips peer escape/control/bidi
sequences while retaining Unicode text. `NO_COLOR`, `TERM=dumb`, `CLICOLOR=0`
and `FORCE_COLOR=0` veto ANSI colors; cursor/screen control still needs a terminal.

Bulk cell changes are sequential, **not atomic**. Each cell gets a fresh peer /
table / latch preflight and one boot-pinned POST, requiring `accepted:true`. The first error,
unknown receipt or changed peer stops the batch without retrying. Accepted count
is not physical LED or persistent-state proof. Quit stops unsent work but cannot
undo an in-flight write; inspect its outcome before repeating. Generic CLI
multi-step operations retain their own transaction logic, not per-step cancellation.

## Actual donor mapping and remaining acceptance

`desktop/src/terminal_ui.rs` adapts RayanLamp `internal/tui/run.go` terminal
ownership/redirect guard, `text.go` safe Unicode terminal text, `keys.go` contextual
navigation, `runtime.go` independent model/network scheduling, and
`internal/terminal/policy.go` color opt-outs. PCController
`Tools/Controller/internal/tui/table.go` supplies the actual `tableWindow`
cursor-centred viewport formula. Go Bubble Tea/gocui-specific types are replaced
by crossterm and the existing Rust client; not a second transport or selection
engine. Exact source hashes, consumers, tests and concrete omissions are recorded
in `file-comparison-evidence.json`, not inferred from filename inventory.

This is bounded progress, not full issue #2 acceptance. Still required: terminal
hardware/manual navigation and resize acceptance, richer device-setting forms,
mapping/reorder and arbitrary effects editors, graphical half-cell staggering /
six-pixel diagnostic parity, mouse interaction, complete localized help/theme
controls and USB/discovery transport choice. ESP HTTP board/boot fencing now
exists (see HTTP-PEER-FENCING.md); AVR-only reset and other transports/clients
still need their own session identity and ownership acceptance.
Full desktop/Web/mobile parity and physical tests remain open. The donor macOS
diagnostic sampler and Linux/macOS native CI need real Pakar platform coverage;
no libmpv/media jobs are copied into this media-free client.

Native model/HTTP tests exercise shared selection, sanitization, no-ACK/offline
controls, peer-change stop, false acceptance and post-preflight cancellation.
`tests/terminal_ui_test.mjs` exercises the actual built executable redirected
guard with a loopback server and proves zero requests, not real terminal input.
