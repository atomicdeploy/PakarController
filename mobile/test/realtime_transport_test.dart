import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/models.dart';

import 'peer_fixture.dart';

// Real loopback transport, separate from widget binding's HTTP interception.
void main() {
  test('An older HTTP read cannot undo newer same-boot WS state', () {
    DeviceStatus state(int clock, String boot) => DeviceStatus({
      'board_id': 'esp32s3-fixture',
      'boot_id': boot,
      'command_clock_ms': clock,
    });
    expect(state(1, 'a').olderThan(state(2, 'a')), true);
    expect(state(2, 'a').olderThan(state(1, 'a')), false);
    expect(state(1, 'b').olderThan(state(2, 'a')), false);
  });
  for (final scenario in ['websocket', 'lost-ack', 'http-only']) {
    test(
      'Realtime $scenario returns correlated state without a second mutation',
      () async {
        final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
        final api = PakarApi(baseUrl: 'http://127.0.0.1:${server.port}');
        final base = peerFixture();
        var cells = List.filled(80, 'white'), writes = 0, posts = 0, gets = 0;
        final packets = <String, String>{};
        Map<String, dynamic> status() => {
          ...base.json,
          'board_id': 'esp32s3-fixture',
          'boot_id': 'a' * 32,
          'http_peer_fencing': true,
          'command_clock_ms': 1000,
          'cells': cells,
          'capabilities': {
            ...(base.json['capabilities'] as Map<String, dynamic>),
            'acknowledged_features': [...base.peer.features, 'command-ack'],
          },
        };
        Map<String, dynamic> apply(String packet) {
          final q = jsonDecode(packet) as Map<String, dynamic>,
              id = q['request_id'] as String;
          final cached = packets.containsKey(id);
          if (cached) {
            expect(packets[id], packet);
          } else {
            packets[id] = packet;
            writes++;
            cells = List.of(cells);
            for (final id in (q['ids'] as String).split(',').map(int.parse)) {
              cells[id - 1] = q['state'] as String;
            }
          }
          return {
            'type': 'command-result',
            'request_id': id,
            'board_id': q['board_id'],
            'boot_id': q['boot_id'],
            'accepted': true,
            'count': 2,
            'cached': cached,
            'status': status(),
          };
        }

        final sockets = <WebSocket>[];
        final listener = server.listen((request) async {
          if (request.uri.path == '/ws') {
            final socket = await WebSocketTransformer.upgrade(request);
            sockets.add(socket);
            socket.listen((message) {
              final q = jsonDecode(message as String) as Map<String, dynamic>;
              if (q['type'] == 'command') {
                final reply = apply(message);
                if (scenario != 'lost-ack') socket.add(jsonEncode(reply));
              } else {
                socket.add(jsonEncode(status()));
              }
            });
          } else {
            request.response.headers.contentType = ContentType.json;
            if (request.method == 'POST') {
              posts++;
              expect(request.uri.path, '/api/command');
              final body = await utf8.decoder.bind(request).join();
              final packet = Uri.splitQueryString(body)['packet']!;
              request.response.write(jsonEncode(apply(packet)));
            } else {
              gets++;
              request.response.write(jsonEncode(status()));
            }
            await request.response.close();
          }
        });
        StreamSubscription<DeviceStatus>? live;
        try {
          await api.status();
          if (scenario != 'http-only') {
            final ready = Completer<void>();
            live = api.liveStatus().listen((_) {
              if (!ready.isCompleted) ready.complete();
            });
            await ready.future.timeout(const Duration(seconds: 3));
          }
          final actual = await api.fastCommand('/api/cells', {
            'ids': '1,80',
            'state': 'red',
            'effect': 'immediate',
          }, authorize: () => true);
          expect(actual!.cells[0], 'red');
          expect(actual.cells[79], 'red');
          expect(writes, 1);
          expect(gets, 1);
          expect(posts, scenario == 'websocket' ? 0 : 1);
        } finally {
          await api.close();
          await live?.cancel();
          for (final socket in sockets) {
            await socket.close();
          }
          await listener.cancel();
          await server.close(force: true);
        }
      },
    );
  }
}
