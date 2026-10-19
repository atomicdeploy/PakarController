import 'dart:async';
import 'dart:convert';
import 'dart:math';

import 'package:flutter/services.dart';

import 'ble_codec.dart';
import 'peer_identity.dart';

class BleReply {
  const BleReply(this.status, this.body);
  final int status;
  final Map<String, dynamic> body;
}

class BleBusy implements Exception {
  const BleBusy();
  @override
  String toString() =>
      'Bluetooth operation pending; the new request was not dispatched';
}

/// One selected GATT session. Actual command acknowledgement lives in the
/// correlated JSON reply, not an Android write callback or fragment ACK.
class PakarBleTransport {
  // One native EventChannel listener per engine. Temporary identity probes
  // must not replace or cancel the selected API's notification subscription.
  static final Stream<dynamic> _platformEvents = const EventChannel(
    'ir.digitalogic.pakar/ble-events',
  ).receiveBroadcastStream();
  PakarBleTransport({MethodChannel? channel, Stream<dynamic>? nativeEvents})
    : channel = channel ?? const MethodChannel('ir.digitalogic.pakar/ble'),
      nativeEvents = nativeEvents ?? _platformEvents;
  final MethodChannel channel;
  final Stream<dynamic> nativeEvents;
  final events = StreamController<Map<String, dynamic>>.broadcast(sync: true);
  final _assembler = BleAssembler();
  final _clock = Stopwatch()..start();
  StreamSubscription<dynamic>? _subscription;
  Future<void>? _connecting;
  int _epoch = 0, _nativeSession = -1, _message = 1;
  int mtu = 23;
  String address = '', boardId = '', bootId = '';
  Map<String, dynamic> info = {};
  bool ready = false, _busy = false, _closed = false;
  bool get busy => _busy;
  Completer<BleFrame>? _ack;
  BleFrame? _outgoing;
  Completer<Map<String, dynamic>>? _reply;
  String? _requestId;
  Future<Map<String, dynamic>> adapter() async => Map<String, dynamic>.from(
    await channel.invokeMapMethod<String, dynamic>('status') ?? {},
  );
  Future<void> permission() async {
    await channel.invokeMethod<dynamic>('permission');
  }

  Future<void> enable() async {
    await channel.invokeMethod<dynamic>('enable');
  }

  Future<List<Map<String, dynamic>>> scan() async =>
      (await channel.invokeListMethod<dynamic>('scan') ?? [])
          .whereType<Map>()
          .map((value) => Map<String, dynamic>.from(value))
          .toList();
  static String endpoint(String address) {
    if (!RegExp(r'^(?:[A-Fa-f0-9]{2}:){5}[A-Fa-f0-9]{2}$').hasMatch(address)) {
      throw const FormatException('Select a Bluetooth device address');
    }
    return 'ble://${address.replaceAll(':', '').toLowerCase()}';
  }

  static String addressFromEndpoint(String endpoint) {
    final uri = Uri.tryParse(endpoint);
    if (uri?.scheme != 'ble' ||
        !RegExp(r'^[a-f0-9]{12}$').hasMatch(uri!.host) ||
        uri.path.isNotEmpty ||
        uri.hasQuery ||
        uri.hasFragment ||
        uri.userInfo.isNotEmpty) {
      throw const FormatException('Invalid Bluetooth endpoint');
    }
    return [for (var i = 0; i < 12; i += 2) uri.host.substring(i, i + 2)]
        .join(':')
        .toUpperCase();
  }

  Future<void> connect(String selected, {String expectedBoard = ''}) {
    if (_closed) throw StateError('Bluetooth transport closed');
    if (ready && address == selected) {
      if (expectedBoard.isNotEmpty && expectedBoard != boardId) {
        throw const FormatException(
          'Connected Bluetooth device belongs to another board',
        );
      }
      return Future<void>.value();
    }
    final existing = _connecting;
    if (existing != null) return existing;
    late final Future<void> task;
    task = _connect(selected, expectedBoard).whenComplete(() {
      if (identical(_connecting, task)) _connecting = null;
    });
    return _connecting = task;
  }

