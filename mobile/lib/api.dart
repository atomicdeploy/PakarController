import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:http/http.dart' as http;
import 'package:web_socket_channel/io.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

import 'models.dart';

class PakarApi {
  PakarApi({
    String baseUrl = 'http://ledboard.local',
    String user = 'admin',
    String password = 'pakarcontroller',
    http.Client? client,
    this.discovery,
  }) : _client = client ?? http.Client(),
       _baseUrl = normalizeEndpoint(baseUrl),
       // Preserve public argument names; private setters own revisions.
       // ignore: prefer_initializing_formals
       _user = user,
       // ignore: prefer_initializing_formals
       _password = password;

  String _baseUrl, _user, _password;
  int _connectionRevision = 0;
  int get connectionRevision => _connectionRevision;
  String get baseUrl => _baseUrl;
  set baseUrl(String value) {
    final normalized = normalizeEndpoint(value);
    if (_baseUrl != normalized) {
      _baseUrl = normalized;
      resetTransport();
    }
  }

  String get user => _user;
  set user(String value) {
    if (_user != value) {
      _user = value;
      resetTransport();
    }
  }

  String get password => _password;
  set password(String value) {
    if (_password != value) {
      _password = value;
      resetTransport();
    }
  }

  final http.Client _client;
  final Future<Set<String>> Function()? discovery;
  WebSocketChannel? _channel;
  String? _discoveredBaseUrl;
  String? _transportOwner;
  String? _verifiedEndpoint;

  Map<String, String> get _headers => {
    'authorization': 'Basic ${base64Encode(utf8.encode('$user:$password'))}',
  };

  String get transportUrl =>
      _transportOwner == baseUrl ? _discoveredBaseUrl ?? baseUrl : baseUrl;

  static String normalizeEndpoint(String value) {
    final text = value.trim();
    final uri = Uri.tryParse(text.contains('://') ? text : 'http://$text');
    if (uri == null ||
        !['http', 'https'].contains(uri.scheme) ||
        uri.host.isEmpty ||
        uri.userInfo.isNotEmpty ||
        uri.hasQuery ||
        uri.hasFragment ||
        (uri.path.isNotEmpty && uri.path != '/')) {
      throw const FormatException(
        'Enter an HTTP(S) device hostname or origin without credentials or a path',
      );
    }
    return uri.replace(path: '').toString();
  }

  Uri _uri(String base, String path, [Map<String, String>? query]) =>
      Uri.parse('${base.replaceFirst(RegExp(r'/$'), '')}$path')
          .replace(queryParameters: query);

  Future<Map<String, dynamic>> _get(String path) async {
    final revision = connectionRevision, headers = _headers;
    final response = await _withDiscovery(
      (base) => _client
          .get(_uri(base, path), headers: headers)
          .timeout(const Duration(seconds: 6)),
    );
    if (revision != connectionRevision) throw const StaleConnection();
    if (response.statusCode ~/ 100 != 2) throw Exception(response.body);
    return jsonDecode(response.body) as Map<String, dynamic>;
  }

  Future<Map<String, dynamic>> getJson(String path) => _get(path);

  /// Single dispatch, form BODY (never secret-bearing URL parameters), and
  /// caller-owned capability check after any awaited identity verification.
  Future<Map<String, dynamic>> postJson(
    String path,
    Map<String, String> values, {
    bool Function()? authorize,
  }) async {
    final revision = connectionRevision;
    if (_verifiedEndpoint != baseUrl) await status();
    if (revision != connectionRevision || authorize?.call() == false) {
      throw const StaleConnection();
    }
    late final http.Response response;
    try {
      response = await _client
          .post(_uri(transportUrl, path), headers: _headers, body: values)
          .timeout(const Duration(seconds: 12));
    } on Exception {
      throw const OperationUncertain();
    }
    if (revision != connectionRevision) throw const StaleConnection();
    if (response.statusCode ~/ 100 != 2) {
      // Never echo a possibly secret-bearing server error or request value.
      throw FormatException(
        'Device write rejected (HTTP ${response.statusCode}); not retried',
      );
    }
    final result = jsonDecode(response.body);
    if (result is! Map<String, dynamic>) throw const OperationUncertain();
    return result;
  }

  Future<void> post(
    String path, [
    Map<String, String> values = const {},
  ]) async {
    final revision = connectionRevision;
    // Resolve/verify through a read first; a POST must never be automatically
    // replayed after a timeout or disconnect (the board may have applied it).
    if (_verifiedEndpoint != baseUrl) await status();
    if (revision != connectionRevision) throw const StaleConnection();
    late final http.Response response;
    try {
      response = await _client
          .post(_uri(transportUrl, path, values), headers: _headers)
          .timeout(const Duration(seconds: 12));
    } on Exception {
      throw const OperationUncertain();
    }
    if (revision != connectionRevision) throw const StaleConnection();
    if (response.statusCode ~/ 100 != 2) throw Exception(response.body);
  }

  Future<DeviceStatus> status() async {
    final result = DeviceStatus(await _get('/api/v1/status'));
    final expected = _localLabel();
    if (expected != null && result.json['hostname'] != expected) {
      throw const FormatException(
        'Device identity does not match the requested hostname',
      );
    }
    _verifiedEndpoint = baseUrl;
    return result;
  }

  Future<CellMap> mapping() async =>
      CellMap.fromJson(await _get('/api/v1/cells/mapping'));

