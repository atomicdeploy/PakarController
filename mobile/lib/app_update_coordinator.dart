import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'app_updates.dart';
import 'peer_identity.dart';
import 'app_updates_peer.dart';
import 'app_update_discovery.dart';
import 'device_registry.dart';
import 'firmware_feed.dart';
import 'app_client_management.dart';
import 'connection_preferences.dart' show SecretStore;

/// Foreground app-package distribution, independent of the selected board.
/// Reuses PCController package pull/push; the OS installer is local-only.
class AppUpdateCoordinator extends ChangeNotifier {
  AppUpdateCoordinator({
    AppUpdates? updates,
    AppUpdateDiscovery? discovery,
    this.saveSnapshot,
    this.connectFeed,
    this.feedConnectTimeout = const Duration(seconds: 8),
    SecretStore? managementSecrets,
    ManagedRequest? managementRequest,
    Map<String, dynamic> Function()? supportInfo,
    required Future<bool> Function(Map<String, dynamic>) approve,
  }) : updates = updates ?? AppUpdates(),
       discovery = discovery ?? AppUpdateDiscovery() {
    peers = AppUpdatePeers(
      this.updates,
      supportInfo: supportInfo,
      approve: (manifest) async {
        final generation = epoch;
        if (closed || !foreground || settings['sharing'] != true) return false;
        if (!await announce(
          manifest,
          sourceKind: 'incoming',
          source: '${manifest['source'] ?? ''}',
        )) {
          return false;
        }
        if (settings['auto_download'] != true && !await approve(manifest)) {
          return false;
        }
        if (closed || !foreground || generation != epoch) return false;
        final key = 'peer:${manifest['sha256']}';
        if (consumed.contains(key)) return false;
        await remember(key);
        return !closed &&
            foreground &&
            generation == epoch &&
            settings['sharing'] == true;
      },
      onVerified: (_) async {
        try {
          await _autoInstall(epoch);
        } on Object catch (error) {
          failure(error);
        }
        changed();
      },
    );
    this.updates.addListener(changed);
    this.discovery.addListener(changed);
    management = AppClientManagement(
      policy: () => settings,
      package: () => package,
      checkUpdate: checkNow,
      cleanup: cleanup,
      secrets: managementSecrets ?? const ManagedClientSecrets(),
      request: managementRequest,
    );
  }
  static const storageKey = 'pakar.app_updates';
  final AppUpdates updates;
  final AppUpdateDiscovery discovery;
  final Future<void> Function(String)? saveSnapshot;
  final Future<WebSocket> Function(String endpoint)? connectFeed;
  final Duration feedConnectTimeout;
  late final AppUpdatePeers peers;
  late final AppClientManagement management;
  Map<String, dynamic> package = {};
  static const defaults = <String, dynamic>{
    'sharing': true,
    'discover': true,
    'auto_download': true,
    'auto_check': true,
    'check_interval_seconds': 900,
    'auto_install': true,
    'auto_cleanup': true,
    'notify_updates': true,
    'catalog': 'https://office.digitalogic.ir/pakar/api/app-updates',
    'origin': 'https://office.digitalogic.ir',
    'management_base': 'https://office.digitalogic.ir/pakar',
    'management_enabled': false,
    'allow_remote_check': false,
    'allow_remote_cleanup': false,
    'feed': '',
    'socket_io': false,
    'namespace': '/',
    'event': 'app-update-offer',
  };
  Map<String, dynamic> settings = Map.of(defaults);
  final consumed = <String>{};
  final notified = <String>{};
  Map<String, dynamic>? available;
  DateTime? _availableAt;
  Map<String, dynamic>? pending;
  String get instanceId => PeerIdentity.current.installationId;
  String message = '', feedState = 'Stopped';
  bool foreground = true, closed = false, checking = false;
  WebSocket? socket;
  StreamSubscription<dynamic>? events;
  Timer? timer, retry, heartbeat, managementTimer;
  String cleanupSummary = '';
  DateTime? _lastCleanup;
  int epoch = 0, retryCount = 0;
  int _feedEpoch = 0;
  String session = '', _feedSession = '';
  bool _feedStarting = false,
      _configuring = false,
      _downloading = false,
      _installing = false;
  Future<void> _saving = Future<void>.value();
  Future<void> _lifecycle = Future<void>.value();
  Future<void> _receipts = Future<void>.value();
  bool get sharing => peers.running;
  void changed() {
    if (!closed) notifyListeners();
  }

