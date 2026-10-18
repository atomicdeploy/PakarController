import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';

import 'peer_fixture.dart';

// Real transport tests must stay separate from TestWidgetsFlutterBinding.
void main() {
  test(
    'Explicit live same-board diagnostic readback, no optical acceptance',
    () async {
      final origin = Platform.environment['PAKAR_DIAGNOSTIC_LIVE_ORIGIN'];
      if (origin == null) return;
      final api = PakarApi(baseUrl: origin);
      StreamSubscription<DeviceStatus>? live;
      try {
        final before = await api.status();
        expect(
          before.json['board_id'],
          Platform.environment['PAKAR_DIAGNOSTIC_BOARD'],
        );
        expect(
          before.json['boot_id'],
          Platform.environment['PAKAR_DIAGNOSTIC_BOOT'],
        );
        expect(before.mode, 'cell');
        expect(before.effect, 'none');
        expect(
          before.cells,
          List.filled(80, 'off'),
          reason: 'Do not overwrite an active user scene',
        );
        final ready = Completer<void>();
        live = api.liveStatus().listen((_) {
          if (!ready.isCompleted) ready.complete();
        });
        await ready.future.timeout(const Duration(seconds: 8));
        final owner = api.connectionRevision;
        for (final probe in [
          const PixelProbe(),
          const PixelProbe(index: 5, rgb: 0xff0000),
          const PixelProbe(index: 6, rgb: 0x0000ff),
          const PixelProbe(index: 479, rgb: 0x00ff00),
          const PixelProbe(index: 479, group: true, rgb: 0xff0000),
        ]) {
          final actual = await api.pixelDiagnostic(
            probe,
            authorize: () => owner == api.connectionRevision,
          );
          expect(matchesDiagnosticFrame(actual, probe.frame()), true);
          expect(actual.json['boot_id'], before.json['boot_id']);
          // Published buffer, not the electrical wire or human visual acceptance.
          // ignore: avoid_print
          print('LIVE ${probe.range} #${probe.hex}: exact RGB readback');
        }
        final off = await api.pixelDiagnostic(
          const PixelProbe(),
          off: true,
          authorize: () => owner == api.connectionRevision,
        );
        expect(matchesDiagnosticFrame(off, Uint8List(1440)), true);
        final restored = await api.fastCommand('/api/led/output', {
          'enabled': 'true',
          'mode': 'static',
        }, authorize: () => owner == api.connectionRevision);
        expect(restored?.ledOutputMode, 'static');
        final finalState = DeviceStatus(
          await api.getJson('/api/status?pixels=1'),
        );
        expect(matchesDiagnosticFrame(finalState, Uint8List(1440)), true);
        // ignore: avoid_print
        print(
          'LIVE all480 off verified; enabled static fallback restored; no AVR operations',
        );
      } finally {
        await api.close();
        await live?.cancel();
      }
    },
    timeout: const Timeout(Duration(minutes: 2)),
    skip: Platform.environment['PAKAR_DIAGNOSTIC_LIVE_ORIGIN'] == null,
  );
  for (final scenario in [
    'exact',
    'delayed-read',
    'corrupt',
    'reboot',
    'busy',
    'no-websocket',
  ]) {
    test('Pixel diagnostic $scenario: one complete frame, no replay', () async {
      final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
      final api = PakarApi(baseUrl: 'http://127.0.0.1:${server.port}');
      final sockets = <WebSocket>[];
      var frame = Uint8List(1440),
          stage = Uint8List(1440),
          mode = 'static',
          boot = 'a' * 32;
      var frameRevision = 1, chunks = 0, commits = 0, writes = 0;
      var servedStale = false;
      Map<String, dynamic> status() => {
        ...peerFixture(features: ['command-ack', 'table', 'leds', 'streaming'])
            .json,
        'board_id': 'fixture',
        'boot_id': boot,
        'command_clock_ms': 1000,
        'pixels': 480,
        'effect': 'none',
        'led_output': {'enabled': true, 'mode': mode},
        'programming_latch': scenario == 'busy',
        'pixel_frame': {
          'revision': frameRevision,
          'rgb': frame.map((v) => v.toRadixString(16).padLeft(2, '0')).join(),
        },
      };
      final listener = server.listen((request) async {
        if (request.uri.path == '/ws') {
          final socket = await WebSocketTransformer.upgrade(request);
          sockets.add(socket);
          socket.listen((message) {
            if (message is List<int>) {
              chunks++;
              final offset = message[6] | message[7] << 8;
              stage.setRange(
                offset * 3,
                (offset + message[8]) * 3,
                message,
                10,
              );
              if (message[9] == 1) {
                commits++;
                frame = Uint8List.fromList(stage);
                frameRevision++;
                if (scenario == 'corrupt') frame[0] ^= 1;
                if (scenario == 'reboot') boot = 'b' * 32;
              }
            } else {
              final q = jsonDecode(message as String) as Map;
              if (q['type'] == 'command') {
                writes++;
                expect(q['command'], 'led-output');
                mode = q['mode'] as String;
                socket.add(
                  jsonEncode({
                    'type': 'command-result',
                    'request_id': q['request_id'],
                    'board_id': q['board_id'],
                    'boot_id': q['boot_id'],
                    'accepted': true,
                    'status': status(),
                  }),
                );
              } else {
                socket.add(jsonEncode(status()));
              }
            }
          });
        } else {
          expect(
            request.method,
            'GET',
            reason: 'Raw frames must not have HTTP mutation fallback',
          );
          request.response.headers.contentType = ContentType.json;
          final snapshot = status();
          if (scenario == 'delayed-read' && commits == 1 && !servedStale) {
            // A slow read may have captured the pre-commit state. Waiting for
            // another read is allowed; repeating a frame is never allowed.
            snapshot['pixel_frame'] = {
              'revision': frameRevision - 1,
              'rgb': '000000' * 480,
            };
            await Future<void>.delayed(const Duration(milliseconds: 2100));
            servedStale = true;
          }
          request.response.write(jsonEncode(snapshot));
          await request.response.close();
        }
      });
      StreamSubscription<DeviceStatus>? live;
      try {
        await api.status();
        if (scenario != 'no-websocket') {
          final ready = Completer<void>();
          live = api.liveStatus().listen((_) {
            if (!ready.isCompleted) ready.complete();
          });
          await ready.future.timeout(const Duration(seconds: 3));
        }
        Future<DeviceStatus> send(PixelProbe probe, {bool off = false}) =>
            api.pixelDiagnostic(probe, off: off, authorize: () => true);
        if (scenario == 'exact') {
          for (final probe in [
            const PixelProbe(rgb: 0xff0000),
            const PixelProbe(index: 479, rgb: 0x1234ab),
            const PixelProbe(index: 479, group: true),
          ]) {
            final result = await send(probe);
            expect(matchesDiagnosticFrame(result, probe.frame()), true);
          }
          final off = await send(const PixelProbe(), off: true);
          expect(matchesDiagnosticFrame(off, Uint8List(1440)), true);
          expect(commits, 4);
          expect(chunks, 8);
          expect(writes, 1);
        } else if (scenario == 'delayed-read') {
          final result = await send(const PixelProbe());
          expect(
            matchesDiagnosticFrame(result, const PixelProbe().frame()),
            true,
          );
          expect(chunks, 2);
          expect(writes, 1);
          expect(commits, 1);
        } else {
          await expectLater(
            send(const PixelProbe()),
            throwsA(isA<Exception>()),
          );
          expect(commits, ['busy', 'no-websocket'].contains(scenario) ? 0 : 1);
          expect(chunks, commits * 2);
        }
      } finally {
        await api.close();
        await live?.cancel();
        for (final socket in sockets) {
          await socket.close();
        }
        await listener.cancel();
        await server.close(force: true);
      }
    });
  }
}
