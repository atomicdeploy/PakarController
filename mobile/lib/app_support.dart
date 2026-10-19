import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/widgets.dart';
import 'package:crypto/crypto.dart' as crypto;

import 'api.dart';
import 'command_wire.dart';
import 'device_registry.dart';
import 'models.dart';
import 'peer_identity.dart';

enum SupportIncident {
  connectionLost,
  commandRejected,
  commandUncertain,
  requestRejected,
}

/// Explicit foreground-only LAN support, independent of the selected board.
/// Adapted from PCController mobile/lib/bridge.dart: generation retirement,
/// bounded HTTP reader, authenticated socket admission and non-replayed intents.
/// Unlike that donor, there is no raw HTTP proxy or firmware/OS execution here.
class AppSupportSession extends ChangeNotifier with WidgetsBindingObserver {
  AppSupportSession({
    required this.api,
    required this.observed,
    required this.health,
    this.applyAssets,
    this.checkAssets,
  });
  final PakarApi? Function() api;
  final DeviceStatus? Function() observed;
  final Map<String, Object?> Function() health;
  final Future<void> Function(
    String bundle,
    String sha256,
    bool Function() authorize,
  )?
  applyAssets;
  final Future<void> Function()? checkAssets;
  HttpServer? _server;
  final _sockets = <WebSocket>{};
  final _seen = <String>{};
  final _incidents = <SupportIncident, int>{};
  Timer? _expiry;
  bool _disposed = false, _foreground = true, _busy = false, _starting = false;
  bool _commands = false, _assets = false, _uncertain = false;
  bool _observing = false;
  int _epoch = 0, _requests = 0;
  PakarApi? _owner;
  int? _ownerRevision;
  String? _board, _boot;
  String _token = '', _session = '';
  bool get running => _server != null && !_disposed && _foreground;
  bool get commandsEnabled => running && _commands && !_uncertain;
  bool get assetsEnabled => running && _assets && applyAssets != null;
  String get token =>
      _token; // Local opt-in UI only; never in health/RPC replies.
  String get session => _session;
  int? get port => _server?.port;
  bool get uncertain => _uncertain;

  Future<void> start({bool commands = false, bool assets = false}) async {
    if (_disposed || _starting) throw StateError('Support unavailable');
    _starting = true;
    try {
      await stop();
      final lifecycle = WidgetsBinding.instance.lifecycleState;
      if (!_foreground ||
          (lifecycle != null && lifecycle != AppLifecycleState.resumed)) {
        throw StateError('Support requires the foreground app');
      }
      _owner = api();
      _ownerRevision = _owner?.connectionRevision;
      final current = observed();
      _board = _identity(current?.json['board_id']);
      _boot = _identity(current?.json['boot_id']);
      if (commands &&
          (_owner == null ||
              _board == null ||
              _boot == null ||
              current?.peer.known != true)) {
        throw StateError(
          'A known board is required to enable support commands',
        );
      }
      final generation = ++_epoch;
      final server = await HttpServer.bind(InternetAddress.anyIPv4, 0);
      if (_disposed || !_foreground || generation != _epoch) {
        await server.close(force: true);
        throw StateError('Support retired during startup');
      }
      _server = server;
      server.idleTimeout = const Duration(seconds: 8);
      _token = randomIdentifier() + randomIdentifier();
      _session = randomIdentifier();
      _commands = commands;
      _assets = assets;
      _uncertain = false;
      _seen.clear();
      if (!_observing) {
        WidgetsBinding.instance.addObserver(this);
        _observing = true;
      }
      _expiry = Timer(const Duration(minutes: 15), () => unawaited(stop()));
      server.listen(
        (request) => unawaited(_http(request, generation)),
        onError: (Object _) {
          unawaited(stop());
        },
      );
      notifyListeners();
    } finally {
      _starting = false;
    }
  }

  void recordIncident(SupportIncident kind) {
    if (_disposed) return;
    _incidents[kind] = (_incidents[kind] ?? 0) + 1;
    notifyListeners();
  }

