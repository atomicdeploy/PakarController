# Third-party notices

PakarController is original integration code informed by earlier
AtomicDeploy projects. No pu850-esp source was copied because that repository
does not currently carry a license file.

- `docs/legacy/esp8266_programmer_reference.ino` is an adapted ArduinoISP
  reference sketch. Its in-file BSD notice and Randall Bohn copyright remain
  authoritative.
- The desktop app's native palette strategy and dependency choices are adapted
  from the MIT-licensed Pealayer project by its contributors.
- FastLED, Arduino ESP8266 core, MiniCore, TM1637, eframe/egui, reqwest, and
  serde are build/runtime dependencies under their respective upstream terms;
  their sources are not vendored here.

Historical PuzzleBoard, 40LED, Puzzles, and PCController behavior was audited
and reimplemented behind the bounded PakarController protocol. Exact inspected
files and revisions are recorded in `docs/LEGACY-CODE-AUDIT.md`.
