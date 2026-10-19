#!/usr/bin/env bash
set -euo pipefail
# Dedicated additive service only. Never restart or modify PBX/MQTT services.
test "$(id -u)" = 0
test "$(/usr/bin/node -p 'Number(process.versions.node.split(".")[0])>=24')" = true
incoming=/root/.cache/pakarcontroller-management-deploy
app=/opt/pakarcontroller-management
data=/var/lib/pakarcontroller-management
config=/etc/pakarcontroller-management
vhost=/etc/apache2/sites-enabled/office.digitalogic.ir-cloud.conf
test -f "$vhost"
test ! -L "$app"
if test -d "$app" && test ! -f "$app/.pakar-managed"; then
  echo 'Unowned deployment directory; refusing replacement' >&2; exit 1
fi
getent passwd pakarcontroller >/dev/null || useradd --system --home-dir "$data" --shell /usr/sbin/nologin pakarcontroller
install -d -m 0755 "$app"
install -d -o pakarcontroller -g pakarcontroller -m 0700 "$data"
install -d -o root -g pakarcontroller -m 0750 "$config"
touch "$app/.pakar-managed"
for directory in services services/client-management tools interfaces; do
  test ! -L "$app/$directory"
  install -d -m 0755 "$app/$directory"
done
for file in services/client-management/service.mjs services/client-management/operator.mjs services/client-management/dashboard.html services/client-management/package.json services/client-management/package-lock.json tools/peer-identity.mjs interfaces/client-release.json interfaces/peer-policy.json app-icon.png.base64; do
  source="$incoming/$(basename "$file")"
  test -f "$source"
  install -d -m 0755 "$(dirname "$app/$file")"
  test ! -L "$app/$file"
  if ! cmp -s "$source" "$app/$file"; then
    if test -f "$app/$file"; then cp -p "$app/$file" "$app/$file.previous"; fi
    install -m 0644 "$source" "$app/$file.new"
    mv -f "$app/$file.new" "$app/$file"
  fi
done
lock=$(sha256sum "$app/services/client-management/package-lock.json" | cut -d' ' -f1)
read -r release_version release_build < <(node -e 'const v=require(process.argv[1]);if(v.product!=="PakarController"||!/^[0-9A-Za-z.+-]+$/.test(v.version)||!Number.isSafeInteger(v.build))process.exit(1);console.log(v.version,v.build)' "$app/interfaces/client-release.json")
if test ! -d "$app/services/client-management/node_modules/ws" || test ! -f "$app/.installed-lock" || test "$(cat "$app/.installed-lock")" != "$lock"; then
  npm ci --prefix "$app/services/client-management" --ignore-scripts --no-audit --no-fund --loglevel=error
  printf '%s\n' "$lock" > "$app/.installed-lock"
fi
if test ! -f "$config/admin-token"; then
  (umask 077; node -e 'process.stdout.write(require("node:crypto").randomBytes(32).toString("base64url"))' > "$config/admin-token")
fi
chmod 0600 "$config/admin-token"
admin_hash=$(node -e 'const f=require("node:fs"),c=require("node:crypto");const t=f.readFileSync(process.argv[1],"utf8").trim();if(!/^[A-Za-z0-9_-]{43}$/.test(t))process.exit(1);process.stdout.write(c.createHash("sha256").update(t).digest("hex"))' "$config/admin-token")
cat > "$config/service.env.new" <<EOF
PAKAR_DATA=$data
PAKAR_PUBLIC_BASE=https://office.digitalogic.ir/pakar
PAKAR_ADMIN_TOKEN_HASH=$admin_hash
PAKAR_ICON_FILE=$app/app-icon.png.base64
PAKAR_PORT=18743
EOF
chown root:pakarcontroller "$config/service.env.new"
chmod 0640 "$config/service.env.new"
mv -f "$config/service.env.new" "$config/service.env"
install -m 0644 "$incoming/pakarcontroller-management.service" /etc/systemd/system/pakarcontroller-management.service
# Preserve the exact current vhost before its single idempotent include edit.
resolved_vhost=$(readlink -f "$vhost")
test -f "$resolved_vhost"
test "$(grep -c '<VirtualHost \*:443>' "$resolved_vhost")" = 1
cp -p "$resolved_vhost" "$config/office-vhost.before"
include=/etc/apache2/pakarcontroller-management.conf
if test -f "$include"; then cp -p "$include" "$config/apache-include.before"; fi
cat > "$include.new" <<'EOF'
# PakarController management: isolated versionless API and release feed.
RedirectMatch 302 ^/pakar$ /pakar/
ProxyPass "/pakar/api/events" "ws://127.0.0.1:18743/pakar/api/events" retry=0 timeout=130
ProxyPassReverse "/pakar/api/events" "ws://127.0.0.1:18743/pakar/api/events"
ProxyPass "/pakar/" "http://127.0.0.1:18743/pakar/" retry=0 timeout=130
ProxyPassReverse "/pakar/" "http://127.0.0.1:18743/pakar/"
EOF
chmod 0644 "$include.new"
mv -f "$include.new" "$include"
if ! grep -Fq "Include $include" "$resolved_vhost"; then
  awk -v line="    Include $include" '/<VirtualHost \*:443>/{print;print line;next}{print}' "$resolved_vhost" > "$config/office-vhost.new"
  cat "$config/office-vhost.new" > "$resolved_vhost"
fi
if ! apache2ctl configtest; then
  cat "$config/office-vhost.before" > "$resolved_vhost"
  if test -f "$config/apache-include.before"; then cp -p "$config/apache-include.before" "$include"; fi
  echo 'Apache validation failed; vhost restored without reload' >&2; exit 1
fi
systemctl daemon-reload
systemctl enable pakarcontroller-management.service
systemctl restart pakarcontroller-management.service
probe_health() {
  curl --noproxy '*' --fail --silent --user-agent "PakarController/$release_version (service installer)" --header 'X-Pakar-Client: pakarcontroller-tool' --header "X-Pakar-Version: $release_version" --header "X-Pakar-Build: $release_build" http://127.0.0.1:18743/pakar/api/health
}
for i in 1 2 3 4 5; do
  if probe_health >/dev/null; then break; fi
  sleep 1
done
probe_health
systemctl reload apache2
systemctl is-active pakarcontroller-management apache2 asterisk mosquitto
echo 'Admin token is stored in /etc/pakarcontroller-management/admin-token (root only).'