  static String? _identity(Object? value) =>
      value is String && RegExp(r'^[!-~]{1,80}$').hasMatch(value)
      ? value
      : null;
  Map<String, dynamic> snapshot() {
    final state = health();
    final current = observed();
    return {
      'session': session,
      'application': 'PakarController',
      'version': PeerIdentity.current.version,
      'build': PeerIdentity.current.build,
      'foreground': _foreground,
      'commands_enabled': commandsEnabled,
      'assets_enabled': assetsEnabled,
      'operations': [
        'health',
        'board.status',
        'board.mapping',
        if (assetsEnabled) 'assets.apply',
        if (assetsEnabled && checkAssets != null) 'assets.check',
        if (commandsEnabled) 'command',
      ],
      'uncertain': _uncertain,
      'board_available': current?.peer.known == true,
      'board_id': _identity(current?.json['board_id']),
      'boot_id': _identity(current?.json['boot_id']),
      'health': {
        for (final key in ['connected', 'websocket_live', 'busy', 'updating'])
          if (state[key] is bool) key: state[key],
        for (final key in [
          'selection_count',
          'pending_commands',
          'reconnect_count',
        ])
          if (state[key] is int && (state[key] as int) >= 0) key: state[key],
      },
      'incidents': [
        for (final entry in _incidents.entries)
          {'code': entry.key.name, 'count': entry.value},
      ],
    };
  }

  bool _active(int generation) => running && generation == _epoch;
  bool _peerCurrent() {
    final value = observed();
    return identical(api(), _owner) &&
        _owner?.connectionRevision == _ownerRevision &&
        value?.peer.known == true &&
        value?.json['board_id'] == _board &&
        value?.json['boot_id'] == _boot;
  }

