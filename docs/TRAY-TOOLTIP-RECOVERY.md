# GUID-bound native tray tooltip

Actual local tray-icon0.26.1 source registers the icon with a GUID, but
set_tooltip builds NIM_MODIFY with only NIF_TIP/uID and omits apply_guid.
Microsoft requires the same identity for later icon operations. This explains
the observed native startup E_FAIL; it is not a reason to remove tray integration
or suppress distinct errors. No dependency upgrade or registry-source edit.

Adapt actual Pealayer windows.rs::update_system_tray_icon (2483..2513)
NIM_MODIFY/NOTIFYICONDATAW/NIF_TIP on the existing owned tray HWND. Use its
original shared GUID and existing bounded UTF16 helper. Keep tray-icon owning
creation, menus, icon changes, TaskbarCreated recreation and disposal. No second
tray icon or guessed crate-internal numeric ID. The large windows.rs file remains
pending COMPLETE-file comparison: the exact read sections are 2364..2421 and
2483..2513, not every unrelated platform function.

Full reads of the donor taskbar/tray plan and WINDOWS-SHELL-RECOVERY document
guide lifecycle/acceptance, not execute their historical commands or certify
their media/timeline tests. Generic Shell readiness, action/receipt distinctions,
graceful replacement and actual visible acceptance remain applicable. Media
playback percentages, chapters, mpv and video thumbnail business do not apply.
Root taskbar retry, complete shortcut parity and visible shell action acceptance
remain useful pending requirements, not silently excluded.

TooltipSync caches only Shell-accepted presentation. Failure retries the latest
desired value after2,4,8,16,30seconds (then30), without a frame-rate write flood.
Existing EventLog coalesces same source/cause, retains distinct causes, and marks
recovery only after success. Nothing retries notices or device commands.
TaskbarCreated passes to the crate first; an observed epoch then invalidates the
cache so the latest tooltip is reapplied on the normal UI tick. That observed
message is not itself proof Explorer restored the icon. No forced Explorer kill
or desktop input is needed or performed.

IPC tray_available still means object presence for compatibility. tray_shell
separately exposes tooltip_applied, attempts, explorer_epoch, last_error and
remaining retry_ms. Shell acceptance is not visibility, notification delivery or
physical board state. Lost initial registration may still need the crate's next
TaskbarCreated; root-taskbar/absent-Explorer recovery is not declared complete.

Native tests exercise real production retry state with injected clock/outcomes,
GUID-bound NOTIFYICONDATA and UTF16 bounds. Source guard checks actual integration
and incident/health paths. Real replacement/shell acceptance, actual Explorer
restart and visible user menu/notification behavior require separate evidence.

Reference: [Microsoft NOTIFYICONDATA](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-notifyicondataw)
and [Shell_NotifyIcon](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shell_notifyiconw).
