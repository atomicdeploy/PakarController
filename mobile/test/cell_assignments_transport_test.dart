import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/cell_assignments.dart';
import 'package:pakarcontroller/command_readback.dart';
import 'package:pakarcontroller/models.dart';

import 'peer_fixture.dart';

/// Real loopback WS/HTTP, no widget binding and no physical board authority.
class AssignmentServer {
  late HttpServer server;
  late StreamSubscription<HttpRequest> requests;
  final sockets = <WebSocket>[];
  final receipts = <String, Map<String, dynamic>>{};
  final packets = <Map<String, dynamic>>[];
  late Map<String, dynamic> status;
  bool replyOnSocket = true;
  int httpWrites = 0, applications = 0;

  Map<String, dynamic> handle(Map<String, dynamic> packet) {
    packets.add(packet);
    final id = packet['request_id'] as String;
    return receipts.putIfAbsent(id, () {
      applications++;
      if (packet['command'] == 'cell-assignments') {
        final values = CellAssignments.fromRows(packet['assignments']);
        final cells = List<String>.from(status['cells']);
        for (final entry in values.values.entries) {
          cells[entry.key - 1] = entry.value;
        }
        status = {...status, 'cells': cells};
      } else if (packet['command'] == 'cell-lock') {
        status = {...status, 'red_lock': packet['enabled']};
      }
      return {
        'type': 'command-result',
        'request_id': id,
        'board_id': status['board_id'],
        'boot_id': status['boot_id'],
        'accepted': true,
        if (packet['command'] == 'cell-assignments')
          'count': (packet['assignments'] as List).length,
        'status': status,
      };
    });
  }

  Future<void> start() async {
    status = peerFixture().json;
    status['board_id'] = 'fixture-board';
    status['boot_id'] = 'a' * 32;
    status['command_clock_ms'] = 1000;
    status['red_lock'] = false;
    status['capabilities']['role'] = 'table-selection';
    status['capabilities']['acknowledged_features'] = [
      ...status['capabilities']['acknowledged_features'],
      'cell-assignments',
      'cell-lock',
      'command-ack',
    ];
    server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    requests = server.listen((request) async {
      if (request.uri.path == '/ws') {
        final socket = await WebSocketTransformer.upgrade(request);
        sockets.add(socket);
        socket.add(jsonEncode(status));
        socket.listen((message) {
          final packet = Map<String, dynamic>.from(
            jsonDecode(message as String) as Map,
          );
          if (packet['type'] == 'command') {
            final receipt = handle(packet);
            if (replyOnSocket) socket.add(jsonEncode(receipt));
          } else {
            socket.add(jsonEncode(status));
          }
        });
        return;
      }
      request.response.headers.contentType = ContentType.json;
      if (request.method == 'POST') {
        httpWrites++;
        final values = Uri.splitQueryString(
          await utf8.decoder.bind(request).join(),
        );
        request.response.write(
          jsonEncode(
            handle(
              Map<String, dynamic>.from(jsonDecode(values['packet']!) as Map),
            ),
          ),
        );
      } else {
        request.response.write(jsonEncode(status));
      }
      await request.response.close();
    });
  }

  PakarApi api({bool fallback = false}) => PakarApi(
    baseUrl: 'http://127.0.0.1:${server.port}',
    commandHttpFallback: fallback,
  );

  Future<StreamSubscription<DeviceStatus>> live(PakarApi api) async {
    await api.status();
    final ready = Completer<void>();
    final subscription = api.liveStatus().listen((_) {
      if (!ready.isCompleted) ready.complete();
    });
    await ready.future.timeout(const Duration(seconds: 2));
    return subscription;
  }

  Future<void> close() async {
    for (final socket in sockets) {
      await socket.close();
    }
    await requests.cancel();
    await server.close(force: true);
  }
}

void main() {
  final full = CellAssignments.fromRows([
    for (var cell = 1; cell <= 80; cell++)
      {'cell': cell, 'state': cell.isEven ? 'red' : 'off'},
  ]);
  for (final fallback in [false, true]) {
    test(
      '${fallback ? 'HTTP fallback' : 'WebSocket'} carries all80 assignments and lock with correlated readback',
      () async {
        final server = AssignmentServer();
        await server.start();
        final api = server.api(fallback: fallback);
        StreamSubscription<DeviceStatus>? live;
        try {
          if (fallback) {
            await api.status();
          } else {
            live = await server.live(api);
          }
          final values = {'assignments': full.commandValue};
          final actual = await api.fastCommand(
            '/api/cells/assignments',
            values,
            authorize: () => true,
          );
          expect(actual, isNotNull);
          expect(
            commandReadbackMatches(actual!, '/api/cells/assignments', values),
            true,
          );
          final locked = await api.fastCommand('/api/cells/lock', {
            'enabled': 'true',
          }, authorize: () => true);
          expect(locked!.json['red_lock'], true);
          expect(server.packets.map((p) => p['command']), [
            'cell-assignments',
            'cell-lock',
          ]);
          expect((server.packets.first['assignments'] as List).length, 80);
          expect(
            utf8.encode(jsonEncode(server.packets.first)).length,
            lessThan(4096),
          );
          expect(server.applications, 2);
          expect(server.httpWrites, fallback ? 2 : 0);
        } finally {
          await api.close();
          await live?.cancel();
          await server.close();
        }
      },
    );
  }
  test('Lost WS receipt uses same request ID over HTTP, never a second assignment intent', () async {
    final server = AssignmentServer()..replyOnSocket = false;
    await server.start();
    final api = server.api(fallback: true);
    final live = await server.live(api);
    try {
      final actual = await api.fastCommand('/api/cells/assignments', {
        'assignments': full.commandValue,
      }, authorize: () => true);
      expect(
        CellAssignments.fromObservedCells(actual!.cells).values.values
            .where((state) => state == 'lost')
            .length,
        40,
      );
      expect(server.packets.length, 2);
      expect(server.packets.map((p) => p['request_id']).toSet().length, 1);
      expect(server.applications, 1);
      expect(server.httpWrites, 1);
    } finally {
      await api.close();
      await live.cancel();
      await server.close();
    }
  });
}
