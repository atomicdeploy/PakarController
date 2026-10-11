# Cooperative callbacks and AVR reset safety

Actual PCController LocalLib/Tasks.h/cpp code already existed, unwired, in
BoardCore. It is now a real AVR loop consumer, not another duplicate scheduler.
Puzzles/Timer's ten slots are retained; context callbacks, explicit occupancy,
count/clear/cancel and signed rollover deadlines adapt the newer PCController
implementation. No business game, relay, sensor or pin policy is imported.

Each update admits only callbacks due at entry. It retires a slot before calling
its callback; cancellation/clear can suppress remaining callbacks, new/replaced
slots wait for the next outer update, and recursive update returns immediately.
No heap, callback-copy batch, timer ISR or blocking wait is introduced. Callbacks
must return promptly; this cannot preempt an arbitrary blocking callback. The
API is loop-only, not interrupt/thread-safe. Slot indices remain reusable as in
the donor, not permanent generation tokens; owners must discard canceled handles.

Zero and exact deadlines are valid. Delays beyond INT32_MAX are rejected instead
of interpreting them as already due; the service must run within half the clock
range (about24.8days). Optional explicit `now` shares the AVR command's clock and
makes the production implementation deterministic in host tests. Context pointers
remain caller-owned and must live through execution/cancellation.

The AVR Reset operation schedules one100ms callback. Repeated requests replace
that pending slot, a full scheduler returns Busy, and entering Prog cancels it.
The callback checks the programming latch again before safe-stop/watchdog reset.
ACK is scheduling acceptance, not proof of reboot; no reset is replayed when Prog
is released. Existing AVR hardware UART, busy/poll/buffered flags, A4/A5 TM1637,
melody engine and front-panel deadlines remain unchanged.

Tasks host tests execute production code with a clock double, including ten-slot
bounds, null/half-range rejection, zero/exact/wrapped deadlines, cancellation,
clear/reentrant replacement, newly added high-slot deferral and reset-owner
cancellation. Actual sketch ordering is source-guarded and must compile in all
three AVR profiles. Neither is physical watchdog, reset/ISP contention, audible
timing or board acceptance. Updated hashes/omissions are in the existing file
comparison ledger; all remaining donor/production requirements stay open.

Verification checkpoint: guarded production Tasks tests, wiring guards and the
complete source-quality phase passed. All three warm-cache AVR builds passed:

| UART build | Program bytes /32384 | Static SRAM /2048 | Remaining stack/locals |
|---|---:|---:|---:|
| busy-wire | 27656 | 1317 | 731 |
| polling | 27356 | 1315 | 733 |
| buffered | 27538 | 1317 | 731 |

The extra scheduler storage raises static SRAM by about90bytes; this budget is
not a measured stack high-water mark. Physical UART/ISP/animation stress and
reset/Prog ordering remain required before accepting this firmware on hardware.
No board deployment, native/mobile rebuild or runtime replacement occurred.