  Future<Map<String, dynamic>> _rpc(String raw, int generation) async {
    String? id;
    bool dispatched = false;
    try {
      if (!_active(generation) || utf8.encode(raw).length > 32768) {
        throw const FormatException();
      }
      final value = jsonDecode(raw);
      if (value is! Map<String, dynamic>) throw const FormatException();
      final candidate = value['id'], deadline = value['expires_at_ms'];
      final now = DateTime.now().millisecondsSinceEpoch;
      if (candidate is! String ||
          !RegExp(r'^[A-Za-z0-9_-]{1,64}$').hasMatch(candidate) ||
          value['session'] != session ||
          deadline is! int ||
          deadline <= now ||
          deadline > now + 60000 ||
          _seen.length >= 1024 ||
          !_seen.add(candidate)) {
        throw const FormatException();
      }
      id = candidate;
      if (value['operation'] == 'health') {
        return {'id': id, 'accepted': true, 'health': snapshot()};
      }
      if (value['operation'] == 'board.status' ||
          value['operation'] == 'board.mapping') {
        if (_busy || !_peerCurrent()) throw const StaleConnection();
        _busy = true;
        try {
          final remaining = Duration(
            milliseconds: (deadline - DateTime.now().millisecondsSinceEpoch)
                .clamp(1, 8000),
          );
          final Map<String, dynamic> readback;
          if (value['operation'] == 'board.status') {
            final actual = await _owner!.status().timeout(remaining);
            readback = {
              for (final key in [
                'board_id',
                'boot_id',
                'hostname',
                'version',
                'mode',
                'effect',
                'brightness',
                'cells',
                'led_output',
                'capabilities',
                'avr_online',
                'programming_latch',
              ])
                if (actual.json.containsKey(key)) key: actual.json[key],
            };
          } else {
            final mapping = await _owner!.mapping().timeout(remaining);
            readback = {'physical': mapping.physical, 'names': mapping.names};
          }
          if (!_active(generation) ||
              !_peerCurrent() ||
              DateTime.now().millisecondsSinceEpoch >= deadline ||
              (value['operation'] == 'board.status' &&
                  (readback['board_id'] != _board ||
                      readback['boot_id'] != _boot))) {
            throw const StaleConnection();
          }
          return {
            'id': id,
            'accepted': true,
            'board_id': _board,
            'boot_id': _boot,
            'readback': readback,
          };
        } finally {
          _busy = false;
        }
      }
      if (value['operation'] == 'assets.check') {
        if (!assetsEnabled || checkAssets == null || _busy) {
          throw const FormatException();
        }
        _busy = true;
        try {
          await checkAssets!();
          if (!_active(generation) ||
              DateTime.now().millisecondsSinceEpoch >= deadline) {
            throw const StaleConnection();
          }
          return {
            'id': id,
            'accepted': true,
            'outcome': 'presentation_source_checked',
          };
        } finally {
          _busy = false;
        }
      }
      if (value['operation'] == 'assets.apply') {
        final bundle = value['bundle'], digest = value['sha256'];
        if (!assetsEnabled ||
            _busy ||
            bundle is! String ||
            utf8.encode(bundle).length > 24576 ||
            digest is! String ||
            !RegExp(r'^[a-fA-F0-9]{64}$').hasMatch(digest) ||
            crypto.sha256.convert(utf8.encode(bundle)).toString() !=
                digest.toLowerCase()) {
          throw const FormatException();
        }
        _busy = true;
        try {
          bool authorize() =>
              _active(generation) &&
              assetsEnabled &&
              DateTime.now().millisecondsSinceEpoch < deadline;
          if (!authorize()) throw const StaleConnection();
          await applyAssets!(bundle, digest.toLowerCase(), authorize);
          if (!authorize()) throw const StaleConnection();
          return {
            'id': id,
            'accepted': true,
            'outcome': 'presentation_assets_applied',
            'sha256': digest.toLowerCase(),
          };
        } finally {
          _busy = false;
        }
      }
      if (value['operation'] != 'command' ||
          !commandsEnabled ||
          _busy ||
          !_peerCurrent() ||
          value['board_id'] != _board ||
          value['boot_id'] != _boot) {
        throw const FormatException();
      }
      final path = value['path'], rawValues = value['values'];
      if (path is! String ||
          !commandNames.containsKey(path) ||
          rawValues is! Map ||
          rawValues.length > 16 ||
          rawValues.keys.any((k) => k is! String || k.length > 64) ||
          rawValues.values.any((v) => v is! String || v.length > 512)) {
        throw const FormatException();
      }
      final values = Map<String, String>.from(rawValues);
      if (!observed()!.peer.allows(path, values)) throw const FormatException();
      _busy = true;
      try {
        bool authorize() =>
            _active(generation) &&
            commandsEnabled &&
            _peerCurrent() &&
            DateTime.now().millisecondsSinceEpoch < deadline;
        if (!authorize()) throw const StaleConnection();
        dispatched = true;
        final receipt = await _owner!.fastCommand(
          path,
          values,
          authorize: authorize,
        );
        if (!authorize()) {
          throw const OperationUncertain('Support retired before receipt');
        }
        if (receipt == null) {
          return {
            'id': id,
            'accepted': false,
            'error': 'command_unavailable',
            'replayed': false,
          };
        }
        return {
          'id': id,
          'accepted': true,
          'outcome': 'board_acknowledged',
          'physical_verified': false,
          'board_id': _board,
          'boot_id': _boot,
        };
      } finally {
        _busy = false;
      }
    } on Object {
      // Do not return raw exceptions: transport URLs can contain credentials.
      // After dispatch, uncertainty blocks more commands until explicit rotation.
      if (dispatched && generation == _epoch) _uncertain = true;
      recordIncident(
        dispatched
            ? SupportIncident.commandUncertain
            : SupportIncident.requestRejected,
      );
      return {
        'id': id,
        'accepted': false,
        'error': dispatched ? 'outcome_unverified' : 'request_rejected',
        'replayed': false,
      };
    }
  }