  Future<void> _connect(String selected, String expectedBoard) async {
    endpoint(selected);
    final epoch = ++_epoch;
    _assembler.reset();
    _subscription ??= nativeEvents.listen(
      _receive,
      onError: (Object error) => _lost(error),
      onDone: () => _lost(StateError('Bluetooth event stream closed')),
    );
    final raw = await channel
        .invokeMapMethod<String, dynamic>('connect', {'address': selected})
        .timeout(const Duration(seconds: 17));
    if (raw == null || raw['session'] is! int) {
      throw const FormatException('Missing Bluetooth session');
    }
    final session = raw['session'] as int;
    if (epoch != _epoch || _closed) {
      unawaited(
        channel
            .invokeMethod<dynamic>('disconnect', {'session': session})
            .catchError((Object _) {}),
      );
      throw StateError('Bluetooth connection retired');
    }
    final identity = jsonDecode('${raw['info']}');
    bool token(Object? value) =>
        value is String && RegExp(r'^[!-~]{1,80}$').hasMatch(value);
    if (identity is! Map<String, dynamic> ||
        identity['product'] != 'PakarController' ||
        !token(identity['board_id']) ||
        !token(identity['boot_id']) ||
        identity['auth_required'] != false ||
        expectedBoard.isNotEmpty && identity['board_id'] != expectedBoard) {
      unawaited(
        channel
            .invokeMethod<dynamic>('disconnect', {'session': session})
            .catchError((Object _) {}),
      );
      throw const FormatException(
        'Pakar Bluetooth identity/authentication not accepted',
      );
    }
    info = identity;
    address = selected;
    boardId = identity['board_id'];
    bootId = identity['boot_id'];
    _nativeSession = session;
    mtu = raw['mtu'] is int ? (raw['mtu'] as int).clamp(23, 517) : 23;
    ready = true;
    _message = 1;
  }

  void _receive(dynamic raw) {
    if (raw is! Map || raw['session'] != _nativeSession || !ready) return;
    if (raw['type'] == 'disconnected') {
      _lost(StateError('Bluetooth link lost; no command replay'));
      return;
    }
    if (raw['type'] != 'data' || raw['value'] is! Uint8List) return;
    try {
      final frame = BleFrame.decode(raw['value'] as Uint8List);
      if (frame.kind == 1) {
        final current = _outgoing;
        if (current != null &&
            frame.id == current.id &&
            frame.total == current.total &&
            frame.offset == current.offset + current.payload.length &&
            _ack?.isCompleted == false) {
          _ack!.complete(frame);
        }
        return;
      }
      final received = _assembler.receive(frame, _clock.elapsedMilliseconds);
      unawaited(
        _write(received.ack, _epoch).catchError((Object error) => _lost(error)),
      );
      if (received.message == null) return;
      final value = jsonDecode(utf8.decode(received.message!));
      if (value is! Map<String, dynamic>) {
        throw const FormatException('Malformed Bluetooth JSON');
      }
      if (value['type'] == 'ble-response' &&
          value['request_id'] == _requestId) {
        if (_reply?.isCompleted == false) _reply!.complete(value);
      } else if (value['type'] != 'ble-response' &&
          value['board_id'] == boardId &&
          value['boot_id'] == bootId) {
        events.add(value);
      }
    } on Object catch (error) {
      _lost(error);
    }
  }

  Future<void> _write(Uint8List value, int epoch) async {
    if (!ready || _closed || epoch != _epoch) {
      throw StateError('Bluetooth owner retired');
    }
    await channel
        .invokeMethod<dynamic>('write', {
          'session': _nativeSession,
          'value': value,
        })
        .timeout(const Duration(seconds: 5));
    if (epoch != _epoch) throw StateError('Bluetooth owner retired');
  }

