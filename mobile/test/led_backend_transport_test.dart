import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/models.dart';

import 'peer_fixture.dart';

void main() {
  for (final failure in ['', 'driver', 'storage']) {
    final failed = failure.isNotEmpty;
    test(
      'WebSocket backend handoff ${failed ? 'reports $failure failure' : 'waits for active state'} without replay',
      () async {
        final state = {
          ...peerFixture(features: ['leds', 'table', 'command-ack']).json,
          'board_id': 'ledcontroller-fixture',
          'boot_id': 'a' * 32,
          'command_clock_ms': 1000,
          'capabilities': {
            ...peerFixture(features: ['leds', 'table', 'command-ack'])
                    .json['capabilities']
                as Map,
            'led_output': {
              'backend': 'gpio-static',
              'rmt': false,
              'dma': false,
              'available_backends': ['gpio-static', 'rmt-dma'],
              'runtime_switch': true,
            },
          },
          'led_output': {
            'enabled': true,
            'mode': 'effects',
            'backend_saved': true,
            'driver': {
              'backend': 'gpio-static',
              'requested_backend': 'gpio-static',
              'available_backends': ['gpio-static', 'rmt-dma'],
              'ready': true,
              'switch_pending': false,
              'switch_error': 0,
              'error_code': 0,
            },
          },
        };
        final driver = (state['led_output'] as Map)['driver'] as Map;
        final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
        final api = PakarApi(baseUrl: 'http://127.0.0.1:${server.port}')
          ..commandHttpFallback = false;
        final sockets = <WebSocket>[];
        StreamSubscription<DeviceStatus>? live;
        Timer? completion;
        var mutations = 0, httpMutations = 0;
        final listener = server.listen((request) async {
          if (request.uri.path == '/ws') {
            final socket = await WebSocketTransformer.upgrade(request);
            sockets.add(socket);
            socket.add(jsonEncode(state));
            socket.listen((raw) {
              final packet = jsonDecode(raw as String) as Map;
              if (packet['type'] == 'status') socket.add(jsonEncode(state));
              if (packet['type'] == 'command') {
                mutations++;
                expect(packet['command'], 'led-output');
                driver['requested_backend'] = packet['backend'];
                driver['switch_pending'] = true;
                socket.add(
                  jsonEncode({
                    'type': 'command-result',
                    'accepted': true,
                    'request_id': packet['request_id'],
                    'board_id': state['board_id'],
                    'boot_id': state['boot_id'],
                    'status': state,
                  }),
                );
                completion = Timer(const Duration(milliseconds: 150), () {
                  driver['switch_pending'] = false;
                  driver['switch_error'] = failure == 'driver' ? 261 : 0;
                  if (failure == 'storage') {
                    (state['led_output'] as Map)['backend_settings_error'] =
                        'backend_commit_failed';
                  }
                  if (!failed) driver['backend'] = 'rmt-dma';
                  socket.add(jsonEncode(state));
                });
              }
            });
          } else {
            if (request.method == 'POST' &&
                request.uri.path == '/api/command') {
              httpMutations++;
            }
            request.response.headers.contentType = ContentType.json;
            request.response.write(jsonEncode(state));
            await request.response.close();
          }
        });
        try {
          await api.status();
          final ready = Completer<void>();
          live = api.liveStatus().listen((_) {
            if (!ready.isCompleted) ready.complete();
          });
          await ready.future.timeout(const Duration(seconds: 2));
          final result = api.fastCommand('/api/led/output', {
            'backend': 'rmt-dma',
          }, authorize: () => true);
          if (failed) {
            await expectLater(
              result,
              throwsA(
                isA<FormatException>().having(
                  (error) => error.message,
                  'failure',
                  contains(
                    'switch failed (${failure == 'driver' ? '261' : 'backend_commit_failed'})',
                  ),
                ),
              ),
            );
          } else {
            final applied = await result;
            expect(applied?.json['led_output']['driver']['backend'], 'rmt-dma');
            expect(
              applied?.json['led_output']['driver']['switch_pending'],
              false,
            );
          }
          expect(mutations, 1);
          expect(httpMutations, 0);
        } finally {
          completion?.cancel();
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
