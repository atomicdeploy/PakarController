# ledboard MQTT broker checkpoint

The existing Mosquitto service on `office.digitalogic.ir` was inspected through
the user-authorized SSH path. Hostname `mqtt.digitalogic.ir:8883` provides MQTT
over TLS; OpenSSL hostname/certificate-chain verification returned 0 (OK).
The configured broker refuses anonymous login (MQTT return/exit code 5).
The chain also validates when OpenSSL is restricted to the exact ISRG Root X1
PEM embedded in `CloudServices.cpp`, with default CA path/store disabled.
The served YE1 / Root YE / cross-signed X2 chain therefore does not require
disabling verification or blindly adding trust anchors. A second check forces
TLS 1.2 and explicitly validates the hostname; verification again returns 0.
The ESP's BearSSL handshake and buffer limits still require a live
device test. This CA compatibility check is not a positive MQTT login.

The prior device ACL allowed only its old suffixed topic. The four rules in
`ledboard.acl` were added under the **existing** device account, preserving all
previous rules and credentials. No account password was rotated, no unrelated
tenant rules were removed, and no new wildcard device write permissions added.
The device can read only `pakarcontroller/ledboard/command` and write its own
status/availability/event. The existing operator account already observes and
commands named Pakar devices; its rules are unchanged.

The candidate ACL was validated with an isolated two-second loopback broker
before replacing the live ACL. Mosquitto received SIGHUP (no service restart)
and remained active. Full deployed ACL SHA256:
`2991371ec87c4d86297f72fc610c2731eecedc7e272c46a49dd8b8e3ccfa6cba`.
Previous SHA256:
`b3452d582824c77cfe4ba998609a78ea647879f52f1b53bfe197ea09c45bb675`.
Recovery copy: `/etc/mosquitto/acl.pakar-before-ledboard` on that server.

This is **broker-only evidence**, not the current ESP's authenticated cloud
round-trip. The ASUS/board is unreachable. Positive authentication using the
stored device credentials, live command/event exchange, reconnect and outbound
WebSocket acceptance remain unchecked in issue #2. No fabricated retained
hardware state or LED command was published for these checks.

Any future configured hostname/base-topic change needs matching scoped broker
permissions. Do not replace the complete multi-tenant ACL with this fragment,
rotate existing secrets blindly, or publish device passwords in GitHub.
