"""Bounded real-broker acceptance, not a replacement firmware/cloud client.

Telemetry framing is adapted directly from telemetry-reassembly.mjs. A command
is published once, never retained; unknown outcomes are never replayed.
"""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import ssl
import threading
import time
import uuid
import paho.mqtt.client as mqtt


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--credentials', default='/root/.config/pakarcontroller/ledboard-mqtt.json')
    parser.add_argument('--board', required=True)
    parser.add_argument('--seconds', type=int, default=18)
    parser.add_argument('--command', choices=('telemetry', 'status', 'firmware-status'), default='telemetry')
    parser.add_argument('--request')
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    if not 5 <= args.seconds <= 300 or Path(args.output).exists():
        raise SystemExit('Bounded5..300second window and a new output file required')
    config = json.loads(Path(args.credentials).read_text())
    topic = config['topic']
    if not re.fullmatch(r'[a-zA-Z0-9_.-]+(?:/[a-zA-Z0-9_.-]+)*', topic):
        raise SystemExit('A concrete topic without MQTT wildcards is required')
    result = {'board_id': args.board, 'tls': True, 'connected': False, 'published': False, 'status': None, 'telemetry': None, 'event': None, 'errors': []}
    packets = {}
    publish_mid = None
    done = threading.Event()
    request = json.loads(Path(args.request).read_text()) if args.request else {'command': args.command, 'request_id': 'probe-' + uuid.uuid4().hex}
    if not re.fullmatch(r'[a-zA-Z0-9_-]{1,40}', request.get('request_id', '')):
        raise SystemExit('Every probe requires an explicit correlation identity')
    if request.get('command') not in ('status', 'telemetry', 'firmware-status'):
        expiry = request.get('expires_at', 0)
        if request.get('board_id') != args.board or not isinstance(expiry, int) or not time.time() < expiry <= time.time() + 60 or not re.fullmatch(r'[a-zA-Z0-9_-]{1,40}', request.get('request_id', '')) or not re.fullmatch(r'[a-f0-9]{32}', request.get('boot_id', '')):
            raise SystemExit('Mutation requires explicit current peer/deadline/unique request identity')
    callback_api = {'callback_api_version': mqtt.CallbackAPIVersion.VERSION2} if hasattr(mqtt, 'CallbackAPIVersion') else {}
    client = mqtt.Client(client_id='pakar-probe-' + uuid.uuid4().hex, **callback_api)
    client.username_pw_set(config['control']['username'], config['control']['password'])
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED)

    def connected(c, _userdata, _flags, code, _properties=None):
        result['connected'] = code == 0
        if code != 0:
            result['errors'].append('MQTT connect rejected: ' + str(code))
            done.set()
            return
        c.subscribe([(topic + '/status', 0), (topic + '/event', 0), (topic + '/telemetry/part', 0)])

    def subscribed(c, _userdata, _mid, granted, _properties=None):
        nonlocal publish_mid
        if any(getattr(code, 'is_failure', code == 128) for code in granted):
            result['errors'].append('MQTT subscription rejected')
            done.set()
        elif publish_mid is None:
            publication = c.publish(topic + '/command', json.dumps(request), qos=0, retain=False)
            if publication.rc != mqtt.MQTT_ERR_SUCCESS:
                result['errors'].append('Command was not queued; no retry')
                done.set()
            publish_mid = publication.mid

    def published(_client, _userdata, mid, *_extra):
        if mid == publish_mid:
            result['published'] = True  # QoS0 socket write, not command acceptance.

    def message(_client, _userdata, msg):
        try:
            value = json.loads(msg.payload)
            if msg.topic.endswith('/status'):
                if value.get('board_id') != args.board:
                    raise ValueError('Foreign board status')
                result['status'] = value
            elif msg.topic.endswith('/event'):
                if value.get('request_id') == request['request_id'] and isinstance(value.get('accepted'), bool) and isinstance(value.get('code'), str):
                    result['event'] = value
            else:
                p = value
                total, count, index = p['total_bytes'], p['count'], p['index']
                if p.get('type') != 'telemetry-fragment' or p.get('encoding') != 'base64' or not re.fullmatch(r'[a-f0-9]{32}-\d{1,10}', p['snapshot_id']) or not 1 <= total <= 16384 or count != (total + 767) // 768 or not 0 <= index < count or not re.fullmatch(r'[a-f0-9]{64}', p['sha256']):
                    raise ValueError('Invalid telemetry bounds/identity')
                raw = base64.b64decode(p['data'], validate=True)
                if len(raw) != min(768, total - index * 768):
                    raise ValueError('Invalid fragment length')
                now = time.monotonic()
                for identity in list(packets):
                    if now - packets[identity]['at'] > 30:
                        del packets[identity]
                if p['snapshot_id'] not in packets and len(packets) >= 4:
                    raise ValueError('Too many incomplete snapshots')
                job = packets.setdefault(p['snapshot_id'], {'at': now, 'total': total, 'count': count, 'sha': p['sha256'], 'parts': {}})
                if (job['total'], job['count'], job['sha']) != (total, count, p['sha256']) or index in job['parts'] and job['parts'][index] != raw:
                    raise ValueError('Mixed/conflicting telemetry')
                job['parts'][index] = raw
                if len(job['parts']) == count:
                    full = b''.join(job['parts'][i] for i in range(count))
                    del packets[p['snapshot_id']]
                    if len(full) != total or hashlib.sha256(full).hexdigest() != p['sha256']:
                        raise ValueError('Telemetry checksum mismatch')
                    telemetry = json.loads(full)
                    if telemetry.get('type') != 'telemetry' or telemetry.get('product') != 'PakarController' or telemetry.get('board_id') != args.board:
                        raise ValueError('Foreign full telemetry')
                    result['telemetry'] = telemetry
                    result['telemetry_sha256'] = p['sha256']
                    result['telemetry_bytes'] = total
            if (result['telemetry'] or request.get('command') != 'telemetry') and result['status'] and result['event'] is not None:
                done.set()
        except (ValueError, KeyError, TypeError) as error:
            result['errors'].append(str(error))

    client.on_connect, client.on_subscribe, client.on_message, client.on_publish = connected, subscribed, message, published
    client.connect(config['host'], config['port'], keepalive=30)
    client.loop_start()
    try:
        done.wait(args.seconds)
    finally:
        client.disconnect()
        client.loop_stop()
    with Path(args.output).open('x') as output:
        json.dump(result, output)
    print(json.dumps({key: result.get(key) for key in ('board_id', 'connected', 'published', 'telemetry_bytes', 'telemetry_sha256', 'event', 'errors')}))
    if not result['published'] or not result['event'] or not result['event']['accepted'] or result['errors'] or request.get('command') == 'telemetry' and not result['telemetry']:
        raise SystemExit(2)


if __name__ == '__main__':
    main()
