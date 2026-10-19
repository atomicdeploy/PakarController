import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';

// Real IOClient/socket tests, deliberately separate from widget HTTP overrides.
class StalledPeer {
  StalledPeer(this.server, this.mode) {
    listener = server.listen((socket) {
      sockets.add(socket);
      var request = '', handled = false;
      socket.listen(
        (bytes) {
          request += utf8.decode(bytes);
          if (handled || !request.contains('\r\n\r\n')) return;
          handled = true;
          ++requests;
          if (mode == 'headers') return;
          if (mode == 'body') {
            socket.write(
              'HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 1024\r\n\r\n{',
            );
          } else if (mode == 'oversize') {
            socket.write('HTTP/1.1 200 OK\r\nContent-Length: 65537\r\n\r\n{');
          } else {
            const body = '{"version":"0.2.8","hostname":"ledboard"}';
            socket.write(
              'HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ${body.length}\r\nConnection: close\r\n\r\n$body',
            );
            unawaited(socket.close());
          }
        },
        onDone: () => sockets.remove(socket),
        onError: (Object _) => sockets.remove(socket),
      );
    });
  }
  final ServerSocket server;
  String mode;
  final sockets = <Socket>{};
  late final StreamSubscription<Socket> listener;
  int requests = 0;
  String get origin => 'http://127.0.0.1:${server.port}';

  Future<void> waitFor(bool Function() ready) async {
    final elapsed = Stopwatch()..start();
    while (!ready()) {
      if (elapsed.elapsed > const Duration(seconds: 2)) {
        fail(
          'Socket condition not reached: $mode, ${sockets.length} open, $requests requests',
        );
      }
      await Future<void>.delayed(const Duration(milliseconds: 10));
    }
  }

  Future<void> close() async {
    for (final socket in sockets.toList()) {
      socket.destroy();
    }
    await listener.cancel();
    await server.close();
  }
}

void main() {
  for (final stage in ['headers', 'body']) {
    test(
      'Repeated stalled $stage reads abort actual sockets instead of accumulating them',
      () async {
        final peer = StalledPeer(
          await ServerSocket.bind(InternetAddress.loopbackIPv4, 0),
          stage,
        );
        final api = PakarApi(
          baseUrl: peer.origin,
          readTimeout: const Duration(milliseconds: 250),
        );
        try {
          for (var attempt = 0; attempt < 3; ++attempt) {
            await expectLater(api.status(), throwsA(isA<TimeoutException>()));
            await peer.waitFor(() => peer.sockets.isEmpty);
          }
          expect(peer.requests, 3);
          peer.mode = 'healthy';
          expect((await api.status()).hostname, 'ledboard');
          expect(peer.requests, 4);
        } finally {
          await api.close();
          await peer.close();
        }
      },
    );
  }

  test('Changing owner aborts a pending actual GET immediately, not at its six-second deadline', () async {
    final peer = StalledPeer(
      await ServerSocket.bind(InternetAddress.loopbackIPv4, 0),
      'body',
    );
    final api = PakarApi(baseUrl: peer.origin);
    try {
      final read = api.status();
      final rejected = expectLater(read, throwsA(isA<StaleConnection>()));
      await peer.waitFor(() => peer.requests == 1);
      api.baseUrl = 'http://127.0.0.1:9';
      await rejected.timeout(const Duration(seconds: 2));
      await peer.waitFor(() => peer.sockets.isEmpty);
      expect(api.latestStatus, isNull);
    } finally {
      await api.close();
      await peer.close();
    }
  });

  test(
    'Oversized declared JSON aborts before reading an unbounded body',
    () async {
      final peer = StalledPeer(
        await ServerSocket.bind(InternetAddress.loopbackIPv4, 0),
        'oversize',
      );
      final api = PakarApi(baseUrl: peer.origin);
      try {
        await expectLater(api.status(), throwsFormatException);
        await peer.waitFor(() => peer.sockets.isEmpty);
        expect(peer.requests, 1);
        expect(api.latestStatus, isNull);
      } finally {
        await api.close();
        await peer.close();
      }
    },
  );
}
