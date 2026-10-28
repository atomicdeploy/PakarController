# UPnP discovery and board icons

PakarController advertises a UPnP **Basic:1 device**, not an Internet gateway.
SSDP `LOCATION` points to `/description.xml`; the description contains identity,
version, presentation URL and one role-specific PNG icon:

| Board role | Icon master | Public HTTP URL |
| --- | --- | --- |
| Table / ledcontroller | `assets/branding/table.png.base64` | `/board-icon.png` |
| Keycontroller | `assets/branding/keys.png.base64` | `/board-icon.png` |
| Generic | `assets/branding/app-icon.png.base64` | `/board-icon.png` |

The icon URL is relative to the description's HTTP origin. An AP, station or
Ethernet control point therefore fetches from the same reachable interface and
port, without resolving a hostname or following a different interface address.
Width, height, depth and strong ETag come from the actual PNG. GET/HEAD work
without application authentication; this does not admit control or file writes.
Artwork stays compiled in flash, with only the matching role included.

The description uses UPnP 1.1 element ordering and `configId`, XML-escapes dynamic
identity fields and omits `URLBase`. Its cache identity includes artwork and
device metadata. Basic:1 defines no standard services: no fabricated SOAP or
GENA endpoints, gateway services or router port mappings are advertised.

## Verification

Run `tools/test-upnp.ps1` for the actual S3 schema method, all three PNG variants,
XML parsing, cache identity, interface-relative URLs and offline router-tool
fixtures. CI quality invokes it. `tools/test-web-access.ps1` checks public reads
and denied unauthenticated writes. `tools/test-async-http.ps1` covers static
asset HEAD/ETag/304 and byte integrity under short writes/backpressure.

Regenerate artwork with `node tools/generate-branding.mjs`; `--check` verifies
the generated flash asset without rewriting it.

For an authorized, read-only gateway probe:

```powershell
./tools/check-router-upnp.ps1 -LocalAddress <this-PC-LAN-IP> -Gateway <router-IP> -OutputPath build/diagnostics/router-upnp.json
```

No WAN port is opened. No response means only that no IGD response was observed,
not that the router is unsupported. SSDP does not provide acknowledgements of
router acceptance. Actual discovery/icon fetching on each board/interface,
control-point rendering, reboot/lifecycle behavior and router interoperability
still require deployment and live captures; fixtures are not conformance
certification. Full IGD mapping/control is a separate, opt-in feature, not part
of board discovery.

References: [UPnP Device Architecture 1.1](https://upnp.org/specs/arch/UPnP-arch-DeviceArchitecture-v1.1.pdf),
[Basic:1 device template](https://upnp.org/specs/basic/UPnP-basic-Basic-v1-Device.pdf).
