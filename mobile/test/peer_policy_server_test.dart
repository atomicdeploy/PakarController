import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:crypto/crypto.dart' as crypto;
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/app_updates.dart';
import 'package:pakarcontroller/app_updates_peer.dart';
import 'package:pakarcontroller/peer_identity.dart';

// Same real loopback + MethodChannel binding as app_updates_test.dart.
class _PeerPolicyTransportBinding extends AutomatedTestWidgetsFlutterBinding {
  @override
  bool get overrideHttpClient => false;
}

void main() {
  final binding = _PeerPolicyTransportBinding();
  final calls = <String>[];
  setUp(() {
    calls.clear();
    binding.defaultBinaryMessenger.setMockMethodCallHandler(
      AppUpdates.channel,
      (call) async {
        calls.add(call.method);
        final bytes = Uint8List.fromList([1, 2, 3, 4]);
        if (call.method == 'read') return {'offset': 0, 'bytes': bytes};
        return {
          'package': 'ir.digitalogic.pakar.pakarcontroller',
          'version_code': 15,
          'version_name': '0.2.8',
          'bytes': 4,
          'sha256': crypto.sha256.convert(bytes).toString(),
          'signers': ['b' * 64],
        };
      },
    );
  });
  tearDown(() {
    binding.defaultBinaryMessenger.setMockMethodCallHandler(
      AppUpdates.channel,
      null,
    );
  });

  test('One installation identity reuses existing app discovery ID; sessions stay distinct', () async {
    final existing = '7' * 32;
    SharedPreferences.setMockInitialValues({
      'pakar.app_updates': jsonEncode({'instance_id': existing}),
    });
    final first = PeerIdentity(), second = PeerIdentity();
    await first.initialize();
    await second.initialize();
    expect(first.installationId, existing);
    expect(second.installationId, existing);
    expect(first.sessionId, isNot(second.sessionId));
    expect(first.headers['X-Pakar-Client-Id'], existing);
    expect(first.report()['id'], existing);
  });

  test('Public identity bootstrap stays available; unsupported app RPC fails before dispatch', () async {
    final updates = AppUpdates();
    final peers = AppUpdatePeers(updates, approve: (_) async => false);
    final client = HttpClient();
    try {
      await peers.start(port: 0);
      final origin = Uri.parse('http://127.0.0.1:${peers.port}');
      final request = await client.getUrl(origin.resolve('/api/peer/info'));
      final response = await request.close();
      expect(response.headers.value('server'), PeerIdentity.current.userAgent);
      expect(response.headers.value('x-pakar-version'), '0.2.8');
      final info = jsonDecode(await utf8.decoder.bind(response).join()) as Map;
      expect(info['version'], '0.2.8');
      expect(info['compatibility']['enforced'], true);
      // HttpConnectionInfo does not expose a local interface address; do not
      // invent one from the arbitrary-interface HttpServer binding.
      expect(info['peer'], isNot(contains('reported_ip')));
      // Recovery reads remain public, even for a below-floor client. These
      // are the existing bounded self-package methods, not control adapters.
      for (final method in ['app.package.info', 'app.package.read']) {
        final read = await client.postUrl(origin.resolve('/api/rpc'));
        read.headers.set('X-Pakar-Version', '0.2.7');
        read.headers.contentType = ContentType.json;
        read.write(
          jsonEncode({
            'jsonrpc': '2.0',
            'id': 1,
            'method': method,
            'params': method == 'app.package.read'
                ? {
                    'offset': 0,
                    'length': 4,
                    'sha256': crypto.sha256.convert([1, 2, 3, 4]).toString(),
                  }
                : {},
          }),
        );
        final recovered = await read.close();
        expect(recovered.statusCode, 200);
        final result =
            jsonDecode(await utf8.decoder.bind(recovered).join()) as Map;
        expect(result['error'], isNull);
      }
      final before = calls.length;
      final rejected = await client.postUrl(origin.resolve('/api/rpc'));
      rejected.headers.set('X-Pakar-Version', '0.2.7');
      rejected.headers.contentType = ContentType.json;
      rejected.write(
        jsonEncode({
          'jsonrpc': '2.0',
          'id': 1,
          'method': 'app.upgrade.offer',
          'params': {},
        }),
      );
      final denied = await rejected.close();
      expect(denied.statusCode, 426);
      await denied.drain<void>();
      expect(calls.length, before);
      expect(peers.peerRegistry.peers, isEmpty);
    } finally {
      client.close(force: true);
      await peers.close();
      updates.dispose();
      peers.dispose();
    }
  });

  test('Supported peer introduces safe hardware/settings once; registry exposes observed source', () async {
    final updates = AppUpdates();
    final peers = AppUpdatePeers(updates, approve: (_) async => false);
    final client = HttpClient();
    try {
      await peers.start(port: 0);
      final origin = Uri.parse('http://127.0.0.1:${peers.port}');
      final report = await client.postUrl(
        origin.resolve('/api/clients/report'),
      );
      identifyRequest(report);
      report.headers.contentType = ContentType.json;
      report.write(
        jsonEncode({
          ...PeerIdentity.current.report(),
          'settings': {'theme': 'dark', 'password': 'secret-must-not-store'},
        }),
      );
      final receipt = await report.close();
      expect(receipt.statusCode, 200);
      await receipt.drain<void>();
      final list = await client.getUrl(origin.resolve('/api/clients'));
      identifyRequest(list);
      final response = await list.close();
      final json = jsonDecode(await utf8.decoder.bind(response).join()) as Map;
      final peer = (json['clients'] as List).single as Map;
      expect(peer['id'], PeerIdentity.current.installationId);
      expect(peer['observed_source'], '127.0.0.1');
      expect(jsonEncode(peer), isNot(contains('secret-must-not-store')));
    } finally {
      client.close(force: true);
      await peers.close();
      updates.dispose();
      peers.dispose();
    }
  });
}
