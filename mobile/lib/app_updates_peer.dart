import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:math';

import 'package:flutter/foundation.dart';

import 'app_updates.dart';

typedef AppUpdateApproval = Future<bool> Function(
  Map<String, dynamic> manifest,
);

String _token() => base64Url
    .encode(List<int>.generate(32, (_) => Random.secure().nextInt(256)))
    .replaceAll('=', '');
bool _sameSecret(String a, String b) {
  if (a.isEmpty || a.length != b.length) return false;
  var different = 0;
  for (var i = 0; i < a.length; ++i) {
    different |= a.codeUnitAt(i) ^ b.codeUnitAt(i);
  }
  return different == 0;
}

/// Actual PCController peer_upgrades.dart approval/capability mechanism,
/// adapted to the living Pakar /api/rpc contract. Advertising has one separate
/// owner (AppUpdateDiscovery), not another competing multicast/board server.
/// Explicit opt-in, foreground only: no settings/commands/installer exposed.
class AppUpdatePeers extends ChangeNotifier {
  AppUpdatePeers(this.updates, {required this.approve});
  final AppUpdates updates;
  final AppUpdateApproval approve;
  final String instanceId = _token();
  HttpServer? _server;
  Timer? _expiry;
  int _epoch = 0, _active = 0;
  bool _offering = false, _closed = false;
  String? _capability, _transfer;
  DateTime? _expires;
  bool get running => _server != null;
  int? get port => _server?.port;

  Future<void> start({int port = 18992}) async {
    if (_closed) throw StateError('App sharing disposed');
    if (running) return;
    if (port < 0 || port > 65535) {
      throw const FormatException('Invalid sharing port');
    }
    final epoch = ++_epoch;
    await updates.rpc(
      'app.package.info',
      {},
    ); // Never advertise split/unreadable APK.
    final server = await HttpServer.bind(InternetAddress.anyIPv4, port);
    if (_closed || epoch != _epoch) {
      await server.close(force: true);
      return;
    }
    _server = server;
    server.idleTimeout = const Duration(seconds: 5);
    server.listen((request) => unawaited(_request(request, epoch)));
    _expiry = Timer.periodic(
      const Duration(seconds: 10),
      (_) => unawaited(_expire()),
    );
    notifyListeners();
  }

  Future<Map<String, dynamic>> manifest() async => {
    'product': 'PakarController',
    'kind': 'android-app',
    'name': 'PakarController Android',
    'instance_id': instanceId,
    'package_sharing': true,
    'app_updates': await updates.rpc('app.package.info', {}),
    'rpc': '/api/rpc',
    'capabilities': [
      'app.package.info',
      'app.package.read',
      'app.upgrade.offer',
    ],
  };

  Future<void> _request(HttpRequest request, int epoch) async {
    final response = request.response;
    response.headers.contentType = ContentType.json;
    response.headers.set('Cache-Control', 'no-store');
    response.headers.set('X-Content-Type-Options', 'nosniff');
    if (!running || epoch != _epoch || _active >= 4) {
      response.statusCode = 429;
      await response.close();
      return;
    }
    _active++;
    dynamic id;
    try {
      if (request.method == 'GET' && request.uri.path == '/healthz') {
        response.write(
          jsonEncode({'status': 'ok', 'service': 'pakarcontroller-mobile'}),
        );
      } else if (request.method == 'GET' &&
          request.uri.path == '/api/app-updates') {
        response.write(jsonEncode(await manifest()));
      } else if (request.method == 'POST' && request.uri.path == '/api/rpc') {
        // An arbitrary web page must not approve/write a local app update.
        if (request.headers.value('origin') != null ||
            request.contentLength > 32768) {
          throw const FormatException('Browser or oversized APK RPC rejected');
        }
        final bytes = await readAppUpdateBytes(request);
        final value = object(jsonDecode(utf8.decode(bytes)));
        id = value['id'];
        if (value['jsonrpc'] != '2.0' ||
            (id is! int && id is! String) ||
            (id is String && id.length > 128) ||
            value['method'] is! String ||
            value['params'] is! Map) {
          throw const FormatException('Invalid APK RPC');
        }
        final result = await handleRpc(
          value['method'],
          object(value['params']),
          authorization: request.headers.value('authorization') ?? '',
          source: request.connectionInfo?.remoteAddress.address ?? '',
        );
        if (!running || epoch != _epoch) {
          throw StateError('Sharing session changed');
        }
        response.write(
          jsonEncode({'jsonrpc': '2.0', 'id': id, 'result': result}),
        );
      } else {
        response.statusCode = 404;
      }
    } catch (error) {
      response.write(
        jsonEncode({
          'jsonrpc': '2.0',
          'id': id,
          'error': {'code': -32000, 'message': '$error'},
        }),
      );
    } finally {
      _active--;
      try {
        await response.close();
      } on Object {
        /* Request was retired. */
      }
    }
  }

