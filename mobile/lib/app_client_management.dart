import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_secure_storage/flutter_secure_storage.dart';

import 'app_updates.dart';
import 'peer_identity.dart';
import 'connection_preferences.dart' show SecretStore;

typedef ManagedRequest = Future<Map<String, dynamic>> Function(
  String method,
  Uri endpoint,
  String? bearer,
  Map<String, dynamic>? body,
);

class ManagedClientSecrets implements SecretStore {
  const ManagedClientSecrets();
  static const vault = FlutterSecureStorage();
  static const key = 'pakarcontroller.managed_client';
  @override
  Future<String?> read() => vault.read(key: key);
  @override
  Future<void> write(String value) => vault.write(key: key, value: value);
}

/// Optional foreground management of THIS app, not a board command tunnel.
/// No arbitrary execution, package name, filesystem path or install bypass.
class AppClientManagement {
  AppClientManagement({
    required this.policy,
    required this.package,
    required this.checkUpdate,
    required this.cleanup,
    this.secrets = const ManagedClientSecrets(),
    ManagedRequest? request,
  }) : request = request ?? _request;
  final Map<String, dynamic> Function() policy, package;
  final Future<void> Function() checkUpdate, cleanup;
  final SecretStore secrets;
  final ManagedRequest request;
  final uptime = Stopwatch()..start();
  String _base = '', _client = '', _token = '';
  final _claimed = <String>{};
  Future<void> _writes = Future<void>.value();
  bool foreground = true, closed = false, _busy = false;
  int _epoch = 0;
  String state = 'Not enrolled';
  String get clientId => _client;
  bool get enrolled =>
      _token.isNotEmpty && _base == policy()['management_base'];
  static bool _clientId(Object? value) =>
      value is String && RegExp(r'^[A-Za-z0-9_-]{16,80}$').hasMatch(value);
  static bool _commandId(Object? value) =>
      value is String && RegExp(r'^[a-f0-9]{32}$').hasMatch(value);
  static bool _secret(Object? value) =>
      value is String && RegExp(r'^[!-~]{16,512}$').hasMatch(value);
  static Uri endpoint(String base, String path) {
    final uri = Uri.tryParse(base);
    if (uri == null ||
        uri.scheme != 'https' ||
        uri.host.isEmpty ||
        uri.userInfo.isNotEmpty ||
        uri.hasQuery ||
        uri.hasFragment ||
        base.length > 1024 ||
        RegExp(r'[\x00-\x20\x7f-\uffff\\]').hasMatch(base) ||
        uri.path.contains('..') ||
        uri.path.endsWith('/')) {
      throw const FormatException(
        'Management requires a direct HTTPS base without credentials or trailing slash',
      );
    }
    return Uri.parse('$base$path');
  }

  Future<void> load() async {
    if (_busy) return;
    await _writes.catchError((Object _) {});
    final text = await secrets.read();
    if (text == null || text.isEmpty) return;
    final raw = object(jsonDecode(text));
    endpoint('${raw['base']}', '/api/clients/report');
    if (!_clientId(raw['client_id']) ||
        !_secret(raw['token']) ||
        raw['claimed'] is! List ||
        (raw['claimed'] as List).length > 256 ||
        (raw['claimed'] as List).any((id) => !_commandId(id))) {
      throw const FormatException(
        'Invalid secure client enrollment; management disabled',
      );
    }
    _base = raw['base'];
    _client = raw['client_id'];
    _token = raw['token'];
    _claimed.addAll((raw['claimed'] as List).cast<String>());
    state = 'Enrolled';
  }

  Future<void> enroll(
    String base,
    String enrollmentToken,
    String clientId,
  ) async {
    if (_busy || closed || !foreground) {
      throw StateError('Enrollment is not active');
    }
    endpoint(base, '/api/clients/enroll');
    if (!_secret(enrollmentToken) || !_clientId(clientId)) {
      throw const FormatException('Use a valid one-use enrollment token');
    }
    final epoch = _epoch;
    _busy = true;
    try {
      final receipt = await request(
        'POST',
        endpoint(base, '/api/clients/enroll'),
        null,
        {
          'enrollment_token': enrollmentToken,
          'client_id': clientId,
          'platform': 'android',
          'application': 'PakarController',
        },
      );
      if (receipt['client_id'] != clientId ||
          !_clientId(receipt['client_id']) ||
          !_secret(receipt['token'])) {
        throw const FormatException('Invalid enrollment receipt; not retried');
      }
      // Store first. Failure never falls back to plaintext or repeats enrollment.
      final stored = jsonEncode({
        'base': base,
        'client_id': receipt['client_id'],
        'token': receipt['token'],
        'claimed': <String>[],
      });
      if (closed || epoch != _epoch) return;
      await _store(stored);
      if (closed || epoch != _epoch) return;
      _base = base;
      _client = receipt['client_id'];
      _token = receipt['token'];
      _claimed.clear();
      state = 'Enrolled';
    } finally {
      _busy = false;
    }
  }

  Future<void> forget() async {
    ++_epoch;
    await _store('');
    _base = _client = _token = '';
    _claimed.clear();
    state = 'Not enrolled';
  }

  void setForeground(bool value) {
    foreground = value;
    if (!value) ++_epoch;
  }

  void retireRequests() {
    ++_epoch;
  }

  bool _current(int epoch) =>
      !closed &&
      foreground &&
      epoch == _epoch &&
      enrolled &&
      policy()['management_enabled'] == true;

