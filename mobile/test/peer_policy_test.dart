import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/peer_identity.dart';
import 'package:pakarcontroller/peer_policy.dart';

import 'peer_fixture.dart';

Map<String, dynamic> board({
  String version = '0.2.8',
  Map<String, dynamic>? policy,
}) => {
  ...peerFixture().json,
  'version': version,
  'board_id': 'fixture-board',
  'boot_id': 'a' * 32,
  'compatibility': ?policy,
};

void main() {
  test('Numeric SemVer ranges, prereleases and descriptive build identity', () {
    for (final version in [
      '0.2.8',
      '0.2.8+different-build',
      '0.2.10',
      '0.3.0',
      '1.0.0-beta',
    ]) {
      expect(
        () => validatePeerBoard(board(version: version), appVersion: '0.2.8'),
        returnsNormally,
      );
    }
    for (final version in [
      '0.2.6',
      '0.2.7',
      '0.2.8-alpha',
      '0.2.07',
      'garbage',
      '0.2',
      '4294967296.0.0',
    ]) {
      expect(
        () => validatePeerBoard(board(version: version), appVersion: '0.2.8'),
        throwsA(isA<UnsupportedPeerVersion>()),
      );
    }
    expect(
      PeerVersion.parse('0.2.7-beta.2')!
          .compareTo(PeerVersion.parse('0.2.7-beta.10')!),
      lessThan(0),
    );
    expect(
      () => validatePeerBoard(
        board(
          policy: {
            'minimum_app': '0.2.8',
            'maximum_app': '0.3.0',
            'extra_optional': true,
          },
        ),
        appVersion: '0.3.0',
      ),
      throwsA(isA<UnsupportedPeerVersion>()),
    );
    expect(
      () => validatePeerBoard(
        board(policy: {'minimum_app': '0.3.0', 'maximum_app': '0.2.7'}),
        appVersion: '0.3.1',
      ),
      throwsA(isA<UnsupportedPeerVersion>()),
    );
    expect(
      () => validatePeerBoard(
        {'version': null},
        appVersion: 'bad',
        enabled: false,
      ),
      returnsNormally,
    );
  });

  test('Unsupported peer never becomes online, dispatches no command and does not retry reads', () async {
    var requests = 0;
    final api = PakarApi(
      client: MockClient((request) async {
        requests++;
        expect(request.method, 'GET');
        expect(request.url.path, '/api/status');
        return http.Response(jsonEncode(board(version: '0.2.7')), 200);
      }),
    );
    try {
      await expectLater(api.status(), throwsA(isA<UnsupportedPeerVersion>()));
      expect(api.latestStatus, isNull);
      expect(api.unsupportedPeer, isNotNull);
      await expectLater(
        api.post('/api/cells', {'ids': '1', 'state': 'red'}),
        throwsA(isA<UnsupportedPeerVersion>()),
      );
      await expectLater(api.status(), throwsA(isA<UnsupportedPeerVersion>()));
      expect(requests, 1);
      api.connect(); // Explicit retry after updating a peer is allowed.
      await expectLater(api.status(), throwsA(isA<UnsupportedPeerVersion>()));
      expect(requests, 2);
    } finally {
      await api.close();
    }
  });

  test(
    'Version enforcement override does not disable hostname identity checks',
    () async {
      final api = PakarApi(
        enforcePeerVersions: false,
        client: MockClient(
          (_) async => http.Response(
            jsonEncode({...board(version: '0.0.1'), 'hostname': 'foreign'}),
            200,
          ),
        ),
      );
      try {
        await expectLater(api.status(), throwsFormatException);
      } finally {
        await api.close();
      }
    },
  );

  test('HTTP identity headers sent; relay Server version cannot replace board JSON', () async {
    final api = PakarApi(
      client: MockClient((request) async {
        expect(
          request.headers['User-Agent'],
          contains('PakarController/0.2.8'),
        );
        expect(request.headers['X-Pakar-Version'], '0.2.8');
        expect(request.headers['X-Pakar-Build'], '15');
        expect(request.headers.containsKey('X-Pakar-Client-Build'), isFalse);
        expect(request.headers['X-Pakar-Client'], 'pakarcontroller-mobile');
        expect(
          request.headers['X-Pakar-Client-Id'],
          PeerIdentity.current.installationId,
        );
        return http.Response(
          jsonEncode({
            ...board(),
            'new_optional_capability': {'x': 1},
          }),
          200,
          headers: {
            'server': 'PakarController/9.4.0',
            'x-pakar-version': '9.4.0',
            'x-pakar-board-version': '0.2.8',
          },
        );
      }),
    );
    try {
      expect((await api.status()).json['version'], '0.2.8');
      expect(api.receivedServerHeaders['server'], 'PakarController/9.4.0');
      expect(api.receivedServerHeaders['x-pakar-board-version'], '0.2.8');
      expect(api.selectedRouteLocalIp, isNull); // MockClient is not a socket.
      expect(PeerIdentity.current.report(), isNot(contains('reported_ip')));
    } finally {
      await api.close();
    }
  });

  test('WebSocket introduces one bounded peer report per verified boot, not per frame', () async {
    final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    final api = PakarApi(baseUrl: 'http://127.0.0.1:${server.port}');
    final introduced = Completer<void>();
    var reports = 0;
    String? actualClientIp;
    WebSocket? socket;
    final state = board(policy: peerCompatibility());
    final listener = server.listen((request) async {
      expect(request.uri.path, '/ws');
      expect(
        request.headers.value('user-agent'),
        PeerIdentity.current.userAgent,
      );
      expect(
        request.headers.value('x-pakar-client-session'),
        PeerIdentity.current.sessionId,
      );
      expect(request.headers.value('x-pakar-build'), '15');
      expect(request.headers.value('x-pakar-client-build'), isNull);
      actualClientIp = request.connectionInfo?.remoteAddress.address;
      socket = await WebSocketTransformer.upgrade(request);
      socket!.listen((message) {
        final packet = jsonDecode(message as String) as Map;
        if (packet['type'] == 'status') socket!.add(jsonEncode(state));
        if (packet['type'] == 'peer-introduction') {
          reports++;
          expect(utf8.encode(message).length, lessThanOrEqualTo(1300));
          final peer = packet['report'] as Map;
          expect(peer['version'], PeerIdentity.current.version);
          expect(peer['id'], PeerIdentity.current.installationId);
          expect(peer['reported_ip'], actualClientIp);
          expect(peer['settings'], isNot(contains('password')));
          socket!.add(jsonEncode(state));
          if (!introduced.isCompleted) introduced.complete();
        }
      });
    });
    final live = api.liveStatus().listen((_) {});
    try {
      await introduced.future.timeout(const Duration(seconds: 3));
      api.requestLiveStatus();
      await Future<void>.delayed(const Duration(milliseconds: 50));
      expect(reports, 1);
      expect(api.latestStatus, isNotNull);
    } finally {
      await api.close();
      await live.cancel();
      await socket?.close();
      await listener.cancel();
      await server.close(force: true);
    }
  });

  test('HTTP reports only the verified selected socket address and revokes it on disconnect', () async {
    final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    final api = PakarApi(baseUrl: 'http://localhost:${server.port}');
    final reported = Completer<void>();
    String? actualIp;
    final listener = server.listen((request) async {
      actualIp = request.connectionInfo?.remoteAddress.address;
      if (request.uri.path == '/api/status') {
        request.response.headers.contentType = ContentType.json;
        request.response.write(jsonEncode(board(policy: peerCompatibility())));
      } else {
        expect(request.uri.path, '/api/clients/report');
        expect(request.headers.value('x-pakar-build'), '15');
        final form = Uri.splitQueryString(
          await utf8.decoder.bind(request).join(),
        );
        final report = jsonDecode(form['report']!) as Map;
        expect(report['reported_ip'], actualIp);
        expect(report['settings']['theme'], 'dark');
        request.response.write('{"accepted":true}');
        if (!reported.isCompleted) reported.complete();
      }
      await request.response.close();
    });
    try {
      expect(api.selectedRouteLocalIp, isNull);
      await api.status();
      await reported.future.timeout(const Duration(seconds: 3));
      expect(api.selectedRouteLocalIp, actualIp);
      api.disconnect();
      expect(api.selectedRouteLocalIp, isNull);
      expect(
        PeerIdentity.current.report(transport: 'bluetooth'),
        isNot(contains('reported_ip')),
      );
    } finally {
      await api.close();
      await listener.cancel();
      await server.close(force: true);
    }
  });

  test(
    'Remote WebSocket default emits one proper User-Agent plus client metadata',
    () async {
      final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
      WebSocket? accepted;
      final listener = server.listen((request) async {
        expect(
          request.headers.value('user-agent'),
          PeerIdentity.current.userAgent,
        );
        expect(
          request.headers.value('x-pakar-version'),
          PeerIdentity.current.version,
        );
        accepted = await WebSocketTransformer.upgrade(request);
        accepted!.add('accepted');
      });
      WebSocket? socket;
      try {
        socket = await WebSocket.connect(
          'ws://127.0.0.1:${server.port}/feed',
          headers: PeerIdentity.current.webSocketHeaders,
        );
        expect(await socket.first, 'accepted');
      } finally {
        await socket?.close();
        await accepted?.close();
        await listener.cancel();
        await server.close(force: true);
      }
    },
  );

  test('An unsupported full WebSocket status revokes ownership before publishing online', () async {
    final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    final api = PakarApi(baseUrl: 'http://127.0.0.1:${server.port}');
    var httpReads = 0, observations = 0;
    WebSocket? socket;
    final listener = server.listen((request) async {
      if (request.uri.path != '/ws') {
        httpReads++;
        request.response.statusCode = 500;
        await request.response.close();
        return;
      }
      socket = await WebSocketTransformer.upgrade(request);
      socket!.listen((_) => socket!.add(jsonEncode(board(version: '0.2.7'))));
    });
    final updates = api.statusUpdates.listen((_) => observations++);
    final owner = api.connectionRevision;
    try {
      await expectLater(
        api.liveStatus().first,
        throwsA(isA<UnsupportedPeerVersion>()),
      );
      expect(api.connectionRevision, greaterThan(owner));
      expect(observations, 0);
      expect(api.latestStatus, isNull);
      await expectLater(api.status(), throwsA(isA<UnsupportedPeerVersion>()));
      expect(httpReads, 0);
    } finally {
      await updates.cancel();
      await api.close();
      await socket?.close();
      await listener.cancel();
      await server.close(force: true);
    }
  });

  test('Peer registry separates observed source and filters secrets and unknown settings', () {
    final registry = PeerRegistry();
    registry.observe({
      ...PeerIdentity.current.report(),
      'reported_ip': '192.0.2.44',
      'password': 'never-store',
      'settings': {
        'theme': 'dark',
        'password': 'never-store',
        'version_check': true,
      },
      'hardware': {'model': 'fixture', 'serial_number': 'never-store'},
    }, source: '127.0.0.1');
    final peer = registry.peers.single;
    expect(peer['observed_source'], '127.0.0.1');
    expect(peer['reported_ip'], '192.0.2.44');
    expect(peer['ip_self_reported'], true);
    expect(jsonEncode(peer), isNot(contains('never-store')));
    expect(() => registry.observe({'id': 'bad'}), throwsFormatException);
  });
}
