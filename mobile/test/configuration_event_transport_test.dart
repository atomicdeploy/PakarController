import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/models.dart';

void main() {
  test('Retiring a silent WebSocket allows bounded subscription cancellation and replacement without resetting peer/UI owner', () async {
    final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    final sockets = <WebSocket>[];
    var connections = 0;
    final serving = server.listen((request) async {
      final socket = await WebSocketTransformer.upgrade(request);
      sockets.add(socket);
      connections++;
      socket.listen(
        (_) => socket.add(
          jsonEncode({
            'version': '0.2.8',
            'hostname': 'ledboard',
            'board_id': 'fixture',
            'boot_id': 'boot',
          }),
        ),
      );
    });
    final api = PakarApi(baseUrl: 'http://127.0.0.1:${server.port}');
    StreamSubscription<DeviceStatus>? active;
    try {
      final first = Completer<void>();
      active = api.liveStatus().listen((_) {
        if (!first.isCompleted) first.complete();
      });
      await first.future.timeout(const Duration(seconds: 3));
      final owner = api.connectionRevision;
      await api.retireLiveSocket().timeout(const Duration(seconds: 2));
      await active.cancel().timeout(const Duration(seconds: 2));
      expect(api.connectionRevision, owner);
      expect(api.freshStatus?.json['board_id'], 'fixture');
      final second = Completer<void>();
      active = api.liveStatus().listen((_) {
        if (!second.isCompleted) second.complete();
      });
      await second.future.timeout(const Duration(seconds: 3));
      expect(connections, 2);
      expect(api.connectionRevision, owner);
      expect(api.liveFresh, true);
    } finally {
      await api.close();
      await active?.cancel().timeout(const Duration(seconds: 2));
      for (final socket in sockets) {
        await socket.close().timeout(const Duration(seconds: 2));
      }
      await server.close(force: true);
      await serving.cancel();
    }
  });
  test('Same-boot configuration edges coalesce once and cannot refresh status authority or replay mutations', () async {
    final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    final ready = Completer<WebSocket>();
    var writes = 0, changes = 0;
    final serving = server.listen((request) async {
      if (request.method != 'GET') writes++;
      final socket = await WebSocketTransformer.upgrade(request);
      socket.listen((_) {
        socket.add(
          jsonEncode({
            'version': '0.2.8',
            'hostname': 'ledboard',
            'board_id': 'fixture',
            'boot_id': 'boot',
          }),
        );
        if (!ready.isCompleted) ready.complete(socket);
      });
    });
    final api = PakarApi(baseUrl: 'http://127.0.0.1:${server.port}');
    StreamSubscription<DeviceStatus>? subscription;
    WebSocket? socket;
    final first = Completer<void>(), updated = Completer<void>();
    try {
      api.onConfigurationChanged = () {
        changes++;
        if (!updated.isCompleted) updated.complete();
      };
      subscription = api.liveStatus().listen((_) {
        if (!first.isCompleted) first.complete();
      });
      await first.future.timeout(const Duration(seconds: 3));
      socket = await ready.future;
      for (final revision in [1, 1, 2]) {
        socket.add(
          jsonEncode({
            'type': 'configuration-changed',
            'revision': revision,
            'board_id': 'fixture',
            'boot_id': 'boot',
          }),
        );
      }
      socket.add(
        jsonEncode({
          'type': 'configuration-changed',
          'revision': 3,
          'board_id': 'foreign',
          'boot_id': 'boot',
        }),
      );
      await updated.future.timeout(const Duration(seconds: 2));
      await Future<void>.delayed(const Duration(milliseconds: 350));
      expect(changes, 1);
      expect(writes, 0);
      final before = api.statusObservationAge!;
      socket.add(
        jsonEncode({
          'type': 'configuration-changed',
          'revision': 2,
          'board_id': 'fixture',
          'boot_id': 'boot',
        }),
      );
      await Future<void>.delayed(const Duration(milliseconds: 350));
      expect(changes, 1);
      expect(api.statusObservationAge!, greaterThan(before));
    } finally {
      // Follow the existing realtime_transport fixture: retire the socket
      // owner first so an async-generator cancellation cannot wait forever
      // for the still-open WebSocket's stream cancellation handshake.
      await api.close();
      await subscription?.cancel().timeout(const Duration(seconds: 2));
      await socket?.close().timeout(const Duration(seconds: 2));
      await server.close(force: true);
      await serving.cancel();
    }
  });
}
