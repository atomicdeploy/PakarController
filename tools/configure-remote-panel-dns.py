"""Scoped office-side DNS setup. Uses the existing private certbot credential.

Plan by default; --run creates only the missing ledboard CNAME. Never changes an
existing record, API token, CDN proxy setting or another application's hostname.
"""
import argparse
import json
from pathlib import Path
import urllib.error
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", action="store_true")
    args = parser.parse_args()
    credential = Path("/etc/letsencrypt/arvancloud.ini")
    if credential.is_symlink() or credential.stat().st_mode & 0o077:
        raise SystemExit("DNS credential must be a private regular file")
    values = dict(line.split("=", 1) for line in credential.read_text().splitlines()
                  if "=" in line and not line.lstrip().startswith(("#", ";")))
    token = next((v.strip() for k, v in values.items()
                  if k.strip() == "dns_arvancloud_api_token"), "")
    if not token:
        raise SystemExit("Existing DNS credential unavailable")
    endpoint = "https://napi.arvancloud.ir/cdn/4.0/domains/digitalogic.ir/dns-records"

    def request(method, data=None):
        req = urllib.request.Request(endpoint, method=method,
            data=None if data is None else json.dumps(data).encode(),
            headers={"Authorization": token, "Content-Type": "application/json",
                     "User-Agent": "PakarController-DNS-Setup"})
        try:
            with urllib.request.urlopen(req, timeout=15) as response:
                return json.load(response)
        except urllib.error.HTTPError as error:
            raise SystemExit(f"DNS API rejected request: HTTP {error.code}") from None

    def current():
        result = request("GET")
        rows = result.get("data")
        if not isinstance(rows, list) or result.get("meta", {}).get("next_page_url"):
            raise SystemExit("Unexpected or paginated DNS inventory; no write")
        matches = [row for row in rows if row.get("name", "").rstrip(".") in
                   ("ledboard", "ledboard.digitalogic.ir")]
        if len(matches) > 1:
            raise SystemExit("Ambiguous existing ledboard records; no write")
        return matches[0] if matches else None

    def check(record):
        if record.get("type", "").lower() != "cname" or record.get("cloud") is not False or \
                record.get("value", {}).get("host", "").rstrip(".") != "office.digitalogic.ir":
            raise SystemExit("Conflicting existing ledboard DNS record preserved")

    record = current()
    if record:
        check(record)
        print(json.dumps({"hostname": "ledboard.digitalogic.ir", "existing": True,
                          "target": "office.digitalogic.ir", "changed": False}))
        return
    if not args.run:
        print("Plan: create ledboard CNAME -> office.digitalogic.ir; TTL 120, CDN proxy off")
        return
    # Recheck immediately before the sole additive mutation. An uncertain POST
    # must be followed by a fresh GET, never an automatic publication retry.
    if current():
        raise SystemExit("DNS changed during preparation; no write")
    request("POST", {"type": "cname", "name": "ledboard", "value": {
        "host": "office.digitalogic.ir.", "host_header": "source", "port": -1},
        "ttl": 120, "cloud": False})
    record = current()
    if not record:
        raise SystemExit("Creation returned, but no DNS record was read back; do not retry blindly")
    check(record)
    print(json.dumps({"hostname": "ledboard.digitalogic.ir", "target": "office.digitalogic.ir",
                      "changed": True, "record_id": record["id"]}))


if __name__ == "__main__":
    main()
