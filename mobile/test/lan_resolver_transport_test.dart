import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/lan_resolver.dart';

// Reuse mirroring_transport_test.dart's separate pure-transport isolate. A
// testWidgets registration initializes TestWidgetsFlutterBinding and replaces
// HttpClient with its deliberate HTTP400 stub, even for preceding plain tests.
// This exercises real loopback HTTP; it does not prove LAN or physical output.
void main() {
  test(
    'Hostname WebSocket conversion preserves default and explicit ports',
    () async {
      for (final entry in [
        ('http://ledboard.local', 'ws', 80),
        ('https://ledboard.local', 'wss', 443),
        ('http://ledboard.local:18081', 'ws', 18081),
      ]) {
        final api = PakarApi(baseUrl: entry.$1);
        expect(api.webSocketUri.scheme, entry.$2);
        expect(api.webSocketUri.port, entry.$3);
        expect(api.webSocketUri.hasPort, true);
        expect(api.webSocketUri.path, '/ws');
        await api.close();
      }
    },
  );
  test(
    'WebSocket establishes pinned identity without any HTTP preflight',
    () async {
      final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
      var httpReads = 0;
      final sockets = <WebSocket>[];
      final serving = server.listen((request) async {
        if (request.uri.path != '/ws') {
          httpReads++;
          request.response.statusCode = 503;
          await request.response.close();
          return;
        }
        final socket = await WebSocketTransformer.upgrade(request);
        sockets.add(socket);
        socket.listen(
          (_) => socket.add(
            jsonEncode({
              'version': '0.2.8',
              'hostname': 'ledboard',
              'board_id': 'fixture',
              'boot_id': 'boot',
            }),
          ),
        );
      });
      final api = PakarApi(baseUrl: 'http://127.0.0.1:${server.port}');
      try {
        final observed = await api.liveStatus().first.timeout(
          const Duration(seconds: 3),
        );
        expect(observed.json['board_id'], 'fixture');
        expect(httpReads, 0);
      } finally {
        await api.close();
        for (final socket in sockets) {
          await socket.close();
        }
        await server.close(force: true);
        await serving.cancel();
      }
    },
  );
  test('Native hostname connection avoids a failing DNS preflight and caches its successful route', () async {
    var lookups = 0, requests = 0;
    final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    final serving = server.listen((request) async {
      requests++;
      expect(
        request.headers.value(HttpHeaders.hostHeader),
        'localhost:${server.port}',
      );
      request.response.persistentConnection = false;
      request.response.write(
        '{"version":"0.2.8","hostname":"localhost","board_id":"fixture","boot_id":"boot"}',
      );
      await request.response.close();
    });
    final api = PakarApi(
      baseUrl: 'http://localhost:${server.port}',
      resolver: LanResolver(
        nativeFirst: true,
        lookup: (_) async {
          lookups++;
          throw const SocketException('mDNS preflight unavailable');
        },
      ),
    );
    try {
      await api.status();
      api.disconnect();
      api.connect();
      await api.status();
      expect(requests, 2);
      expect(lookups, 0);
    } finally {
      await api.close();
      await server.close(force: true);
      await serving.cancel();
    }
  });
  test('LAN proxy bypass matches donor ranges; remote origins keep configured proxy', () {
    final environment = {
      'http_proxy': 'http://192.0.2.90:10808',
      'https_proxy': 'http://192.0.2.90:10808',
    };
    for (final host in [
      'ledboard.local',
      'ledboard.local.',
      'localhost',
      'board.home.arpa',
      'board.lan',
      'board.internal',
      'board.localdomain',
      'board.localhost',
      'ledboard',
      '127.0.0.2',
      '10.2.3.4',
      '100.64.0.1',
      '169.254.3.4',
      '172.31.255.1',
      '192.168.4.1',
      '[::1]',
      '[fd00::1]',
      '[fe80::1]',
      '[fec0::1]',
    ]) {
      expect(
        controllerProxyFor(
          Uri.parse('http://$host/'),
          environment: environment,
        ),
        'DIRECT',
        reason: host,
      );
    }
    for (final host in [
      'example.com',
      'board.local.example.com',
      '192.0.2.1',
      '[2001:db8::1]',
    ]) {
      expect(
        controllerProxyFor(
          Uri.parse('https://$host/'),
          environment: environment,
        ),
        'PROXY 192.0.2.90:10808',
        reason: host,
      );
    }
  });
  test('HTTP retains authoritative hostname and route through retirement; no ownership from cache', () async {
    var lookups = 0;
    var phase = 'first status', responses = 0;
    final hosts = <String>[];
    final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    final serving = server.listen((request) async {
      hosts.add(request.headers.value(HttpHeaders.hostHeader)!);
      request.response.persistentConnection = false;
      request.response.write(
        jsonEncode({
          'version': '0.2.8',
          'hostname': 'ledboard',
          'board_id': 'fixture',
          'boot_id': 'boot',
        }),
      );
      await request.response.close();
      ++responses;
    });
    final api = PakarApi(
      baseUrl: 'http://ledboard.local:${server.port}',
      resolver: LanResolver(
        lookup: (_) async {
          ++lookups;
          return [InternetAddress.loopbackIPv4];
        },
      ),
    );
    try {
      await api.status();
      phase = 'second status after retirement';
      final revision = api.connectionRevision;
      api.disconnect();
      api.connect();
      expect(api.connectionRevision, greaterThan(revision));
      await api.status();
      expect(lookups, 1);
      expect(hosts, List.filled(2, 'ledboard.local:${server.port}'));
    } catch (error) {
      fail(
        '$phase: $error; resolver lookups=$lookups, received HTTP requests=${hosts.length}, completed responses=$responses',
      );
    } finally {
      await api.close();
      await server.close(force: true);
      await serving.cancel();
    }
  });
  test('Actual direct TLS rejects untrusted and wrong-host certificates; explicit fixture CA succeeds', () async {
    final cert = File('test/fixtures/lan_tls_cert.pem');
    final key = File('test/fixtures/lan_tls_key.pem');
    final serverContext = SecurityContext()
      ..useCertificateChainBytes(await cert.readAsBytes())
      ..usePrivateKeyBytes(await key.readAsBytes());
    final trusted = SecurityContext(withTrustedRoots: false)
      ..setTrustedCertificatesBytes(await cert.readAsBytes());
    final server = await HttpServer.bindSecure(
      InternetAddress.loopbackIPv4,
      0,
      serverContext,
    );
    var requests = 0;
    final serving = server.listen(
      (request) async {
        ++requests;
        request.response.write(
          '{"version":"0.2.8","hostname":"ledboard","board_id":"fixture","boot_id":"boot"}',
        );
        await request.response.close();
      },
      onError: (Object _) {
        // Client certificate rejection intentionally aborts the server handshake.
      },
    );
    final untrustedApi = PakarApi(baseUrl: 'https://localhost:${server.port}');
    final verifiedApi = PakarApi(
      baseUrl: 'https://localhost:${server.port}',
      resolver: LanResolver(securityContext: trusted),
    );
    final wrongHostApi = PakarApi(
      baseUrl: 'https://127.0.0.1:${server.port}',
      resolver: LanResolver(securityContext: trusted),
    );
    try {
      await expectLater(
        untrustedApi.status(),
        throwsA(isA<HandshakeException>()),
      );
      expect(requests, 0);
      expect((await verifiedApi.status()).hostname, 'ledboard');
      expect(requests, 1);
      await expectLater(
        wrongHostApi.status(),
        throwsA(isA<HandshakeException>()),
      );
      expect(requests, 1); // No authenticated HTTP request escaped invalid TLS.
    } finally {
      await untrustedApi.close();
      await verifiedApi.close();
      await wrongHostApi.close();
      await server.close(force: true);
      await serving.cancel();
    }
  });
}
