import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';
import 'package:pakarcontroller/minigame.dart';

import 'peer_fixture.dart';
import 'minigame_test.dart' show miniStatus;

Map<String, dynamic> status({PixelProbe? probe}) => {
  ...peerFixture(
    features: [
      'command-ack',
      'pixel-diagnostic',
      'leds',
      'table',
      'cell-batch',
    ],
  ).json,
  'board_id': 'fixture',
  'boot_id': 'a' * 32,
  'command_clock_ms': 1000,
  'pixels': 480,
  'mode': 'cell',
  'effect': 'none',
  'led_output': {'enabled': true, 'mode': 'effects'},
  if (probe != null)
    'pixel_frame': {
      'revision': 1,
      'pending': false,
      'rgb': probe
          .frame()
          .map((v) => v.toRadixString(16).padLeft(2, '0'))
          .join(),
    },
};

class CommandServer {
  CommandServer({this.snapshot});
  final Map<String, dynamic> Function()? snapshot;
  late HttpServer server;
  final sockets = <WebSocket>[];
  final timers = <Timer>[];
  late StreamSubscription<HttpRequest> requests;
  int reads = 0, writes = 0;
  final packets = <Map>[];
  Duration delay = Duration.zero;
  bool reply = true;
  Future<void> start() async {
    server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    requests = server.listen((request) async {
      if (request.uri.path == '/ws') {
        final socket = await WebSocketTransformer.upgrade(request);
        sockets.add(socket);
        socket.add(jsonEncode(snapshot?.call() ?? status()));
        socket.listen((message) {
          final q = jsonDecode(message as String) as Map;
          if (q['type'] != 'command') {
            socket.add(jsonEncode(snapshot?.call() ?? status()));
            return;
          }
          packets.add(q);
          if (!reply) return;
          final probe = q['command'] == 'pixel-diagnostic'
              ? PixelProbe(
                  index: q['index'],
                  group: q['count'] == 6,
                  rgb: q['rgb'],
                )
              : null;
          timers.add(
            Timer(
              delay,
              () => socket.add(
                jsonEncode({
                  'type': 'command-result',
                  'request_id': q['request_id'],
                  'board_id': q['board_id'],
                  'boot_id': q['boot_id'],
                  'accepted': true,
                  if (q['command'] == 'cells') 'count': 2,
                  'status': snapshot?.call() ?? status(probe: probe),
                }),
              ),
            ),
          );
        });
      } else {
        if (request.method == 'GET') {
          reads++;
        } else {
          writes++;
        }
        request.response.headers.contentType = ContentType.json;
        request.response.write(jsonEncode(snapshot?.call() ?? status()));
        await request.response.close();
      }
    });
  }

  PakarApi api() => PakarApi(
    baseUrl: 'http://127.0.0.1:${server.port}',
    commandHttpFallback: false,
  );
  Future<StreamSubscription<DeviceStatus>> live(PakarApi api) async {
    await api.status();
    final ready = Completer<void>();
    final sub = api.liveStatus().listen((_) {
      if (!ready.isCompleted) ready.complete();
    });
    await ready.future.timeout(const Duration(seconds: 2));
    return sub;
  }

  Future<void> close() async {
    for (final timer in timers) {
      timer.cancel();
    }
    for (final socket in sockets) {
      await socket.close();
    }
    await requests.cancel();
    await server.close(force: true);
  }
}

void main() {
  test('Game box taps prefer WS with typed values and no HTTP write or preflight read', () async {
    final base = miniStatus(phase: 'input').json;
    final observed = {...base, 'board_id': 'fixture', 'boot_id': 'a' * 32};
    final server = CommandServer(snapshot: () => observed);
    await server.start();
    final api = server.api();
    final live = await server.live(api);
    try {
      final reads = server.reads;
      final actual = await api.fastCommand(minigameInput, {
        'key': '2',
        'action': 'tap',
      }, authorize: () => true);
      expect(actual?.peer.minigame?.phase, 'input');
      expect(server.packets.single['command'], 'minigame-input');
      expect(server.packets.single['key'], 2);
      expect(server.writes, 0);
      expect(server.reads, reads);
    } finally {
      await api.close();
      await live.cancel();
      await server.close();
    }
  });
  test('HTTP command fallback defaults enabled', () async {
    final api = PakarApi();
    expect(api.commandHttpFallback, true);
    await api.close();
  });
  test('WS-only cell, bulk and pixel commands need no HTTP mutation or fresh-status read', () async {
    final server = CommandServer();
    await server.start();
    final api = server.api();
    final live = await server.live(api);
    try {
      final reads = server.reads;
      await api.fastCommand('/api/cell', {
        'id': '15',
        'state': 'red',
      }, authorize: () => true);
      await api.fastCommand('/api/cells', {
        'ids': '1-2',
        'state': 'off',
      }, authorize: () => true);
      const probe = PixelProbe(index: 270, group: true, rgb: 0x00ff00);
      final actual = await api.pixelDiagnostic(probe, authorize: () => true);
      expect(matchesDiagnosticFrame(actual, probe.frame()), true);
      expect(server.packets.map((q) => q['command']), [
        'cell',
        'cells',
        'pixel-diagnostic',
      ]);
      expect(server.writes, 0);
      expect(server.reads, reads);
    } finally {
      await api.close();
      await live.cancel();
      await server.close();
    }
  });
  test(
    'WS-only selection without a socket rejects before HTTP mutation',
    () async {
      final server = CommandServer();
      await server.start();
      final api = server.api();
      try {
        await api.status();
        await expectLater(
          api.fastCommand('/api/cell', {
            'id': '15',
            'state': 'red',
          }, authorize: () => true),
          throwsA(
            isA<FormatException>().having(
              (e) => e.message,
              'reason',
              contains('no command was sent'),
            ),
          ),
        );
        expect(server.writes, 0);
        expect(server.packets, isEmpty);
      } finally {
        await api.close();
        await server.close();
      }
    },
  );
  test(
    'WS-only delayed receipt remains available beyond preferred window',
    () async {
      final server = CommandServer()..delay = const Duration(milliseconds: 950);
      await server.start();
      final api = server.api();
      final live = await server.live(api);
      try {
        final command = api
            .fastCommand('/api/cell', {
              'id': '15',
              'state': 'red',
            }, authorize: () => true)
            .timeout(const Duration(seconds: 2));
        await Future<void>.delayed(const Duration(milliseconds: 100));
        // Changing the setting affects the next intent, not this pending one.
        api.commandHttpFallback = true;
        final result = await command;
        expect(result, isNotNull);
        expect(server.packets, hasLength(1));
        expect(server.writes, 0);
      } finally {
        await api.close();
        await live.cancel();
        await server.close();
      }
    },
  );
  test(
    'WS-only missing receipt is bounded and uncertain without HTTP replay',
    () async {
      final server = CommandServer()..reply = false;
      await server.start();
      final api = server.api();
      final live = await server.live(api);
      try {
        final clock = Stopwatch()..start();
        await expectLater(
          api.fastCommand('/api/cell', {
            'id': '15',
            'state': 'red',
          }, authorize: () => true),
          throwsA(
            isA<OperationUncertain>().having(
              (e) => e.toString(),
              'reason',
              contains('may have been applied'),
            ),
          ),
        );
        expect(clock.elapsed, lessThan(const Duration(seconds: 6)));
        expect(server.packets, hasLength(1));
        expect(server.writes, 0);
      } finally {
        await api.close();
        await live.cancel();
        await server.close();
      }
    },
  );
}