  Future<void> reportAndReceive(
    String updateState, {
    int? receivedBytes,
    int? expectedBytes,
  }) async {
    if (_busy || !_current(_epoch)) return;
    _busy = true;
    final epoch = _epoch;
    try {
      final info = package();
      final abis = info['device_abis'];
      final abi = abis is List && abis.isNotEmpty && abis.first is String
          ? abis.first
          : '';
      final architecture = switch (abi) {
        'arm64-v8a' => 'arm64',
        'armeabi-v7a' => 'armv7',
        'x86_64' => 'x86_64',
        'x86' => 'x86',
        _ => 'unknown',
      };
      await request('POST', endpoint(_base, '/api/clients/report'), _token, {
        'client_id': _client,
        'application': 'PakarController',
        'platform': 'android',
        'version': info['version_name'] is String ? info['version_name'] : '',
        'build': info['version_code'] is int ? info['version_code'] : 0,
        'uptime_seconds': uptime.elapsed.inSeconds,
        'update_state': RegExp(r'^[a-z_ -]{1,48}$').hasMatch(updateState)
            ? updateState
            : 'unknown',
        'architecture': architecture,
        'os': 'android',
        'foreground': foreground,
        'auto_check': policy()['auto_check'] == true,
        'auto_update':
            policy()['auto_install'] == true &&
            policy()['auto_download'] == true,
        'auto_cleanup': policy()['auto_cleanup'] == true,
        'allow_remote_uninstall': false,
        if (receivedBytes != null &&
            receivedBytes >= 0 &&
            receivedBytes <= AppUpdates.maximum)
          'received_bytes': receivedBytes,
        if (expectedBytes != null &&
            expectedBytes >= 0 &&
            expectedBytes <= AppUpdates.maximum &&
            (receivedBytes == null || receivedBytes <= expectedBytes))
          'expected_bytes': expectedBytes,
      });
      if (!_current(epoch)) return;
      final response = await request(
        'GET',
        endpoint(_base, '/api/client/commands'),
        _token,
        null,
      );
      if (!_current(epoch)) return;
      final commands = response['commands'];
      if (commands is! List || commands.length > 16) {
        throw const FormatException('Invalid management command batch');
      }
      for (final raw in commands) {
        if (!_current(epoch)) break;
        if (raw is! Map) continue;
        final command = Map<String, dynamic>.from(raw);
        final id = command['id'],
            action = command['action'],
            expires = command['expires_at_ms'];
        final now = DateTime.now().millisecondsSinceEpoch;
        final allowed =
            action == 'check-update' &&
                policy()['allow_remote_check'] == true ||
            action == 'cleanup' && policy()['allow_remote_cleanup'] == true;
        // Remote uninstall is deliberately unavailable pending its explicit
        // local/user-consent policy. It is not silently mapped to cleanup.
        if (!_commandId(id) ||
            !allowed ||
            expires is! int ||
            expires <= now ||
            expires > now + 24 * 60 * 60 * 1000 ||
            _claimed.contains(id)) {
          continue;
        }
        if (_claimed.length >= 256) {
          throw StateError(
            'Management receipt limit reached; no command was replayed',
          );
        }
        final next = {..._claimed, id as String};
        await _store(
          jsonEncode({
            'base': _base,
            'client_id': _client,
            'token': _token,
            'claimed': next.toList(),
          }),
        );
        _claimed.add(id); // Local write-ahead survives restart/uncertain claim.
        if (!_current(epoch)) break;
        final claim = await request(
          'POST',
          endpoint(
            _base,
            '/api/client/commands/${Uri.encodeComponent(id)}/claim',
          ),
          _token,
          {},
        );
        if (!_current(epoch) ||
            claim['claimed'] != true ||
            DateTime.now().millisecondsSinceEpoch >= expires) {
          continue;
        }
        final stillAllowed = action == 'check-update'
            ? policy()['allow_remote_check'] == true
            : policy()['allow_remote_cleanup'] == true;
        if (!stillAllowed) continue;
        var result = 'requested',
            detail =
                'Foreground action requested; no installation/removal proof';
        try {
          if (action == 'check-update') {
            await checkUpdate();
          } else {
            await cleanup();
          }
        } on Object {
          result = 'failed';
          detail = 'Local action failed; not replayed';
        }
        if (!_current(epoch)) break;
        await request(
          'POST',
          endpoint(
            _base,
            '/api/client/commands/${Uri.encodeComponent(id)}/result',
          ),
          _token,
          {'state': result, 'detail': detail},
        );
      }
      state = 'Reported';
    } on Object {
      state = 'Management unavailable; no command replay';
      rethrow;
    } finally {
      _busy = false;
    }
  }

  static Future<Map<String, dynamic>> _request(
    String method,
    Uri uri,
    String? bearer,
    Map<String, dynamic>? body,
  ) async {
    final client = HttpClient()..connectionTimeout = const Duration(seconds: 5);
    try {
      final request = await client
          .openUrl(method, uri)
          .timeout(const Duration(seconds: 5));
      request.followRedirects = false;
      identifyRequest(request);
      if (bearer != null) {
        request.headers.set('Authorization', 'Bearer $bearer');
      }
      if (body != null) {
        request.headers.contentType = ContentType.json;
        request.write(jsonEncode(body));
      }
      final response = await request.close().timeout(
        const Duration(seconds: 8),
      );
      if (!{200, 201, 202}.contains(response.statusCode)) {
        throw const HttpException('Client management request rejected');
      }
      return object(
        jsonDecode(
          utf8.decode(
            await readAppUpdateBytes(
              response,
              maximum: 32768,
              deadline: const Duration(seconds: 8),
            ),
          ),
        ),
      );
    } finally {
      client.close(force: true);
    }
  }

  Future<void> _store(String value) {
    final task = _writes
        .catchError((Object _) {})
        .then((_) => secrets.write(value));
    _writes = task;
    return task;
  }

  void dispose() {
    closed = true;
    ++_epoch;
    uptime.stop();
  }
}
