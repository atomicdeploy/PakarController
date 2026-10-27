import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';

import 'pixel_diagnostic_advanced_fixture.dart';

// Real loopback WS/HTTP, intentionally separate from the widget HTTP override.
void main() {
  for (final scenario in [
    'websocket',
    'http-only',
    'pending',
    'superseded',
    'rejected',
    'old-firmware',
  ]) {
    test(
      'Advanced diagnostic $scenario verifies semantic publication and sends one intent',
      () async {
        final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
        final api = PakarApi(baseUrl: 'http://127.0.0.1:${server.port}');
        final sockets = <WebSocket>[];
        var commits = 0, reads = 0, revision = 0, pending = false;
        var closing = false;
        var probe = const PixelProbe();
        Map<String, dynamic> status() {
          final value = advancedDiagnosticFixture(
            probe: probe,
            revision: revision,
            pending: pending,
          ).json;
          if (scenario == 'old-firmware') {
            (value['capabilities']['acknowledged_features'] as List)
                .removeWhere(
                  (v) =>
                      v == 'pixel-diagnostic-effects' ||
                      v == 'pixel-diagnostic-overlay',
                );
          }
          return value;
        }

        Map<String, dynamic> execute(Map<String, dynamic> q) {
          expect(q['command'], 'pixel-diagnostic');
          for (final key in [
            'index',
            'count',
            'rgb',
            'period_ms',
            'duty_percent',
            'min_brightness',
            'max_brightness',
          ]) {
            expect(q[key], isA<int>());
          }
          expect(q['enabled'], isA<bool>());
          expect(q['off'], isA<bool>());
          commits++;
          revision++;
          probe = PixelProbe(
            index: q['index'] as int,
            group: q['count'] == 6,
            rgb: q['rgb'] as int,
            effect: q['effect'] as String,
            overlay: q['composite'] == 'overlay',
            periodMs: q['period_ms'] as int,
            dutyPercent: q['duty_percent'] as int,
            minBrightness: q['min_brightness'] as int,
            maxBrightness: q['max_brightness'] as int,
            enabled: q['enabled'] == true,
          );
          pending = scenario == 'pending';
          if (scenario == 'superseded') probe = probe.copyWith(index: 0);
          return {
            'type': 'command-result',
            'request_id': q['request_id'],
            'board_id': 'fixture',
            'boot_id': 'a' * 32,
            'accepted': scenario != 'rejected',
            'error': 'diagnostic_busy',
            'status': status(),
          };
        }

        final listener = server.listen((request) async {
          if (request.uri.path == '/ws') {
            final socket = await WebSocketTransformer.upgrade(request);
            sockets.add(socket);
            socket.listen((message) {
              if (closing || socket.readyState != WebSocket.open) return;
              expect(
                message,
                isA<String>(),
                reason: 'No client-generated animation frames',
              );
              final q = jsonDecode(message as String) as Map<String, dynamic>;
              if (q['type'] == 'command') {
                socket.add(jsonEncode(execute(q)));
              } else {
                if (commits > 0) pending = false;
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
              request.response.write(
                jsonEncode(
                  execute(jsonDecode(form['packet']!) as Map<String, dynamic>),
                ),
              );
            } else {
              reads++;
              if (commits > 0) pending = false;
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
          const wanted = PixelProbe(
            index: 479,
            group: true,
            rgb: 0x123456,
            effect: 'rainbow',
            overlay: true,
            periodMs: 450,
            dutyPercent: 30,
            minBrightness: 15,
            maxBrightness: 220,
          );
          if (['superseded', 'rejected', 'old-firmware'].contains(scenario)) {
            await expectLater(
              api.pixelDiagnostic(wanted, authorize: () => true),
              throwsA(isA<Exception>()),
            );
            expect(commits, scenario == 'old-firmware' ? 0 : 1);
          } else {
            final actual = await api.pixelDiagnostic(
              wanted,
              authorize: () => true,
            );
            expect(
              matchesDiagnosticObservation(actual, wanted, afterRevision: 0),
              true,
            );
            expect(
              actual.effect,
              'rainbow',
              reason: 'Base scene is not overwritten',
            );
            expect(commits, 1);
            expect(reads, 1);
          }
        } finally {
          closing = true;
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
