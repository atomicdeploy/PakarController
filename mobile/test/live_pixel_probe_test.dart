// Explicit hardware opt-in. Uses the shipping Flutter API, not an HTTP mock.
// Buffer/transport evidence is not Android-device or optical acceptance.
import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';

void main() {
  test(
    'Explicit live 480-pixel command and full-frame readback',
    () async {
      final environment = Platform.environment;
      final endpoint = environment['PAKAR_LIVE_ENDPOINT'];
      final board = environment['PAKAR_LIVE_BOARD'];
      final boot = environment['PAKAR_LIVE_BOOT'];
      final path = environment['PAKAR_LIVE_RECEIPT'];
      if (endpoint == null || board == null || boot == null || path == null) {
        fail(
          'Explicit endpoint, exact board/boot and new receipt are required.',
        );
      }
      final receipt = File(path);
      expect(receipt.isAbsolute, isTrue);
      expect(receipt.existsSync(), isFalse);
      receipt.createSync(exclusive: true);
      final api = PakarApi(baseUrl: endpoint);
      StreamSubscription<DeviceStatus>? live;
      final proof = <String, dynamic>{
        'scope': 'shipping-flutter-api-real-esp-not-android-or-optical',
        'endpoint': endpoint,
        'board': board,
        'boot': boot,
        'steps': <Map<String, dynamic>>[],
        'verified': false,
      };
      try {
        final initial = await api.status();
        expect(initial.json['board_id'], board);
        expect(initial.json['boot_id'], boot);
        expect(initial.peer.has('pixel-diagnostic'), isTrue);
        expect(initial.json['programming_latch'], isFalse);
        expect(initial.mode, 'cell');
        expect(initial.effect, 'none');
        final ready = Completer<bool>();
        live = api.liveStatus().listen(
          (_) {
            if (!ready.isCompleted) ready.complete(true);
          },
          onError: (Object error) {
            proof['websocket_error'] = '$error';
            if (!ready.isCompleted) ready.complete(false);
          },
          onDone: () {
            if (!ready.isCompleted) ready.complete(false);
          },
        );
        proof['websocket_status_received'] = await ready.future.timeout(
          const Duration(seconds: 8),
          onTimeout: () => false,
        );
        // Match the app's bounded HTTP recovery; lack of WS is not a reason
        // to block an explicitly requested, identity-pinned pixel command.
        final pixels = DeviceStatus(await api.getJson('/api/status?pixels=1'));
        expect(pixels.json['board_id'], board);
        expect(pixels.json['boot_id'], boot);
        // Do not silently replace an unknown display; the operator clears first.
        expect(matchesDiagnosticFrame(pixels, Uint8List(1440)), isTrue);
        final owner = api.connectionRevision;
        for (final step in [
          (const PixelProbe(), false),
          (const PixelProbe(index: 5, rgb: 0xff0000), false),
          (const PixelProbe(index: 6, rgb: 0x0000ff), false),
          (const PixelProbe(index: 479, rgb: 0x00ff00), false),
          (const PixelProbe(index: 479, group: true, rgb: 0xff0000), false),
          (const PixelProbe(index: 479), true),
        ]) {
          final clock = Stopwatch()..start();
          final actual = await api.pixelDiagnostic(
            step.$1,
            off: step.$2,
            authorize: () => owner == api.connectionRevision,
          );
          expect(actual.json['board_id'], board);
          expect(actual.json['boot_id'], boot);
          expect(
            matchesDiagnosticFrame(actual, step.$1.frame(off: step.$2)),
            isTrue,
          );
          (proof['steps'] as List).add({
            'first': step.$1.first,
            'count': step.$1.count,
            'rgb': step.$1.hex,
            'off': step.$2,
            'elapsed_ms': clock.elapsedMilliseconds,
            'revision': (actual.json['pixel_frame'] as Map)['revision'],
          });
        }
        proof['verified'] = true;
        proof['all_off_confirmed'] = true;
      } catch (error) {
        proof['error'] = '$error';
        rethrow;
      } finally {
        receipt.writeAsStringSync(
          const JsonEncoder.withIndent('  ').convert(proof),
        );
        await api.close();
        await live?.cancel().timeout(const Duration(seconds: 1));
      }
    },
    skip: Platform.environment['PAKAR_LIVE_PIXEL_TEST'] != '1',
    timeout: const Timeout(Duration(seconds: 75)),
  );
}
