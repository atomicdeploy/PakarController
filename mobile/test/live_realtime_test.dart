import 'dart:async';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/models.dart';

// Explicit opt-in, real board mutation through the shipped mobile API class.
// A host-VM result is NOT Android deployment, visual or physical acceptance.
void main() {
  final origin = Platform.environment['PAKAR_REALTIME_ORIGIN'];
  test('Authorized live WS command latency and same-board readback', () async {
    final api = PakarApi(baseUrl: origin!);
    StreamSubscription<DeviceStatus>? subscription;
    try {
      final before = await api.status();
      expect(before.json['board_id'], 'esp32s3-907069eac800');
      expect(before.mode, 'cell');
      expect(before.effect, 'none');
      expect(before.peer.has('command-ack'), true);
      final ready = Completer<void>();
      subscription = api.liveStatus().listen((_) {
        if (!ready.isCompleted) ready.complete();
      });
      await ready.future.timeout(const Duration(seconds: 5));
      final original = before.cells[79];
      expect(['off', 'white', 'red'], contains(original));
      final test = original == 'red' ? 'white' : 'red';
      for (final state in [test, original, test, original]) {
        final timer = Stopwatch()..start();
        final actual = await api.fastCommand('/api/cells', {
          'ids': '80',
          'state': state,
          'effect': 'immediate',
        }, authorize: () => api.connectionWanted);
        timer.stop();
        expect(actual, isNotNull);
        expect(actual!.json['board_id'], before.json['board_id']);
        expect(actual.json['boot_id'], before.json['boot_id']);
        expect(actual.cells[79], state);
        // ignore: avoid_print
        print(
          'LIVE mobile API same-boot ACK/readback: $state ${timer.elapsedMicroseconds / 1000} ms',
        );
      }
      final fallback = PakarApi(baseUrl: origin);
      try {
        final peer = await fallback.status();
        expect(peer.json['boot_id'], before.json['boot_id']);
        for (final state in [test, original]) {
          final timer = Stopwatch()..start();
          final actual = await fallback.fastCommand('/api/cells', {
            'ids': '80',
            'state': state,
            'effect': 'immediate',
          }, authorize: () => fallback.connectionWanted);
          timer.stop();
          expect(actual!.cells[79], state);
          // ignore: avoid_print
          print(
            'LIVE mobile API HTTP-only ACK/readback: $state ${timer.elapsedMicroseconds / 1000} ms',
          );
        }
      } finally {
        await fallback.close();
      }
    } finally {
      await api.close();
      await subscription?.cancel();
    }
  }, skip: origin == null ? 'Explicit board authorization/origin required' : false);
}
