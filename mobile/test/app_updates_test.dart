import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:crypto/crypto.dart' as crypto;
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/app_updates.dart';
import 'package:pakarcontroller/app_updates_peer.dart';

const signer =
    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa';
Map<String, dynamic> manifest(Uint8List bytes, {int version = 8}) => {
  'package': 'ir.digitalogic.pakar.pakarcontroller',
  'version_code': version,
  'version_name': 'fixture',
  'bytes': bytes.length,
  'sha256': crypto.sha256.convert(bytes).toString(),
  'signers': [signer],
};

// As in mirroring_transport_test.dart, these are actual bounded loopback
// sockets. Keep the MethodChannel test messenger without Flutter's unrelated
// blanket HTTP-400 override; never reinterpret that override as a real server.
class _AppUpdateTransportBinding extends AutomatedTestWidgetsFlutterBinding {
  @override
  bool get overrideHttpClient => false;
}

void main() {
  final binding = _AppUpdateTransportBinding();
  final calls = <MethodCall>[];
  final installed = Uint8List.fromList([0x50, 0x4b, 1, 2, 3]);
  var incoming = <int>[];
  var expected = <String, dynamic>{};
  var installAllowed = true;
  setUp(() {
    calls.clear();
    incoming = [];
    expected = {};
    installAllowed = true;
    binding.defaultBinaryMessenger.setMockMethodCallHandler(
      AppUpdates.channel,
      (call) async {
        calls.add(call);
        final args = call.arguments is Map
            ? object(call.arguments)
            : <String, dynamic>{};
        switch (call.method) {
          case 'info':
            return manifest(installed, version: 7);
          case 'begin':
            expected = args;
            return {
              'state': 'receiving',
              'transfer_id': 'fixture',
              'received': 0,
              ...args,
            };
          case 'write':
            expect(args['transfer_id'], 'fixture');
            expect(args['offset'], incoming.length);
            expect(args['bytes'], isA<Uint8List>());
            incoming.addAll((args['bytes'] as Uint8List));
            return {
              'state': 'receiving',
              'transfer_id': 'fixture',
              'received': incoming.length,
              ...expected,
            };
          case 'finish':
            if (incoming.length != expected['bytes'] ||
                crypto.sha256.convert(incoming).toString() !=
                    expected['sha256']) {
              throw PlatformException(
                code: 'upgrade_rejected',
                message: 'APK digest mismatch',
              );
            }
            return {
              'state': 'verified',
              'transfer_id': 'fixture',
              'install_permission': false,
              ...expected,
            };
          case 'abort':
            return {'state': 'idle', 'aborted': true};
          case 'status':
            return {
              'state': 'idle',
              'install_status': '{"status":-1,"session_id":23}',
            };
          case 'permission':
            return {
              'state': 'verified',
              'transfer_id': 'fixture',
              'install_permission': true,
            };
          case 'install':
            expect(args['transfer_id'], 'fixture');
            if (!installAllowed) {
              return {
                'state': 'permission_required',
                'transfer_id': 'fixture',
                'install_permission': false,
              };
            }
            return {'state': 'awaiting_android', 'session_id': 23};
          case 'pick':
            return {
              'state': 'verified',
              'transfer_id': 'fixture',
              'install_permission': false,
            };
          case 'export':
            return {'exported': true, ...manifest(installed, version: 7)};
          case 'read':
            expect(args['sha256'], crypto.sha256.convert(installed).toString());
            final offset = args['offset'] as int;
            return {
              'offset': offset,
              'bytes': Uint8List.fromList(installed.sublist(offset)),
              'eof': true,
            };
          default:
            throw StateError('Unexpected native operation ${call.method}');
        }
      },
    );
  });
  tearDown(
    () => binding.defaultBinaryMessenger.setMockMethodCallHandler(
      AppUpdates.channel,
      null,
    ),
  );

  test(
    'newer same-package signer manifest only; no weak/default compatibility',
    () {
      final local = manifest(installed, version: 7);
      validateAppUpdateManifest(manifest(installed), local);
      for (final patch in [
        {'version_code': 7},
        {'version_code': 6},
        {'bytes': 0},
        {'package': 'another.app'},
        {
          'signers': ['b' * 64],
        },
        {'sha256': 'unknown'},
      ]) {
        expect(
          () => validateAppUpdateManifest({
            ...manifest(installed),
            ...patch,
          }, local),
          throwsFormatException,
        );
      }
      expect(calls, isEmpty);
    },
  );
  test('only local HTTP or remote HTTPS and direct peer origins', () {
    expect(
      validateAppUpdateUrl('http://ledboard.local/PakarController.apk').host,
      'ledboard.local',
    );
    expect(
      validateAppUpdateUrl('http://192.168.1.2:18992/app.apk').host,
      '192.168.1.2',
    );
    expect(
      validateAppUpdateUrl('https://updates.example/app.apk').scheme,
      'https',
    );
    for (final value in [
      'http://updates.example/app.apk',
      'https://user:secret@host/app.apk',
      'file:///tmp/a.apk',
    ]) {
      expect(() => validateAppUpdateUrl(value), throwsFormatException);
    }
    expect(
      () => peerOrigin('http://239.255.255.250:18992'),
      throwsFormatException,
    );
    expect(() => peerOrigin('http://host.local/other'), throwsFormatException);
  });
  test('native bridge refuses oversized chunks before dispatch', () async {
    final updates = AppUpdates();
    addTearDown(updates.dispose);
    await expectLater(
      updates.rpc('app.upgrade.write', {
        'data': base64Encode(Uint8List(AppUpdates.chunkSize + 1)),
      }),
      throwsFormatException,
    );
    await expectLater(
      updates.rpc('unrelated.install', {}),
      throwsFormatException,
    );
    expect(calls, isEmpty);
  });
  test(
    'actual HTTP streamed download has measured receipt and never installs',
    () async {
      final bytes = Uint8List.fromList(List.generate(50000, (i) => i % 256));
      final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
      addTearDown(() => server.close(force: true));
      server.listen((r) async {
        r.response.contentLength = bytes.length;
        r.response.add(bytes);
        await r.response.close();
      });
      final updates = AppUpdates();
      addTearDown(updates.dispose);
      final result = await updates.download(
        'http://127.0.0.1:${server.port}/app.apk',
        bytes: bytes.length,
        sha256: crypto.sha256.convert(bytes).toString(),
      );
      expect(result['state'], 'verified');
      expect(incoming, bytes);
      expect(calls.where((c) => c.method == 'write').length, greaterThan(1));
      expect(
        calls.any((c) => c.method == 'install' || c.method == 'permission'),
        false,
      );
      expect(updates.busy, false);
    },
  );
  test('truncated HTTP is aborted once, never retried/installed', () async {
    var requests = 0;
    final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    addTearDown(() => server.close(force: true));
    server.listen((r) async {
      requests++;
      r.response.add([1, 2]);
      await r.response.close();
    });
    final updates = AppUpdates();
    addTearDown(updates.dispose);
    await expectLater(
      updates.download(
        'http://127.0.0.1:${server.port}/app.apk',
        bytes: 4,
        sha256: 'b' * 64,
      ),
      throwsFormatException,
    );
    expect(requests, 1);
    expect(calls.where((c) => c.method == 'abort').length, 1);
    expect(
      calls.any((c) => c.method == 'finish' || c.method == 'install'),
      false,
    );
  });
  test('SAF staging, unknown-source permission and installer are distinct local actions', () async {
    final updates = AppUpdates();
    addTearDown(updates.dispose);
    await updates.pickLocal();
    expect(calls.map((c) => c.method), ['pick']);
    await updates.permission();
    expect(updates.state['install_permission'], true);
    await updates.install();
    expect(updates.state['state'], 'awaiting_android');
    await updates.refresh();
    expect(updates.state['state'], 'android_receipt');
    expect(
      updates.state['status'],
      -1,
    ); // Pending user action is not installed.
    expect(calls.map((c) => c.method), [
      'pick',
      'permission',
      'install',
      'status',
    ]);
  });
  test('permission-required retry keeps the exact verified native transfer', () async {
    final updates = AppUpdates();
    addTearDown(updates.dispose);
    await updates.pickLocal();
    installAllowed = false;
    await updates.install();
    expect(updates.state['state'], 'permission_required');
    expect(updates.state['transfer_id'], 'fixture');
    // Re-entering the native installer must remain possible; native install()
    // still calls finish(args) to revalidate this same artifact before commit.
    await updates.install();
    expect(updates.state['state'], 'permission_required');
    await updates.permission();
    installAllowed = true;
    await updates.install();
    expect(updates.state['state'], 'awaiting_android');
    expect(calls.map((c) => c.method), [
      'pick',
      'install',
      'install',
      'permission',
      'install',
    ]);
  });
  test('peer approval stages one pinned APK; bearer expires at finish and installer is never shared', () async {
    final updates = AppUpdates();
    addTearDown(updates.dispose);
    var approved = 0;
    final peers = AppUpdatePeers(
      updates,
      approve: (m) async {
        approved++;
        return true;
      },
    );
    addTearDown(peers.dispose);
    await peers.start(port: 0);
    final client = HttpClient();
    addTearDown(() => client.close(force: true));
    final origin = Uri.parse('http://127.0.0.1:${peers.port}');
    final observed = await boundedPeerGet(
      client,
      origin.resolve('/api/app-updates'),
    );
    expect(observed['kind'], 'android-app');
    expect(object(observed['app_updates'])['version_code'], 7);
    final offer = await peerRpc(
      client,
      origin,
      'app.upgrade.offer',
      manifest(installed),
    );
    expect(approved, 1);
    expect(offer['sha256'], crypto.sha256.convert(installed).toString());
    final params = {
      'transfer_id': 'fixture',
      'offset': 0,
      'data': base64Encode(installed),
    };
    await expectLater(
      peerRpc(client, origin, 'app.upgrade.write', params, capability: 'wrong'),
      throwsFormatException,
    );
    expect(calls.where((c) => c.method == 'write'), isEmpty);
    await peerRpc(
      client,
      origin,
      'app.upgrade.write',
      params,
      capability: offer['capability'],
    );
    final result = await peerRpc(client, origin, 'app.upgrade.finish', {
      'transfer_id': 'fixture',
    }, capability: offer['capability']);
    expect(result['state'], 'verified');
    await expectLater(
      peerRpc(client, origin, 'app.upgrade.status', {
        'transfer_id': 'fixture',
      }, capability: offer['capability']),
      throwsFormatException,
    );
    await expectLater(
      peerRpc(client, origin, 'app.upgrade.install', {
        'transfer_id': 'fixture',
      }, capability: offer['capability']),
      throwsFormatException,
    );
    expect(calls.any((c) => c.method == 'install'), false);
  });
  test(
    'declining peer approval never creates a staging file or capability',
    () async {
      final updates = AppUpdates();
      addTearDown(updates.dispose);
      final peers = AppUpdatePeers(updates, approve: (_) async => false);
      addTearDown(peers.dispose);
      await peers.start(port: 0);
      await expectLater(
        peers.handleRpc('app.upgrade.offer', manifest(installed)),
        throwsStateError,
      );
      expect(calls.any((c) => c.method == 'begin'), false);
    },
  );
  test('discovered offer is pinned again before pull; changed peer never creates staging', () async {
    final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    addTearDown(() => server.close(force: true));
    server.listen((request) async {
      request.response.headers.contentType = ContentType.json;
      if (request.uri.path == '/healthz') {
        request.response.write(
          jsonEncode({
            'status': 'ok',
            'service': 'pakarcontroller-mobile',
            'version': '0.2.8',
          }),
        );
      } else {
        final rpc = object(jsonDecode(await utf8.decoder.bind(request).join()));
        request.response.write(
          jsonEncode({
            'jsonrpc': '2.0',
            'id': rpc['id'],
            'result': manifest(installed),
          }),
        );
      }
      await request.response.close();
    });
    final updates = AppUpdates();
    addTearDown(updates.dispose);
    await expectLater(
      updates.pullPeer(
        'http://127.0.0.1:${server.port}',
        expectedManifest: {...manifest(installed), 'sha256': 'b' * 64},
      ),
      throwsFormatException,
    );
    expect(calls.any((c) => c.method == 'begin'), false);
  });
  test(
    'absolute response deadline cancels a trickling peer subscription',
    () async {
      var cancelled = false;
      final input = StreamController<List<int>>(
        onCancel: () {
          cancelled = true;
        },
      );
      final result = readAppUpdateBytes(
        input.stream,
        deadline: const Duration(milliseconds: 20),
      );
      input.add([1]);
      await expectLater(result, throwsA(isA<TimeoutException>()));
      expect(cancelled, true);
      await input.close();
    },
  );
}
