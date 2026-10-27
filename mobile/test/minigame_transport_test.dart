import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/minigame.dart';

import 'minigame_test.dart' show miniStatus;

void main() {
  test('Minigame post uses one correlated command endpoint with typed settings and readback', () async {
    final state =
        jsonDecode(jsonEncode(miniStatus().json)) as Map<String, dynamic>;
    state['boot_id'] = 'a' * 32;
    final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    final commands = <Map<String, dynamic>>[], paths = <String>[];
    final api = PakarApi(baseUrl: 'http://127.0.0.1:${server.port}');
    final listener = server.listen((request) async {
      request.response.headers.contentType = ContentType.json;
      if (request.method == 'POST') {
        paths.add(request.uri.path);
        final fields = Uri.splitQueryString(
          await utf8.decoder.bind(request).join(),
        );
        final packet = jsonDecode(fields['packet']!) as Map<String, dynamic>;
        commands.add(packet);
        final game = state['minigame'] as Map;
        (game['settings'] as Map).addAll(packet['settings'] as Map);
        game['revision'] = 3;
        request.response.write(
          jsonEncode({
            'type': 'command-result',
            'request_id': packet['request_id'],
            'board_id': state['board_id'],
            'boot_id': state['boot_id'],
            'accepted': true,
            'status': state,
          }),
        );
      } else {
        request.response.write(jsonEncode(state));
      }
      await request.response.close();
    });
    try {
      await api.status();
      final result = await api.postJson(minigameConfigure, {
        'settings': '{"limit_ms":30000,"hard":true}',
      }, authorize: () => true);
      expect(paths, ['/api/command']);
      expect(commands.single['command'], 'minigame-configure');
      expect(commands.single['settings'], {'limit_ms': 30000, 'hard': true});
      expect(
        (result['status'] as Map)['minigame']['settings']['limit_ms'],
        30000,
      );
    } finally {
      await api.close();
      await listener.cancel();
      await server.close(force: true);
    }
  });
}
