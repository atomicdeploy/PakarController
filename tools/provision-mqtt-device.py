"""Explicit Linux Mosquitto provisioning for this one board; no anonymous ACL.

Run as broker administrator. Passwords stay in a mode0600 operator file and are
never printed. Existing unrelated users/rules and rollback files are preserved.
"""
import argparse
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import stat
import subprocess


def provisioning_scope(device):
    # Preserve the existing ledboard default, but never share that principal or
    # command topic with a replacement/differently named physical board.
    if not isinstance(device, str) or not re.fullmatch(r'[a-z][a-z0-9-]{0,19}', device):
        raise ValueError('Use a concrete lowercase device hostname, not a path or MQTT wildcard')
    return (device + '-mqtt.json',
            {'device': 'pakar-' + device, 'control': 'pakar-' + device + '-control'},
            'pakarcontroller/' + device)


def provision(device='ledboard'):
    credential_name, accounts, topic = provisioning_scope(device)
    if os.geteuid() != 0:
        raise SystemExit('Run only on the authorized Mosquitto broker as root')
    directory = Path('/root/.config/pakarcontroller')
    directory.mkdir(mode=0o700, parents=True, exist_ok=True)
    credentials = directory / credential_name
    passwd = Path('/etc/mosquitto/passwd')
    acl = Path('/etc/mosquitto/acl')
    known = {line.partition(':')[0] for line in passwd.read_text().splitlines()}
    if credentials.exists():
        if stat.S_IMODE(credentials.stat().st_mode) & 0o077:
            raise SystemExit('Credential file is not private; no modification')
        value = json.loads(credentials.read_text())
        if any(value[key]['username'] != user for key, user in accounts.items()):
            raise SystemExit('Unexpected principal; no credential rotation')
        if value.get('topic') != topic or value.get('host') != 'mqtt.digitalogic.ir' or value.get('port') != 8883:
            raise SystemExit('Unexpected broker/topic scope; no modification')
    else:
        if known.intersection(accounts.values()):
            raise SystemExit('Principal exists without owned credentials; no overwrite')
        value = {'host': 'mqtt.digitalogic.ir', 'port': 8883, 'topic': topic}
        for key, user in accounts.items():
            value[key] = {'username': user, 'password': secrets.token_urlsafe(32)}
        with open(credentials, 'x', opener=lambda path, flags: os.open(path, flags, 0o600)) as output:
            json.dump(value, output)
    for file in (passwd, acl):
        backup = file.with_name(file.name + '.pakar-' + device + '-before')
        if not backup.exists():
            shutil.copy2(file, backup)
            shutil.chown(backup, user=file.stat().st_uid, group=file.stat().st_gid)
    # The broker-supported password writer preserves every unrelated principal.
    for key in accounts:
        account = value[key]
        if account['username'] not in known:
            subprocess.run(['mosquitto_passwd', '-b', str(passwd), account['username'], account['password']], check=True)
    text = acl.read_text()
    additions = ''
    for key, user in accounts.items():
        if ('user ' + user) in text.splitlines():
            continue
        additions += '\n# PakarController ' + device + ' scoped ' + key + '\nuser ' + user + '\n'
        for leaf in ('status', 'availability', 'event', 'telemetry/part'):
            additions += 'topic ' + ('write' if key == 'device' else 'read') + ' ' + value['topic'] + '/' + leaf + '\n'
        additions += 'topic ' + ('read' if key == 'device' else 'write') + ' ' + value['topic'] + '/command\n'
    if additions:
        pending = acl.with_name(acl.name + '.pakar-' + device + '.pending')
        with pending.open('x') as output:
            output.write(text + additions)
        os.chmod(pending, stat.S_IMODE(acl.stat().st_mode))
        shutil.chown(pending, user=acl.stat().st_uid, group=acl.stat().st_gid)
        os.replace(pending, acl)
    subprocess.run(['systemctl', 'reload', 'mosquitto'], check=True)
    subprocess.run(['systemctl', 'is-active', '--quiet', 'mosquitto'], check=True)
    print(json.dumps({'configured': True, 'host': value['host'], 'topic': value['topic'], 'secrets_printed': False}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--device', default='ledboard', help='Exact configured board hostname; existing ledboard scope is the default')
    provision(parser.parse_args().device)
