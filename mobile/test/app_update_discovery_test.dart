import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/app_update_discovery.dart';

// These fixtures verify actual bounded loopback TCP, not Flutter's synthetic
// HTTP-400 client. Keep the messenger binding for native-channel test doubles.
class _SocketTestBinding extends AutomatedTestWidgetsFlutterBinding {
  @override
  bool get overrideHttpClient => false;
}

Map<String, dynamic> manifest({String instance = 'fixture-package-instance'}) =>
    {
      'product': 'PakarController',
      'kind': 'android-app',
      'instance_id': instance,
      'package_sharing': true,
      'rpc': '/api/rpc',
      'app_updates': {
        'package': appUpdatePackage,
        'version_code': 8,
        'version_name': '0.2.8',
        'bytes': 1024,
        'sha256': 'a' * 64,
        'signers': ['b' * 64],
        'chunk_bytes': 49152,
      },
    };

void main() {
  _SocketTestBinding();

  test('Actual manifest GET verifies package, instance and shape, not an install grant', () async {
    final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    final paths = <String>[];
    var value = manifest();
    var redirect = false, oversized = false;
    final subscription = server.listen((request) async {
      paths.add(request.uri.path);
      expect(request.method, 'GET');
      expect(request.headers.value('authorization'), isNull);
      if (redirect) {
        request.response.statusCode = 302;
        request.response.headers.set('location', '/must-not-follow');
      } else {
        request.response.write(oversized ? 'x' * 17000 : jsonEncode(value));
      }
      await request.response.close();
    });
    final candidate = AppUpdatePeer(
      name: 'Phone',
      origin: 'http://127.0.0.1:${server.port}',
      instanceId: 'fixture-package-instance',
      protocols: const {'DNS-SD'},
    );
    try {
      final verified = await verifyAppUpdatePeer(candidate);
      expect(verified?.verified, true);
      expect(verified?.manifest['app_updates']['package'], appUpdatePackage);
      value['instance_id'] = 'different-instance';
      expect(await verifyAppUpdatePeer(candidate), isNull);
      value = manifest();
      value['app_updates']['package'] = 'unrelated.package';
      expect(await verifyAppUpdatePeer(candidate), isNull);
      value = manifest();
      value['app_updates']['signers'] = ['not-a-signer-digest'];
      expect(await verifyAppUpdatePeer(candidate), isNull);
      value = manifest();
      value['kind'] = 'board';
      expect(await verifyAppUpdatePeer(candidate), isNull);
      value = manifest();
      value['rpc'] = 'https://other-host/rpc';
      expect(await verifyAppUpdatePeer(candidate), isNull);
      value = manifest();
      oversized = true;
      expect(await verifyAppUpdatePeer(candidate), isNull);
      oversized = false;
      redirect = true;
      expect(await verifyAppUpdatePeer(candidate), isNull);
      expect(paths, everyElement('/api/app-updates'));
    } finally {
      await subscription.cancel();
      await server.close(force: true);
    }
  });

  test('SSDP requires exact mobile identity, source-pinned location and unique headers', () {
    Uint8List packet(String value) => Uint8List.fromList(utf8.encode(value));
    const response =
        'HTTP/1.1 200 OK\r\n'
        'ST: urn:atomicdeploy:device:PakarControllerApp\r\n'
        'LOCATION: http://192.0.2.8:18992/api/app-updates\r\n'
        'X-Pakar-Product: pakarcontroller-mobile\r\n'
        'X-Pakar-Instance: phone-instance\r\nX-Pakar-Name: Phone\r\n';
    final candidate = parseAppUpdateSsdp(packet(response), '192.0.2.8');
    expect(candidate?.origin, 'http://192.0.2.8:18992');
    expect(candidate?.verified, false);
    expect(parseAppUpdateSsdp(packet(response), '192.0.2.9'), isNull);
    expect(
      parseAppUpdateSsdp(
        packet('$response\nLOCATION: http://192.0.2.8:80/\r\n'),
        '192.0.2.8',
      ),
      isNull,
    );
    expect(
      parseAppUpdateSsdp(
        packet(response.replaceAll('pakarcontroller-mobile', 'router')),
        '192.0.2.8',
      ),
      isNull,
    );
    expect(
      parseAppUpdateSsdp(
        packet(response.replaceAll('/api/app-updates', '/api/status')),
        '192.0.2.8',
      ),
      isNull,
    );
  });

  test(
    'MNDP has explicit application port, distinct product and bounded TLVs',
    () {
      Uint8List packet({
        String product = 'pakarcontroller-mobile',
        bool port = true,
      }) {
        final out = BytesBuilder()..add([0, 0, 0, 1]);
        void field(int type, List<int> value) {
          out.add([
            type >> 8,
            type & 255,
            value.length >> 8,
            value.length & 255,
          ]);
          out.add(value);
        }

        field(1, [2, 0, 0, 0, 0, 1]);
        field(5, utf8.encode('Phone'));
        field(11, utf8.encode('phone-instance'));
        field(0xff01, utf8.encode(product));
        if (port) field(0xff02, [18992 >> 8, 18992 & 255]);
        return out.takeBytes();
      }

      final candidate = parseAppUpdateMndp(packet(), '192.0.2.8');
      expect(candidate?.origin, 'http://192.0.2.8:18992');
      expect(candidate?.verified, false);
      expect(parseAppUpdateMndp(packet(port: false), '192.0.2.8'), isNull);
      expect(
        parseAppUpdateMndp(packet(product: 'pakarcontroller'), '192.0.2.8'),
        isNull,
      );
      expect(parseAppUpdateMndp(packet(), '239.0.0.1'), isNull);
      expect(
        parseAppUpdateMndp(Uint8List.fromList([...packet(), 0]), '192.0.2.8'),
        isNull,
      );
    },
  );

  test('Verified same-instance routes coalesce; mismatched package manifests do not', () async {
    var conflict = false;
    final discovery = AppUpdateDiscovery(
      advertisements: () async => const [
        AppUpdatePeer(
          name: 'Phone',
          origin: 'http://192.0.2.8:18992',
          instanceId: 'phone-instance',
          protocols: {'SSDP'},
        ),
        AppUpdatePeer(
          name: 'Phone',
          origin: 'http://192.0.2.9:18992',
          instanceId: 'phone-instance',
          protocols: {'DNS-SD'},
        ),
      ],
      verify: (candidate) async {
        final value = manifest(instance: candidate.instanceId);
        if (conflict && candidate.origin.contains('.9:')) {
          value['app_updates']['sha256'] = 'c' * 64;
        }
        return AppUpdatePeer(
          name: candidate.name,
          origin: candidate.origin,
          instanceId: candidate.instanceId,
          verified: true,
          manifest: value,
          protocols: candidate.protocols,
        );
      },
    );
    try {
      await discovery.scan();
      expect(discovery.scanning, false);
      expect(discovery.peers, hasLength(1));
      expect(discovery.peers.single.protocols, {'SSDP', 'DNS-SD'});
      conflict = true;
      await discovery.scan();
      expect(discovery.peers, isEmpty);
    } finally {
      discovery.dispose();
    }
  });

  test(
    'Late scan completion cannot notify or repopulate a disposed owner',
    () async {
      final gate = Completer<List<AppUpdatePeer>>();
      final discovery = AppUpdateDiscovery(advertisements: () => gate.future);
      var notifications = 0;
      discovery.addListener(() => notifications++);
      final scan = discovery.scan();
      expect(notifications, 1);
      discovery.dispose();
      gate.complete([]);
      await scan;
      expect(notifications, 1);
      expect(discovery.peers, isEmpty);
    },
  );
}
