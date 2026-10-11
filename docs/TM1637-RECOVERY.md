# TM1637 presence and replay

The actual PCController `LocalLib/SevenSegments.h/.cpp` is retained/adapted,
not replaced by a second library or a random pin probe. Confirmed PuzzleBoard
wiring is AVR A4/SDA to DIO and A5/SCL to CLK. PB3/PB5 remain ISP, and Timer1
continues to own the buzzer. No wiring, EEPROM, public API or clock change.

The driver releases DIO before sampling each ninth-clock ACK. It now enables
the weak internal pull-up AFTER releasing the strong driver, so an absent DIO
does not float low and become a phantom ACK. The target code uses DDRC/PORTC;
the Arduino fallback uses INPUT_PULLUP. Existing intermediate-NACK accumulation,
one-second full-frame/control replay, 70ms brightness fading, zero-as-off and
cached text/raw/number behavior remain. The pull-up is not an excuse to omit
needed board resistors, level compatibility or measured signal rise-time tests.

The semantics follow the [Titan Micro TM1637 datasheet](https://cdn.velleman.eu/downloads/29/infosheets/tm1637_datasheet.pdf)
(ACK timing, page3) and [Microchip ATmega328P datasheet](https://ww1.microchip.com/downloads/en/devicedoc/atmel-7810-automotive-microcontrollers-atmega328p_datasheet.pdf)
(DDRx/PORTx and input pull-up, sections13.2.1/13.2.3). Module ACK means observed
bus response, not proof that LEDs emitted light. A shorted-low wire can still
masquerade as ACK; no such physical fault diagnosis is claimed by these tests.

## Actual donor tests

Completely read PCController
`Tools/VirtualBoard/tests/firmware_controls_test.cpp` and its Arduino mock
dependency. Imported the actual require helper, brightness fade assertions and
ACK/recovery scenario into `tests/tm1637_bus_test.cpp`, adapting only the separate
Pakar health-service invocation, correct pins and stronger assertions. The full
source file's key/transition/menu/tone assertions remain explicitly pending in
the SAME comparison ledger; its unfitted motion/temperature/1-Wire hardware is
not enabled by a display import. The mock dependency was read but is not
certified as a complete generic import.

The new bounded GPIO/port-register double decodes the real bit-banged bytes:

- Four addressed segment bytes, data mode and actual brightness/off commands.
- Every ACK slot is input with pull-up, never a strong output against ACK.
- Both Arduino and actual __AVR__ PC4/PC5 branches; all other port bits retained.
- Unchanged-frame caching, 999/1000ms cadence and exact recovery replay.
- Each of seven possible NACK positions, not only the final control byte.
- An intentionally off display stays off through disconnect/reconnect.
- Brightness clamping/fading and exact 32-bit timer-rollover boundary.

Run `pwsh -NoProfile -File tools/test-display.ps1`. One tiny build/native
executable is reused for both variants, without another toolchain/cache. The
normal protocol runners include it; firmware build wrappers enforce the
tm1637-display hash-bound donor gate. Register/GPIO doubles are not measurements
of capacitance, voltage, clock fuses or real unplug/replug behavior.

## Physical acceptance remains open

With the boards reachable, read authoritative display ACK/segments, disconnect
and reconnect the display safely, verify absence and cached-frame/brightness
recovery without MCU reset, then test true-off, text scroll/capture/release and
simultaneous UART/audio/LED load. Measure VCC/clock speed grade and bus levels
before flashing; do not infer electrical correctness from this source receipt.
Track source/physical acceptance separately on issues3 and6.
