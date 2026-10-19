import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:http/http.dart' as http;
import 'package:http/io_client.dart';
import 'package:crypto/crypto.dart';
import 'package:web_socket_channel/io.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

import 'models.dart';
import 'board_sound.dart';
import 'lan_resolver.dart';
import 'command_wire.dart';
import 'command_failure.dart';
import 'pixel_diagnostic.dart';
import 'ble_transport.dart';
import 'peer_identity.dart';
import 'peer_policy.dart';

enum ConnectionMethod { network, usb, bluetooth }

class PakarApi {
  PakarApi({
    String baseUrl = 'http://ledboard.local',
    String user = 'admin',
    String password = '',
    http.Client? client,
    LanResolver? resolver,
    PakarBleTransport? bluetooth,
    this.discovery,
    this.readTimeout = const Duration(seconds: 6),
    this.enforcePeerVersions = peerVersionChecksEnabled,
  }) : resolver = resolver ?? LanResolver(),
       bluetooth = bluetooth ?? PakarBleTransport(),
       _baseUrl = normalizeEndpoint(baseUrl),
       // Preserve public argument names; private setters own revisions.
       // ignore: prefer_initializing_formals
       _user = user,
       // ignore: prefer_initializing_formals
       _password = password {
    if (readTimeout <= Duration.zero ||
        readTimeout > const Duration(seconds: 30)) {
      throw ArgumentError.value(readTimeout, 'readTimeout');
    }
    _client = client ?? IOClient(_ioClient);
    if (_baseUrl.startsWith('ble://')) {
      _connectionMethod = ConnectionMethod.bluetooth;
    }
  }

  String _baseUrl, _user, _password;
  int _connectionRevision = 0;
  int get connectionRevision => _connectionRevision;
  bool _connectionWanted = true;
  // Optional UI reads must stop synchronously, even before Android renders the
  // frame that propagates inactive page properties to hidden widgets.
  bool _appForeground = true;
  bool get appForeground => _appForeground;
  set appForeground(bool value) {
    if (value == _appForeground) return;
    _appForeground = value;
    if (!_connectionUpdates.isClosed) _connectionUpdates.add(null);
  }

  final _connectionUpdates = StreamController<void>.broadcast(sync: true);
  Stream<void> get connectionUpdates => _connectionUpdates.stream;
  bool get connectionWanted => _connectionWanted;
  ConnectionMethod _connectionMethod = ConnectionMethod.network;
  ConnectionMethod get connectionMethod => _connectionMethod;
  set connectionMethod(ConnectionMethod value) {
    if (_connectionMethod == value) return;
    _connectionMethod = value;
    resetTransport();
  }

  // Android USB-host transport is not implemented yet. Never substitute an
  // HTTP/WS connection, discovery, or a remote bridge for explicit USB intent.
  void _requireNetwork() {
    if (_unsupportedPeer != null) throw _unsupportedPeer!;
    if (!_connectionWanted) throw const ConnectionStopped();
    if (_connectionMethod == ConnectionMethod.usb) {
      throw const FormatException(
        'Direct Android USB is not available in this build; no network fallback was attempted',
      );
    }
  }

  void disconnect() {
    _connectionWanted = false;
    resetTransport();
  }

  void connect() {
    _connectionWanted = true;
    resetTransport();
  }

  void selectTransport(
    ConnectionMethod method,
    String endpoint, {
    String board = '',
  }) {
    final normalized = normalizeEndpoint(endpoint);
    if (method == ConnectionMethod.usb ||
        (method == ConnectionMethod.bluetooth) !=
            normalized.startsWith('ble://')) {
      throw const FormatException('Select an endpoint for this transport');
    }
    _connectionWanted = false;
    _connectionMethod = method;
    _baseUrl = normalized;
    expectedBluetoothBoard = board;
    _identityRequired = false;
    resetTransport();
  }

