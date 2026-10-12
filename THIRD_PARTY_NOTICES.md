# Third-party notices

PakarController is integration code built from earlier AtomicDeploy projects.

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
- The AP+station lifecycle, independent flash-backed network configuration,
  build entrypoints, and CI layout are adapted from AtomicDeploy pu850-esp
  commit `431104072952f694a35b3a83c19f13fbb098ca5b`. PU product/business logic
  and its legacy UART payload implementation are not included. The source
  repository has no standalone license file; this reuse is recorded explicitly
  because both repositories are owner-directed AtomicDeploy work.
- FastLED, Arduino ESP8266 core, MiniCore, WebSockets 2.7.2 by Markus Sattler,
  PubSubClient 2.8.0 by Nick O'Leary, xcap 0.9.8, eframe/egui, reqwest, and serde are
  build/runtime dependencies under their
  respective upstream terms; their sources are not vendored here. WebSockets
  is used under LGPL-2.1-or-later for the authenticated same-origin ESP8266
  event endpoint.

Historical PuzzleBoard, 40LED, and Puzzles behavior was audited and selected
game behavior was reimplemented behind the bounded PakarController protocol.
PCController hardware services listed above are reused as source rather than
rewritten. Exact inspected files and revisions are recorded in
`docs/LEGACY-CODE-AUDIT.md`.

Android advanced network dependencies: mqtt_client 10.12.0 (MIT; included
Observable code also carries the Dart authors' BSD-style notice), event_bus
2.0.1 (MIT), and crypto 3.0.7 (Dart BSD-style license). Original package license
texts remain in the dependency distribution and Flutter's generated license
bundle. mqtt_client implements the MQTT wire protocol; Android DNS-SD uses the
system NsdManager rather than a copied Go runtime. Owner-directed PU850 MNDP
TLV/metadata adaptation and concrete omissions are hash-bound in the existing
file comparison ledger; there is no claim of complete donor parity.
