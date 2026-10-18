import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/models.dart';

import 'upgrade_progress_test.dart' show upgradeFixture, progressPeer;

// Real loopback IO, isolated from TestWidgetsFlutterBinding's HTTP override.
// Follows the actual mirroring_transport_test.dart lifecycle/cleanup pattern.
void main() {
  test('Actual WS lifecycle incrementally updates progress, not full-state lease or replay', () async {
    final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    final api = PakarApi(baseUrl: 'http://127.0.0.1:${server.port}');
    final observed = <DeviceStatus>[], full = <DeviceStatus>[];
    final done = Completer<void>();
    var statusRequests = 0;
    WebSocket? socket;
    StreamSubscription<DeviceStatus>? live;
    final updates = api.statusUpdates.listen((status) {
      observed.add(status);
      if (status.upgrade?.revision == 3 && !done.isCompleted) done.complete();
    });
    final serving = server.listen((request) async {
      if (request.uri.path == '/ws') {
        socket = await WebSocketTransformer.upgrade(request);
        socket!.listen((_) {
          // An event alone cannot establish identity before the full snapshot.
          socket!.add(
            jsonEncode({
              ...upgradeFixture(revision: 99),
              'type': 'update-progress',
            }),
          );
          socket!.add(jsonEncode(progressPeer(upgrade: upgradeFixture())));
          socket!.add(
            jsonEncode({
              ...upgradeFixture(revision: 2),
              'type': 'update-progress',
            }),
          );
          socket!.add(
            jsonEncode({
              ...upgradeFixture(revision: 2),
              'type': 'update-progress',
            }),
          );
          socket!.add(
            jsonEncode({
              ...upgradeFixture(revision: 4),
              'type': 'update-progress',
              'boot_id': 'old',
            }),
          );
          socket!.add(
            jsonEncode({
              ...upgradeFixture(revision: 3),
              'type': 'update-progress',
              'phase': 'verify_esp',
            }),
          );
        });
      } else {
        expect(request.method, 'GET');
        expect(request.uri.path, '/api/status');
        ++statusRequests;
        request.response.headers.contentType = ContentType.json;
        request.response.write(
          jsonEncode(progressPeer(upgrade: upgradeFixture())),
        );
        await request.response.close();
      }
    });
    try {
      live = api.liveStatus().listen(full.add);
      await done.future.timeout(const Duration(seconds: 5));
      expect(observed.map((status) => status.upgrade?.revision), [1, 2, 3]);
      expect(full.length, 1); // Incremental packets cannot renew full status.
      expect(api.latestStatus!.cells, ['red', 'white']);
      expect(api.latestStatus!.selectedCell, 2);
      expect(statusRequests, 0); // No progress-triggered HTTP polling.
      final actual = await api.status();
      expect(
        actual.upgrade!.revision,
        3,
      ); // Late HTTP cannot roll progress back.
      expect(api.latestStatus!.upgrade!.phase, 'verify_esp');
      api.disconnect();
      expect(api.latestStatus, isNull);
    } finally {
      await api.close();
      await live?.cancel();
      await updates.cancel();
      await socket?.close();
      await serving.cancel();
      await server.close(force: true);
    }
  });
}
