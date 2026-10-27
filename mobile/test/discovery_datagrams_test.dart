import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/discovery_datagrams.dart';

void main() {
  for (final asynchronous in [true, false]) {
    test(
      'UDP network unreachable is awaited and cleaned up: async=$asynchronous',
      () async {
        final socket = StreamController<RawSocketEvent>();
        var closes = 0, sends = 0;
        const failure = SocketException(
          'Network is unreachable',
          osError: OSError('Network is unreachable', 101),
        );
        final result = collectDiscoveryDatagrams(
          events: socket.stream,
          receive: () => null,
          send: () {
            sends++;
            if (asynchronous) {
              socket.addError(failure, StackTrace.current);
            } else {
              throw failure;
            }
          },
          close: () => closes++,
          onPacket: (_) => fail('No packet should be invented'),
          window: const Duration(seconds: 4),
        );
        await expectLater(result, throwsA(same(failure)));
        expect(sends, 1);
        expect(closes, 1);
        expect(socket.hasListener, false);
        await socket.close();
      },
    );
  }

  test(
    'UDP receive errors are awaited without escaping callback ownership',
    () async {
      final socket = StreamController<RawSocketEvent>();
      var closes = 0;
      const failure = SocketException('Network changed');
      final result = collectDiscoveryDatagrams(
        events: socket.stream,
        receive: () => throw failure,
        send: () => socket.add(RawSocketEvent.read),
        close: () => closes++,
        onPacket: (_) => fail('No packet should be invented'),
        window: const Duration(seconds: 4),
      );
      await expectLater(result, throwsA(same(failure)));
      expect(closes, 1);
      expect(socket.hasListener, false);
      await socket.close();
    },
  );

  test(
    'UDP window drains actual packets then retires the exact listener',
    () async {
      final socket = StreamController<RawSocketEvent>();
      final packets = [
        Datagram(
          Uint8List.fromList([1, 2]),
          InternetAddress.loopbackIPv4,
          1900,
        ),
        Datagram(
          Uint8List.fromList([3, 4]),
          InternetAddress.loopbackIPv4,
          1900,
        ),
      ];
      final received = <Datagram>[];
      var closes = 0;
      await collectDiscoveryDatagrams(
        events: socket.stream,
        receive: () => packets.isEmpty ? null : packets.removeAt(0),
        send: () => socket.add(RawSocketEvent.read),
        close: () => closes++,
        onPacket: received.add,
        window: const Duration(milliseconds: 10),
      );
      expect(received.map((packet) => packet.data.toList()), [
        [1, 2],
        [3, 4],
      ]);
      expect(closes, 1);
      expect(socket.hasListener, false);
      await socket.close();
    },
  );

  test('UDP socket closure finishes early without failure or replay', () async {
    var closes = 0, sends = 0;
    await collectDiscoveryDatagrams(
      events: const Stream<RawSocketEvent>.empty(),
      receive: () => null,
      send: () => sends++,
      close: () => closes++,
      onPacket: (_) => fail('No packet should be invented'),
      window: const Duration(seconds: 4),
    );
    expect(closes, 1);
    expect(sends, 1);
  });
}
