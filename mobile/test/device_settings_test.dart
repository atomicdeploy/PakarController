import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/device_settings.dart';

import 'peer_fixture.dart';
import 'settings_fixture.dart';

void main() {
  test('Ethernet settings are contextual and saved readback is exact', () {
    final raw = <String, dynamic>{
      'configured': true,
      'enabled': true,
      'lldp_enabled': true,
      'lldp_receive': false,
      'dhcp': false,
      'address': '192.168.9.2',
      'gateway': '192.168.9.1',
      'subnet': '255.255.255.0',
      'dns1': '0.0.0.0',
      'dns2': '0.0.0.0',
    };
    final saved = settingsSnapshot(SettingsGroup.ethernet, raw);
    expect(SettingsGroup.ethernet.path, '/api/network/ethernet');
    expect(saved.containsKey('configured'), false);
    final write = SettingsWrite(SettingsGroup.ethernet, {}, saved);
    expect(() => write.verifyAck({'accepted': true}), returnsNormally);
    expect(
      () => write.verifyReadback({...saved, 'address': '192.168.9.3'}),
      throwsFormatException,
    );
    expect(
      () => settingsSnapshot(SettingsGroup.ethernet, {
        ...raw,
        'configured': false,
      }),
      throwsFormatException,
    );
    expect(
      () => settingsSnapshot(SettingsGroup.ethernet, {
        ...raw,
        'subnet': '255.0.255.0',
      }),
      throwsFormatException,
    );
  });
  test('Wi-Fi off requires actual Ethernet readiness and never exports that observation', () {
    final raw = <String, dynamic>{
      'station_enabled': false,
      'ap_enabled': false,
      'station_ssid': 'Fixture',
    };
    expect(
      () => settingsSnapshot(SettingsGroup.network, raw),
      throwsFormatException,
    );
    final saved = settingsSnapshot(SettingsGroup.network, {
      ...raw,
      'ethernet_ready': true,
    });
    expect(saved.containsKey('ethernet_ready'), false);
    expect(
      () => settingsSnapshot(SettingsGroup.network, saved),
      throwsFormatException,
    );
  });
  test('Retired draft cleanup cannot veto a separately owned verified identity handoff', () {
    final workspace = DeviceSettingsWorkspace();
    workspace.captureIdentity('ledboard', 'Board', 'renamed', 'Board');
    var notifications = 0;
    workspace.addListener(() => ++notifications);
    workspace.dispose();
    expect(workspace.discardIdentity, returnsNormally);
    expect(notifications, 0);
  });
  test('Only public edits restore after a fresh read, secrets and pending writes never do', () async {
    final cache = DeviceSettingsWorkspace();
    final api = PakarApi(
      client: MockClient(
        (request) async => http.Response(
          jsonEncode(
            request.url.path == '/api/status'
                ? peerFixture().json
                : simulatedCloud,
          ),
          200,
        ),
      ),
    );
    final session = DeviceSettingsSession(
      api,
      authorized: () => true,
      workspace: cache,
    );
    await session.load(SettingsGroup.cloud);
    session.set(SettingsGroup.cloud, 'mqtt_user', 'my-draft');
    session.set(SettingsGroup.cloud, 'mqtt_port', 'unfinished');
    session.websocketToken.mode = SecretMode.replace;
    session.websocketToken.value = 'secret-do-not-persist';
    session.mqttPassword.mode = SecretMode.clear;
    session.pending = true;
    session.changed();
    final encoded = jsonEncode(cache.snapshot());
    expect(encoded, isNot(contains('secret-do-not-persist')));
    expect(encoded, isNot(contains('password')));
    expect(encoded, isNot(contains('token')));
    expect(encoded, isNot(contains('pending')));
    session.dispose();
    final restored = DeviceSettingsWorkspace(
      saved: jsonDecode(encoded) as Map<String, dynamic>,
    );
    final next = DeviceSettingsSession(
      api,
      authorized: () => true,
      workspace: restored,
    );
    expect(
      next.drafts,
      isEmpty,
    ); // Cache cannot fabricate a live configuration.
    expect(next.pending, false);
    await next.load(SettingsGroup.cloud);
    expect(next.drafts[SettingsGroup.cloud]!.value['mqtt_user'], 'my-draft');
    expect(next.drafts[SettingsGroup.cloud]!.value['mqtt_port'], 'unfinished');
    expect(next.drafts[SettingsGroup.cloud]!.conflict, false);
    expect(next.websocketToken.mode, SecretMode.keep);
    expect(next.mqttPassword.mode, SecretMode.keep);
    expect(next.websocketToken.value, '');
    expect(() => next.write(SettingsGroup.cloud), throwsFormatException);
    next.dispose();
    cache.dispose();
    restored.dispose();
    await api.close();
  });
  test('Restored public edits retain their original conflict baseline through repeated relaunches', () {
    final workspace = DeviceSettingsWorkspace();
    final original = settingsSnapshot(SettingsGroup.cloud, simulatedCloud);
    final edited = SettingsDraft(original)..value['mqtt_user'] = 'draft';
    workspace.capture(SettingsGroup.cloud, edited);
    var saved = workspace.snapshot();
    for (var pass = 0; pass < 3; pass++) {
      final restored = DeviceSettingsWorkspace(
        saved: jsonDecode(jsonEncode(saved)) as Map<String, dynamic>,
      );
      final draft = restored.restore(SettingsGroup.cloud, {
        ...original,
        'mqtt_user': 'another-client',
      });
      expect(draft.value['mqtt_user'], 'draft');
      expect(draft.saved['mqtt_user'], simulatedCloud['mqtt_user']);
      expect(draft.conflict, true);
      restored.capture(SettingsGroup.cloud, draft);
      saved = restored.snapshot();
      restored.dispose();
    }
    workspace.discard(SettingsGroup.cloud);
    expect(workspace.restore(SettingsGroup.cloud, original).dirty, false);
    workspace.dispose();
  });
  test('Required catalog fields, UTF-8 limits and real hardware constraints fail closed', () {
    expect(
      () => settingsSnapshot(SettingsGroup.network, {}),
      throwsFormatException,
    );
    expect(
      () => settingsSnapshot(SettingsGroup.cloud, {}),
      throwsFormatException,
    );
    for (final patch in [
      {'station_ssid': 'é' * 17},
      {'station_enabled': false, 'ap_enabled': false},
      {'station_enabled': 'true'},
    ]) {
      expect(
        () => settingsSnapshot(SettingsGroup.network, {
          ...simulatedNetwork,
          ...patch,
        }),
        throwsFormatException,
      );
    }
    for (final patch in [
      {'mqtt_port': 8883.0},
      {'mqtt_port': 0},
      {'telemetry_seconds': 3601},
      {'mqtt_host': 'https://example.invalid'},
      {'mqtt_base_topic': 'pakar/#'},
      {'websocket_path': 'path'},
      {'mqtt_user': 'quote"'},
      {'websocket_path': r'/back\slash'},
    ]) {
      expect(
        () => settingsSnapshot(SettingsGroup.cloud, {
          ...simulatedCloud,
          ...patch,
        }),
        throwsFormatException,
      );
    }
  });
  test('Actual Draft.observe preserves public and secret-only edits and reports conflicts', () {
    final draft = SettingsDraft(
      settingsSnapshot(SettingsGroup.cloud, simulatedCloud),
    );
    draft.value['mqtt_user'] = 'edited';
    draft.observe(
      settingsSnapshot(SettingsGroup.cloud, {
        ...simulatedCloud,
        'mqtt_user': 'remote',
      }),
    );
    expect(draft.value['mqtt_user'], 'edited');
    expect(draft.conflict, true);
    final secretOnly = SettingsDraft(
      settingsSnapshot(SettingsGroup.cloud, simulatedCloud),
    );
    secretOnly.observe(
      settingsSnapshot(SettingsGroup.cloud, {
        ...simulatedCloud,
        'mqtt_host': 'new.invalid',
      }),
      secretsDirty: true,
    );
    expect(secretOnly.value['mqtt_host'], simulatedCloud['mqtt_host']);
    expect(secretOnly.conflict, true);
  });
  test('Optional addressing is absent or complete, strictly typed and validated without defaults', () {
    final absent = settingsSnapshot(SettingsGroup.network, simulatedNetwork);
    expect(absent.keys, unorderedEquals(simulatedNetwork.keys));
    const addressing = <String, Object>{
      'dhcp': false,
      'address': '192.168.4.20',
      'gateway': '192.168.4.1',
      'subnet': '255.255.255.0',
      'dns1': '1.1.1.1',
      'dns2': '',
    };
    final complete = settingsSnapshot(SettingsGroup.network, {
      ...simulatedNetwork,
      ...addressing,
    });
    for (final entry in addressing.entries) {
      expect(complete[entry.key], entry.value);
    }
    expect(
      settingsSnapshot(SettingsGroup.network, {
        ...simulatedNetwork,
        ...addressing,
        'dhcp': true,
        'address': '0.0.0.0',
        'gateway': '0.0.0.0',
        'subnet': '0.0.0.0',
      })['dhcp'],
      true,
    );
    for (final field in addressing.keys) {
      final incomplete = {...simulatedNetwork, ...addressing}..remove(field);
      expect(
        () => settingsSnapshot(SettingsGroup.network, incomplete),
        throwsFormatException,
      );
    }
    for (final patch in <Map<String, Object?>>[
      {'dhcp': 'false'},
      {'dhcp': null},
      {'address': 123},
      {'address': '999.1.1.1'},
      {'address': '192.168.4.0'},
      {'address': '192.168.4.255'},
      {'gateway': '192.168.5.1'},
      {'gateway': '192.168.4.20'},
      {'subnet': '255.0.255.0'},
      {'subnet': '255.255.255.254'},
      {'dns1': '224.0.0.1'},
      {'dns2': 'not-an-address'},
    ]) {
      expect(
        () => settingsSnapshot(SettingsGroup.network, {
          ...simulatedNetwork,
          ...addressing,
          ...patch,
        }),
        throwsFormatException,
      );
    }
    final write = SettingsWrite(SettingsGroup.network, {}, complete);
    write.verifyReadback(Map.of(complete));
    expect(() => write.verifyReadback(absent), throwsFormatException);
    expect(
      () => write.verifyReadback({...complete, 'dhcp': true}),
      throwsFormatException,
    );
  });
  for (final behavior in [
    'success',
    'negative',
    'mismatch',
    'uncertain',
    'external',
  ]) {
    test(
      'Settings write $behavior: one POST, positive ACK and exact readback, no replay',
      () async {
        var saved = Map<String, dynamic>.from(simulatedCloud), posts = 0;
        Map<String, String>? posted;
        final api = PakarApi(
          client: MockClient((request) async {
            if (request.url.path.endsWith('/status')) {
              return http.Response(jsonEncode(peerFixture().json), 200);
            }
            if (request.method == 'POST') {
              ++posts;
              expect(
                request.url.hasQuery,
                false,
                reason: 'Secrets never in request URL',
              );
              posted = request.bodyFields;
              if (behavior == 'uncertain') {
                throw const SocketException('lost ACK');
              }
              if (behavior == 'negative') {
                return http.Response('{"updated":false}', 200);
              }
              if (behavior != 'mismatch') {
                for (final entry in request.bodyFields.entries) {
                  if (entry.key == 'mqtt_password') {
                    saved['mqtt_password_set'] = entry.value.isNotEmpty;
                  } else if (entry.key == 'websocket_token') {
                    saved['websocket_token_set'] = entry.value.isNotEmpty;
                  } else {
                    final type = SettingsGroup.cloud.fields[entry.key];
                    saved[entry.key] = type == 'boolean'
                        ? entry.value == 'true'
                        : type == 'port' || type == 'interval'
                        ? int.parse(entry.value)
                        : entry.value;
                  }
                }
              }
              return http.Response('{"updated":true}', 200);
            }
            return http.Response(jsonEncode(saved), 200);
          }),
        );
        await api.status();
        final session = DeviceSettingsSession(api, authorized: () => true);
        await session.load(SettingsGroup.cloud);
        session.set(SettingsGroup.cloud, 'mqtt_user', 'new-user');
        session.mqttPassword.mode = SecretMode.clear;
        session.websocketToken.mode = SecretMode.replace;
        session.websocketToken.value = 'new-secret';
        if (behavior == 'external') saved['mqtt_user'] = 'remote-change';
        expect(await session.save(SettingsGroup.cloud), behavior == 'success');
        expect(posts, behavior == 'external' ? 0 : 1);
        if (posted != null) {
          expect(posted!['mqtt_password'], '');
          expect(posted!['websocket_token'], 'new-secret');
          expect(posted!.containsKey('mqtt_password_set'), false);
        }
        if (behavior == 'success') {
          expect(session.drafts[SettingsGroup.cloud]!.dirty, false);
          expect(session.websocketToken.value, '');
          expect(session.message, contains('verified'));
        } else {
          expect(
            session.drafts[SettingsGroup.cloud]!.value['mqtt_user'],
            'new-user',
          );
          expect(session.message, contains('not retried'));
        }
        session.dispose();
        await api.close();
      },
    );
  }
  test('Keep omits secret keys; Replace/Clear and network open choice have distinct wire semantics', () async {
    final api = PakarApi(
      client: MockClient(
        (request) async => http.Response(
          jsonEncode(
            request.url.path.endsWith('/network')
                ? simulatedNetwork
                : request.url.path.endsWith('/cloud')
                ? simulatedCloud
                : peerFixture().json,
          ),
          200,
        ),
      ),
    );
    final session = DeviceSettingsSession(api, authorized: () => true);
    await session.load(SettingsGroup.network);
    await session.load(SettingsGroup.cloud);
    expect(
      session.drafts.keys,
      containsAll([SettingsGroup.network, SettingsGroup.cloud]),
      reason: 'Optional catalog fields must not silently prevent loading either settings group',
    );
    final keep = session.write(SettingsGroup.cloud);
    expect(keep.form.containsKey('mqtt_password'), false);
    expect(keep.form.containsKey('websocket_token'), false);
    session.mqttPassword.mode = SecretMode.replace;
    expect(() => session.write(SettingsGroup.cloud), throwsFormatException);
    expect(
      session.write(SettingsGroup.network).form.containsKey('password'),
      false,
    );
    session.set(SettingsGroup.network, 'station_ssid', 'Changed fixture');
    expect(() => session.write(SettingsGroup.network), throwsFormatException);
    session.openNetwork = true;
    expect(session.write(SettingsGroup.network).form['password'], '');
    session.openNetwork = false;
    session.wifiPassword = 'fixture-password';
    expect(
      session.write(SettingsGroup.network).form['password'],
      'fixture-password',
    );
    session.invalidate();
    expect(session.drafts, isEmpty);
    expect(session.wifiPassword, '');
    expect(session.mqttPassword.mode, SecretMode.keep);
    session.dispose();
    await api.close();
  });
  test(
    'Late read cannot overwrite a newer read, disposed owner or write epoch',
    () async {
      final replies = <Completer<http.Response>>[];
      final api = PakarApi(
        client: MockClient((request) {
          if (request.url.path == '/api/status') {
            return Future.value(
              http.Response(jsonEncode(peerFixture().json), 200),
            );
          }
          expect(request.url.path, SettingsGroup.cloud.path);
          final reply = Completer<http.Response>();
          replies.add(reply);
          return reply.future;
        }),
      );
      await api.status();
      final session = DeviceSettingsSession(api, authorized: () => true);
      final first = session.load(SettingsGroup.cloud),
          second = session.load(SettingsGroup.cloud);
      await Future<void>.delayed(Duration.zero);
      replies[1].complete(
        http.Response(
          jsonEncode({...simulatedCloud, 'mqtt_user': 'newest'}),
          200,
        ),
      );
      await second;
      replies[0].complete(http.Response(jsonEncode(simulatedCloud), 200));
      await first;
      expect(session.drafts[SettingsGroup.cloud]!.value['mqtt_user'], 'newest');
      final obsolete = session.load(SettingsGroup.cloud);
      await Future<void>.delayed(Duration.zero);
      session.dispose();
      replies[2].complete(http.Response(jsonEncode(simulatedCloud), 200));
      await obsolete;
      expect(session.drafts, isEmpty);
      await api.close();
    },
  );
  test('Changed owner or lost ACK cannot dispatch a previously authorized settings POST', () async {
    var authorized = true, posts = 0;
    final preflight = Completer<http.Response>();
    final api = PakarApi(
      client: MockClient((request) async {
        if (request.method == 'POST') {
          posts++;
          return http.Response('{}', 200);
        }
        return preflight.future;
      }),
    );
    final action = api.postJson('/api/cloud', {
      'websocket_token': 'private',
    }, authorize: () => authorized);
    final check = expectLater(action, throwsA(isA<StaleConnection>()));
    authorized = false;
    preflight.complete(http.Response(jsonEncode(peerFixture().json), 200));
    await check;
    expect(posts, 0);
    await api.close();
  });
}