  void failure(Object error) {
    message = '$error';
    changed();
  }

  Future<void> load() async {
    await PeerIdentity.current.initialize();
    final prefs = await SharedPreferences.getInstance();
    try {
      final raw = jsonDecode(prefs.getString(storageKey) ?? '{}');
      if (raw is Map && raw['settings'] is Map) {
        settings = {
          for (final key in defaults.keys)
            key: (raw['settings'] as Map).containsKey(key)
                ? raw['settings'][key]
                : defaults[key],
        };
      }
      if (raw is Map && raw['consumed'] is List) {
        consumed.addAll(
          (raw['consumed'] as List)
              .whereType<String>()
              .where(
                (key) =>
                    RegExp(r'^(peer|remote|install):[a-f0-9]{64}$')
                        .hasMatch(key),
              )
              .take(128),
        );
      }
      if (raw is Map && raw['notified'] is List) {
        notified.addAll(
          (raw['notified'] as List)
              .whereType<String>()
              .where((hash) => RegExp(r'^[a-f0-9]{64}$').hasMatch(hash))
              .take(32),
        );
      }
      validate(settings);
      if (settings['management_enabled'] == true) await management.load();
      await refresh();
      await reconcile();
    } on Object catch (error) {
      settings = {
        ...defaults,
        'sharing': false,
        'discover': false,
        'auto_download': false,
        'auto_install': false,
        'auto_check': false,
      };
      failure(error);
    }
  }

  static void validate(Map<String, dynamic> value) {
    for (final key in [
      'sharing',
      'discover',
      'auto_download',
      'socket_io',
      'auto_check',
      'auto_install',
      'auto_cleanup',
      'notify_updates',
      'management_enabled',
      'allow_remote_check',
      'allow_remote_cleanup',
    ]) {
      if (value[key] is! bool) {
        throw const FormatException('Invalid app update policy');
      }
    }
    if (value['check_interval_seconds'] is! int ||
        value['check_interval_seconds'] < 60 ||
        value['check_interval_seconds'] > 86400) {
      throw const FormatException(
        'Update check interval must be 60–86400 seconds',
      );
    }
    if (value['auto_install'] == true && value['auto_download'] != true) {
      throw const FormatException(
        'Automatic installation requires automatic download',
      );
    }
    if (value['management_base'] is! String) {
      throw const FormatException('Invalid management base');
    }
    AppClientManagement.endpoint(
      value['management_base'],
      '/api/clients/report',
    );
    for (final key in ['catalog', 'origin', 'feed', 'namespace', 'event']) {
      if (value[key] is! String || (value[key] as String).length > 2048) {
        throw const FormatException('Invalid app update endpoint');
      }
    }
    for (final key in ['catalog', 'origin']) {
      final text = value[key] as String;
      if (text.isEmpty) continue;
      final uri = Uri.tryParse(text);
      if (uri == null ||
          !{'http', 'https'}.contains(uri.scheme) ||
          uri.host.isEmpty ||
          uri.userInfo.isNotEmpty ||
          uri.hasFragment ||
          uri.hasQuery ||
          RegExp(r'[\x00-\x20\x7f-\uffff\\]').hasMatch(text) ||
          (key == 'origin' &&
              (uri.hasQuery || !(uri.path.isEmpty || uri.path == '/')))) {
        throw const FormatException(
          'Use a direct HTTP(S) source without embedded credentials',
        );
      }
      validateAppUpdateUrl(
        text,
      ); // Local HTTP or remote HTTPS, shared with APK transfer.
    }
    if ((value['feed'] as String).isNotEmpty) {
      FirmwareFeedOptions(
        endpoint: value['feed'],
        origin: value['origin'],
        socketIo: value['socket_io'],
        namespace: value['namespace'],
        event: value['event'],
      ).validate();
    }
    if ((value['catalog'] as String).isNotEmpty &&
        (value['origin'] as String).isEmpty) {
      throw const FormatException('Set the trusted APK download origin');
    }
  }

