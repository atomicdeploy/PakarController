# Legacy code audit

## Local PuzzleBoard variants

The historical sources are not Git repositories, so hashes identify the exact
files inspected.

| Source | SHA-256 | Useful behavior retained |
|---|---|---|
| `PuzzleBoard/PuzzleBoard.ino` | `BC9CDDD4ACEF723A803DF02DD22FC09EE681A9F1E72FBA1AF6625EBA87EE9026` | shift-register scan, key debounce, display animation, welcome/error tones, explicit mode transitions |
| `PuzzleBoard/XS/40LED.ino` | `9494993A0AA031A75B709A47A6220330C9B4BB4E90D5A7E9898A37AD9111599A` | FastLED patterns, cell-fill progression, software UART bridge, shift I/O, test/demo modes |
| `PuzzleBoard/XS/40LED.ino.new` | `46158D60DAAAAC4D69B7C9DEEB45A1118578F39C5AE1A4C8CF401E1B9726940F` | alternate Radiogram pin interpretation and simplified value/bonus beeps |
| `ControllerBoardMini/Puzzles/Puzzles.ino` | `EC9EACB6A94A0C1841722520141CFB5F645D87DB92654A88041489F5DD74BC3A` | BASIC, PUZZLE, CODE, and MAZE profiles; nonblocking tone queue; TM1637; reusable shift/key/task modules |

The two 40LED files differ only in a small hardware variation: the older
`FINAL_TABLE` path reads the 74HC165 and swaps clear/door ownership depending
on profile, while `.new` disables the shift read and fixes the Radiogram-style
direct keys. `.new` also beeps only for positive value/bonus changes. The main
effect functions (`rainbow`, `confetti`, `sinelon`, `bpm`, `juggle`, and
`Fire2012`) are otherwise the same.

The older `PuzzleBoard.ino` targets an ATmega32-style pin map and manually
multiplexes a display. The later `Puzzles.ino` targets the ATmega328P board,
uses TM1637, factors hardware into LocalLib, moves melodies to a bounded queue,
and selects game behavior with compile-time `PROGRAM_*` variants. Its current
checked file selects `PROGRAM_MAZE`.

## Git repositories

| Repository | Inspected commit | Reuse decision |
|---|---|---|
| `atomicdeploy/PCController` | `2a0e971e2bdaf186b4e841c64a53b516509beb37` | imported BoardPins, ShiftRegisters, SevenSegments, TonePlayer, pitches, and BootMelody; retained wire envelope, CRC/COBS, Urboot, update safety, host/API and simulator patterns |
| `atomicdeploy/pu850-esp` | `431104072952f694a35b3a83c19f13fbb098ca5b` | Wi-Fi/AP, HTTP/WebSocket, OTA, discovery, LittleFS, firmware-transfer and tooling architecture; product UART/business tables excluded |
| `ToghrolTP/pealayer` local canonical | `005d6596e1c10b7251e652232e01f1763a1784a9` | eframe/egui application structure, themes, controller client and factual-state UI conventions; mpv/media engine excluded |

The PCController and Pealayer source repositories were left unchanged.
Hardware modules are copied into PakarController under PCController's MIT
option, while game-specific integration stays isolated here and numeric framing
remains compatible with the shared PCController protocol family.

The final integration also carries forward PuzzleBoard's edge-based debounce,
task servicing, cached TM1637 updates, nonblocking melody queue, explicit mode
transitions, and shift-register ownership. PCController contributes its
framed multi-transport host contract, peer capability advertisement, EEPROM
safety latch, Urboot image workflow, and factual front-panel state rules.
Historical I2C, DS18B20, RF, product tables, and unrelated demo effects remain
excluded because the present hardware does not provide them.
