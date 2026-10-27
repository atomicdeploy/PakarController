import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/models.dart';

import 'peer_fixture.dart';

void main() {
  test(
    'pixel demand follows foreground and reconnect without renewing status',
    () async {
      final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
      final api = PakarApi(baseUrl: 'http://127.0.0.1:${server.port}');
      final state = {
        ...peerFixture().json,
        'board_id': 'fixture',
        'boot_id': 'a' * 32,
        'command_clock_ms': 100,
      };
      final frame = {
        'revision': 1,
        'pending': false,
        'rgb': '00ff00' * 480,
        'physical': List.generate(80, (i) => i + 1),
      };
      final sockets = <WebSocket>[];
      final demands = StreamController<bool>();
      var full = 0, httpReads = 0;
      final listener = server.listen((request) async {
        if (request.uri.path != '/ws') {
          httpReads++;
          request.response.write(jsonEncode(state));
          await request.response.close();
          return;
        }
        final socket = await WebSocketTransformer.upgrade(request);
        sockets.add(socket);
        socket.listen((raw) {
          final value = jsonDecode(raw as String) as Map;
          if (value['type'] == 'status') socket.add(jsonEncode(state));
          if (value['type'] == 'pixel-subscribe') {
            demands.add(value['enabled'] as bool);
          }
        });
      });
      final demand = StreamIterator(demands.stream);
      StreamSubscription<DeviceStatus>? live;
      try {
        api.setUsbObservations(pixels: true);
        live = api.liveStatus().listen((_) => full++);
        expect(
          await demand.moveNext().timeout(const Duration(seconds: 3)),
          true,
        );
        expect(demand.current, true);
        final sample = api.pixelUpdates.first;
        sockets.last.add(
          jsonEncode({
            'type': 'pixel-frame',
            'board_id': 'fixture',
            'boot_id': 'a' * 32,
            'pixel_preview_requested': true,
            'pixel_frame': frame,
          }),
        );
        final received = await sample.timeout(const Duration(seconds: 3));
        expect(received.json['pixel_frame'], frame);
        expect(received.cells, state['cells']);
        expect(received.json['command_clock_ms'], 100);
        expect(full, 1);
        expect(httpReads, 0);
        expect(
          received.withPixelFrameEvent({
            'type': 'pixel-frame',
            'board_id': 'other',
            'boot_id': 'a' * 32,
            'pixel_preview_requested': true,
            'pixel_frame': frame,
          }),
          isNull,
        );
        expect(
          DeviceStatus(state)
              .preservingNewerPixels(received)
              .json['pixel_frame'],
          frame,
        );
        api.appForeground = false;
        await demand.moveNext().timeout(const Duration(seconds: 3));
        expect(demand.current, false);
        api.appForeground = true;
        await demand.moveNext().timeout(const Duration(seconds: 3));
        expect(demand.current, true);
        await sockets.last.close();
        await live.cancel();
        live = api.liveStatus().listen((_) => full++);
        await demand.moveNext().timeout(const Duration(seconds: 3));
        expect(demand.current, true);
        api.setUsbObservations();
        await demand.moveNext().timeout(const Duration(seconds: 3));
        expect(demand.current, false);
      } finally {
        await api.close();
        await live?.cancel();
        await demand.cancel();
        await demands.close();
        for (final socket in sockets) {
          await socket.close();
        }
        await listener.cancel();
        await server.close(force: true);
      }
    },
  );
}
