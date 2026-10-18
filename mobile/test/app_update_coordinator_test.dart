import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/app_update_coordinator.dart';
import 'package:pakarcontroller/app_update_discovery.dart';
import 'package:pakarcontroller/app_updates.dart';

// Retain platform messenger support while allowing the HTTP fixture to use its
// real loopback server, rather than the binding's default HTTP-400 stub.
class _SocketTestBinding extends AutomatedTestWidgetsFlutterBinding {
  @override
  bool get overrideHttpClient => false;
}

Map<String, dynamic> package({
  int version = 7,
  String? digest,
  String? signer,
}) => {
  'package': appUpdatePackage,
  'version_code': version,
  'version_name': 'fixture',
  'bytes': 20,
  'sha256': digest ?? 'a' * 64,
  'signers': [signer ?? 'b' * 64],
};
Map<String, dynamic> offer(
  String session, {
  String origin = 'https://updates.example',
}) => {
  ...package(version: 8),
  'session': session,
  'expires_at_ms': DateTime.now().millisecondsSinceEpoch + 30000,
  'url': '$origin/PakarController.apk',
};

class PackageUpdates extends AppUpdates {
  final downloads = <Map<String, dynamic>>[], pulls = <Map<String, dynamic>>[];
  final native = <String>[];
  void Function()? beforeTransfer;
  @override
  Future<Json> rpc(String method, Json params) async {
    native.add(method);
    if (method == 'app.package.info') return package();
    if (method == 'app.upgrade.status') return {'state': 'idle'};
    throw StateError('Unexpected native call $method');
  }

  @override
  Future<Json> download(
    String url, {
    required String sha256,
    required int bytes,
  }) async {
    beforeTransfer?.call();
    downloads.add({'url': url, 'sha256': sha256, 'bytes': bytes});
    return {'state': 'verified'};
  }

  @override
  Future<Json> pullPeer(
    String origin, {
    Map<String, dynamic>? expectedManifest,
  }) async {
    beforeTransfer?.call();
    pulls.add({'origin': origin, 'expected': expectedManifest});
    return {'state': 'verified'};
  }
}

class PackageDiscovery extends AppUpdateDiscovery {
  List<AppUpdatePeer> results = [];
  int scans = 0, announcements = 0;
  @override
  List<AppUpdatePeer> get peers => results;
  @override
  Future<void> scan({Iterable<String> seeds = const []}) async {
    scans++;
  }

  @override
  Future<void> advertise({
    required int port,
    required String name,
    required String instanceId,
  }) async {
    announcements++;
  }

  @override
  Future<void> stopAdvertising() async {}
}

class AutomaticPackageUpdates extends PackageUpdates {
  int installations = 0, cleanups = 0;
  @override
  Future<Json> download(
    String url, {
    required String sha256,
    required int bytes,
  }) async {
    await super.download(url, sha256: sha256, bytes: bytes);
    state = {
      'state': 'verified',
      'transfer_id': 'verified-fixture',
      'sha256': sha256,
    };
    return state;
  }

  @override
  Future<Json> install() async {
    installations++;
    return {'state': 'awaiting_android'};
  }

  @override
  Future<Json> cleanup() async {
    cleanups++;
    return {'removed_files': 0, 'removed_bytes': 0};
  }
}

