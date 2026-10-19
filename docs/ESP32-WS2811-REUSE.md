# esp32WS2811 source comparison

Reviewed actual [bertmelis/esp32WS2811](https://github.com/bertmelis/esp32WS2811/tree/0222ef025f2b8e03f8c8aef35d63c545a98ad579)
at0222ef025f2b8e03f8c8aef35d63c545a98ad579, not just its README. All14 C++
implementation/header files were read through EOF. All24 tracked paths are in
the existing source ledger, including documentation/metadata paths not certified
as a full manual comparison. This is a source comparison, NOT proof the current
matrix corruption or all480 physical pixels are fixed.

## Exact driver and API mapping

| Donor code | Integrated Pakar path and decision |
|---|---|
| `WS2811::show`, `_handleRmt`, task notification | `LedOutput::submit/work`: existing dedicated core1 worker, capacity-one complete-frame queue, coalescing and sleep. Retain modern IDF5 RMT/GDMA; do not add another driver/task. |
| `_smphr`, setters and transmit lock | Existing serialized producer-owned frame, independent immutable worker frame and1440-byte internal DMA source. No setter can alter the in-flight source; no per-cell lock/command fan-out. |
| `_setupRMT` idle-output LOW | Newly hardened common worker initialization: reset GPIO4, preload LOW, enable output, then allocate/init either backend. Each GPIO error stops before allocation/RMT/TX with error/readiness reported. Existing static code enabled output before preloading LOW; DMA had no explicit pre-allocation GPIO setup. |
| `setPixel`, `setRed/Green/Blue`, `setAll`, `clearAll`, `getPixel` | Existing `LedGrid::stagePixels`, atomic cell batches, `fill`, pixel diagnostic and `pixelFrameJson`. Use complete RGB snapshots rather than separate channel transactions. Off explicitly sends all480 black pixels; readback distinguishes published RAM from physical light. |
| `_handleRmt` bit/channel packing | Existing logical RGB → board-selected BGR/RGB → MSB-first IDF bytes encoder. Keep the user's R/B swap; do not force donor GRB onto this board. |
| `setItem0/setItem1`, `setTerminator` | Existing explicit40MHz timing, full480 payload,300us LOW latch, completion/timeout/flash ownership. Donor constants use its old clock-divider configuration; do not copy timing numbers across peripheral/strip profiles. Donor allocates a terminator but passes only `numLeds*24` items to `rmt_write_items`; do not copy that omission. |
| `Colour`, saturated `operator+=`, `ColourHSV` | Existing FastLED `CRGB`, `qadd8`, `fill_rainbow` and bounded RGB API. No duplicate colour class, float HSV conversion or per-pixel colour allocation. Old degree/percent constructor semantics are not a new public protocol. |
| `WS2811Effect::start/stop/_effectTask` | Existing `LedGrid::setPattern/stopPattern/service` in one loop owner. No per-effect task deletion, blocking sleeps, hidden FPS limit or second effects engine. |

The donor expands480 pixels into11521 symbols (46084 bytes), uses legacy
`driver/rmt.h`, assumes the original ESP32 channel-memory topology and calls
`ESP_ERROR_CHECK` during output. Those bodies are NOT imported into our pinned
S3/IDF5 application: compact internal payloads, SDK ring, explicit IRQ receipt,
bounded waits and recoverable error state remain. Its task-static transmit
buffer also cannot safely become a second independently sized strip instance.
These are concrete replacement reasons, not a claim the donor was integrated
byte-for-byte or that counters prove a correct waveform.

The upstream conversations were also checked: issue5 reports multiple-strip
cross-talk (the owner had tested only one strip); issue2/merged PR3 adds the
individual channel setters. Open [PR6](https://github.com/bertmelis/esp32WS2811/pull/6)
at3960ca6258fcabfa277cf358a08136984b6542a2 corrects GRB to RGB packing. Its complete
patch, body, comments/reviews and all review-thread pages were read (no threads
or reviews present). Our logical RGB plus explicit board R/B swap already avoids
that implicit GRB assumption. A new actual-worker regression checks unequal R/G/B
across all480 pixels in BOTH RGB/BGR and GPIO/DMA profiles. No installed channel
order was changed just because an old upstream patch recommends RGB.

## Useful effect algorithms not silently claimed as imported

| Exact donor files | Pending integration / reason not copied verbatim |
|---|---|
| `Effects/Aurora.cpp/.h` | Six weighted, moving light waves are useful. Port into bounded loop-owned state with one renderer/capability catalog. Original allocates a colour per wave/pixel, pairs scalar `new Colour` with `delete[]`, repeatedly allocates waves and sleeps20ms. Copying it would introduce allocator bugs, heap churn and an implicit rate cap. |
| `Effects/Autumn.cpp/.h` | Palette transition spreading from a random origin is useful. Port using wall-clock interpolation and validated positive steps/dimensions. Original can use an uninitialized origin before its first interval and divides by `steps`/`numLeds/2`; its loop-count animation stretches under load. |
| `Effects/Circus.cpp/.h` | Twelve-colour random repaint is useful as an optional effect, not a default/diagnostic. Preserve a requested interval without `delay()` or per-effect task ownership. |
| `Effects/SnowSparkle.cpp/.h` | Background/white sparkle/fade is useful. Port a fixed sparkle-slot pool with elapsed-time fades, no per-sparkle `new/delete` or10ms sleep. |

These eight source files remain explicit parity gaps in the SAME hash-bound
file ledger and issue#2. They are not business-only exclusions, dormant code
added to inflate coverage, or advertised working modes. Add their shared
capability discovery/API/USB/persistence and all-client selectors together;
do not expose a mode that a connected board has not acknowledged.

## Hardware/documentation boundaries

The README's level-shifter/OE/pulldown discussion is a hardware requirement to
review, not something firmware GPIO initialization replaces. Do not copy its
GPIO18/23 wiring onto this S3/AVR board or claim its50-LED example proves our480.
No physical wiring, live scene, running firmware, app or board was changed by
this source-reuse pass. Library packaging/examples/Doxygen/PDF paths stay
inventoried; the legacy Arduino package is not installed alongside our driver.

License: donor MIT notice retained in `docs/licenses/esp32WS2811-MIT.txt`.
Its driver headers additionally credit Neil Kolban's Apache-2.0 implementation;
no legacy RMT driver function body is copied here. Aurora separately credits
Matthias Folte. Preserve those notices when later adapting its algorithms.

Verification: actual worker fixture covers both GPIO/RMT backends, RGB/BGR,
startup black, LOW-before-output ordering, all three GPIO-init failures,
immutable frames, queue coalescing, Off black, timeout/fault and flash fences.
Host doubles/SDK build are separate from deployment and optical acceptance.
