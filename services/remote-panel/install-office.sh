#!/usr/bin/env bash
set -euo pipefail
test "$(id -u)" = 0
test "$(/usr/bin/node -p 'Number(process.versions.node.split(".")[0])>=24')" = true
incoming=/root/.cache/pakarcontroller-remote-panel
app=/opt/pakarcontroller-remote-panel
config=/etc/pakarcontroller-remote-panel
test ! -L "$app"
if test -d "$app" && test ! -f "$app/.pakar-managed"; then echo 'Unowned application directory' >&2; exit 1; fi
getent passwd pakarremote >/dev/null || useradd --system --no-create-home --shell /usr/sbin/nologin pakarremote
install -d -m 0755 "$app"
install -d -m 0750 -o root -g pakarremote "$config"
touch "$app/.pakar-managed"
for file in services/remote-panel/service.mjs services/remote-panel/gateway.mjs services/remote-panel/package.json services/remote-panel/package-lock.json tools/telemetry-reassembly.mjs tools/peer-identity.mjs interfaces/peer-policy.json interfaces/client-release.json assets/fonts/Vazirmatn-Regular.ttf ui/WebUi.h ui/peer-http.js ui/basic-cells.js ui/basic.html; do
    source="$incoming/$(basename "$file")"
    test -f "$source"
    test ! -L "$app/$file"
    install -d -m 0755 "$(dirname "$app/$file")"
    if ! cmp -s "$source" "$app/$file"; then
        if test -f "$app/$file"; then cp -p "$app/$file" "$app/$file.previous"; fi
        install -m 0644 "$source" "$app/$file.pending"
        mv -f "$app/$file.pending" "$app/$file"
    fi
done
lock=$(sha256sum "$app/services/remote-panel/package-lock.json" | cut -d' ' -f1)
if test ! -f "$app/.installed-lock" || test "$(<"$app/.installed-lock")" != "$lock"; then
    npm ci --prefix "$app/services/remote-panel" --ignore-scripts --no-audit --no-fund --loglevel=error
    printf '%s\n' "$lock" > "$app/.installed-lock"
fi
node --input-type=module - "$config" <<'JAVASCRIPT'
import {readFileSync,writeFileSync,existsSync,chmodSync} from 'node:fs';
import {randomBytes,createHash} from 'node:crypto';
const dir=process.argv[2],privatePath=dir+'/access.json';
if(!existsSync(privatePath))writeFileSync(privatePath,JSON.stringify({url:'https://ledboard.digitalogic.ir/',username:'david',password:randomBytes(24).toString('base64url')}),{mode:0o600,flag:'wx'});
chmodSync(privatePath,0o600);
const access=JSON.parse(readFileSync(privatePath)),broker=JSON.parse(readFileSync('/root/.config/pakarcontroller/ledboard-mqtt.json'));
if(broker.topic!=='pakarcontroller/ledboard'||broker.control.username!=='pakar-ledboard-control')throw Error('Unexpected MQTT scope');
writeFileSync(dir+'/mqtt.json',JSON.stringify({host:broker.host,port:broker.port,username:broker.control.username,password:broker.control.password}),{mode:0o640});
writeFileSync(dir+'/config.json',JSON.stringify({origin:'https://ledboard.digitalogic.ir',username:access.username,password_sha256:createHash('sha256').update(access.password).digest('hex'),topic:broker.topic,board:'esp32s3-907069eac800',mqtt_file:dir+'/mqtt.json',ui_directory:'/opt/pakarcontroller-remote-panel/ui',port:18744}),{mode:0o640});
JAVASCRIPT
chown root:pakarremote "$config/config.json" "$config/mqtt.json"
chmod 0640 "$config/config.json" "$config/mqtt.json"
install -m 0644 "$incoming/pakarcontroller-remote-panel.service" /etc/systemd/system/pakarcontroller-remote-panel.service
vhost=/etc/apache2/sites-available/ledboard.digitalogic.ir.conf
test ! -L "$vhost"
if test -f "$vhost"; then
    grep -q '^# PakarController managed private remote panel' "$vhost"
    cp -p "$vhost" "$config/apache.previous"
fi
install -m 0644 "$incoming/apache.conf" "$vhost"
a2ensite ledboard.digitalogic.ir.conf
if ! apache2ctl configtest; then
    if test -f "$config/apache.previous"; then cp -p "$config/apache.previous" "$vhost"; else a2dissite ledboard.digitalogic.ir.conf; fi
    echo 'Apache validation failed; no reload performed' >&2; exit 1
fi
systemctl daemon-reload
systemctl enable pakarcontroller-remote-panel
systemctl restart pakarcontroller-remote-panel
for attempt in 1 2 3 4 5; do
    code=$(curl --noproxy '*' --silent --output /dev/null --write-out '%{http_code}' http://127.0.0.1:18744/ || true)
    if test "$code" = 401; then break; fi
    sleep 1
done
test "$code" = 401
systemctl reload apache2
systemctl is-active pakarcontroller-remote-panel apache2 mosquitto asterisk
echo 'Private access file: /etc/pakarcontroller-remote-panel/access.json (root only).'
