import 'dart:async';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';

void main() {
  test('A stalled WebSocket upgrade reports one owned timeout and retires', () async {
    final server = await ServerSocket.bind(InternetAddress.loopbackIPv4, 0);
    final sockets = <Socket>[];
    final listener = server.listen((socket) {
      sockets.add(socket);
      socket.listen((_) {}, onError: (Object _) {});
    });
    final api = PakarApi(baseUrl: 'http://localhost:${server.port}');
    final failures = <Object>[];
    final done = Completer<void>();
    final live = api.liveStatus().listen(
      (_) => fail('A stalled upgrade cannot publish device state'),
      onError: (Object error) {
        failures.add(error);
        done.complete();
      },
      onDone: done.complete,
      cancelOnError: true,
    );
    try {
      await done.future.timeout(const Duration(seconds: 8));
      expect(failures, hasLength(1));
      expect(failures.single, isA<TimeoutException>());
      expect(api.liveFresh, false);
      expect(api.latestStatus, isNull);
    } finally {
      await api.close();
      await live.cancel();
      for (final socket in sockets) {
        socket.destroy();
      }
      await listener.cancel();
      await server.close();
    }
  });
}