  Future<void> persist() {
    final snapshot = jsonEncode({
      'settings': {for (final key in defaults.keys) key: settings[key]},
      'consumed': consumed.toList(),
      'notified': notified.toList(),
      'instance_id': instanceId,
    });
    final task = _saving.catchError((Object _) {}).then((_) async {
      if (saveSnapshot != null) {
        await saveSnapshot!(snapshot);
        return;
      }
      final prefs = await SharedPreferences.getInstance();
      if (!await prefs.setString(storageKey, snapshot)) {
        throw StateError('Update policy was not saved');
      }
    });
    _saving = task;
    return task;
  }

  Future<void> configure(Map<String, dynamic> next) async {
    if (_configuring || closed) {
      throw StateError('Update policy is unavailable');
    }
    next = {
      for (final key in defaults.keys)
        key: next.containsKey(key) ? next[key] : settings[key],
    };
    validate(next);
    _configuring = true;
    final before = settings;
    ++epoch;
    management.retireRequests();
    session = '';
    pending = null;
    available = null;
    timer?.cancel();
    timer = null;
    managementTimer?.cancel();
    managementTimer = null;
    try {
      await stopFeed();
      settings = Map.of(next);
      try {
        await persist();
      } on Object {
        settings = before;
        rethrow;
      }
      await reconcile();
      changed();
    } finally {
      _configuring = false;
    }
  }

  Future<void> refresh() async {
    package = await updates.rpc('app.package.info', {});
    await updates.refresh();
    changed();
  }

  Future<void> setForeground(bool active) async {
    foreground = active;
    management.setForeground(active);
    if (!active) {
      ++epoch;
      session = '';
      pending = null;
    }
    final task = _lifecycle.catchError((Object _) {}).then((_) async {
      if (!active || closed) {
        timer?.cancel();
        timer = null;
        managementTimer?.cancel();
        managementTimer = null;
        await stopFeed();
        await discovery.stopAdvertising();
        await peers.close();
      } else {
        await _reconcile();
      }
      changed();
    });
    _lifecycle = task;
    return task;
  }

  Future<void> reconcile() {
    final task = _lifecycle.catchError((Object _) {}).then((_) => _reconcile());
    _lifecycle = task;
    return task;
  }

  Future<void> _reconcile() async {
    if (closed || !foreground || package.isEmpty) return;
    final generation = epoch;
    if (settings['sharing'] == true) {
      if (!peers.running) await peers.start();
      final port = peers.port;
      if (port == null || !foreground || closed || generation != epoch) return;
      await discovery.advertise(
        port: port,
        name: 'PakarController',
        instanceId: peers.instanceId,
      );
    } else {
      await discovery.stopAdvertising();
      await peers.close();
    }
    if (closed || !foreground || generation != epoch) return;
    timer?.cancel();
    if (settings['auto_check'] == true &&
        (settings['discover'] == true ||
            (settings['catalog'] as String).isNotEmpty)) {
      timer = Timer.periodic(
        Duration(seconds: settings['check_interval_seconds'] as int),
        (_) {
          unawaited(checkNow().catchError(failure));
        },
      );
      unawaited(checkNow().catchError(failure));
    }
    if (settings['auto_cleanup'] == true && !updates.busy) {
      await cleanup();
    }
    managementTimer?.cancel();
    managementTimer = null;
    if (settings['management_enabled'] == true) {
      await management.load();
      if (!foreground || closed || generation != epoch) return;
    }
    if (settings['management_enabled'] == true ||
        settings['auto_cleanup'] == true) {
      managementTimer = Timer.periodic(const Duration(seconds: 60), (_) {
        if (settings['management_enabled'] == true) {
          unawaited(reportManagement().catchError(failure));
        }
        if (settings['auto_cleanup'] == true &&
            !updates.busy &&
            (_lastCleanup == null ||
                DateTime.now().difference(_lastCleanup!) >=
                    const Duration(minutes: 30))) {
          unawaited(cleanup().catchError(failure));
        }
      });
      if (settings['management_enabled'] == true) {
        unawaited(reportManagement().catchError(failure));
      }
    }
    if ((settings['feed'] as String).isNotEmpty && socket == null) {
      unawaited(startFeed().catchError(failure));
    }
  }

