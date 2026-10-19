import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/bridge.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/peer_identity.dart';

import 'peer_fixture.dart';

DeviceStatus board({int brightness = 5}) => DeviceStatus({
  ...peerFixture().json,
  'board_id': 'board-one',
  'boot_id': 'boot-one',
  'brightness': brightness,
  'password': 'never-publish',
  'led_output': {
    'enabled': true,
    'mode': 'static',
    'password': 'never-publish',
    'diagnostic': {
      'revision': 3,
      'index': 10,
      'count': 1,
      'rgb': 0xff0000,
      'active': true,
      'off': false,
      'url': 'never-publish',
    },
  },
});

void main() {
  test(
    'Event journal is bounded, immutable from input and signals lost cursors',
    () {
      final journal = BridgeEventJournal();
      final cells = ['red'];
      journal.add({'cells': cells});
      cells[0] = 'white';
      expect((journal.since(0)['events'] as List).single['cells'], ['red']);
      for (var i = 0; i < 70; i++) {
        journal.add({'type': 'telemetry', 'value': i});
      }
      expect(journal.since(0)['events'], hasLength(64));
      expect(journal.since(0)['gap'], true);
      expect(journal.since(70)['gap'], false);
      expect(journal.since(70)['events'], hasLength(1));
      expect(journal.since(100)['gap'], true);
      expect(() => journal.since(-1), throwsFormatException);
    },
  );

  test('HTTP commands fan out one correlated result to WS; live board changes bypass heartbeat', () async {
    DeviceStatus observed = board();
    var writes = 0;
    final api = PakarApi(
      client: MockClient((r) async {
        if (r.method == 'POST') writes++;
        return http.Response(jsonEncode(observed.json), 200);
      }),
    );
    await api.status();
    final bridge = AndroidBridge(api, () => observed);
    final client = HttpClient();
    WebSocket? socket;
    StreamSubscription<dynamic>? subscription;
    final frames = <Map<String, dynamic>>[];
    final resultArrived = Completer<void>(), liveArrived = Completer<void>();
    try {
      await bridge.start(BridgeOptions(commands: true, telemetrySeconds: 3600));
      final base = 'http://127.0.0.1:${bridge.server!.port}';
      final headers = {
        ...PeerIdentity.current.headers,
        'Authorization': 'Bearer ${bridge.token}',
      };
      socket = await WebSocket.connect(
        '${base.replaceFirst('http:', 'ws:')}/ws',
        headers: headers,
      );
      subscription = socket.listen((raw) {
        final value = jsonDecode(raw as String) as Map<String, dynamic>;
        frames.add(value);
        if (value['type'] == 'command-result' && !resultArrived.isCompleted) {
          resultArrived.complete();
        }
        if ((value['board'] as Map?)?['brightness'] == 91 &&
            !liveArrived.isCompleted) {
          liveArrived.complete();
        }
      });
      Future<Map<String, dynamic>> exchange(
        String path, [
        Map<String, dynamic>? body,
      ]) async {
        final r = body == null
            ? await client.getUrl(Uri.parse('$base$path'))
            : await client.postUrl(Uri.parse('$base$path'));
        headers.forEach(r.headers.set);
        if (body != null) r.write(jsonEncode(body));
        final response = await r.close();
        expect(response.statusCode, 200);
        return jsonDecode(await utf8.decoder.bind(response).join());
      }

      final request = {
        'id': 'one',
        'bridge_session': bridge.admission!.session,
        'board_id': 'board-one',
        'boot_id': 'boot-one',
        'expires_at_ms': DateTime.now().millisecondsSinceEpoch + 30000,
        'type': 'command',
        'path': '/api/brightness',
        'values': {'value': '90'},
      };
      final reply = await exchange('/api/request', request);
      await resultArrived.future.timeout(const Duration(seconds: 2));
      expect(reply['accepted'], true);
      expect(reply['request_id'], 'one');
      expect(reply['board_id'], 'board-one');
      expect(reply['boot_id'], 'boot-one');
      expect(frames.where((f) => f['type'] == 'command-result'), hasLength(1));
      expect(frames.firstWhere((f) => f['type'] == 'command-result'), reply);
      expect(writes, 1);
      final duplicate = await exchange('/api/request', request);
      expect(duplicate['accepted'], false);
      expect(writes, 1);

      observed = board(brightness: 91);
      await api.status();
      await liveArrived.future.timeout(const Duration(seconds: 2));
      final cursor = bridge.journal.cursor;
      // Re-reading identical status does not invent another changed event.
      await api.status();
      await Future<void>.delayed(const Duration(milliseconds: 20));
      expect(bridge.journal.cursor, cursor);
      final events = await exchange(
        '/api/events?after=0&bridge_session=${bridge.admission!.session}',
      );
      expect(events['gap'], false);
      expect(events['board_id'], 'board-one');
      expect(events['cursor'], cursor);
      expect(jsonEncode(events), isNot(contains('never-publish')));
      expect((events['events'] as List).last['board']['cells'], hasLength(80));
      expect(
        (events['events'] as List)
            .last['board']['led_output']['diagnostic']['index'],
        10,
      );
      await bridge.stop();
      observed = board(brightness: 92);
      await api.status();
      await Future<void>.delayed(const Duration(milliseconds: 20));
      expect(bridge.journal.cursor, cursor);
    } finally {
      await subscription?.cancel();
      await socket?.close();
      client.close(force: true);
      await bridge.stop();
      bridge.dispose();
      await api.close();
    }
  });
}
