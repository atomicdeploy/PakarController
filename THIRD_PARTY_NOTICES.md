# Third-party notices

PakarController is original integration code informed by earlier
AtomicDeploy projects. No pu850-esp source was copied because that repository
does not currently carry a license file.

- `firmware/avr/BoardPins.h`, `ShiftRegisters.*`, `SevenSegments.*`,
  `TonePlayer.*`, `pitches.h`, and `BootMelody.*` are imported from
  AtomicDeploy PCController commit `2a0e971e2bdaf186b4e841c64a53b516509beb37`
  under its MIT option. `BootMelody.h` has one include-path adaptation for the
  flat Arduino sketch directory; PakarController integration remains separate.
- `docs/legacy/esp8266_programmer_reference.ino` is an adapted ArduinoISP
  reference sketch. Its in-file BSD notice and Randall Bohn copyright remain
  authoritative.
- The desktop app's native palette strategy and dependency choices are adapted
  from the MIT-licensed Pealayer project by its contributors.
- FastLED, Arduino ESP8266 core, MiniCore, eframe/egui, reqwest, and
  serde are build/runtime dependencies under their respective upstream terms;
  their sources are not vendored here.

Historical PuzzleBoard, 40LED, and Puzzles behavior was audited and selected
game behavior was reimplemented behind the bounded PakarController protocol.
PCController hardware services listed above are reused as source rather than
rewritten. Exact inspected files and revisions are recorded in
`docs/LEGACY-CODE-AUDIT.md`.