  Future<void> checkNow() async {
    if (closed || !foreground || checking || transferBlocked) {
      return;
    }
    final generation = epoch;
    checking = true;
    changed();
    try {
      if (settings['discover'] == true) {
        await discovery.scan();
        if (!foreground || closed || generation != epoch) return;
        final eligible =
            discovery.peers.where((peer) {
              if (!peer.verified || peer.manifest['app_updates'] is! Map) {
                return false;
              }
              try {
                validateAppUpdateManifest(
                  Map<String, dynamic>.from(
                    peer.manifest['app_updates'] as Map,
                  ),
                  package,
                );
                return true;
              } on Object {
                return false;
              }
            }).toList()..sort(
              (a, b) => (b.manifest['app_updates']['version_code'] as int)
                  .compareTo(a.manifest['app_updates']['version_code'] as int),
            );
        var selected = false;
        if (eligible.isNotEmpty) {
          selected = await announce(
            Map<String, dynamic>.from(
              eligible.first.manifest['app_updates'] as Map,
            ),
            sourceKind: 'peer',
            source: eligible.first.origin,
          );
        }
        if (selected && settings['auto_download'] == true) {
          final peer = eligible.first,
              key = 'peer:${eligible.first.manifest['app_updates']['sha256']}';
          if (!consumed.contains(key)) {
            await remember(key);
            if (foreground && !closed && generation == epoch) {
              await updates.pullPeer(
                peer.origin,
                expectedManifest: Map<String, dynamic>.from(
                  peer.manifest['app_updates'] as Map,
                ),
              );
              await _autoInstall(generation);
            }
          }
        }
      }
      final catalog = settings['catalog'] as String;
      if (catalog.isNotEmpty && foreground && !transferBlocked) {
        final catalogSession = beginCatalogSession();
        final client = HttpClient()
          ..findProxy = ((_) => 'DIRECT')
          ..connectionTimeout = const Duration(seconds: 5);
        try {
          final request = await client
              .getUrl(Uri.parse(catalog))
              .timeout(const Duration(seconds: 8));
          request.followRedirects = false;
          identifyRequest(request);
          request.headers.set('X-Pakar-App-Session', catalogSession);
          request.headers.set('X-Pakar-App-Package', '${package['package']}');
          final response = await request.close().timeout(
            const Duration(seconds: 8),
          );
          if (response.statusCode != 200 || response.contentLength > 65536) {
            throw const FormatException('App update catalog rejected');
          }
          final bytes = <int>[];
          final reader = StreamIterator(response);
          final watch = Stopwatch()..start();
          try {
            while (true) {
              final remaining = const Duration(seconds: 12) - watch.elapsed;
              if (remaining <= Duration.zero) {
                throw TimeoutException('Catalog deadline');
              }
              if (!await reader.moveNext().timeout(remaining)) break;
              if (closed ||
                  !foreground ||
                  generation != epoch ||
                  bytes.length + reader.current.length > 65536) {
                throw const FormatException('Catalog limit or owner changed');
              }
              bytes.addAll(reader.current);
            }
          } finally {
            await reader.cancel();
          }
          if (!foreground || closed || generation != epoch) return;
          final raw = jsonDecode(utf8.decode(bytes));
          final offers = raw is Map && raw['offers'] is List
              ? raw['offers'] as List
              : [raw];
          if (offers.length > 16) {
            throw const FormatException('Too many app update offers');
          }
          int version(Object? value) =>
              value is Map && value['version_code'] is int
              ? value['version_code'] as int
              : -1;
          final sorted = offers.toList()
            ..sort((a, b) => version(b).compareTo(version(a)));
          for (final offer in sorted) {
            await acceptOffer(offer, catalogSession);
            if (transferBlocked) break;
          }
        } finally {
          client.close(force: true);
        }
      }
    } finally {
      checking = false;
      changed();
    }
  }