void main() {
  _SocketTestBinding();
  setUp(() => SharedPreferences.setMockInitialValues({}));

  test(
    'Timed-out feed dial closes a late actual WebSocket without subscribing',
    () async {
      final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
      final closed = Completer<void>();
      final subscription = server.listen((request) async {
        final ws = await WebSocketTransformer.upgrade(request);
        ws.listen(
          (_) => fail('A retired dial must not subscribe'),
          onDone: () {
            if (!closed.isCompleted) closed.complete();
          },
        );
      });
      final model = AppUpdateCoordinator(
        updates: PackageUpdates(),
        discovery: PackageDiscovery(),
        approve: (_) async => false,
        saveSnapshot: (_) async {},
        feedConnectTimeout: const Duration(milliseconds: 5),
        connectFeed: (endpoint) async {
          final ws = await WebSocket.connect(endpoint);
          await Future<void>.delayed(const Duration(milliseconds: 30));
          return ws;
        },
      )..package = package();
      model.settings = {
        ...AppUpdateCoordinator.defaults,
        'feed': 'ws://127.0.0.1:${server.port}/',
        'origin': 'https://updates.example',
      };
      try {
        await expectLater(model.startFeed(), throwsA(isA<TimeoutException>()));
        await closed.future.timeout(const Duration(seconds: 3));
        expect(model.socket, isNull);
      } finally {
        model.dispose();
        await subscription.cancel();
        await server.close(force: true);
      }
    },
  );

  AppUpdateCoordinator coordinator({
    PackageUpdates? updates,
    PackageDiscovery? discovery,
    Future<void> Function(String)? save,
  }) {
    final result = AppUpdateCoordinator(
      updates: updates ?? PackageUpdates(),
      discovery: discovery ?? PackageDiscovery(),
      approve: (_) async => false,
      saveSnapshot: save,
    );
    // Explicit fixture policy: never contact the new public default catalog.
    result.settings['catalog'] = '';
    result.settings['auto_check'] = false;
    addTearDown(result.dispose);
    return result;
  }

  test('Persist only public allowlisted policy; reject credentials, queries and remote plain HTTP', () async {
    final snapshots = <Map<String, dynamic>>[];
    final model = coordinator(
      save: (text) async => snapshots.add(jsonDecode(text)),
    );
    await model.configure({
      ...model.settings,
      'catalog': 'https://updates.example/catalog',
      'origin': 'https://updates.example',
      'token': 'must-not-persist',
      'password': 'must-not-persist',
      'unknown': true,
    });
    final saved = snapshots.single['settings'] as Map;
    expect(saved.keys.toSet(), AppUpdateCoordinator.defaults.keys.toSet());
    expect(jsonEncode(snapshots), isNot(contains('must-not-persist')));
    for (final patch in [
      {'catalog': 'https://user:secret@updates.example/catalog'},
      {'catalog': 'https://updates.example/catalog?token=secret'},
      {'origin': 'https://updates.example/path'},
      {'catalog': 'http://updates.example/catalog'},
      {'feed': 'wss://updates.example/feed?token=secret'},
      {'sharing': 'true'},
    ]) {
      expect(
        () => AppUpdateCoordinator.validate({...model.settings, ...patch}),
        throwsFormatException,
      );
    }
  });

  test(
    'Public defaults are configured but destructive automation is opt-in',
    () {
      expect(
        AppUpdateCoordinator.defaults['catalog'],
        'https://office.digitalogic.ir/pakar/api/app-updates',
      );
      expect(AppUpdateCoordinator.defaults['auto_check'], true);
      for (final key in [
        'auto_download',
        'auto_install',
        'auto_cleanup',
        'management_enabled',
        'allow_remote_cleanup',
      ]) {
        expect(AppUpdateCoordinator.defaults[key], false);
      }
      for (final patch in [
        {'check_interval_seconds': 59},
        {'check_interval_seconds': 86401},
        {'auto_install': true, 'auto_download': false},
        {'auto_check': 'true'},
      ]) {
        expect(
          () => AppUpdateCoordinator.validate({
            ...AppUpdateCoordinator.defaults,
            ...patch,
          }),
          throwsFormatException,
        );
      }
    },
  );

  test('Opt-in automatic install requests OS consent once after durable verified-package receipt', () async {
    final updates = AutomaticPackageUpdates(), saved = <Map<String, dynamic>>[];
    final model = coordinator(
      updates: updates,
      save: (text) async => saved.add(jsonDecode(text)),
    );
    model.package = package();
    model.settings.addAll({
      'origin': 'https://updates.example',
      'auto_download': true,
      'auto_install': true,
    });
    final session = model.beginCatalogSession();
    await model.acceptOffer(offer(session), session);
    expect(updates.installations, 1);
    expect(saved.last['consumed'], contains('install:${'a' * 64}'));
    await model.acceptOffer(offer(session), session);
    expect(updates.installations, 1);
    await model.setForeground(false);
    await expectLater(model.cleanup(), throwsStateError);
    expect(updates.cleanups, 0);
  });

  test('Offer is exact-origin, newer, digest/length pinned and active-session leased', () async {
    final model = coordinator();
    model.package = package();
    model.settings['origin'] = 'https://updates.example';
    final session = model.beginCatalogSession();
    final good = offer(session);
    expect(model.parseOffer(good, session)['sha256'], 'a' * 64);
    for (final patch in [
      {'session': 'old-session'},
      {'package': 'other.app'},
      {'version_code': 7},
      {'bytes': 0},
      {'bytes': AppUpdates.maximum + 1},
      {'sha256': 'bad'},
      {'url': 'https://updates.example:444/PakarController.apk'},
      {'url': 'https://other.example/PakarController.apk'},
      {'url': 'https://user:secret@updates.example/PakarController.apk'},
      {'url': 'https://updates.example/PakarController.apk#fragment'},
      {'expires_at_ms': DateTime.now().millisecondsSinceEpoch - 1},
      {'expires_at_ms': DateTime.now().millisecondsSinceEpoch + 61000},
    ]) {
      expect(
        () => model.parseOffer({...good, ...patch}, session),
        throwsFormatException,
      );
    }
    await model.acceptOffer(good, session);
    expect(model.pending, isNotNull);
    model.beginCatalogSession();
    expect(model.pending, isNull);
    expect(() => model.parseOffer(good, session), throwsFormatException);
    await model.setForeground(false);
    expect(() => model.beginCatalogSession(), throwsStateError);
  });

  test('Write-ahead receipt precedes download and duplicate offers never install or replay', () async {
    final updates = PackageUpdates(), saved = <Map<String, dynamic>>[];
    final model = coordinator(
      updates: updates,
      save: (text) async => saved.add(jsonDecode(text)),
    );
    model.package = package();
    model.settings.addAll({
      'origin': 'https://updates.example',
      'auto_download': true,
    });
    final session = model.beginCatalogSession();
    updates.beforeTransfer = () {
      expect(saved.last['consumed'], contains('remote:${'a' * 64}'));
    };
    await model.acceptOffer(offer(session), session);
    await model.acceptOffer(offer(session), session);
    expect(updates.downloads, hasLength(1));
    expect(updates.native, isEmpty);
    expect(model.consumed, {'remote:${'a' * 64}'});
  });

  test('Storage failure or background retirement during write-ahead prevents transfer', () async {
    final updates = PackageUpdates();
    var fail = true;
    final saved = Completer<void>();
    final entered = Completer<void>();
    final model = coordinator(
      updates: updates,
      save: (_) async {
        if (fail) throw StateError('Storage failed');
        entered.complete();
        await saved.future;
      },
    );
    model.package = package();
    model.settings.addAll({
      'origin': 'https://updates.example',
      'auto_download': true,
    });
    var session = model.beginCatalogSession();
    await expectLater(
      model.acceptOffer(offer(session), session),
      throwsStateError,
    );
    expect(model.consumed, isEmpty);
    expect(updates.downloads, isEmpty);
    fail = false;
    session = model.beginCatalogSession();
    final delivery = model.acceptOffer(offer(session), session);
    await entered.future;
    await model.setForeground(false);
    saved.complete();
    await delivery;
    expect(updates.downloads, isEmpty);
    expect(updates.native, isEmpty);
  });

  test('Nested discovered manifest must be compatible and fresh pull is pinned to it', () async {
    final updates = PackageUpdates(), discovery = PackageDiscovery();
    final model = coordinator(
      updates: updates,
      discovery: discovery,
      save: (_) async {},
    );
    final remote = package(version: 8);
    discovery.results = [
      AppUpdatePeer(
        name: 'Old app',
        origin: 'http://phone.local:18992',
        instanceId: 'old',
        verified: true,
        manifest: {'app_updates': package(version: 7)},
      ),
      AppUpdatePeer(
        name: 'Wrong signer',
        origin: 'http://other.local:18992',
        instanceId: 'wrong',
        verified: true,
        manifest: {'app_updates': package(version: 10, signer: 'c' * 64)},
      ),
      AppUpdatePeer(
        name: 'New app',
        origin: 'http://new.local:18992',
        instanceId: 'new',
        verified: true,
        manifest: {'app_updates': remote},
      ),
    ];
    model.package = package();
    model.settings.addAll({'discover': true, 'auto_download': true});
    await model.checkNow();
    await model.checkNow();
    expect(discovery.scans, 2);
    expect(updates.pulls, [
      {'origin': 'http://new.local:18992', 'expected': remote},
    ]);
    expect(model.consumed, {'peer:${'a' * 64}'});
    expect(updates.native, isEmpty);
  });

  test('Actual read-only HTTP catalog echoes fresh session; review policy does not download', () async {
    final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    addTearDown(() => server.close(force: true));
    final requests = <String>[];
    final origin = 'http://127.0.0.1:${server.port}';
    final subscription = server.listen((request) async {
      requests.add(request.uri.path);
      expect(request.method, 'GET');
      expect(request.headers.value('authorization'), isNull);
      final session = request.headers.value('x-pakar-app-session')!;
      request.response.write(
        jsonEncode({
          'offers': [offer(session, origin: origin)],
        }),
      );
      await request.response.close();
    });
    addTearDown(subscription.cancel);
    final updates = PackageUpdates(), model = coordinator(updates: updates);
    await model.configure({
      ...model.settings,
      'catalog': '$origin/catalog',
      'origin': origin,
    });
    model.package = package();
    await model.checkNow();
    expect(requests, ['/catalog']);
    expect(model.pending?['session'], model.session);
    expect(updates.downloads, isEmpty);
    expect(updates.pulls, isEmpty);
    expect(updates.native, isEmpty);
    await model.setForeground(false);
    expect(model.pending, isNull);
    await model.checkNow();
    expect(requests, hasLength(1));
  });
}