  Stream<DeviceStatus> liveStatus() async* {
    final revision = connectionRevision;
    if (_verifiedEndpoint != baseUrl) await status();
    if (revision != connectionRevision) throw const StaleConnection();
    await _channel?.sink.close();
    if (revision != connectionRevision) throw const StaleConnection();
    final source = Uri.parse(transportUrl);
    final uri = source.replace(
      scheme: source.scheme == 'https' ? 'wss' : 'ws',
      path: '/ws',
      query: null,
    );
    final active = IOWebSocketChannel.connect(
      uri,
      headers: _headers,
      pingInterval: const Duration(seconds: 20),
    );
    _channel = active;
    await active.ready;
    if (revision != connectionRevision) {
      await active.sink.close();
      return;
    }
    active.sink.add('{"type":"status"}');
    await for (final message in active.stream) {
      if (revision != connectionRevision) return;
      if (message is! String) continue;
      final value = jsonDecode(message);
      if (value is Map<String, dynamic> && value.containsKey('hostname')) {
        final expected = _localLabel();
        if (expected != null && value['hostname'] != expected) {
          throw const StaleConnection();
        }
        yield DeviceStatus(value);
      }
    }
  }

  Future<void> close() async {
    ++_connectionRevision;
    final active = _channel;
    _channel = null;
    _verifiedEndpoint = _transportOwner = _discoveredBaseUrl = null;
    await active?.sink.close();
    _client.close();
  }

  void resetTransport() {
    ++_connectionRevision;
    _discoveredBaseUrl = _transportOwner = _verifiedEndpoint = null;
    final active = _channel;
    _channel = null;
    if (active != null) {
      unawaited(active.sink.close().then<void>((_) {}, onError: (Object _) {}));
    }
  }

  String? _localLabel() {
    final host = Uri.tryParse(baseUrl)?.host.toLowerCase();
    if (host == null || !host.endsWith('.local')) return null;
    final label = host.substring(0, host.length - 6);
    return RegExp(r'^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$').hasMatch(label)
        ? label
        : null;
  }

  Future<http.Response> _withDiscovery(
    Future<http.Response> Function(String base) request,
  ) async {
    final revision = connectionRevision, headers = _headers;
    try {
      return await request(transportUrl);
    } on Exception {
      if (revision != connectionRevision) throw const StaleConnection();
      if (_localLabel() == null) rethrow;
    }

    _discoveredBaseUrl = null;
    _transportOwner = baseUrl;
    for (final candidate in await (discovery?.call() ?? _discoverSsdp())) {
      if (revision != connectionRevision) throw const StaleConnection();
      final origin = Uri.tryParse(candidate);
      if (origin == null ||
          !['http', 'https'].contains(origin.scheme) ||
          origin.userInfo.isNotEmpty) {
        continue;
      }
      try {
        final probe = await _client
            .get(_uri(candidate, '/api/v1/status'), headers: headers)
            .timeout(const Duration(seconds: 2));
        if (probe.statusCode ~/ 100 != 2) continue;
        if (revision != connectionRevision) throw const StaleConnection();
        final json = jsonDecode(probe.body);
        if (json is! Map<String, dynamic> ||
            json['hostname'] != _localLabel()) {
          continue;
        }
      } on Object {
        if (revision != connectionRevision) throw const StaleConnection();
        continue;
      }
      _discoveredBaseUrl = candidate;
      // Only the read probe can advance to another candidate. Do not swallow
      // an error from the selected request and silently choose another board.
      return await request(candidate);
    }
    throw SocketException(
      '${Uri.parse(baseUrl).host} did not resolve and no matching authenticated SSDP device replied',
    );
  }

  Future<Set<String>> _discoverSsdp() async {
    final socket = await RawDatagramSocket.bind(InternetAddress.anyIPv4, 0);
    socket.multicastHops = 2;
    final results = <String>{};
    final done = Completer<void>();
    late final StreamSubscription<RawSocketEvent> subscription;
    subscription = socket.listen((event) {
      if (event != RawSocketEvent.read) return;
      final packet = socket.receive();
      if (packet == null) return;
      final text = utf8.decode(packet.data, allowMalformed: true);
      final lines = const LineSplitter().convert(text);
      if (lines.isEmpty ||
          !lines.first.toUpperCase().startsWith('HTTP/1.1 200')) {
        return;
      }
      for (final line in lines.skip(1)) {
        final separator = line.indexOf(':');
        if (separator < 0 ||
            line.substring(0, separator).trim().toLowerCase() != 'location') {
          continue;
        }
        final location = Uri.tryParse(line.substring(separator + 1).trim());
        if (location?.host.isEmpty ?? true) continue;
        results.add(
          location!
              .replace(path: '', query: null, fragment: null)
              .toString()
              .replaceFirst(RegExp(r'/$'), ''),
        );
      }
    });
    const request =
        'M-SEARCH * HTTP/1.1\r\n'
        'HOST: 239.255.255.250:1900\r\n'
        'MAN: "ssdp:discover"\r\n'
        'MX: 1\r\n'
        'ST: ssdp:all\r\n\r\n';
    socket.send(utf8.encode(request), InternetAddress('239.255.255.250'), 1900);
    Timer(const Duration(milliseconds: 1400), done.complete);
    await done.future;
    await subscription.cancel();
    socket.close();
    return results;
  }
}

class StaleConnection implements Exception {
  const StaleConnection();
  @override
  String toString() =>
      'Connection changed; obsolete board replies and unsent commands were discarded.';
}

class OperationUncertain implements Exception {
  const OperationUncertain();
  @override
  String toString() =>
      'Command outcome unknown; it was not retried. Refresh device state before deciding whether to send again.';
}