  Future<void> remember(String key) {
    final task = _receipts.catchError((Object _) {}).then((_) async {
      if (consumed.contains(key)) {
        throw const FormatException(
          'Update already attempted; inspect the existing receipt',
        );
      }
      if (!RegExp(r'^(peer|remote|install):[a-f0-9]{64}$').hasMatch(key)) {
        throw const FormatException('Invalid update receipt');
      }
      final previous = Set<String>.of(consumed);
      if (consumed.length >= 128) consumed.remove(consumed.first);
      consumed.add(key);
      // Write-ahead receipt: process restarts must never replay a transfer/install.
      try {
        await persist();
      } on Object {
        consumed
          ..clear()
          ..addAll(previous);
        rethrow;
      }
    });
    _receipts = task;
    return task;
  }

  /// Fresh read-only catalog ownership; tests and HTTP use the same API. Creating
  /// a session does not authorize a transfer or install anything.
  String beginCatalogSession() {
    if (closed || !foreground) {
      throw StateError('App update catalog is not active');
    }
    session = randomIdentifier();
    if (pending?['session'] != _feedSession) pending = null;
    return session;
  }

  Map<String, dynamic> parseOffer(Object? raw, String expectedSession) {
    if (raw is! Map<String, dynamic>) {
      throw const FormatException('Invalid app update offer');
    }
    final uri = raw['url'] is String ? Uri.tryParse(raw['url']) : null;
    final origin = Uri.tryParse(settings['origin'] as String);
    final now = DateTime.now().millisecondsSinceEpoch;
    if (closed ||
        !foreground ||
        expectedSession.isEmpty ||
        (expectedSession != session && expectedSession != _feedSession) ||
        raw['session'] != expectedSession ||
        raw['package'] != package['package'] ||
        raw['version_code'] is! int ||
        package['version_code'] is! int ||
        raw['version_code'] <= package['version_code'] ||
        raw['expires_at_ms'] is! int ||
        raw['expires_at_ms'] <= now ||
        raw['expires_at_ms'] > now + 60000 ||
        raw['bytes'] is! int ||
        raw['bytes'] < 1 ||
        raw['bytes'] > AppUpdates.maximum ||
        !RegExp(r'^[a-f0-9]{64}$').hasMatch('${raw['sha256']}') ||
        uri == null ||
        origin == null ||
        !{'http', 'https'}.contains(uri.scheme) ||
        uri.scheme != origin.scheme ||
        uri.host != origin.host ||
        uri.port != origin.port ||
        uri.userInfo.isNotEmpty ||
        uri.hasFragment ||
        '${raw['url']}'.length > 2048 ||
        RegExp(r'[\x00-\x20\x7f-\uffff\\]').hasMatch('${raw['url']}')) {
      throw const FormatException(
        'Rejected stale, incompatible or wrong-origin APK offer',
      );
    }
    validateAppUpdateManifest(raw, package);
    return Map.unmodifiable(raw);
  }

  bool get transferBlocked =>
      updates.busy ||
      updates.state['transfer_id'] != null ||
      updates.state['state'] == 'awaiting_android' ||
      _installing;

  /// One visible offer and one OS notification per compatible digest. The
  /// actual staged APK is independently verified by Android before install.
  Future<bool> announce(
    Map<String, dynamic> manifest, {
    required String sourceKind,
    required String source,
  }) async {
    validateAppUpdateManifest(manifest, package);
    if (closed || !foreground) return false;
    if (available != null &&
        _availableAt != null &&
        DateTime.now().difference(_availableAt!) < const Duration(minutes: 1) &&
        (available!['version_code'] as int) >
            (manifest['version_code'] as int)) {
      return false;
    }
    final same = available?['sha256'] == manifest['sha256'];
    available = {...manifest, 'source_kind': sourceKind, 'source': source};
    _availableAt = DateTime.now();
    if (!same) {
      message =
          'PakarController ${manifest['version_name'] ?? manifest['version_code']} is available';
    }
    changed();
    final hash = manifest['sha256'] as String;
    if (settings['notify_updates'] == true && !notified.contains(hash)) {
      notified.add(hash);
      if (notified.length > 32) notified.remove(notified.first);
      try {
        await persist();
      } on Object {
        notified.remove(hash);
        rethrow;
      }
      if (closed || !foreground || settings['notify_updates'] != true) {
        return true;
      }
      try {
        final receipt = await updates.notifyUpdate(manifest);
        if (receipt['permission_required'] == true) {
          message =
              'Update available; allow Android notifications in App updates';
        }
      } on Object {
        message = 'Update available; Android notification could not be posted';
      }
      changed();
    }
    return true;
  }

