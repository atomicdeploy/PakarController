import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';

import 'peer_fixture.dart';

Map<String, dynamic> state({PixelProbe? probe}) => {
  ...peerFixture(features: ['command-ack', 'pixel-diagnostic', 'leds', 'table']).json,
  'board_id': 'fixture', 'boot_id': 'a' * 32,
  'command_clock_ms': 1000, 'pixels': 480, 'mode': 'cell', 'effect': 'none',
  'led_output': {'enabled': true, 'mode': 'effects'},
  if (probe != null) 'pixel_frame': {
    'revision': 1, 'pending': false,
    'rgb': probe.frame().map((v) => v.toRadixString(16).padLeft(2, '0')).join(),
  },
};

void main() {
  test('Fresh same-boot WS admission overtakes a stalled background HTTP read', () async {
    final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    final api = PakarApi(baseUrl: 'http://127.0.0.1:${server.port}');
    final sockets = <WebSocket>[];
    final stalled = Completer<void>();
    var reads = 0, commits = 0;
    final listener = server.listen((request) async {
      if (request.uri.path == '/ws') {
        final socket = await WebSocketTransformer.upgrade(request);
        sockets.add(socket);
        socket.add(jsonEncode(state()));
        socket.listen((message) {
          final q = jsonDecode(message as String) as Map;
          if (q['type'] == 'command') {
            commits++;
            final probe = PixelProbe(index: q['index'], group: q['count'] == 6, rgb: q['rgb']);
            socket.add(jsonEncode({'type': 'command-result',
              'request_id': q['request_id'], 'board_id': 'fixture',
              'boot_id': 'a' * 32, 'accepted': true, 'status': state(probe: probe)}));
          } else if (q['type'] == 'status') {
            socket.add(jsonEncode(state()));
          }
        });
      } else if (++reads == 1) {
        request.response.headers.contentType = ContentType.json;
        request.response.write(jsonEncode(state()));
        await request.response.close();
      } else {
        stalled.complete(); // Deliberately leave headers pending.
      }
    });
    StreamSubscription<DeviceStatus>? live;
    try {
      await api.status();
      final ready = Completer<void>();
      live = api.liveStatus().listen((_) { if (!ready.isCompleted) ready.complete(); });
      await ready.future.timeout(const Duration(seconds: 2));
      await Future<void>.delayed(const Duration(milliseconds: 2100));
      final background = api.status();
      final retired = expectLater(background, throwsA(isA<SupersededRead>()));
      await stalled.future.timeout(const Duration(seconds: 2));
      final probe = const PixelProbe(index: 270, group: true, rgb: 0x00ff00);
      final actual = await api.pixelDiagnostic(probe, authorize: () => true)
          .timeout(const Duration(seconds: 2));
      expect(matchesDiagnosticFrame(actual, probe.frame()), true);
      await retired;
      expect(commits, 1);
      expect(reads, 2);
    } finally {
      await api.close(); await live?.cancel();
      for (final socket in sockets) { await socket.close(); }
      await listener.cancel(); await server.close(force: true);
    }
  });

  test('HTTP command headers share the four-second receipt deadline and socket abort', () async {
    final server = await ServerSocket.bind(InternetAddress.loopbackIPv4, 0);
    final api = PakarApi(baseUrl: 'http://127.0.0.1:${server.port}');
    final sockets = <Socket>{};
    final closed = Completer<void>();
    var commands = 0;
    final listener = server.listen((socket) {
      sockets.add(socket);
      var text = '', handled = false, command = false;
      socket.listen((bytes) {
        text += utf8.decode(bytes);
        if (handled || !text.contains('\r\n\r\n')) return;
        handled = true;
        if (text.startsWith('POST ')) {
          command = true; commands++;
          return; // Accept request bytes but never return headers.
        }
        final body = jsonEncode(state());
        socket.write('HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ${utf8.encode(body).length}\r\nConnection: close\r\n\r\n$body');
        unawaited(socket.close());
      }, onDone: () {
        sockets.remove(socket);
        if (command && !closed.isCompleted) closed.complete();
      }, onError: (Object _) {
        sockets.remove(socket);
        if (command && !closed.isCompleted) closed.complete();
      });
    });
    try {
      await api.status();
      final clock = Stopwatch()..start();
      await expectLater(api.fastCommand('/api/effect', {'value': 'rainbow'}, authorize: () => true),
          throwsA(isA<OperationUncertain>().having((e) => e.toString(), 'reason', contains('within 4 seconds'))));
      expect(clock.elapsed, lessThan(const Duration(seconds: 6)));
      await closed.future.timeout(const Duration(seconds: 2));
      expect(commands, 1);
    } finally {
      await api.close(); for (final socket in sockets.toList()) { socket.destroy(); }
      await listener.cancel(); await server.close();
    }
  });
}