  void Function()? onPeerChanged;
  void Function()? onTransportRetired;
  void Function(Map<String, dynamic>)? onPeripheralEvent;
  void Function()? onPeripheralStreamLost;
  bool _identityRequired = false;
  String get baseUrl => _baseUrl;
  set baseUrl(String value) {
    final normalized = normalizeEndpoint(value);
    if (_baseUrl != normalized) {
      _baseUrl = normalized;
      _identityRequired = false;
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

  late final http.Client _client;
  final LanResolver resolver;
  final PakarBleTransport bluetooth;
  String expectedBluetoothBoard = '';
  bool get isBluetooth => connectionMethod == ConnectionMethod.bluetooth;
  bool get supportsBinaryFirmware =>
      connectionMethod == ConnectionMethod.network;
  bool supportsRoute(String method, String path) {
    if (!isBluetooth) return connectionMethod == ConnectionMethod.network;
    final ble = _latestStatus?.json['ble'];
    return ble is Map &&
        ble['routes'] is List &&
        (ble['routes'] as List).contains('$method $path');
  }

  late final HttpClient _ioClient = resolver.createClient()
    ..userAgent = PeerIdentity.current.userAgent;
  final Future<Set<String>> Function()? discovery;
  final Duration readTimeout;
  final bool enforcePeerVersions;
  UnsupportedPeerVersion? _unsupportedPeer;
  UnsupportedPeerVersion? get unsupportedPeer => _unsupportedPeer;
  Map<String, String> receivedServerHeaders = const {};
  String? _introducedPeer;
  String? get selectedRouteLocalIp =>
      _verifiedEndpoint == baseUrl &&
          _verifiedBoard != null &&
          _verifiedBoot != null &&
          !isBluetooth
      ? resolver.localAddressFor(Uri.parse(transportUrl))
      : null;
  Stopwatch? _introductionAge;
  final _readAborts = <Completer<void>>{};
  WebSocketChannel? _channel;
  bool _socketReady = false;
  DeviceStatus? _latestStatus;
  final _statusUpdates = StreamController<DeviceStatus>.broadcast(sync: true);
  Stream<DeviceStatus> get statusUpdates => _statusUpdates.stream;
  // RGB-only observations never drive full-status consumers/firmware policy.
  final _rgbUpdates = StreamController<DeviceStatus>.broadcast(sync: true);
  Stream<DeviceStatus> get rgbUpdates => _rgbUpdates.stream;
  DeviceStatus? get latestStatus => _latestStatus;
  Stopwatch? _statusAge;
  Stopwatch? _liveAge, _socketAge;
  Duration? get liveSilence => _liveAge?.elapsed ?? _socketAge?.elapsed;
  final _commandReplies = <String, Completer<Map<String, dynamic>?>>{};
  bool get liveFresh =>
      (isBluetooth ? bluetooth.ready : _socketReady) &&
      _liveAge != null &&
      _liveAge!.elapsedMilliseconds < 4000;

  Uri get webSocketUri {
    final source = Uri.parse(transportUrl);
    return source.replace(
      scheme: source.scheme == 'https' ? 'wss' : 'ws',
      // Dart does not infer a default port for ws/wss when replacing schemes.
      port: source.port,
      path: '/ws',
      query: null,
    );
  }

  /// A resume probe is a read on the existing socket, not a new connection.
  bool requestLiveStatus() {
    if (isBluetooth) {
      if (!_connectionWanted ||
          !bluetooth.ready ||
          bluetooth.busy ||
          !appForeground) {
        return false;
      }
      unawaited(status().then<void>((_) {}, onError: (Object _) {}));
      return true;
    }
    if (!_connectionWanted || !_socketReady || _channel == null) return false;
    _channel!.sink.add('{"type":"status"}');
    return true;
  }

  bool _diagnosticPending = false;

  /// One deliberate full-frame commit on the selected peer. Prefer its atomic
  /// acknowledged command; older firmware uses the existing raw socket path.
  /// No reconnect, UDP substitution, delayed queue or new-intent replay.
  Future<DeviceStatus> pixelDiagnostic(
    PixelProbe probe, {
    bool off = false,
    required bool Function() authorize,
  }) async {
    _requireNetwork();
    if (_diagnosticPending) {
      throw const FormatException('A diagnostic frame is still pending');
    }
    final revision = connectionRevision;
    final board = _verifiedBoard, boot = _verifiedBoot;
    void admit() {
      _requireNetwork();
      if (board == null ||
          boot == null ||
          !appForeground ||
          revision != connectionRevision ||
          _verifiedBoard != board ||
          _verifiedBoot != boot ||
          !authorize()) {
        throw const StaleConnection();
      }
    }

    _diagnosticPending = true;
    try {
      final frame = probe.frame(off: off);
      admit();
      var current = await _commandStatus(authorize);
      admit();
      if (!supportsPixelDiagnostic(current) ||
          current.json['programming_latch'] == true ||
          (current.json['remote_firmware'] as Map?)?['busy'] == true) {
        throw const FormatException(
          'Pixel diagnostics unavailable during programming or updates',
        );
      }
      if (current.peer.has('pixel-diagnostic')) {
        // Board owns clear/stage/show in one transaction. Existing request IDs
        // allow lost WS receipts to recover over HTTP without a second intent.
        final actual = await fastCommand('/api/led/diagnostic', {
          'index': '${probe.first}',
          'count': '${probe.count}',
          'rgb': '${probe.rgb}',
          'off': '$off',
        }, authorize: authorize);
        admit();
        if (actual == null) {
          throw const FormatException(
            'The board advertises pixel diagnostics but did not acknowledge the command contract',
          );
        }
        if (!diagnosticReady(actual)) {
          throw const OperationUncertain(
            'The board left diagnostic output mode before readback.',
          );
        }
        final pixels = actual.json['pixel_frame'];
        if (pixels is! Map) {
          throw const OperationUncertain(
            'The board accepted the command but returned no pixel frame.',
          );
        }
        if (pixels['pending'] == true) {
          throw const OperationUncertain(
            'The LED frame is still pending; completed output has not been observed.',
          );
        }
        if (!matchesDiagnosticFrame(actual, frame)) {
          throw const OperationUncertain(
            'The board pixel readback differs from the requested frame.',
          );
        }
        return actual;
      }
      if (!_socketReady || _channel == null) {
        throw const FormatException(
          'This firmware needs WebSocket streaming for diagnostics. Update the board for acknowledged HTTP fallback.',
        );
      }
      Future<void> configure(String path, Map<String, String> values) async {
        admit();
        current =
            await fastCommand(path, values, authorize: authorize) ??
            (() => throw const FormatException(
              'Acknowledged commands required for diagnostics',
            ))();
        admit();
      }

      if (current.mode != 'cell') {
        await configure('/api/mode', {'value': 'cell'});
      }
      if (current.effect != 'none') {
        await configure('/api/effect', {'value': 'none'});
      }
      if (current.ledOutputEnabled != true ||
          current.ledOutputMode != 'effects') {
        await configure('/api/led/output', {
          'enabled': 'true',
          'mode': 'effects',
        });
      }
      // Correlated command replies contain actual same-boot output state.
      // Reuse that or the fresh WS snapshot instead of three HTTP round trips.
      var prior =
          (current.json['pixel_frame'] as Map?)?['revision'] ??
          ((current.json['led_output'] as Map?)?['driver']
              as Map?)?['revision'];
      if (prior is! int) {
        current = DeviceStatus(await _get('/api/status?pixels=1'));
        admit();
        _acceptIdentity(current.json);
        admit();
        prior = (current.json['pixel_frame'] as Map?)?['revision'];
      }
      if (!diagnosticReady(current)) {
        throw const FormatException('Diagnostic output mode not acknowledged');
      }
      final socket = _channel!;
      for (final packet in diagnosticPackets(frame)) {
        admit();
        if (!identical(socket, _channel)) throw const StaleConnection();
        socket.sink.add(packet);
      }
      final clock = Stopwatch()..start();
      do {
        final actual = DeviceStatus(await _get('/api/status?pixels=1'));
        admit();
        _acceptIdentity(actual.json);
        admit();
        if (!diagnosticReady(actual)) {
          throw const OperationUncertain(
            'The board left diagnostic output mode before readback.',
          );
        }
        final next = (actual.json['pixel_frame'] as Map?)?['revision'];
        if (next != prior && matchesDiagnosticFrame(actual, frame)) {
          _rememberStatus(actual);
          return actual;
        }
        await Future<void>.delayed(const Duration(milliseconds: 50));
      } while (clock.elapsed < const Duration(seconds: 8));
      throw const OperationUncertain(
        'Timed out waiting for the requested pixel frame to be published.',
      );
    } finally {
      _diagnosticPending = false;
    }
  }

  bool _rememberStatus(DeviceStatus status) {
    _checkPeerVersion(status.json);
    if (status.olderThan(_latestStatus)) return false;
    status = status.preservingNewerUpgrade(_latestStatus);
    status = status.preservingNewerRgb(_latestStatus);
    _latestStatus = status;
    _statusAge = Stopwatch()..start();
    if (!_statusUpdates.isClosed) _statusUpdates.add(status);
    _introducePeer();
    return true;
  }

  void _checkPeerVersion(Map<String, dynamic> value) {
    try {
      validatePeerBoard(
        value,
        appVersion: PeerIdentity.current.version,
        enabled: enforcePeerVersions && peerVersionChecksEnabled,
      );
    } on UnsupportedPeerVersion catch (error) {
      // Revoke every captured command/maintenance owner synchronously, not
      // just its status lease. Keep this one persistent rejection until an
      // explicit reconnect/endpoint edit, with no automatic retry chatter.
      if (_unsupportedPeer == null) {
        _connectionWanted =
            false; // Rejected, not an endlessly reconnecting link.
        resetTransport(unsupported: error);
      }
      rethrow;
    }
  }

  // One optional, nonsecret introduction per verified board boot, refreshed
  // at most once a minute on normal foreground observations for the board's
  // 120s registry lease. No timer, per-frame metadata or blocking readback.
  void _introducePeer() {
    if (_verifiedBoard == null ||
        _verifiedBoot == null ||
        _latestStatus == null ||
        !appForeground ||
        _latestStatus!.json['compatibility'] is! Map) {
      return;
    }
    final owner = '$_verifiedBoard/$_verifiedBoot/$connectionRevision';
    if (_introducedPeer == owner &&
        (_introductionAge?.elapsed ?? Duration.zero) <
            const Duration(seconds: 60)) {
      return;
    }
    _introducedPeer = owner;
    _introductionAge = Stopwatch()..start();
    final revision = connectionRevision;
    unawaited(() async {
      try {
        final report = PeerIdentity.current.reportJson(
          transport: isBluetooth ? 'bluetooth' : 'network',
          enforce: enforcePeerVersions && peerVersionChecksEnabled,
          reportedIp: selectedRouteLocalIp,
        );
        if (revision != connectionRevision) return;
        if (isBluetooth) {
          // Sequential/ACKed transport, no alternate network path.
          await bluetooth.request(
            'POST',
            '/api/clients/report',
            {'report': report},
            board: _verifiedBoard!,
            boot: _verifiedBoot!,
            expires: 0, // Metadata report, never an actuator command.
          );
        } else if (_socketReady &&
            _channel != null &&
            utf8.encode(report).length <= 1200) {
          _channel!.sink.add(
            jsonEncode({
              'type': 'peer-introduction',
              'report': jsonDecode(report),
            }),
          );
        } else {
          final request =
              http.Request('POST', _uri(transportUrl, '/api/clients/report'))
                ..headers.addAll(_writeHeaders)
                ..followRedirects = false
                ..bodyFields = {'report': report};
          final response = await _client.send(request).timeout(readTimeout);
          await response.stream.listen((_) {}).cancel();
        }
      } on Object {
        // Metadata failure does not become a control timeout or reconnect toast.
      }
    }());
  }

  /// A single intent, preferred WS receipt, then the SAME idempotent envelope
  /// over HTTP if its reply is lost. Never fall back to a second legacy write.
  Future<DeviceStatus?> fastCommand(
    String path,
    Map<String, String> values, {
    required bool Function() authorize,
  }) async {
    if (!commandNames.containsKey(path)) return null;
    var snapshot = _latestStatus;
    if (snapshot?.peer.has('command-ack') != true) return null;
    final revision = connectionRevision;
    snapshot = await _commandStatus(authorize);
    _requireNetwork();
    if (revision != connectionRevision || !authorize()) {
      throw const StaleConnection();
    }
    if (isBluetooth) {
      final clock = await _freshBluetoothClock();
      snapshot = DeviceStatus({...snapshot.json, 'command_clock_ms': clock});
      if (revision != connectionRevision || !authorize()) {
        throw const StaleConnection();
      }
    }
    final packet = commandPacket(snapshot, path, values);
    final id = (jsonDecode(packet) as Map)['request_id'] as String;
    Map<String, dynamic>? reply;
    if (isBluetooth) {
      _retireStatusRead();
      try {
        reply = await _bleRequest('POST', '/api/command', {
          'packet': packet,
        }, authorize: authorize);
      } finally {
        _retireStatusRead();
      }
      if (revision != connectionRevision || !authorize()) {
        throw const StaleConnection();
      }
      final actual = verifyCommandReply(reply, packet, path, values);
      _acceptIdentity(actual.json);
      _rememberStatus(actual);
      return actual;
    }
    if (_socketReady && _channel != null) {
      final pending = Completer<Map<String, dynamic>?>();
      _commandReplies[id] = pending;
      try {
        _retireStatusRead();
        _channel!.sink.add(packet);
        reply = await pending.future.timeout(
          const Duration(milliseconds: 800),
          onTimeout: () => null,
        );
      } on Exception {
        /* Same-ID HTTP recovery below; no second intent. */
      } finally {
        _commandReplies.remove(id);
        _retireStatusRead();
      }
    }
    if (revision != connectionRevision || !authorize()) {
      throw const StaleConnection();
    }
    if (reply == null) {
      final request = http.Request('POST', _uri(transportUrl, '/api/command'))
        ..headers.addAll(_writeHeaders)
        ..bodyFields = {'packet': packet}
        ..followRedirects = false;
      _retireStatusRead();
      late final http.Response response;
      try {
        response = await _writeResponse(request, const Duration(seconds: 4));
      } on TimeoutException {
        throw const OperationUncertain(
          'No correlated HTTP command receipt arrived within 4 seconds.',
        );
      } on SocketException {
        throw const OperationUncertain(
          'The connection was lost while waiting for a command receipt.',
        );
      } on http.ClientException {
        throw const OperationUncertain(
          'The connection was lost while waiting for a command receipt.',
        );
      } finally {
        _retireStatusRead();
      }
      _rejectWriteResponse(response);
      try {
        final decoded = jsonDecode(response.body);
        if (decoded is! Map<String, dynamic>) {
          throw const OperationUncertain(
            'The board returned a malformed command receipt.',
          );
        }
        reply = decoded;
      } on FormatException {
        throw const OperationUncertain(
          'The board returned a malformed command receipt.',
        );
      }
    }
    if (revision != connectionRevision || !authorize()) {
      throw const StaleConnection();
    }
    final actual = verifyCommandReply(reply, packet, path, values);
    _acceptIdentity(actual.json);
    _rememberStatus(actual);
    return actual;
  }

  /// A current full WS observation can satisfy command admission while an
  /// older background GET is stalled. Preserve the same two-second clock and
  /// peer/owner checks; neither source may revive a retired command owner.
  Future<DeviceStatus> _commandStatus(bool Function() authorize) async {
    final owner = connectionRevision, board = _verifiedBoard, boot = _verifiedBoot;
    void admit() {
      _requireNetwork();
      if (owner != connectionRevision || !appForeground || !authorize() ||
          board != _verifiedBoard || boot != _verifiedBoot) {
        throw const StaleConnection();
      }
    }
    bool fresh() => _latestStatus != null && _statusAge != null &&
        _statusAge!.elapsedMilliseconds < 2000;
    admit();
    if (fresh()) return _latestStatus!;
    final result = Completer<DeviceStatus>();
    final updates = statusUpdates.listen((value) {
      if (result.isCompleted || !_socketReady || !fresh()) return;
      try {
        admit();
        // Retire only the joined status read. Its ignored result remains
        // handled below, and its real HTTP socket is aborted before control.
        final abort = _statusReadAbort;
        _retireStatusRead();
        if (abort != null && !abort.isCompleted) abort.complete();
        result.complete(_latestStatus!);
      } on Object catch (error, stack) {
        result.completeError(error, stack);
      }
    });
    final connection = connectionUpdates.listen((_) {
      if (result.isCompleted) return;
      try { admit(); }
      on Object catch (error, stack) { result.completeError(error, stack); }
    });
    unawaited(status().then<void>((value) {
      if (!result.isCompleted) result.complete(value);
    }, onError: (Object error, StackTrace stack) {
      if (!result.isCompleted) {
        result.completeError(error is TimeoutException
            ? TimeoutException('Fresh board status unavailable; command was not sent.', readTimeout)
            : error, stack);
      }
    }));
    requestLiveStatus();
    var admitted = false;
    try {
      final value = await result.future.timeout(readTimeout, onTimeout: () {
        throw TimeoutException('Fresh board status unavailable; command was not sent.', readTimeout);
      });
      admit();
      admitted = true;
      return value;
    } finally {
      await updates.cancel();
      await connection.cancel();
      if (!admitted && owner == connectionRevision) {
        final abort = _statusReadAbort;
        _retireStatusRead();
        if (abort != null && !abort.isCompleted) abort.complete();
      }
    }
  }

  /// One deadline owns both connection/headers and body. Aborting the socket
  /// after that deadline never implies the board rolled a command back.
  Future<http.Response> _writeResponse(http.Request source, Duration limit) async {
    final abort = Completer<void>();
    final request = http.AbortableRequest(source.method, source.url,
        abortTrigger: abort.future)
      ..headers.addAll(source.headers)
      ..followRedirects = source.followRedirects
      ..bodyBytes = source.bodyBytes;
    final elapsed = Stopwatch()..start();
    var timedOut = false;
    void stop() { if (!abort.isCompleted) abort.complete(); }
    final timer = Timer(limit, () { timedOut = true; stop(); });
    StreamIterator<List<int>>? reader;
    try {
      final response = await _client.send(request).timeout(limit);
      reader = StreamIterator(response.stream);
      final bytes = BytesBuilder(copy: false);
      while (true) {
        final remaining = limit - elapsed.elapsed;
        if (remaining <= Duration.zero) throw TimeoutException('Command receipt deadline');
        if (!await reader.moveNext().timeout(remaining)) break;
        if (bytes.length + reader.current.length > 64 * 1024) {
          throw const OperationUncertain('The command receipt exceeds its size limit.');
        }
        bytes.add(reader.current);
      }
      return http.Response.bytes(bytes.takeBytes(), response.statusCode,
          headers: response.headers, request: request,
          isRedirect: response.isRedirect,
          persistentConnection: response.persistentConnection,
          reasonPhrase: response.reasonPhrase);
    } on http.RequestAbortedException {
      if (timedOut) throw TimeoutException('Command receipt deadline');
      rethrow;
    } finally {
      timer.cancel();
      stop();
      try { await reader?.cancel().timeout(const Duration(milliseconds: 500)); }
      on Object { /* Cleanup cannot keep a timed-out command pending. */ }
    }
  }

  String? _discoveredBaseUrl;
  String? _transportOwner;
  String? _verifiedEndpoint;
  String? _verifiedBoard, _verifiedBoot;

  Map<String, String> get _headers => {
    ...PeerIdentity.current.headers,
    if (password.isNotEmpty)
      'authorization': 'Basic ${base64Encode(utf8.encode('$user:$password'))}',
  };
  Map<String, String> get _writeHeaders => {
    ..._headers,
    if (_verifiedBoard != null && _verifiedBoot != null) ...{
      'X-Pakar-Board': _verifiedBoard!,
      'X-Pakar-Boot': _verifiedBoot!,
    },
  };

  String get transportUrl =>
      _transportOwner == baseUrl ? _discoveredBaseUrl ?? baseUrl : baseUrl;

  static String normalizeEndpoint(String value) {
    final text = value.trim();
    if (text.startsWith('ble://')) {
      return PakarBleTransport.endpoint(
        PakarBleTransport.addressFromEndpoint(text),
      );
    }
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

  Future<http.Response> _readResponse(
    String base,
    String path,
    Map<String, String> headers, {
    Duration? timeout,
  }) async {
    final owner = connectionRevision, limit = timeout ?? readTimeout;
    const maximum = 64 * 1024;
    final abort = Completer<void>();
    _readAborts.add(abort);
    if (path == '/api/status') _statusReadAbort = abort;
    var timedOut = false;
    void stop() {
      if (!abort.isCompleted) abort.complete();
    }

    final elapsed = Stopwatch()..start();
    final timer = Timer(limit, () {
      timedOut = true;
      stop();
    });
    final request =
        http.AbortableRequest(
            'GET',
            _uri(base, path),
            abortTrigger: abort.future,
          )
          ..headers.addAll(headers)
          ..followRedirects = false;
    StreamIterator<List<int>>? reader;
    StreamSubscription<List<int>>? rejectedBody;
    try {
      final response = await _client.send(request).timeout(limit);
      if (owner == connectionRevision) {
        receivedServerHeaders = Map.unmodifiable({
          for (final key in [
            'server',
            'x-pakar-product',
            'x-pakar-version',
            'x-pakar-build',
            'x-pakar-board-version',
            'x-pakar-instance',
          ])
            if (response.headers[key] != null) key: response.headers[key]!,
        });
      }
      if (owner != connectionRevision ||
          (response.contentLength ?? 0) > maximum) {
        // StreamIterator subscribes lazily at moveNext. The installed IOClient
        // attaches its response-abort hook only when the body is listened to;
        // cancelling an untouched iterator leaves that HTTP socket open. Start
        // this discard listener synchronously before abort/cancel in finally.
        // Nothing is retained or decoded, and no unbounded drain is awaited.
        rejectedBody = response.stream.listen((_) {}, onError: (Object _) {});
      }
      if (owner != connectionRevision) throw const StaleConnection();
      if ((response.contentLength ?? 0) > maximum) {
        throw const FormatException(
          'Device JSON response exceeds its safety limit',
        );
      }
      reader = StreamIterator(response.stream);
      final bytes = BytesBuilder(copy: false);
      while (true) {
        final remaining = limit - elapsed.elapsed;
        if (remaining <= Duration.zero) {
          throw TimeoutException('Device read deadline');
        }
        if (!await reader.moveNext().timeout(remaining)) break;
        if (owner != connectionRevision) throw const StaleConnection();
        if (bytes.length + reader.current.length > maximum) {
          throw const FormatException(
            'Device JSON response exceeds its safety limit',
          );
        }
        bytes.add(reader.current);
      }
      return http.Response.bytes(
        bytes.takeBytes(),
        response.statusCode,
        headers: response.headers,
        request: request,
        isRedirect: response.isRedirect,
        persistentConnection: response.persistentConnection,
        reasonPhrase: response.reasonPhrase,
      );
    } on http.RequestAbortedException {
      if (owner != connectionRevision) throw const StaleConnection();
      if (timedOut) throw TimeoutException('Device read deadline');
      rethrow;
    } finally {
      timer.cancel();
      // Future.timeout alone leaves IO and body subscriptions alive. The
      // installed http IOClient supports AbortableRequest before/after headers.
      stop();
      try {
        await reader?.cancel().timeout(const Duration(milliseconds: 500));
        await rejectedBody?.cancel().timeout(const Duration(milliseconds: 500));
      } on Object {
        // An unresponsive old read cannot hold ownership retirement forever.
      }
      _readAborts.remove(abort);
      if (identical(_statusReadAbort, abort)) _statusReadAbort = null;
    }
  }

  void _abortReads() {
    for (final abort in _readAborts.toList(growable: false)) {
      if (!abort.isCompleted) abort.complete();
    }
  }

  Future<Map<String, dynamic>> _get(String path) async {
    _requireNetwork();
    if (isBluetooth) return _bleRequest('GET', path, const {});
    if (path != '/api/status' &&
        _verifiedEndpoint != baseUrl &&
        enforcePeerVersions) {
      await status();
      _requireNetwork();
    }
    final revision = connectionRevision, headers = _headers;
    final response = await _withDiscovery(
      (base) => _readResponse(base, path, headers),
    );
    if (revision != connectionRevision) throw const StaleConnection();
    if (response.statusCode == 401) throw const AuthenticationRequired();
    if (response.statusCode ~/ 100 != 2) {
      throw FormatException(
        'Device read rejected (HTTP ${response.statusCode})',
      );
    }
    return jsonDecode(response.body) as Map<String, dynamic>;
  }

  Future<Map<String, dynamic>> getJson(String path) => _get(path);

  /// Bounded single-dispatch maintenance transport. Even ISP probe uses this
  /// path: its GET resets AVR, so it must not be retried through discovery.
  Future<Uint8List> maintenanceBytes(
    String path, {
    bool post = false,
    bool mayMutate = false,
    Map<String, String> form = const {},
    Uint8List? upload,
    String? uploadField,
    int maximum = 8192,
    bool verifySketch = false,
    void Function(int done, int total)? uploadProgress,
    Duration timeout = const Duration(seconds: 180),
    required bool Function() authorize,
  }) async {
    _requireNetwork();
    if (isBluetooth) {
      if (upload != null ||
          verifySketch ||
          !supportsRoute(post ? 'POST' : 'GET', Uri.parse(path).path)) {
        throw const FormatException(
          'This Bluetooth peer does not advertise binary firmware transfer or this maintenance route',
        );
      }
      if (!authorize()) throw const StaleConnection();
      final value = await _bleRequest(
        post ? 'POST' : 'GET',
        path,
        form,
        authorize: authorize,
      );
      return Uint8List.fromList(utf8.encode(jsonEncode(value)));
    }
    final revision = connectionRevision;
    if (_verifiedEndpoint != baseUrl) await status();
    if (revision != connectionRevision || !authorize()) {
      throw const StaleConnection();
    }
    final uri = _uri(transportUrl, path);
    final http.BaseRequest request;
    var uploaded = 0;
    if (upload != null) {
      if (!post || !{'image', 'firmware'}.contains(uploadField)) {
        throw const FormatException('Invalid firmware upload contract');
      }
      request = http.MultipartRequest('POST', uri)
        ..files.add(
          http.MultipartFile(
            uploadField!,
            Stream<List<int>>.fromIterable([
              for (var at = 0; at < upload.length; at += 16384)
                Uint8List.sublistView(
                  upload,
                  at,
                  (at + 16384).clamp(0, upload.length),
                ),
            ]).map((chunk) {
              if (revision != connectionRevision || !authorize()) {
                throw const StaleConnection();
              }
              uploaded += chunk.length;
              uploadProgress?.call(uploaded, upload.length);
              return chunk;
            }),
            upload.length,
            filename: uploadField == 'image' ? 'avr.bin' : 'esp.bin',
          ),
        );
    } else {
      request = http.Request(post ? 'POST' : 'GET', uri)..bodyFields = form;
    }
    request.headers.addAll(_writeHeaders);
    if (upload != null && uploadField == 'firmware') {
      request.headers['X-Pakar-Firmware-Size'] = '${upload.length}';
      request.headers['X-Pakar-Firmware-MD5'] = md5.convert(upload).toString();
    }
    request.followRedirects = false; // No credential forwarding or POST replay.
    final mutation = post || mayMutate;
    final elapsed = Stopwatch()..start();
    try {
      if (mutation) _retireStatusRead();
      final response = await _client.send(request).timeout(timeout);
      if (revision != connectionRevision) {
        await response.stream.listen((_) {}).cancel();
        throw const StaleConnection();
      }
      if (response.statusCode ~/ 100 != 2 ||
          (!post && response.statusCode != 200) ||
          (response.contentLength ?? 0) > maximum) {
        await response.stream.listen((_) {}).cancel();
        throw const FormatException(
          'Maintenance rejected or response exceeds its limit; nothing replayed',
        );
      }
      final bytes = BytesBuilder(copy: false);
      final reader = StreamIterator(response.stream);
      try {
        while (true) {
          final remaining = timeout - elapsed.elapsed;
          if (remaining <= Duration.zero) {
            throw TimeoutException('Maintenance deadline');
          }
          if (!await reader.moveNext().timeout(remaining)) break;
          if (revision != connectionRevision) throw const StaleConnection();
          final chunk = reader.current;
          if (bytes.length + chunk.length > maximum) {
            throw const FormatException(
              'Maintenance response exceeds its limit; nothing replayed',
            );
          }
          bytes.add(chunk);
        }
      } finally {
        await reader.cancel();
      }
      if (revision != connectionRevision) throw const StaleConnection();
      final result = bytes.takeBytes();
      if (response.contentLength != null &&
          response.contentLength != result.length) {
        throw const FormatException(
          'Truncated firmware transfer; no export created',
        );
      }
      if (verifySketch) {
        final digest = response.headers['x-firmware-md5'];
        if (digest == null ||
            !RegExp(r'^[a-fA-F0-9]{32}$').hasMatch(digest) ||
            md5.convert(result).toString() != digest.toLowerCase()) {
          throw const FormatException(
            'Running firmware transfer digest unavailable or mismatched; no export created',
          );
        }
      }
      return result;
    } on StaleConnection {
      rethrow;
    } on FormatException {
      rethrow;
    } on Exception {
      if (mutation) throw const OperationUncertain();
      throw const FormatException(
        'Maintenance read failed; no operation was replayed',
      );
    } finally {
      if (mutation) _retireStatusRead();
    }
  }

  Future<Map<String, dynamic>> maintenanceJson(
    String path, {
    bool post = false,
    bool mayMutate = false,
    Map<String, String> form = const {},
    Uint8List? upload,
    String? uploadField,
    void Function(int done, int total)? uploadProgress,
    required bool Function() authorize,
  }) async {
    final bytes = await maintenanceBytes(
      path,
      post: post,
      mayMutate: mayMutate,
      form: form,
      upload: upload,
      uploadField: uploadField,
      uploadProgress: uploadProgress,
      authorize: authorize,
    );
    final value = jsonDecode(utf8.decode(bytes));
    if (value is! Map<String, dynamic>) {
      throw const FormatException(
        'Malformed maintenance acknowledgment; inspect before any new request',
      );
    }
    return value;
  }

  /// Single dispatch, form BODY (never secret-bearing URL parameters), and
  /// caller-owned capability check after any awaited identity verification.
  Future<Map<String, dynamic>> postJson(
    String path,
    Map<String, String> values, {
    bool Function()? authorize,
  }) async {
    _requireNetwork();
    if (isBluetooth) {
      return _bleRequest('POST', path, values, authorize: authorize);
    }
    final revision = connectionRevision;
    if (_verifiedEndpoint != baseUrl) await status();
    if (revision != connectionRevision || authorize?.call() == false) {
      throw const StaleConnection();
    }
    late final http.Response response;
    try {
      final request = http.Request('POST', _uri(transportUrl, path))
        ..headers.addAll(_writeHeaders)
        ..bodyFields = values
        ..followRedirects = false;
      _retireStatusRead();
      response = await _writeResponse(request, const Duration(seconds: 12));
    } on TimeoutException {
      throw const OperationUncertain(
        'No device write receipt arrived within 12 seconds.',
      );
    } on Exception {
      throw const OperationUncertain(
        'The connection failed while waiting for a device write receipt.',
      );
    } finally {
      _retireStatusRead();
    }
    if (revision != connectionRevision) throw const StaleConnection();
    _rejectWriteResponse(response);
    try {
      final result = jsonDecode(response.body);
      if (result is! Map<String, dynamic>) {
        throw const OperationUncertain(
          'The board returned a malformed write receipt.',
        );
      }
      return result;
    } on FormatException {
      throw const OperationUncertain(
        'The board returned a malformed write receipt.',
      );
    }
  }

  Future<void> post(
    String path, [
    Map<String, String> values = const {},
  ]) async {
    _requireNetwork();
    if (isBluetooth) {
      await _bleRequest('POST', path, values);
      return;
    }
    final revision = connectionRevision;
    // Resolve/verify through a read first; a POST must never be automatically
    // replayed after a timeout or disconnect (the board may have applied it).
    if (_verifiedEndpoint != baseUrl) await status();
    if (revision != connectionRevision) throw const StaleConnection();
    late final http.Response response;
    try {
      final request = http.Request('POST', _uri(transportUrl, path))
        ..headers.addAll(_writeHeaders)
        ..bodyFields = values
        ..followRedirects = false;
      _retireStatusRead();
      response = await _writeResponse(request, const Duration(seconds: 12));
    } on TimeoutException {
      throw const OperationUncertain(
        'No device write receipt arrived within 12 seconds.',
      );
    } on Exception {
      throw const OperationUncertain(
        'The connection failed while waiting for a device write receipt.',
      );
    } finally {
      _retireStatusRead();
    }
    if (revision != connectionRevision) throw const StaleConnection();
    _rejectWriteResponse(response);
  }

  void _rejectWriteResponse(http.Response response) {
    if (response.statusCode == 401) throw const AuthenticationRequired();
    Map<String, dynamic>? receipt;
    if (response.body.isNotEmpty) {
      try {
        final decoded = jsonDecode(response.body);
        if (decoded is Map<String, dynamic>) receipt = decoded;
      } on FormatException {
        // Older successful REST routes may return plain text; never echo it.
      }
    }
    if (response.statusCode ~/ 100 != 2 ||
        (receipt?.containsKey('accepted') == true &&
            receipt?['accepted'] != true)) {
      throw DeviceCommandRejected(
        error: receipt?['error'],
        httpStatus: response.statusCode,
      );
    }
  }

  Future<DeviceStatus>? _statusTask;
  Completer<void>? _statusReadAbort;
  int? _statusTaskRevision;
  int _statusReadSequence = 0;

  void _retireStatusRead() {
    ++_statusReadSequence;
    _statusTask = null;
    _statusTaskRevision = null;
  }

  /// Adapt PeerSession.refresh's single-flight ownership. Concurrent consumers
  /// share only this read; commands and post-command readback are never queued
  /// or replayed. A replacement connection starts a separate owned read.
  Future<DeviceStatus> status() {
    final owner = connectionRevision;
    final active = _statusTask;
    if (active != null && _statusTaskRevision == owner) return active;
    late final Future<DeviceStatus> task;
    task = _readStatus(_statusReadSequence).whenComplete(() {
      if (identical(_statusTask, task)) {
        _statusTask = null;
        _statusTaskRevision = null;
      }
    });
    _statusTaskRevision = owner;
    return _statusTask = task;
  }

  Future<DeviceStatus> _readStatus(int sequence) async {
    late final DeviceStatus result;
    try { result = DeviceStatus(await _get('/api/status')); }
    on Object {
      if (sequence != _statusReadSequence) throw const SupersededRead();
      rethrow;
    }
    if (sequence != _statusReadSequence) throw const SupersededRead();
    if (!_matchesLocalIdentity(result.json)) {
      throw const FormatException(
        'Device identity does not match the requested hostname',
      );
    }
    _acceptIdentity(result.json);
    _verifiedEndpoint = baseUrl;
    if (!_rememberStatus(result)) throw const SupersededRead();
    return _latestStatus!;
  }

  // Adapt Session's synchronous retirement boundary: retire captured owners
  // before a new board/boot may authorize any old dialog or in-flight request.
  bool _acceptIdentity(Map<String, dynamic> value) {
    _checkPeerVersion(value);
    final board = value['board_id'], boot = value['boot_id'];
    bool identity(Object? v) =>
        v is String && RegExp(r'^[!-~]{1,80}$').hasMatch(v);
    if ((_identityRequired ||
            value.containsKey('board_id') ||
            value.containsKey('boot_id')) &&
        (!identity(board) || !identity(boot))) {
      if (_verifiedBoard != null) {
        resetTransport();
        onPeerChanged?.call();
      }
      throw const StaleConnection();
    }
    final changed =
        _verifiedBoard != null &&
        (board != _verifiedBoard || boot != _verifiedBoot);
    if (changed) {
      resetTransport();
      onPeerChanged?.call();
    }
    _verifiedBoard = identity(board) && identity(boot) ? board as String : null;
    _verifiedBoot = identity(board) && identity(boot) ? boot as String : null;
    if (_verifiedBoard != null) _identityRequired = true;
    return changed;
  }

  Future<BoardSound> boardSound({bool? silent}) async {
    final revision = connectionRevision;
    final before = await status();
    void admit() {
      _requireNetwork();
      if (revision != connectionRevision) throw const StaleConnection();
    }

    admit();
    if (before.json['avr_online'] != true ||
        !before.peer.has('settings') ||
        !before.peer.has('buzzer') ||
        (silent != null && before.json['programming_latch'] == true)) {
      throw const FormatException('Board sound settings unavailable');
    }
    final boot = before.json['boot_id'];
    if (_verifiedBoard == null || _verifiedBoot == null) {
      throw const StaleConnection();
    }
    Future<Map<String, dynamic>> contract(int opcode, String payload) async {
      admit();
      final queued = await postJson('/api/avr/contracts', {
        'opcode': '$opcode',
        'payload_hex': payload,
      }, authorize: () => revision == connectionRevision);
      final id = queued['request_id'];
      if (id is! int || id <= 0 || queued['boot_id'] != boot) {
        throw const OperationUncertain();
      }
      final deadline = DateTime.now().add(const Duration(seconds: 3));
      while (true) {
        admit();
        final receipt = await _get('/api/avr/contracts/result?id=$id');
        if (receipt['request_id'] != id ||
            receipt['boot_id'] != boot ||
            receipt['request_opcode'] != opcode) {
          throw const OperationUncertain();
        }
        if (receipt['state'] == 'responded') {
          final response = receipt['response_opcode'],
              hex = receipt['payload_hex'];
          if (hex is! String ||
              (response != 0x82 &&
                  (opcode == 4
                      ? response != 0x91
                      : response != 0x80 || hex != '0500'))) {
            throw const OperationUncertain();
          }
          // Consume only the matching terminal receipt, never a queued write.
          admit();
          final request =
              http.Request(
                  'DELETE',
                  _uri(transportUrl, '/api/avr/contracts/result', {
                    'id': '$id',
                    'boot_id': '$boot',
                    'opcode': '$opcode',
                  }),
                )
                ..headers.addAll(_writeHeaders)
                ..followRedirects = false;
          if (!isBluetooth) {
            await http.Response.fromStream(await _client.send(request))
                .timeout(const Duration(seconds: 4));
          }
          admit();
          if (response == 0x82) {
            throw const FormatException(
              'AVR rejected sound settings; not retried',
            );
          }
          return receipt;
        }
        if (!['queued', 'awaiting'].contains(receipt['state']) ||
            DateTime.now().isAfter(deadline)) {
          throw const OperationUncertain();
        }
        await Future<void>.delayed(const Duration(milliseconds: 40));
      }
    }

    final read = await contract(4, '');
    var value = BoardSound.parse(read['payload_hex'] as String);
    if (silent != null && (!value.persisted || value.silent != silent)) {
      final expected = value.withSilent(silent);
      await contract(5, expected);
      final readback = await contract(4, '');
      value = BoardSound.parse(readback['payload_hex'] as String);
      if (!value.persisted || BoardSound.encode(value.bytes) != expected) {
        throw const FormatException(
          'Persistent Silent mode readback did not match',
        );
      }
    }
    await status();
    admit();
    return value;
  }

  Future<CellMap> mapping() async {
    final revision = connectionRevision;
    await status();
    if (revision != connectionRevision) throw const StaleConnection();
    final value = await _get('/api/cells/mapping');
    await status();
    if (revision != connectionRevision) throw const StaleConnection();
    return CellMap.fromJson(value);
  }

  Stream<DeviceStatus> liveStatus() async* {
    _requireNetwork();
    if (isBluetooth) {
      yield* _liveBluetooth();
      return;
    }
    final revision = connectionRevision;
    // A same-origin socket may establish identity from its first full status.
    // Do not make working real-time control depend on a slow HTTP preflight.
    if (revision != connectionRevision) throw const StaleConnection();
    final previous = _channel;
    _channel = null;
    _socketReady = false;
    _liveAge = _socketAge = null;
    // A vanished peer must not hold replacement behind an unbounded close
    // handshake. Retire ownership before awaiting it, like the donor session.
    await _retireSocket(previous);
    if (revision != connectionRevision) throw const StaleConnection();
    final uri = webSocketUri;
    final active = IOWebSocketChannel.connect(
      uri,
      headers: {..._headers}..remove('User-Agent'),
      // Android may pause Dart timers/network while backgrounded. A built-in
      // ping timeout would turn that suspension into a needless disconnect.
      // Foreground supervision owns read probes and bounded silent-socket reset.
      pingInterval: null,
      connectTimeout: const Duration(seconds: 6),
      customClient: _ioClient,
    );
    _channel = active;
    try {
      // Failed HTTP upgrades must pass through the same cleanup as a socket
      // that was already live; otherwise each reconnect retains a dead owner.
      await active.ready;
      if (revision != connectionRevision || !identical(_channel, active)) {
        return;
      }
      _socketReady = true;
      _socketAge = Stopwatch()..start();
      active.sink.add('{"type":"status"}');
      await for (final message in active.stream) {
        if (revision != connectionRevision || !identical(_channel, active)) {
          return;
        }
        if (message is! String) continue;
        final value = jsonDecode(message);
        if (value is Map<String, dynamic> &&
            value['type'] == 'command-result') {
          if (value['board_id'] == _verifiedBoard &&
              value['boot_id'] == _verifiedBoot) {
            final pending = _commandReplies[value['request_id']];
            if (pending != null && !pending.isCompleted) {
              pending.complete(value);
            }
          }
          continue;
        }
        if (value is Map<String, dynamic> && value['type'] == 'status-rgb') {
          if (liveFresh &&
              value['board_id'] == _verifiedBoard &&
              value['boot_id'] == _verifiedBoot) {
            final next = _latestStatus?.withStatusRgbEvent(value);
            if (next != null) {
              _latestStatus = next;
              if (!_rgbUpdates.isClosed) _rgbUpdates.add(next);
            }
          }
          continue; // No _statusAge/_liveAge update, full-status yield or replay.
        }
        if (value is Map<String, dynamic> &&
            value['type'] == 'update-progress') {
          // An incremental lifecycle packet cannot establish peer identity or
          // renew full-status/hardware freshness. Existing progress consumers
          // share the statusUpdates stream; the main full-status stream stays
          // authoritative for cell state, capability and selection handling.
          if (_verifiedBoard != null &&
              _verifiedBoot != null &&
              value['board_id'] == _verifiedBoard &&
              value['boot_id'] == _verifiedBoot) {
            final next = _latestStatus?.withUpgradeEvent(value);
            if (next != null) {
              _latestStatus = next;
              if (!_statusUpdates.isClosed) _statusUpdates.add(next);
            }
          }
          continue;
        }
        if (value is Map<String, dynamic> && value['type'] == 'panel-event') {
          if (liveFresh &&
              value['board_id'] == _verifiedBoard &&
              value['boot_id'] == _verifiedBoot) {
            onPeripheralEvent?.call(value);
          }
          continue; // Does not refresh any full-status or hardware lease.
        }
        if (value is Map<String, dynamic> &&
            value['type'] == 'pc-contract-event') {
          // Events cannot establish identity or refresh an expired status lease.
          if (_verifiedBoard != null &&
              value['board_id'] == _verifiedBoard &&
              value['boot_id'] == _verifiedBoot) {
            onPeripheralEvent?.call(value);
          }
          continue;
        }
        if (value is Map<String, dynamic> && value.containsKey('hostname')) {
          if (!_matchesLocalIdentity(value)) {
            throw const StaleConnection();
          }
          if (_acceptIdentity(value)) {
            return; // Reconnect only reads; never replay.
          }
          if (_verifiedBoard != null && _verifiedBoot != null) {
            _verifiedEndpoint = baseUrl;
          }
          final status = DeviceStatus(value);
          if (!_rememberStatus(status)) continue;
          _liveAge = Stopwatch()..start();
          yield _latestStatus!;
        }
      }
    } finally {
      if (identical(_channel, active)) {
        _socketReady = false;
        _liveAge = _socketAge = null;
        _channel = null;
      }
      unawaited(_retireSocket(active));
    }
  }

  Future<void> _ensureBluetooth() async {
    _requireNetwork();
    final owner = connectionRevision, endpoint = baseUrl;
    if (!isBluetooth) throw const StaleConnection();
    await bluetooth.connect(
      PakarBleTransport.addressFromEndpoint(endpoint),
      expectedBoard: expectedBluetoothBoard,
    );
    if (owner != connectionRevision ||
        !isBluetooth ||
        endpoint != baseUrl ||
        !connectionWanted) {
      throw const StaleConnection();
    }
  }

  Future<Map<String, dynamic>> _bleRequest(
    String method,
    String path,
    Map<String, String> values, {
    bool Function()? authorize,
  }) async {
    final owner = connectionRevision;
    if (method == 'POST' && !appForeground) throw const ConnectionStopped();
    await _ensureBluetooth();
    final uri = Uri.parse(path);
    final bootstrap = method == 'GET' && uri.path == '/api/status';
    if (!bootstrap &&
        (_latestStatus == null ||
            method == 'POST' &&
                (_statusAge?.elapsedMilliseconds ?? 9999) >= 2000)) {
      await status();
    }
    if (owner != connectionRevision ||
        !isBluetooth ||
        !connectionWanted ||
        authorize?.call() == false) {
      throw const StaleConnection();
    }
    if (!bootstrap && !supportsRoute(method, uri.path)) {
      throw const FormatException(
        'This Bluetooth board does not advertise that operation',
      );
    }
    var clock = _latestStatus?.json['command_clock_ms'];
    if (method == 'POST' &&
        (_verifiedBoard == null ||
            _verifiedBoot == null ||
            clock is! int ||
            clock < 0)) {
      throw const StaleConnection();
    }
    if (method == 'POST') clock = await _freshBluetoothClock();
    if (owner != connectionRevision ||
        authorize?.call() == false ||
        method == 'POST' && !appForeground) {
      throw const StaleConnection();
    }
    try {
      final response = await bluetooth.request(
        method,
        uri.path,
        {...uri.queryParameters, ...values},
        board: _verifiedBoard ?? '',
        boot: _verifiedBoot ?? '',
        expires: method == 'POST' ? (clock as int) + 8000 : 0,
      );
      if (owner != connectionRevision ||
          !isBluetooth ||
          authorize?.call() == false) {
        throw const StaleConnection();
      }
      if (response.status == 401) throw const AuthenticationRequired();
      if (response.status ~/ 100 != 2) {
        throw FormatException(
          'Bluetooth device $method rejected (${response.status}); not retried',
        );
      }
      if (bootstrap) {
        final status = DeviceStatus(response.body);
        if (!status.peer.known ||
            response.body['board_id'] != bluetooth.boardId ||
            response.body['boot_id'] != bluetooth.bootId) {
          throw const FormatException(
            'Bluetooth full board/application identity is not verified',
          );
        }
      }
      return response.body;
    } on StaleConnection {
      rethrow;
    } on AuthenticationRequired {
      rethrow;
    } on FormatException {
      rethrow;
    } on BleBusy {
      rethrow;
    } on Exception {
      if (method == 'POST') throw const OperationUncertain();
      rethrow;
    } finally {
      if (owner == connectionRevision && isBluetooth && !bluetooth.ready) {
        onPeripheralStreamLost?.call();
      }
    }
  }

  Future<int> _freshBluetoothClock() async {
    final value = await _bleRequest('GET', '/api/ble/clock', const {});
    final clock = value['command_clock_ms'];
    if (value['board_id'] != _verifiedBoard ||
        value['boot_id'] != _verifiedBoot ||
        clock is! int ||
        clock < 0 ||
        clock > 0x1fffffffffffff - 8000) {
      throw const StaleConnection();
    }
    return clock;
  }

  Stream<DeviceStatus> _liveBluetooth() async* {
    final owner = connectionRevision;
    await _ensureBluetooth();
    final updates = StreamController<DeviceStatus>();
    Timer? poll;
    bool retired = false;
    void accept(Map<String, dynamic> value) {
      if (retired ||
          owner != connectionRevision ||
          !isBluetooth ||
          !connectionWanted) {
        return;
      }
      if (value['board_id'] != bluetooth.boardId ||
          value['boot_id'] != bluetooth.bootId) {
        return;
      }
      final event = value['type'] == 'ble-event'
          ? value['event']
          : value['type'];
      final body = value['type'] == 'ble-event' ? value['body'] : value;
      if (body is! Map<String, dynamic>) return;
      if (event == 'update-progress') {
        final next = _latestStatus?.withUpgradeEvent(body);
        if (next != null) {
          _latestStatus = next;
          if (!_statusUpdates.isClosed) _statusUpdates.add(next);
        }
        return;
      }
      if (event == 'pc-contract-event' || event == 'pc-contract-result') {
        onPeripheralEvent?.call(body);
        return;
      }
      if (event == 'panel-event') {
        if (liveFresh &&
            body['board_id'] == _verifiedBoard &&
            body['boot_id'] == _verifiedBoot) {
          onPeripheralEvent?.call(body);
        }
        return;
      }
      final raw = event == 'status' || value['type'] != 'ble-event'
          ? body
          : null;
      if (raw is! Map<String, dynamic> ||
          raw['hostname'] is! String ||
          raw['board_id'] != bluetooth.boardId ||
          raw['boot_id'] != bluetooth.bootId) {
        return;
      }
      try {
        if (_acceptIdentity(raw)) return;
        final actual = DeviceStatus(raw);
        if (!actual.peer.known || !_rememberStatus(actual)) return;
        _verifiedEndpoint = baseUrl;
        _liveAge = Stopwatch()..start();
        updates.add(_latestStatus!);
      } on Object catch (error) {
        updates.addError(error);
      }
    }

    final subscription = bluetooth.events.stream.listen(
      accept,
      onError: (Object error) {
        if (!retired && owner == connectionRevision && isBluetooth) {
          onPeripheralStreamLost?.call();
          updates.addError(error);
        }
      },
    );
    try {
      final initial = await status();
      if (owner != connectionRevision) return;
      _socketAge = Stopwatch()..start();
      _liveAge = Stopwatch()..start();
      updates.add(initial);
      final ble = initial.json['ble'];
      final push =
          ble is Map &&
          ble['push'] is List &&
          (ble['push'] as List).contains('status');
      if (!push) {
        poll = Timer.periodic(const Duration(seconds: 1), (_) {
          if (retired ||
              owner != connectionRevision ||
              !appForeground ||
              bluetooth.busy) {
            return;
          }
          unawaited(
            status().then<void>(
              (value) {
                if (!retired) {
                  _liveAge = Stopwatch()..start();
                  updates.add(value);
                }
              },
              onError: (Object error) {
                if (!retired) updates.addError(error);
              },
            ),
          );
        });
      }
      yield* updates.stream;
    } finally {
      retired = true;
      poll?.cancel();
      await subscription.cancel();
      await updates.close();
    }
  }

  Future<void> _retireSocket(WebSocketChannel? socket) async {
    if (socket == null) return;
    try {
      await socket.sink.close().timeout(const Duration(milliseconds: 500));
    } on Object {
      // Retirement is cleanup, never a reason to replay a request or clear UI.
    }
  }

  Future<void> close() async {
    await bluetooth.close();
    _abortReads();
    _socketReady = false;
    _latestStatus = null;
    _statusAge = null;
    _liveAge = _socketAge = null;
    for (final pending in _commandReplies.values) {
      if (!pending.isCompleted) pending.complete(null);
    }
    _commandReplies.clear();
    ++_connectionRevision;
    _statusTask = null;
    _statusTaskRevision = null;
    final active = _channel;
    _channel = null;
    _verifiedEndpoint = null;
    _verifiedBoard = _verifiedBoot = null;
    try {
      await active?.sink.close().timeout(const Duration(milliseconds: 500));
    } on Exception {
      // A vanished board cannot hold client disposal/connection retirement.
    } finally {
      _client.close();
      unawaited(_statusUpdates.close());
      unawaited(_connectionUpdates.close());
      unawaited(_rgbUpdates.close());
      _ioClient.close(force: true);
    }
  }

  void resetTransport({UnsupportedPeerVersion? unsupported}) {
    _unsupportedPeer = unsupported;
    _introducedPeer = null;
    _introductionAge = null;
    receivedServerHeaders = const {};
    unawaited(bluetooth.disconnect());
    _abortReads();
    _socketReady = false;
    _latestStatus = null;
    _statusAge = null;
    _liveAge = _socketAge = null;
    for (final pending in _commandReplies.values) {
      if (!pending.isCompleted) pending.complete(null);
    }
    _commandReplies.clear();
    ++_connectionRevision;
    _statusTask = null;
    _statusTaskRevision = null;
    _verifiedEndpoint = null;
    _verifiedBoard = _verifiedBoot = null;
    onTransportRetired?.call();
    if (!_connectionUpdates.isClosed) _connectionUpdates.add(null);
    final active = _channel;
    _channel = null;
    if (active != null) {
      unawaited(_retireSocket(active));
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

  bool _matchesLocalIdentity(Map<String, dynamic> value) {
    final expected = _localLabel();
    if (expected == null || value['hostname'] == expected) return true;
    final bridge = value['bridge'];
    if (bridge is! Map ||
        bridge['product'] != 'PakarController' ||
        bridge['hostname'] != expected ||
        bridge['instance_id'] is! String) {
      return false;
    }
    final path = bridge['path'];
    return path is List &&
        path.isNotEmpty &&
        path.length <= 4 &&
        path.toSet().length == path.length &&
        path.last == bridge['instance_id'] &&
        path.every(
          (id) =>
              id is String &&
              RegExp(r'^[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}$')
                  .hasMatch(id),
        );
  }

  Future<http.Response> _withDiscovery(
    Future<http.Response> Function(String base) request,
  ) async {
    final revision = connectionRevision, headers = _headers;
    final lookupBefore = resolver.lookupFailureRevision(
      Uri.parse(baseUrl).host,
    );
    try {
      return await request(transportUrl);
    } on Exception catch (error) {
      if (revision != connectionRevision) throw const StaleConnection();
      if (_localLabel() == null ||
          (error is! HostLookupFailure &&
              (error is! SocketException ||
                  resolver.lookupFailureRevision(Uri.parse(baseUrl).host) ==
                      lookupBefore))) {
        rethrow;
      }
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
        final probe = await _readResponse(
          candidate,
          '/api/status',
          headers,
          timeout: const Duration(seconds: 2),
        );
        if (probe.statusCode ~/ 100 != 2) continue;
        if (revision != connectionRevision) throw const StaleConnection();
        final json = jsonDecode(probe.body);
        if (json is! Map<String, dynamic> || !_matchesLocalIdentity(json)) {
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

class AuthenticationRequired extends FormatException {
  const AuthenticationRequired() : super('Sign in required');
}

class StaleConnection implements Exception {
  const StaleConnection();
  @override
  String toString() =>
      'Connection changed; obsolete board replies and unsent commands were discarded.';
}

class ConnectionStopped implements Exception {
  const ConnectionStopped();
  @override
  String toString() => 'Disconnected; press Connect to resume device access.';
}

class OperationUncertain implements Exception {
  const OperationUncertain([this.detail]);
  final String? detail;
  @override
  String toString() =>
      '${detail == null ? '' : '$detail '}Command outcome unknown; it was not retried. Refresh device state before deciding whether to send again.';
}

class SupersededRead implements Exception {
  const SupersededRead();
  @override
  String toString() =>
      'Read superseded by a command; waiting for fresh device state.';
}