  Future<String> _body(HttpRequest request) async {
    final elapsed = Stopwatch()..start();
    final bytes = <int>[];
    final reader = StreamIterator(request);
    try {
      while (true) {
        final remaining = const Duration(seconds: 4) - elapsed.elapsed;
        if (remaining <= Duration.zero) throw TimeoutException('Body deadline');
        if (!await reader.moveNext().timeout(remaining)) break;
        if (bytes.length + reader.current.length > 32768) {
          throw const FormatException();
        }
        bytes.addAll(reader.current);
      }
    } finally {
      await reader.cancel();
    }
    return utf8.decode(bytes);
  }

  Future<void> _http(HttpRequest request, int generation) async {
    bool admitted = false, upgraded = false;
    try {
      request.response.headers.set('Server', PeerIdentity.current.userAgent);
      request.response.headers.set('Cache-Control', 'no-store');
      if (!_active(generation) ||
          request.headers.value('authorization') != 'Bearer $_token' ||
          request.headers.value('origin') != null) {
        request.response.statusCode = 403;
        return;
      }
      if (_requests >= 4) {
        request.response.statusCode = 429;
        return;
      }
      _requests++;
      admitted = true;
      if (request.method == 'GET' &&
          request.uri.path == '/api/support/ws' &&
          WebSocketTransformer.isUpgradeRequest(request)) {
        if (_sockets.length >= 4) {
          request.response.statusCode = 429;
          return;
        }
        final socket = await WebSocketTransformer.upgrade(request);
        upgraded = true;
        if (!_active(generation)) {
          unawaited(socket.close());
          return;
        }
        _sockets.add(socket);
        socket.pingInterval = const Duration(seconds: 20);
        bool pending = false;
        socket.listen(
          (message) {
            if (pending ||
                message is! String ||
                utf8.encode(message).length > 32768) {
              unawaited(socket.close(WebSocketStatus.policyViolation));
              return;
            }
            pending = true;
            unawaited(
              _rpc(message, generation)
                  .then((reply) {
                    if (_active(generation) &&
                        socket.readyState == WebSocket.open) {
                      socket.add(jsonEncode(reply));
                    }
                  })
                  .whenComplete(() => pending = false),
            );
          },
          onDone: () => _sockets.remove(socket),
          onError: (Object _) => _sockets.remove(socket),
        );
        return;
      }
      request.response.headers.contentType = ContentType.json;
      if (request.method == 'GET' &&
          request.uri.path == '/api/support/health') {
        request.response.write(jsonEncode(snapshot()));
      } else if (request.method == 'POST' &&
          request.uri.path == '/api/support/request') {
        request.response.write(
          jsonEncode(await _rpc(await _body(request), generation)),
        );
      } else {
        request.response.statusCode = 404;
      }
    } on Object {
      try {
        request.response.statusCode = 400;
      } on Object {
        /* Closed request. */
      }
    } finally {
      if (admitted) _requests--;
      if (!upgraded) {
        try {
          await request.response.close();
        } on Object {
          /* Peer retired. */
        }
      }
    }
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) =>
      setForeground(state == AppLifecycleState.resumed);
  void setForeground(bool value) {
    _foreground = value;
    if (!value) {
      unawaited(stop()); // Returning never re-enables a support session.
    }
  }

  Future<void> stop() async {
    ++_epoch; // Revoke before any asynchronous socket teardown.
    _token = '';
    _session = '';
    _commands = false;
    _assets = false;
    _expiry?.cancel();
    _expiry = null;
    final server = _server;
    _server = null;
    final sockets = _sockets.toList();
    _sockets.clear();
    for (final socket in sockets) {
      unawaited(socket.close(WebSocketStatus.goingAway));
    }
    if (!_disposed) notifyListeners();
    await server?.close(force: true);
  }

  @override
  void dispose() {
    _disposed = true;
    if (_observing) WidgetsBinding.instance.removeObserver(this);
    _observing = false;
    unawaited(stop());
    super.dispose();
  }
}
