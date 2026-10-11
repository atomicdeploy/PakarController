# Native lifecycle continuation

Actual source consulted: Pealayer `src/app.rs`, clean snapshot
`3265bf80b6f7543831e096a05b04c3b58b770584`, original-byte SHA-256
`d32e15fa4dc42581f8b44b5690950b086fe3ed08cb030e2aaf93b2587edf116e`.
The `App::logic` command/result/snapshot work and `process_shell_commands`
were consulted. **This is a function-slice adaptation, not a complete-file
comparison.** The inventory row remains pending and is now specialist-routed
to native-lifecycle/issue8, not silently excluded as application business.

| Donor mechanism | Actual Pakar consumer |
|---|---|
| eframe `App::logic` independent of painting | main.rs `App::logic` calls native_logic; shell.rs drains typed IPC/tray, polls device results, updates contextual authority and status outside draw_shell_contents |
| hidden-window restore commands | Existing dispatch retains Visible/Minimized/Focus for Show and Preferences; Screenshot never changes visibility or focus |
| authoritative snapshot outside painting | Existing mutex snapshot now updated in native_logic, including offline capability state and capture progress |
| one command application path | CLI, IPC and tray retain the same typed dispatch; UI no longer drains them a second time |

Actual locked egui 0.36.2 `Context::run_logic` was read: it updates viewport
window state without a UI pass and returns requested viewport commands. It
is used by the new screenshot deadline regression with a minimized root.
Framebuffer capture still requests pixels only after a rendered frame.
Its deadline/repaint lease runs without painting and never reveals a hidden
window. Status includes request_id/path so the wrapper correlates completion
with the requested file rather than accepting an old idle snapshot.
The wrapper still validates process ownership and refuses overwrite.

Live evidence for the prior deployed `0c3eca9` process54616:
Show/Preferences/capture requests were accepted but remained queued while
minimized. Supported Windows restoration let the queue drain; an actual
2039x1799 framebuffer at outputs/PakarController/desktop-native-dropdowns.png
was inspected. It shows working Phosphor transport carets, truthful offline
capability gating, repeated connection errors and a Windows-shell error.
Those last two defects remain pending, not successful UI acceptance.
A second Preferences capture timed out; no PNG was fabricated or saved.
Windows Computer Use was stopped by physical Escape; no subsequent desktop
input was performed. Preferences remained open, so replacement must preserve
its draft and await a safe close, not force-kill the owner.

User subsequently confirmed Preferences closed; read-only owner enumeration
found zero Pakar processes, with the installed executable hash unchanged.
No force-kill or speculative restart was used. New CLI `--minimize` provides
a typed product action, with real observed minimized/preferences_dirty status.
`tools/test-native-lifecycle.ps1` validates one explicit PID/executable, requires
closed drafts, minimizes via product IPC, checks a capture request is applied
while hidden and times out without revealing it, restores via the same Show
dispatcher and takes a correlated visible framebuffer. Definitions are not
execution proof; record actual results separately.

First deployed48e0165 live smoke failed on a96x96 minimized framebuffer;
that inspected5474-byte failure is preserved as desktop-minimized-invalid.png.
No usable capture or full live pass was claimed. Explicit visibility guards now
cover request and receipt, plus320x240 minimum root-frame dimensions consistent
with the imported Win32 target filter. Locked egui visible()/egui-winit sampling
and eframe Glow/epi control paths were read to investigate the transition.
The new actual native regression injects that stale minimized framebuffer even
when an integration paints; it must neither save nor request more pixels.
Native130/0/0 pass; low-level2x2 PNG encoder tests remain valid and unchanged.
Close-to-tray cancellation also runs outside painting. The full source quality
phase passed after correcting its relocated capability assertion and using the
existing screened/noncertified ledger status, not relaxing completion checks.
The1dbfd0b release installedPID15492 and passed minimized command application,
correlated no-file hidden deadline and Show observation. Visible capture failed:
read-only Win32 confirmed79x101 visible/non-iconic geometry. Fullscreen via the
product dispatcher recovered an inspected3840x2160 framebuffer, not a complete
normal-window pass. Existing invalid96x96 failure evidence remains preserved.

Read actual Pealayer App::logic non-fullscreen/non-minimized valid geometry
observation and config.rs WindowGeometry::is_valid, plus locked winit restore,
egui-winit viewport commands/window settings and eframe builder restoration.
New window_geometry.rs adapts its finite/bounded valid-size sampling, keeps the
last normal size in memory only, and requests size recovery once when a visible
restored window is below320x240. No visibility/focus or per-frame disk writes;
real valid user sizes remain untouched. Two actual native tests cover hidden/
fullscreen sampling, retained size, storm prevention and run_logic commands.
Execution/new release/live acceptance for this geometry repair remains pending.
Corrected guarded Test executed132 cases: all pass, zero failed/ignored
(2m47 compile,0.26s execution). New geometry tests both ran. Release and fresh
normal-window live smoke remain separate pending gates.
The first Test compile rejected the new assertion's assumed command tuple shape.
Read locked egui LogicOutput: OrderedViewportIdMap<Vec<ViewportCommand>>. Inspect
values().flatten() and retain resize/no-reveal assertions; not a gate bypass.
The protocol CI runners now include the small S3 pin/menu-audio contract fixture.
All prior runner commands and hardware/donor assertions remain unchanged. Rebind
only their reviewed proof hashes; the PCController controls snapshot is explicitly
live-unavailable in this lookup, not a fresh donor read or physical acceptance.

Remaining generic donor imports: complete app.rs/platform ownership review,
config-revision/idle repaint scheduling, shell initialization/recovery,
native dialog-host capture, correlated application-vs-queue acknowledgements,
and hidden-window end-to-end IPC acceptance. Media engine, playback controls,
timeline/cue business are outside Pakar's mpv-free controller scope; generic
effect timing, reconnect policy and state authority are not excluded with them.
