import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';

import 'peer_fixture.dart';

// Real network stack, not the widget binding's intercepted HTTP client.
void main() {
  for (final scenario in [
    'websocket',
    'http-only',
    'lost-reply',
    'wrong-frame',
    'wrong-boot',
    'rejected',
    'background',
  ]) {
    test(
      'Atomic pixel command $scenario: complete readback and one intent',
      () async {
        final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
        final api = PakarApi(baseUrl: 'http://127.0.0.1:${server.port}');
        final sockets = <WebSocket>[];
        final requests = <String>[];
        final accepted = <String>{};
        var commits = 0, reads = 0;
        Uint8List frame = Uint8List(1440);
        Map<String, dynamic> status() => {
          ...peerFixture(
            features: ['command-ack', 'pixel-diagnostic', 'leds', 'table'],
          ).json,
          'board_id': 'fixture',
          'boot_id': 'a' * 32,
          'command_clock_ms': 1000,
          'pixels': 480,
          'effect': 'none',
          'led_output': {'enabled': true, 'mode': 'effects'},
          'pixel_frame': {
            'revision': commits + 1,
            'pending': false,
            'rgb': frame.map((v) => v.toRadixString(16).padLeft(2, '0')).join(),
          },
        };
        Map<String, dynamic> execute(String packet) {
          final q = jsonDecode(packet) as Map;
          expect(q['command'], 'pixel-diagnostic');
          expect(q['index'], isA<int>());
          expect(q['count'], isA<int>());
          expect(q['rgb'], isA<int>());
          expect(q['off'], isA<bool>());
          final id = q['request_id'] as String;
          requests.add(id);
          if (accepted.add(id) && scenario != 'rejected') {
            commits++;
            frame = PixelProbe(
              index: q['index'],
              group: q['count'] == 6,
              rgb: q['rgb'],
            ).frame(off: q['off']);
            if (scenario == 'wrong-frame') frame[0] ^= 1;
          }
          return {
            'type': 'command-result',
            'request_id': id,
            'board_id': 'fixture',
            'boot_id': 'a' * 32,
            'accepted': scenario != 'rejected',
            'error': 'command_rejected',
            'status': {
              ...status(),
              if (scenario == 'wrong-boot') 'boot_id': 'b' * 32,
            },
          };
        }

        final listener = server.listen((request) async {
          if (request.uri.path == '/ws') {
            final socket = await WebSocketTransformer.upgrade(request);
            sockets.add(socket);
            socket.listen((message) {
              expect(message, isA<String>(), reason: 'No raw streaming chunks');
              final q = jsonDecode(message as String) as Map;
              if (q['type'] == 'command') {
                final reply = execute(message);
                if (scenario != 'lost-reply') socket.add(jsonEncode(reply));
              } else {
                socket.add(jsonEncode(status()));
              }
            });
          } else {
            request.response.headers.contentType = ContentType.json;
            if (request.method == 'POST') {
              expect(request.uri.path, '/api/command');
              final form = Uri.splitQueryString(
                await utf8.decoder.bind(request).join(),
              );
              request.response.write(jsonEncode(execute(form['packet']!)));
            } else {
              reads++;
              expect(
                request.uri.query,
                isEmpty,
                reason: 'No per-step full-frame HTTP polling',
              );
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
          if (scenario == 'background') api.appForeground = false;
          if ([
            'wrong-frame',
            'wrong-boot',
            'rejected',
            'background',
          ].contains(scenario)) {
            await expectLater(
              api.pixelDiagnostic(
                const PixelProbe(index: 479),
                authorize: () => true,
              ),
              throwsA(isA<Exception>()),
            );
            expect(
              commits,
              ['rejected', 'background'].contains(scenario) ? 0 : 1,
            );
          } else {
            for (final probe in [
              const PixelProbe(index: 479, rgb: 0x1234ab),
              const PixelProbe(index: 479, group: true, rgb: 0xff0000),
            ]) {
              final actual = await api.pixelDiagnostic(
                probe,
                authorize: () => true,
              );
              expect(matchesDiagnosticFrame(actual, probe.frame()), true);
            }
            final off = await api.pixelDiagnostic(
              const PixelProbe(index: 479),
              off: true,
              authorize: () => true,
            );
            expect(matchesDiagnosticFrame(off, Uint8List(1440)), true);
            expect(commits, 3);
            expect(requests.length, scenario == 'lost-reply' ? 6 : 3);
            expect(requests.toSet().length, 3);
          }
          expect(reads, 1);
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
