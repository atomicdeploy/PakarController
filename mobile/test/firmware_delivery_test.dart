import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:crypto/crypto.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/firmware.dart';
import 'package:pakarcontroller/firmware_feed.dart';
import 'package:pakarcontroller/firmware_progress.dart';
import 'package:pakarcontroller/models.dart';

import 'firmware_test.dart' show MaintenanceClient, jsonResponse, firmwareJob;

void main() {
  test('Urboot progress is stage-local, boot-bound and awaits application recovery', () {
    final a = <String, dynamic>{
      'job_id': 1,
      'boot_id': 'boot',
      'method': 'urboot',
      'phase': 'program_avr',
      'busy': true,
      'received_bytes': 128,
      'expected_bytes': 256,
    };
    final s = DeviceStatus({'boot_id': 'boot', 'avr_maintenance': a});
    expect(FirmwareProgress.observed(s)!.fraction, .5);
    a['phase'] = 'awaiting_application';
    expect(FirmwareProgress.observed(s)!.fraction, isNull);
    a['phase'] = 'complete';
    a['verified'] = true;
    expect(FirmwareProgress.observed(s)!.verified, false);
    a['application_recovered'] = true;
    expect(FirmwareProgress.observed(s)!.verified, true);
    a['method'] = 'isp';
    a['phase'] = 'program_isp';
    expect(FirmwareProgress.observed(s)!.fraction, isNull);
    expect(FirmwareProgress.observed(s)!.source, contains('ISP'));
    s.json['boot_id'] = 'other';
    expect(FirmwareProgress.observed(s), isNull);
  });
  test(
    'External board OTA and remote jobs use measured stage-only progress',
    () {
      final local = {
        'phase': 'receiving',
        'received_bytes': 50,
        'expected_bytes': 100,
        'progress_known': true,
        'completion_verified': false,
      };
      final status = DeviceStatus({'update': local});
      expect(FirmwareProgress.observed(status)!.fraction, .5);
      local['phase'] = 'verifying';
      final p = FirmwareProgress.observed(status)!;
      expect(p.fraction, isNull);
      expect(p.active, true);
      expect(p.verified, false);
      final remote = firmwareJob(id: 7, phase: 'program_esp')
        ..['received_bytes'] = 100
        ..['expected_bytes'] = 100;
      final s = DeviceStatus({
        'boot_id': 'fixture-boot',
        'remote_firmware': remote,
      });
      expect(FirmwareProgress.observed(s)!.fraction, isNull);
      remote['phase'] = 'complete';
      expect(FirmwareProgress.observed(s)!.verified, true);
      s.json['boot_id'] = 'different';
      expect(FirmwareProgress.observed(s), isNull);
    },
  );
  testWidgets(
    'Actual progress widget displays indeterminate verification, not 100% flash',
    (tester) async {
      await tester.pumpWidget(
        const MaterialApp(
          home: Scaffold(
            body: FirmwareProgressView(
              FirmwareProgress('Board OTA', 'verifying', 100, null),
            ),
          ),
        ),
      );
      expect(
        tester
            .widget<LinearProgressIndicator>(
              find.byType(LinearProgressIndicator),
            )
            .value,
        isNull,
      );
      expect(find.text('Board OTA · verifying'), findsOneWidget);
      await tester.pumpWidget(const SizedBox());
    },
  );
  test('HTTP file verifies hash before one upload and forwards no board credentials to source', () async {
    var writes = 0;
    final progress = <String>[];
    final bytes = Uint8List(16384)..[0] = 0xe9;
    final api = PakarApi(
      client: MaintenanceClient((r) async {
        if (r.url.path == '/api/status') {
          return jsonResponse({'hostname': 'ledboard'});
        }
        if (r.url.path == '/api/firmware/remote') {
          return jsonResponse(firmwareJob());
        }
        expect(r.url.path, '/update');
        ++writes;
        expect(r.headers['X-Pakar-Firmware-Size'], '${bytes.length}');
        expect(
          r.headers['X-Pakar-Firmware-MD5'],
          md5.convert(bytes).toString(),
        );
        final body = await r.finalize().toBytes();
        expect(body.length, greaterThan(bytes.length));
        return jsonResponse({'updated': true, 'rebooting': true});
      }),
    );
    api.password = 'private-board-password';
    final controller = FirmwareController(api, authorized: (_) => true);
    final source = MaintenanceClient((r) async {
      expect(r.followRedirects, false);
      expect(
        r.headers.keys.any((k) => k.toLowerCase() == 'authorization'),
        false,
      );
      expect(
        r.headers.keys.any((k) => k.toLowerCase().startsWith('x-pakar')),
        false,
      );
      return http.StreamedResponse(
        Stream.value(bytes),
        200,
        contentLength: bytes.length,
      );
    });
    await controller.fileUrl(
      'http://updates.example/image.bin',
      sha256.convert(bytes).toString(),
      true,
      downloadClient: source,
      progress: (phase, done, total) => progress.add(phase),
    );
    expect(writes, 1);
    expect(progress, contains('downloading file'));
    expect(progress, contains('uploading to board'));
    await expectLater(
      controller.fileUrl(
        'https://updates.example/image.bin',
        '0' * 64,
        true,
        downloadClient: source,
      ),
      throwsFormatException,
    );
    expect(writes, 1);
    await expectLater(
      controller.fileUrl(
        'https://updates.example/image.bin',
        sha256.convert(bytes).toString(),
        true,
        downloadClient: source,
        beforeUpload: () =>
            throw const FormatException('Expired before upload'),
      ),
      throwsFormatException,
    );
    expect(writes, 1);
    await api.close();
    source.close();
  });
  test('Socket.IO handles open, namespace acknowledgment, heartbeat and exact events', () {
    const options = FirmwareFeedOptions(
      endpoint: 'wss://updates.example/socket.io/',
      origin: 'https://updates.example/',
      socketIo: true,
      namespace: '/updates',
      token: 'private',
    );
    options.validate();
    final wire = FirmwareFeedWire(options);
    expect(
      wire
          .receive('0{"sid":"engine","pingInterval":25000,"pingTimeout":20000}')
          .$2,
      '40/updates,{"token":"private"}',
    );
    expect(wire.receive('2probe').$2, '3probe');
    expect(wire.receive('40/other,{}').$1, 'ignore');
    expect(wire.receive('40/updates,{"sid":"namespace"}').$1, 'ready');
    expect(
      wire.receive('42/updates,7["firmware-offer",{"id":"one"}]').$1,
      'offer',
    );
    expect(wire.receive('42/other,["firmware-offer",{}]').$1, 'ignore');
    expect(
      () => wire.receive('44/updates,{"message":"denied"}'),
      throwsFormatException,
    );
  });
  test(
    'Firmware offers require exact fresh session/board/boot/hash/origin',
    () {
      const options = FirmwareFeedOptions(
        endpoint: 'wss://updates.example/events',
        origin: 'https://updates.example/',
      );
      final offer = {
        'id': 'one',
        'session': 's',
        'board_id': 'board',
        'boot_id': 'boot',
        'expires_at_ms': 1100,
        'url': 'https://updates.example/esp.bin',
        'sha256': 'a' * 64,
        'kind': 'binary',
        'target': 'esp',
      };
      expect(
        FirmwareOffer.parse(offer, options, 's', 'board', 'boot', now: 1000).id,
        'one',
      );
      expect(
        () => FirmwareOffer.parse(
          offer,
          options,
          's',
          'board',
          'boot',
          now: 1100,
        ),
        throwsFormatException,
      );
      expect(
        () => FirmwareOffer.parse(
          {...offer, 'session': 'old'},
          options,
          's',
          'board',
          'boot',
          now: 1000,
        ),
        throwsFormatException,
      );
      expect(
        () => FirmwareOffer.parse(
          {...offer, 'url': 'https://other.example/image'},
          options,
          's',
          'board',
          'boot',
          now: 1000,
        ),
        throwsFormatException,
      );
    },
  );
  test('Actual WS/Socket.IO listener deduplicates offers and sends no board mutation', () async {
    for (final socketIo in [false, true]) {
      final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
      final feed = FirmwareFeed(), delivered = Completer<void>();
      var deliveries = 0;
      final serving = server.listen((request) async {
        final ws = await WebSocketTransformer.upgrade(request);
        if (socketIo) {
          ws.add('0{"sid":"engine","pingInterval":25000,"pingTimeout":20000}');
        }
        ws.listen((raw) {
          if (socketIo && raw == '40/updates,') {
            ws.add('40/updates,{"sid":"namespace"}');
            return;
          }
          if (raw == '3') return;
          final data = socketIo
              ? (jsonDecode((raw as String).substring('42/updates,'.length))
                    as List)[1]
              : jsonDecode(raw as String);
          final offer = {
            'type': 'firmware-offer',
            'id': 'one',
            'session': data['session'],
            'board_id': 'board',
            'boot_id': 'boot',
            'expires_at_ms': DateTime.now().millisecondsSinceEpoch + 30000,
            'url': 'https://updates.example/image.bin',
            'sha256': 'a' * 64,
            'kind': 'binary',
            'target': 'esp',
          };
          final packet = socketIo
              ? '42/updates,${jsonEncode(['firmware-offer', offer])}'
              : jsonEncode(offer);
          ws.add(packet);
          ws.add(packet);
          if (socketIo) ws.add('2');
        });
      });
      await feed.start(
        FirmwareFeedOptions(
          endpoint: 'ws://127.0.0.1:${server.port}/events',
          origin: 'https://updates.example/',
          socketIo: socketIo,
          namespace: '/updates',
        ),
        'board',
        'boot',
        armed: true,
        deliver: (offer) async {
          ++deliveries;
          if (!delivered.isCompleted) delivered.complete();
        },
      );
      await delivered.future.timeout(const Duration(seconds: 5));
      await Future<void>.delayed(const Duration(milliseconds: 80));
      expect(deliveries, 1);
      await feed.stop();
      feed.dispose();
      await serving.cancel();
      await server.close(force: true);
    }
  });

  test('Retired feed delivery cannot overwrite a replacement session or dispatch concurrently', () async {
    final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    final sockets = <WebSocket>[];
    final subscriptions = <Map<String, dynamic>>[];
    final subscribedOne = Completer<void>(), subscribedTwo = Completer<void>();
    final firstDelivered = Completer<void>(), oldDelivery = Completer<void>();
    final serving = server.listen((request) async {
      final ws = await WebSocketTransformer.upgrade(request);
      sockets.add(ws);
      ws.listen((raw) {
        subscriptions.add(jsonDecode(raw as String) as Map<String, dynamic>);
        if (subscriptions.length == 1) subscribedOne.complete();
        if (subscriptions.length == 2) subscribedTwo.complete();
      });
    });
    final feed = FirmwareFeed();
    var deliveries = 0;
    Future<void> start() => feed.start(
      FirmwareFeedOptions(
        endpoint: 'ws://127.0.0.1:${server.port}/events',
        origin: 'https://updates.example/',
      ),
      'board',
      'boot',
      armed: true,
      deliver: (_) async {
        ++deliveries;
        if (!firstDelivered.isCompleted) firstDelivered.complete();
        await oldDelivery.future;
      },
    );
    void offer(int socket, String id) {
      final subscription = subscriptions[socket];
      sockets[socket].add(
        jsonEncode({
          'type': 'firmware-offer',
          'id': id,
          'session': subscription['session'],
          'board_id': 'board',
          'boot_id': 'boot',
          'expires_at_ms': DateTime.now().millisecondsSinceEpoch + 30000,
          'url': 'https://updates.example/image.bin',
          'sha256': 'a' * 64,
          'kind': 'binary',
          'target': 'esp',
        }),
      );
    }

    try {
      await start();
      await subscribedOne.future.timeout(const Duration(seconds: 3));
      offer(0, 'first');
      await firstDelivered.future.timeout(const Duration(seconds: 3));
      await feed.stop();
      await start();
      await subscribedTwo.future.timeout(const Duration(seconds: 3));
      offer(1, 'second');
      await Future<void>.delayed(const Duration(milliseconds: 30));
      expect(deliveries, 1);
      expect(feed.dispatching, true);
      final state = feed.state;
      oldDelivery.completeError(const FormatException('old session retired'));
      await Future<void>.delayed(const Duration(milliseconds: 30));
      expect(feed.state, state);
      expect(feed.running, true);
      expect(feed.dispatching, false);
      expect(subscriptions[0]['session'], isNot(subscriptions[1]['session']));
    } finally {
      if (!oldDelivery.isCompleted) oldDelivery.complete();
      await feed.stop();
      feed.dispose();
      for (final socket in sockets) {
        await socket.close();
      }
      await serving.cancel();
      await server.close(force: true);
    }
  });
}
