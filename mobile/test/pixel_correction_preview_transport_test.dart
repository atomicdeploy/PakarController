import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/pixel_correction_preview.dart';
import 'package:pakarcontroller/pixel_corrections_editor.dart';

import 'peer_fixture.dart';

class PreviewServer {
  PreviewServer({
    this.command = true,
    this.reject = false,
    this.foreign = false,
    this.dropWs = false,
  });
  final bool command, reject, foreign, dropWs;
  late HttpServer server;
  late StreamSubscription<HttpRequest> requests;
  final sockets = <WebSocket>[];
  int httpWrites = 0, wsWrites = 0, executions = 0;
  final receipts = <String, Map<String, dynamic>>{};
  Map<String, dynamic>? active;
  Map<String, dynamic> status() => {
    ...peerFixture(
      features: [
        'leds',
        'table',
        'command-ack',
        'pixel-corrections',
        'pixel-corrections-preview',
        if (command) 'pixel-corrections-preview-command',
      ],
    ).json,
    'board_id': 'preview-fixture',
    'boot_id': 'a' * 32,
    'command_clock_ms': 1000,
    'pixels': 480,
  };
  Map<String, dynamic> preview() => {
    'active': active != null,
    'lease_id': active == null ? 0 : 41,
    'base_revision': 3,
    'remaining_ms': active == null ? 0 : 10000,
  };
  Map<String, dynamic> config() => {
    'revision': 3,
    'enabled': true,
    'pixels': 480,
    'rules': <dynamic>[],
    'active_sources': pixelCorrectionSources(
      480,
      active?['enabled'] == true,
      (active?['rules'] as List? ?? const [])
          .map((r) => Map<String, dynamic>.from(r as Map))
          .toList(),
    ),
    'preview': preview(),
  };
  Map<String, dynamic> apply(Map q) {
    final id = q['request_id'] as String;
    return receipts.putIfAbsent(id, () {
      if (!reject) {
        executions++;
        final c = Map<String, dynamic>.from(q['configuration'] as Map);
        active = c['clear'] == true ? null : c;
      }
      return {
        'type': 'command-result',
        'request_id': id,
        'board_id': 'preview-fixture',
        'boot_id': foreign ? 'b' * 32 : 'a' * 32,
        'accepted': !reject,
        'error': reject ? 'preview_owned' : null,
        'pixel_corrections_preview': preview(),
        'status': status(),
      };
    });
  }

  Future<void> start() async {
    server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    requests = server.listen((request) async {
      if (request.uri.path == '/ws') {
        final socket = await WebSocketTransformer.upgrade(request);
        sockets.add(socket);
        socket.add(jsonEncode(status()));
        socket.listen((raw) {
          final q = jsonDecode(raw as String) as Map;
          if (q['type'] == 'command') {
            wsWrites++;
            final reply = apply(q);
            if (!dropWs) socket.add(jsonEncode(reply));
          } else {
            socket.add(jsonEncode(status()));
          }
        });
        return;
      }
      request.response.headers.contentType = ContentType.json;
      Object result;
      if (request.method == 'POST') {
        httpWrites++;
        final fields = Uri.splitQueryString(
          await utf8.decoder.bind(request).join(),
        );
        if (request.uri.path == '/api/command') {
          result = apply(jsonDecode(fields['packet']!) as Map);
        } else if (request.uri.path == '/api/pixels/corrections/preview') {
          executions++;
          final c =
              jsonDecode(fields['configuration']!) as Map<String, dynamic>;
          active = c['clear'] == true ? null : c;
          result = config();
        } else {
          result = status();
        }
      } else {
        result = request.uri.path == '/api/pixels/corrections'
            ? config()
            : status();
      }
      request.response.write(jsonEncode(result));
      await request.response.close();
    });
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
  for (final mode in [
    'ws',
    'same-id-fallback',
    'old-http',
    'rejected',
    'foreign',
  ]) {
    test(
      'correction preview $mode preserves one intent and exact readback',
      () async {
        final server = PreviewServer(
          command: mode != 'old-http',
          reject: mode == 'rejected',
          foreign: mode == 'foreign',
          dropWs: mode == 'same-id-fallback',
        );
        await server.start();
        final api = PakarApi(baseUrl: 'http://127.0.0.1:${server.server.port}');
        await api.status();
        final ready = Completer<void>();
        final live = api.liveStatus().listen((_) {
          if (!ready.isCompleted) ready.complete();
        });
        await ready.future.timeout(const Duration(seconds: 2));
        final controller = PixelCorrectionPreview(
          api: api,
          allowed: () => true,
          pixels: () => 480,
          configuration: () => {
            'revision': 3,
            'enabled': true,
            'rules': [
              {'kind': 'remove', 'pixel': 0, 'offset': 0},
            ],
          },
        );
        try {
          await controller.start();
          if (mode == 'rejected' || mode == 'foreign') {
            expect(controller.enabled, false);
            expect(controller.error, isNotNull);
            // A foreign WS receipt cannot settle this intent. The permitted
            // HTTP fallback retains the same peer fence and request id; it
            // must never execute another mutation or accept foreign state.
            expect(server.httpWrites, mode == 'foreign' ? 1 : 0);
            expect(server.wsWrites, 1);
            expect(server.receipts, hasLength(1));
            expect(server.executions, mode == 'foreign' ? 1 : 0);
          } else {
            expect(controller.enabled, true);
            expect(controller.observed!['active_sources'].first, 1);
            expect(server.executions, 1);
            expect(server.wsWrites, mode == 'old-http' ? 0 : 1);
            expect(
              server.httpWrites,
              (mode == 'old-http' || mode == 'same-id-fallback') ? 1 : 0,
            );
            expect(server.receipts.length, mode == 'old-http' ? 0 : 1);
          }
        } finally {
          await controller.stop();
          controller.dispose();
          await api.close();
          await live.cancel();
          await server.close();
        }
      },
    );
  }
}
