# Interactive terminal checkpoint

## Board-owned Telnet / encapsulated serial shell

The device shell is distinct from the host TUI described below. Connect with
`node tools/telnet-shell.mjs --host keycontroller.local` (or the configured board
hostname), then enter `tui`. It reuses the existing authenticated, loop-owned
command router; navigation alone never changes an output. Telnet is plaintext:
enable it only on an appropriately restricted network.

The board negotiates ECHO, suppress-go-ahead, BINARY, terminal type and window
size. The bundled client reports resizes and escapes IAC in NAWS values. Unknown
options are refused, fragmented messages are bounded, and malformed terminal
names cannot inject control sequences. These implement the NAWS and TTYPE
contracts in RFC1073 and RFC1091, not every optional Telnet extension.

`terminal` shows negotiated type/size and rendering preferences.
`terminal unicode off` selects ASCII borders; `terminal color off` disables TUI
ANSI presentation. `terminal size 80 24` supplies dimensions when the client does
not negotiate them. Terminal settings are per-session, not flash writes. The
Terminal page edits the same values. TUI needs at least 40 columns and 24 rows;
resizing below this pauses editing/actions while retaining the draft. Escape
returns to the shell. Unicode currently covers the rendered frame, not general
Unicode text entry: editable property values remain ASCII; full UTF-8 editing,
grapheme-width layout, RTL and CHARSET negotiation are still pending.

The colored, scrollable property table exposes Device, Network, capability-gated
LEDs/Pixels, role-gated Game, and Terminal pages. Tab changes pages; arrows select
rows; Enter edits/commits; Escape cancels a draft. Hidden passwords are never
rendered or retained in command history. A selected pixel/group follows the
board's actual group spans, including twelve-pixel keycontroller groups, rather
than assuming six pixels.

First-wins builds add an enabled/dimmed nine-box view, live winner/readiness,
explicit `1` through `9` press and `r` reset, plus editable enabled mask, effects,
auto-reset, chase period/neighbors/inversion, state colors and cue names. These
use the existing game-settings persistence and game command admission, including
AVR readiness and maintenance checks. Equivalent commands:

```text
game status
game press 1
game reset
get game.enabled_mask
set game.enabled_mask 3
set game.effects true
set game.chase_ms 900
set game.auto_reset_ms 0
set game.win_rgb 65280
```

These examples are output-changing when submitted; opening this document/TUI
does not run them. Mask 3 means boxes 1 and 2; colors are decimal packed RGB,
and auto-reset 0 disables the timer. Validators remain authoritative. Table
builds do not advertise first-wins commands; key builds omit table commands from
help/completion. Existing generic network, telemetry, audio, display, firmware,
get/set and watch commands remain available according to capabilities/ownership.

Host fixtures cover negotiation, resize/draft retention, malicious terminal
input, role metadata, guarded actions and two sessions. Candidate compilation
and these fixtures do not establish installed-board, interactive-terminal or
physical LED/audio acceptance. Full game-presentation override editing,
localized Unicode input and graphical-client parity remain open in issue #29.

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