  Future<void> acceptOffer(Object? raw, String expectedSession) async {
    final offer = parseOffer(raw, expectedSession);
    if (consumed.contains('remote:${offer['sha256']}')) return;
    if (!await announce(offer, sourceKind: 'remote', source: offer['url'])) {
      return;
    }
    if (settings['auto_download'] == true) {
      await downloadOffer(offer, expectedSession);
    } else {
      pending = offer;
      changed();
    }
  }

  Future<void> downloadPending() async {
    final offer = pending;
    if (offer != null) {
      await downloadOffer(offer, offer['session']);
    } else if (available?['source_kind'] == 'peer' && !transferBlocked) {
      final manifest = available!, generation = epoch;
      final key = 'peer:${manifest['sha256']}';
      if (consumed.contains(key)) return;
      await remember(key);
      if (closed || !foreground || generation != epoch) return;
      await updates.pullPeer(manifest['source'], expectedManifest: manifest);
      await _autoInstall(generation);
    }
  }

  Future<void> downloadOffer(
    Map<String, dynamic> value,
    String expectedSession,
  ) async {
    if (closed || !foreground || transferBlocked || _downloading) return;
    final generation = epoch;
    final offer = parseOffer(value, expectedSession);
    _downloading = true;
    try {
      await remember('remote:${offer['sha256']}');
      if (closed || !foreground || generation != epoch) return;
      parseOffer(
        offer,
        expectedSession,
      ); // Save latency cannot extend an offer's lease.
      pending = null;
      await updates.download(
        offer['url'],
        sha256: offer['sha256'],
        bytes: offer['bytes'],
      );
      await _autoInstall(generation);
      changed();
    } finally {
      _downloading = false;
    }
  }

  Future<void> _autoInstall(int generation) async {
    if (closed ||
        !foreground ||
        epoch != generation ||
        settings['auto_install'] != true ||
        _installing ||
        updates.state['state'] != 'verified' ||
        updates.state['transfer_id'] is! String) {
      return;
    }
    final hash = updates.state['sha256'];
    if (hash is! String || !RegExp(r'^[a-f0-9]{64}$').hasMatch(hash)) return;
    if (consumed.contains('install:$hash')) return;
    _installing = true;
    try {
      await remember('install:$hash');
      if (closed ||
          !foreground ||
          epoch != generation ||
          settings['auto_install'] != true) {
        return;
      }
      await updates
          .install(); // Android still verifies and requires its own consent.
    } finally {
      _installing = false;
    }
  }

  Future<void> cleanup() async {
    if (closed || !foreground || updates.busy) {
      throw StateError('Cleanup unavailable during transfer/background');
    }
    final result = await updates.cleanup();
    _lastCleanup = DateTime.now();
    cleanupSummary =
        '${result['removed_files'] ?? 0} abandoned APK files removed · ${result['removed_bytes'] ?? 0} bytes';
    changed();
  }

  Future<void> reportManagement() async {
    final received = updates.state['received'] ?? updates.state['sent'];
    final expected = updates.state['bytes'];
    await management.reportAndReceive(
      '${updates.state['state'] ?? 'unknown'}',
      receivedBytes: received is int ? received : null,
      expectedBytes: expected is int ? expected : null,
    );
    changed();
  }

