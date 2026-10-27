# Portable Shell source integration

Exact donor: https://github.com/atomicdeploy/portable-shell/tree/67c5dcb8a63f4055cbdaf02e9ff271c53b46410d
Requested branch: `copilot/organize-readme-into-plan`. License: MIT, retained in
`upstream/LICENSE` and in every original source notice.

`upstream/` is an unmodified source/reference snapshot, **not a second firmware
shell or compiled dependency**. Original README/implementation claims describe
the donor and are not Pakar acceptance claims. The one-byte CodeQL environment
marker is a Git symlink to `.` (mode 120000), inventoried but not installed: it
contains no program or reusable tool and would introduce recursive traversal.
Every file's original SHA-256, exact mapping, omissions and remaining acceptance
are in the existing `docs/legacy-source-ledger.csv` and
`docs/file-comparison-evidence.json`, tracked by issue #29.

## Live integration

`firmware/esp/src/PortableShellCore.h` adapts PSCommand's name/help descriptors,
strcmp command lookup and VT100 callback emitters. The catalogue now owns command
recognition, aliases, maintenance admission, help and completion. It uses immutable
descriptors rather than heap-linked registration; dynamic command removal is not
exposed on a production controller. `edit` opens the existing real Pakar TUI.

`TelnetShell.h` uses the adapted VT100 output helpers and adds bounded command and
property completion. Tab extends a unique/common prefix; repeated ambiguous Tab
lists matches. Completion never executes, remembers a draft, exposes a password,
or replaces text before/after a cursor in the middle of a command. `set` completion
only lists writable property names, never secret values.

`ShellService.h` retains the authenticated, loop-owned command handlers. Typed
properties keep validated durable settings writes rather than donor raw-pointer
mutation. There is still only one device shell, reached via existing Telnet or
owned binary USB shell envelopes (`tools/device-shell.ps1`); no raw serial escape,
new unauthenticated UDP listener, blocking network loop or second API is added.

## Source comparison findings

- Donor serial functions are stubs; the telnet example neither binds nor accepts.
  Its UDP socket never records/connects a destination for replies. Pakar's actual
  USB transport, Telnet negotiation, multi-session leases and output ACK windows
  remain authoritative instead of copying nonfunctional transports.
- Donor escape parsing synchronously reads subsequent bytes; Pakar handles
  fragmented CSI/SS3/NVT input without blocking LED/UART work.
- Donor edit is a print-and-wait loop, not an editable TUI. Pakar retains its
  staged property/table editor and explicit command admission.
- Donor argv parsing uses strtok and heap allocation; its setters use unchecked
  atoi/atof and raw pointers. Pakar retains bounded validators and protected owner
  APIs. Arbitrary runtime float-variable binding is not a board requirement.
- History remains eight fixed entries with password/credential filtering and
  disconnect scrubbing, not heap strdup of every command.
- Platform samples' direct GPIO writes and unbounded startup/stdio loops are not
  installed: they bypass board pin ownership and production scheduling. The
  examples are retained as original source, not described as working Pakar tools.
- Donor Makefile's clean/install/uninstall and debug profile do not override the
  existing low-write, cross-platform guarded build. Tests run in the established
  native cache via `tools/test-shell.ps1`, not a second donor build directory.

No claim of universal superiority or physical acceptance follows from this import.
Focused local fixtures cover the imported code; live USB/Telnet sessions and
production firmware deployment require their separate acceptance gate.
