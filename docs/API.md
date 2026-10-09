# HTTP and streaming API

All HTTP routes use Basic authentication. The development image starts an
access point named `pakarcontroller-<chip-id>` at `192.168.4.1`. The temporary
credentials are `admin` / `pakarcontroller` and must be replaced before a
public installation.

| Method | Route | Purpose |
|---|---|---|
| GET | `/api/v1/status` | peer-advertised AVR, mode, winner, cell, brightness, and LED state |
| POST | `/api/v1/mode` | form `value=cell` or `value=first-wins` |
| POST | `/api/v1/cell` | form `id=1..80`, `state=white|red|off`, `effect=immediate|fade|pulse` |
| POST | `/api/v1/game/reset` | reset the current game |
| POST | `/api/v1/brightness` | form `value=1..255` |
| POST | `/api/v1/effect` | form `value=none|rainbow|fire` |
| POST | `/api/v1/buzzer` | form `frequency=20..12000`, `duration_ms=1..5000` |
| POST | `/api/v1/display` | form `value=-999..9999` for the AVR TM1637 |
| POST | `/api/v1/outputs` | form `mask=0..255` for direct 74HC595 diagnostics |
| GET | `/api/v1/avr/probe` | read and validate ATmega328P signature through ISP |
| POST | `/api/v1/avr/backup` | store flash, EEPROM, fuses, and lock byte in LittleFS |
| GET | `/api/v1/avr/backup/flash` | download the last 32 KiB raw flash backup |
| GET | `/api/v1/avr/backup/eeprom` | download the last 1 KiB EEPROM backup |
| POST | `/api/v1/avr/image` | multipart upload of one complete 32768-byte AVR+Urboot raw image |
| POST | `/api/v1/avr/program` | backup, erase, program, and byte-verify the staged image |
| POST | `/update` | ESP8266 HTTP self-update page |

UDP port 7890 accepts bounded `PKR1` chunks. Each datagram contains the four
ASCII magic bytes, two reserved/version bytes, little-endian pixel offset,
one pixel count, one flags byte, then RGB triples. Bit zero of flags commits
the staged frame. A packet is rejected unless its exact size and pixel range
are valid.