  /// Shared HTTP/WS dispatch. Only callable while explicitly sharing; remote
  /// callers can stage one approved digest but can never invoke Android install.
  Future<Map<String, dynamic>> handleRpc(
    String method,
    Map<String, dynamic> params, {
    String authorization = '',
    String source = '',
  }) async {
    if (!running || _closed) throw StateError('App sharing is off');
    final epoch = _epoch;
    if (method == 'app.package.info' || method == 'app.package.read') {
      return updates.rpc(method, params);
    }
    if (method == 'app.upgrade.offer') {
      if (_offering ||
          _transfer != null ||
          updates.busy ||
          updates.state['transfer_id'] != null) {
        throw StateError('Another APK transfer is pending');
      }
      _offering = true;
      try {
        final installed = await updates.rpc('app.package.info', {});
        validateAppUpdateManifest(params, installed);
        final accepted = await approve({...params, 'source': source})
            .timeout(const Duration(seconds: 30), onTimeout: () => false);
        if (!accepted || !running || epoch != _epoch) {
          throw StateError('APK offer not approved');
        }
        final started = await updates.rpc('app.upgrade.begin', {
          'bytes': params['bytes'],
          'sha256': params['sha256'],
        });
        final transfer = started['transfer_id'] as String;
        if (!running || epoch != _epoch) {
          await updates.rpc('app.upgrade.abort', {'transfer_id': transfer});
          throw StateError('Sharing session changed');
        }
        _transfer = transfer;
        _capability = _token();
        _expires = DateTime.now().add(const Duration(minutes: 5));
        return {...started, 'capability': _capability};
      } finally {
        _offering = false;
      }
    }
    if (!{
      'app.upgrade.write',
      'app.upgrade.finish',
      'app.upgrade.abort',
      'app.upgrade.status',
    }.contains(method)) {
      throw const FormatException(
        'Only self-package transfer methods are shared',
      );
    }
    if (_capability == null ||
        _transfer == null ||
        !_expires!.isAfter(DateTime.now()) ||
        params['transfer_id'] != _transfer ||
        !_sameSecret(authorization, 'Bearer $_capability')) {
      throw StateError('Transfer authorization expired or invalid');
    }
    final result = await updates.rpc(method, params);
    if (method == 'app.upgrade.finish' || method == 'app.upgrade.abort') {
      _capability = null;
      _transfer = null;
      _expires = null;
    }
    return result;
  }

  Future<void> _expire() async {
    if (_transfer == null || _expires!.isAfter(DateTime.now())) return;
    final transfer = _transfer;
    _transfer = null;
    _capability = null;
    _expires = null;
    try {
      await updates.rpc('app.upgrade.abort', {'transfer_id': transfer});
    } on Object {
      /* Retain failure; never delete another transfer. */
    }
  }

  Future<void> close() async {
    ++_epoch;
    _expiry?.cancel();
    _expiry = null;
    final server = _server;
    _server = null;
    final transfer = _transfer;
    _transfer = null;
    _capability = null;
    _expires = null;
    await server?.close(force: true);
    if (transfer != null) {
      try {
        await updates.rpc('app.upgrade.abort', {'transfer_id': transfer});
      } on Object {
        /* Exact abandoned staging only, no retry. */
      }
    }
    if (!_closed) notifyListeners();
  }

  @override
  void dispose() {
    _closed = true;
    unawaited(close());
    super.dispose();
  }
}
