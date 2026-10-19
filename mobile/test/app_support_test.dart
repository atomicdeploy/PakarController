import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:crypto/crypto.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/app_support.dart';
import 'package:pakarcontroller/models.dart';

import 'peer_fixture.dart';

class SupportApi extends PakarApi {
  SupportApi(this.current);
  DeviceStatus current;
  int calls = 0;
  bool fail = false;
  @override
  Future<DeviceStatus?> fastCommand(
    String path,
    Map<String, String> values, {
    required bool Function() authorize,
  }) async {
    if (!authorize()) throw const StaleConnection();
    calls++;
    if (fail) {
      throw const OperationUncertain('secret URL must never be returned');
    }
    return current;
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  // These are real bounded loopback sockets, not widget HTTP fixtures.
  HttpOverrides.global = null;
  Map<String, dynamic> envelope(
    AppSupportSession service,
    String id,
    String operation,
  ) => {
    'session': service.session,
    'id': id,
    'operation': operation,
    'expires_at_ms': DateTime.now().millisecondsSinceEpoch + 10000,
  };
  Future<(int, Map<String, dynamic>)> request(
    AppSupportSession service, {
    String? token,
    Map<String, dynamic>? body,
    String? origin,
  }) async {
    final client = HttpClient();
    try {
      final uri = Uri.parse(
        'http://127.0.0.1:${service.port}/api/support/${body == null ? 'health' : 'request'}',
      );
      final r = await client.openUrl(body == null ? 'GET' : 'POST', uri);
      if (token != null) r.headers.set('Authorization', 'Bearer $token');
      if (origin != null) r.headers.set('Origin', origin);
      if (body != null) r.write(jsonEncode(body));
      final response = await r.close().timeout(const Duration(seconds: 3));
      final raw = await utf8.decoder.bind(response).join();
      return (
        response.statusCode,
        raw.isEmpty
            ? <String, dynamic>{}
            : jsonDecode(raw) as Map<String, dynamic>,
      );
    } finally {
      client.close(force: true);
    }
  }

  test(
    'Opt-in support health works without board, authenticates and redacts',
    () async {
      final service = AppSupportSession(
        api: () => null,
        observed: () => null,
        health: () => {
          'connected': false,
          'selection_count': 2,
          'password': 'secret',
          'url': 'https://token@private',
        },
      );
      try {
        await service.start();
        expect(service.token.length, 64);
        expect((await request(service)).$1, 403);
        expect(
          (await request(
            service,
            token: service.token,
            origin: 'https://untrusted',
          )).$1,
          403,
        );
        final result = await request(service, token: service.token);
        expect(result.$1, 200);
        expect(result.$2['board_available'], false);
        expect(result.$2['health'], {'connected': false, 'selection_count': 2});
        expect(jsonEncode(result.$2), isNot(contains('secret')));
        expect(result.$2.containsKey('token'), false);
      } finally {
        await service.stop();
        service.dispose();
      }
    },
  );
  test('HTTP and WS share replay policy; rotation and foreground revoke credentials', () async {
    final service = AppSupportSession(
      api: () => null,
      observed: () => null,
      health: () => {},
    );
    WebSocket? socket;
    try {
      await service.start();
      final oldToken = service.token, oldSession = service.session;
      final packet = envelope(service, 'same', 'health');
      expect(
        (await request(service, token: oldToken, body: packet)).$2['accepted'],
        true,
      );
      socket = await WebSocket.connect(
        'ws://127.0.0.1:${service.port}/api/support/ws',
        headers: {'Authorization': 'Bearer $oldToken'},
      );
      final replies = StreamIterator(socket);
      socket.add(jsonEncode(packet));
      expect(
        await replies.moveNext().timeout(const Duration(seconds: 3)),
        true,
      );
      expect(jsonDecode(replies.current as String)['accepted'], false);
      await replies.cancel();
      await service.start();
      expect(service.token, isNot(oldToken));
      expect(service.session, isNot(oldSession));
      expect((await request(service, token: oldToken)).$1, 403);
      service.setForeground(false);
      expect(service.running, false);
      expect(service.token, isEmpty);
      service.setForeground(true);
      expect(service.running, false);
    } finally {
      await socket?.close();
      await service.stop();
      service.dispose();
    }
  });
  test('Commands require capability, peer, deadline and fresh owner; uncertainty halts writes', () async {
    var current = DeviceStatus({
      ...peerFixture(features: ['command-ack', 'leds', 'table']).json,
      'board_id': 'board',
      'boot_id': 'boot',
    });
    final api = SupportApi(current);
    final service = AppSupportSession(
      api: () => api,
      observed: () => current,
      health: () => {},
    );
    try {
      await service.start(commands: true);
      Map<String, dynamic> command(String id) => {
        ...envelope(service, id, 'command'),
        'board_id': 'board',
        'boot_id': 'boot',
        'path': '/api/effect',
        'values': {'pattern': 'rainbow'},
      };
      final unsupported = command('bad')..['path'] = '/api/reset';
      expect(
        (await request(
          service,
          token: service.token,
          body: unsupported,
        )).$2['accepted'],
        false,
      );
      final expired = command('expired')..['expires_at_ms'] = 0;
      expect(
        (await request(
          service,
          token: service.token,
          body: expired,
        )).$2['accepted'],
        false,
      );
      expect(api.calls, 0);
      expect(
        (await request(
          service,
          token: service.token,
          body: command('ok'),
        )).$2['accepted'],
        true,
      );
      expect(api.calls, 1);
      current = DeviceStatus({...current.json, 'boot_id': 'different'});
      expect(
        (await request(
          service,
          token: service.token,
          body: command('stale'),
        )).$2['accepted'],
        false,
      );
      expect(api.calls, 1);
      current = api.current;
      api.fail = true;
      final uncertain = (await request(
        service,
        token: service.token,
        body: command('lost'),
      )).$2;
      expect(uncertain['error'], 'outcome_unverified');
      expect(jsonEncode(uncertain), isNot(contains('secret')));
      expect(service.commandsEnabled, false);
      expect(
        (await request(
          service,
          token: service.token,
          body: command('again'),
        )).$2['accepted'],
        false,
      );
      expect(api.calls, 2);
    } finally {
      await service.stop();
      service.dispose();
      await api.close();
    }
  });
  test('Assets are opt-in, hash verified and revoked before commit', () async {
    var applies = 0;
    final ready = Completer<void>(), release = Completer<void>();
    late AppSupportSession service;
    service = AppSupportSession(
      api: () => null,
      observed: () => null,
      health: () => {},
      applyAssets: (bundle, hash, authorize) async {
        ready.complete();
        await release.future;
        if (authorize()) applies++;
      },
    );
    try {
      const bundle = '{"palette":{"accent":"#112233"}}';
      final hash = sha256.convert(utf8.encode(bundle)).toString();
      await service.start(assets: true);
      final packet = {
        ...envelope(service, 'assets', 'assets.apply'),
        'bundle': bundle,
        'sha256': hash,
      };
      final pending = request(
        service,
        token: service.token,
        body: packet,
      ).then<void>((_) {}, onError: (Object _) {});
      await ready.future.timeout(const Duration(seconds: 3));
      service.setForeground(false);
      release.complete();
      await pending;
      expect(applies, 0);
    } finally {
      if (!release.isCompleted) release.complete();
      await service.stop();
      service.dispose();
    }
  });
  test(
    'Assets reject disabled/mismatched hashes and apply exactly once',
    () async {
      var applies = 0;
      final service = AppSupportSession(
        api: () => null,
        observed: () => null,
        health: () => {},
        applyAssets: (bundle, hash, authorize) async {
          if (authorize()) applies++;
        },
      );
      try {
        const bundle = '{"palette":{"accent":"#112233"}}';
        final digest = sha256.convert(utf8.encode(bundle)).toString();
        Map<String, dynamic> packet(String id) => {
          ...envelope(service, id, 'assets.apply'),
          'bundle': bundle,
          'sha256': digest,
        };
        await service.start();
        expect(
          (await request(
            service,
            token: service.token,
            body: packet('disabled'),
          )).$2['accepted'],
          false,
        );
        await service.start(assets: true);
        final bad = packet('bad')..['sha256'] = '0' * 64;
        expect(
          (await request(
            service,
            token: service.token,
            body: bad,
          )).$2['accepted'],
          false,
        );
        final good = packet('good');
        expect(
          (await request(
            service,
            token: service.token,
            body: good,
          )).$2['accepted'],
          true,
        );
        expect(
          (await request(
            service,
            token: service.token,
            body: good,
          )).$2['accepted'],
          false,
        );
        expect(applies, 1);
      } finally {
        await service.stop();
        service.dispose();
      }
    },
  );
}