  Future<void> startFeed() async {
    if (_feedStarting || closed || !foreground || socket != null) return;
    final selected = Map<String, dynamic>.of(settings),
        generation = ++_feedEpoch;
    final options = FirmwareFeedOptions(
      endpoint: selected['feed'],
      origin: selected['origin'],
      socketIo: selected['socket_io'],
      namespace: selected['namespace'],
      event: selected['event'],
    );
    options.validate();
    _feedStarting = true;
    _feedSession = randomIdentifier();
    final feedSession = _feedSession;
    feedState = 'Connecting';
    changed();
    try {
      final uri = Uri.parse(options.endpoint);
      Future<WebSocket> connectIdentified(String endpoint) =>
          connectFeed != null
          ? connectFeed!(endpoint)
          : WebSocket.connect(
              endpoint,
              headers: PeerIdentity.current.webSocketHeaders,
            );
      final connecting = connectIdentified(
        options.socketIo
            ? uri
                  .replace(
                    queryParameters: {'EIO': '4', 'transport': 'websocket'},
                  )
                  .toString()
            : options.endpoint,
      );
      final ws = await connecting.timeout(
        feedConnectTimeout,
        onTimeout: () {
          // Match FirmwareFeed: timeout retires the dial, not its eventual socket.
          unawaited(
            connecting
                .then<void>((late) async {
                  await late.close();
                })
                .catchError((Object _) {}),
          );
          throw TimeoutException('App update feed connection deadline');
        },
      );
      if (generation != _feedEpoch || closed || !foreground) {
        await ws.close();
        return;
      }
      socket = ws;
      final wire = FirmwareFeedWire(options);
      var last = DateTime.now();
      final subscription = {
        'type': 'subscribe',
        'events': [options.event],
        'session': feedSession,
        'package': package['package'],
        'instance_id': instanceId,
      };
      void ready() {
        ws.add(
          options.socketIo
              ? wire.event('subscribe', subscription)
              : jsonEncode(subscription),
        );
        feedState = 'Listening';
        retryCount = 0;
        changed();
      }

      if (!options.socketIo) {
        ws.pingInterval = const Duration(seconds: 20);
        ready();
      }
      events = ws.listen(
        (raw) {
          if (generation != _feedEpoch || closed || !foreground) return;
          try {
            if (raw is! String) {
              throw const FormatException('Expected a bounded APK offer');
            }
            final packet = wire.receive(raw);
            last = DateTime.now();
            if (packet.$1 == 'reply') ws.add(packet.$2 as String);
            if (packet.$1 == 'ready') ready();
            if (packet.$1 == 'offer') {
              unawaited(
                acceptOffer(packet.$2, feedSession).catchError(failure),
              );
            }
          } on Object catch (error) {
            failure(error);
            unawaited(reconnectFeed(generation));
          }
        },
        onError: (Object error) {
          failure(error);
          unawaited(reconnectFeed(generation));
        },
        onDone: () {
          unawaited(reconnectFeed(generation));
        },
      );
      heartbeat = Timer.periodic(const Duration(seconds: 1), (_) {
        if (options.socketIo &&
            DateTime.now().difference(last) > wire.heartbeat) {
          unawaited(reconnectFeed(generation));
        }
      });
    } on Object {
      await reconnectFeed(generation);
      rethrow;
    } finally {
      _feedStarting = false;
    }
  }

  Future<void> reconnectFeed(int generation) async {
    if (generation != _feedEpoch || closed) return;
    await stopFeed();
    if (!foreground || (settings['feed'] as String).isEmpty) return;
    feedState = 'Reconnecting';
    changed();
    final delay = [1, 2, 4, 8, 30][retryCount.clamp(0, 4)];
    retryCount++;
    retry = Timer(Duration(seconds: delay), () {
      unawaited(startFeed().catchError(failure));
    });
  }

  Future<void> stopFeed() async {
    ++_feedEpoch;
    if (pending?['session'] == _feedSession) pending = null;
    _feedSession = '';
    retry?.cancel();
    heartbeat?.cancel();
    retry = null;
    heartbeat = null;
    final old = socket;
    socket = null;
    await events?.cancel();
    events = null;
    if (old != null) {
      try {
        await old.close().timeout(const Duration(milliseconds: 500));
      } on Object {
        /* bounded */
      }
    }
    feedState = 'Stopped';
  }

  @override
  void dispose() {
    closed = true;
    ++epoch;
    session = '';
    pending = null;
    timer?.cancel();
    managementTimer?.cancel();
    management.dispose();
    unawaited(stopFeed());
    peers.dispose();
    updates.removeListener(changed);
    discovery.removeListener(changed);
    discovery.dispose();
    updates.dispose();
    super.dispose();
  }
}