  Future<BleReply> request(
    String method,
    String path,
    Map<String, String> form, {
    required String board,
    required String boot,
    required int expires,
  }) async {
    if (_busy) throw const BleBusy();
    if (!ready || _closed) throw StateError('Bluetooth session unavailable');
    if (!{'GET', 'POST'}.contains(method) ||
        !path.startsWith('/api/') ||
        path.contains('..') ||
        form.length > 32) {
      throw const FormatException('Invalid Bluetooth request');
    }
    final epoch = _epoch;
    final id = List.generate(
      16,
      (_) => Random.secure().nextInt(256).toRadixString(16).padLeft(2, '0'),
    ).join();
    final bytes = Uint8List.fromList(
      utf8.encode(
        jsonEncode({
          'type': 'ble-request',
          'request_id': id,
          'method': method,
          'path': path,
          'form': form,
          'board_id': board,
          'boot_id': boot,
          'expires_at_ms': expires,
          'client_version': PeerIdentity.current.version,
          'client_id': PeerIdentity.current.installationId,
          'client_application': 'pakarcontroller-mobile',
          'client_build': PeerIdentity.current.build,
        }),
      ),
    );
    if (bytes.isEmpty || bytes.length > 8192) {
      throw const FormatException('Bluetooth request exceeds 8192 bytes');
    }
    _busy = true;
    _requestId = id;
    _reply = Completer<Map<String, dynamic>>();
    // Attach the terminal handler now: a link loss during fragmented send must
    // not leave a completed-error Future without an observer.
    final response = _reply!.future;
    unawaited(response.then<void>((_) {}, onError: (Object _) {}));
    try {
      final message = _message;
      _message = _message == 65535 ? 1 : _message + 1;
      final piece = blePayloadBytes(mtu);
      for (var offset = 0; offset < bytes.length; offset += piece) {
        final end = (offset + piece).clamp(0, bytes.length);
        final frame = BleFrame(
          0,
          message,
          offset,
          bytes.length,
          Uint8List.sublistView(bytes, offset, end),
        );
        _outgoing = frame;
        _ack = Completer<BleFrame>();
        unawaited(_ack!.future.then<void>((_) {}, onError: (Object _) {}));
        final packet = frame.encode();
        var acknowledged = false;
        for (var attempt = 0; attempt < 3 && !acknowledged; attempt++) {
          await _write(packet, epoch);
          try {
            await _ack!.future.timeout(const Duration(milliseconds: 1500));
            acknowledged = true;
          } on TimeoutException {
            if (attempt == 2) rethrow;
          }
        }
        if (epoch != _epoch) throw StateError('Bluetooth owner retired');
      }
      _outgoing = null;
      _ack = null;
      // A tiny negotiated MTU legitimately takes longer. This is a response
      // deadline only, never permission to replay the logical request.
      final value = await response.timeout(
        Duration(seconds: mtu < 100 ? 180 : 30),
      );
      if (epoch != _epoch ||
          value['board_id'] != boardId ||
          value['boot_id'] != bootId ||
          value['status'] is! int ||
          value['body'] is! Map<String, dynamic>) {
        throw const FormatException(
          'Bluetooth reply owner/shape mismatch; inspect before another action',
        );
      }
      return BleReply(
        value['status'],
        Map<String, dynamic>.from(value['body']),
      );
    } on Object {
      await disconnect();
      rethrow;
    } finally {
      _busy = false;
      _reply = null;
      _requestId = null;
      _ack = null;
      _outgoing = null;
    }
  }

  void _lost(Object error) {
    if (!ready) return;
    ready = false;
    _assembler.reset();
    if (_reply?.isCompleted == false) _reply!.completeError(error);
    if (_ack?.isCompleted == false) _ack!.completeError(error);
    events.addError(error);
  }

  Future<void> disconnect() async {
    final session = _nativeSession;
    ++_epoch;
    _connecting = null;
    _lost(StateError('Bluetooth connection retired'));
    _assembler.reset();
    _nativeSession = -1;
    if (session >= 0) {
      try {
        await channel
            .invokeMethod<dynamic>('disconnect', {'session': session})
            .timeout(const Duration(seconds: 1));
      } on Object {
        /* bounded cleanup */
      }
    }
  }

  Future<void> close() async {
    if (_closed) return;
    _closed = true;
    await disconnect();
    await _subscription?.cancel();
    await events.close();
    _clock.stop();
  }
}
