import 'dart:async';
import 'dart:convert';

import 'package:flutter/services.dart';

import 'models.dart';
import 'peer_identity.dart';
import 'peer_policy.dart';
import 'cell_assignments.dart';
import 'pixel_diagnostic.dart';
import 'board_sound.dart';
import 'panel_event.dart';

/// Same fixed COBS/CRC-8 envelope as BoardCore::Protocol and the Rust USB actor.
class UsbFrame {
  const UsbFrame(this.opcode, this.sequence, this.payload);
  final int opcode, sequence;
  final List<int> payload;
  static int crc(Iterable<int> bytes) {
    var value = 0;
    for (final byte in bytes) {
      value ^= byte;
      for (var bit = 0; bit < 8; bit++) {
        value = ((value << 1) ^ (value & 128 != 0 ? 7 : 0)) & 255;
      }
    }
    return value;
  }

  Uint8List encode() {
    if (payload.length > 48 || payload.any((b) => b < 0 || b > 255)) {
      throw const FormatException('USB payload exceeds 48 bytes');
    }
    final raw = [165, 0, opcode, sequence, payload.length, ...payload];
    raw.add(crc(raw));
    final out = [0];
    var at = 0, code = 1;
    for (final byte in raw) {
      if (byte == 0) {
        out[at] = code;
        at = out.length;
        out.add(0);
        code = 1;
      } else {
        out.add(byte);
        code++;
      }
    }
    out[at] = code;
    out.add(0);
    return Uint8List.fromList(out);
  }

  static UsbFrame? decode(List<int> encoded) {
    if (encoded.isEmpty || encoded.length > 55) {
      return null;
    }
    final raw = <int>[];
    var at = 0;
    while (at < encoded.length) {
      final code = encoded[at++];
      if (code < 1 || at + code - 1 > encoded.length) {
        return null;
      }
      raw.addAll(encoded.sublist(at, at + code - 1));
      at += code - 1;
      if (code != 255 && at < encoded.length) {
        raw.add(0);
      }
    }
    if (raw.length < 6 ||
        raw[0] != 165 ||
        raw[1] != 0 ||
        raw[4] > 48 ||
        raw.length != raw[4] + 6 ||
        crc(raw.take(raw.length - 1)) != raw.last) {
      return null;
    }
    return UsbFrame(raw[2], raw[3], raw.sublist(5, raw.length - 1));
  }
}

class PakarUsbTransport {
  PakarUsbTransport({MethodChannel? channel, this.nativeEvents})
    : channel = channel ?? const MethodChannel('ir.digitalogic.pakar/usb');
  final MethodChannel channel;
  final Stream<dynamic>? nativeEvents;
  static Stream<dynamic>? _sharedNative;
  Stream<dynamic> get _native =>
      nativeEvents ??
      (_sharedNative ??= const EventChannel('ir.digitalogic.pakar/usb-events')
          .receiveBroadcastStream()
          .asBroadcastStream());
  final events = StreamController<Map<String, dynamic>>.broadcast(sync: true);
  final _pending = <int, Completer<UsbFrame>>{};
  final _expected = <int, Set<int>>{};
  final _encoded = <int>[];
  StreamSubscription<dynamic>? _subscription;
  Timer? _renew;
  int? _session;
  static int _nextNativeOwner = 0;
  int? _nativeOwner;
  int _epoch = 0, _sequence = 0, _nonce = 0, _demand = 0;
  bool _busy = false,
      _foreground = true,
      _overflow = false,
      _diagnosticPixels = false;
  Future<void>? _connecting;
  String _endpoint = '';
  DeviceStatus? latest;
  Map<String, dynamic>? table;
  Stopwatch? _captureAge;
  bool get ready => _session != null && latest != null;
  bool get busy => _busy;
  static String endpoint(int device, int interface) =>
      'usb://$device/$interface';
  static (int, int) address(String value) {
    final uri = Uri.tryParse(value);
    final device = int.tryParse(uri?.host ?? ''),
        port = int.tryParse(uri?.path.replaceFirst('/', '') ?? '');
    if (uri?.scheme != 'usb' ||
        device == null ||
        device < 0 ||
        port == null ||
        port < 0 ||
        uri!.hasQuery ||
        uri.hasFragment ||
        uri.userInfo.isNotEmpty ||
        uri.hasPort) {
      throw const FormatException('Select an attached USB CDC interface');
    }
    return (device, port);
  }

  Future<List<Map<String, dynamic>>> devices() async =>
      (await channel.invokeListMethod<dynamic>('devices') ?? [])
          .whereType<Map>()
          .map((v) => Map<String, dynamic>.from(v))
          .toList();

  Future<void> connect(
    String selected, {
    String expectedBoard = '',
    bool enforce = true,
  }) async {
    if (ready && _endpoint == selected) {
      return;
    }
    if (_connecting != null) {
      return _connecting;
    }
    final task = _connect(selected, expectedBoard, enforce);
    _connecting = task;
    try {
      await task;
    } finally {
      if (identical(_connecting, task)) {
        _connecting = null;
      }
    }
  }

  Future<void> _connect(
    String selected,
    String expectedBoard,
    bool enforce,
  ) async {
    final (device, interface) = address(selected);
    await disconnect();
    final owner = _epoch;
    final nativeOwner = ++_nextNativeOwner;
    _nativeOwner = nativeOwner;
    _subscription = _native.listen(_receive, onError: _fail);
    try {
      final opened = await channel
          .invokeMapMethod<String, dynamic>('connect', {
            'device_id': device,
            'interface': interface,
            'owner': nativeOwner,
          })
          .timeout(const Duration(seconds: 45));
      if (owner != _epoch || opened?['session'] is! int) {
        if (opened?['session'] is int) {
          await channel.invokeMethod<void>('disconnect', {
            'session': opened!['session'],
          });
        }
        throw const FormatException('USB owner retired');
      }
      _session = opened!['session'];
      _endpoint = selected;
      await PeerIdentity.current.initialize();
      final version = PeerVersion.parse(PeerIdentity.current.version);
      if (version == null || version.parts.any((p) => p > 255)) {
        throw const FormatException(
          'USB application release is not representable',
        );
      }
      final hello = await _exchange(1, [3, ...version.parts], {0x81});
      if (hello.payload.length < 9 || hello.payload[1] != 2) {
        throw const FormatException('USB HELLO is not an ESP board');
      }
      final report = utf8.encode(
        PeerIdentity.current.reportJson(transport: 'usb', enforce: enforce),
      );
      for (var offset = 0; offset < report.length; offset += 44) {
        final end = (offset + 44).clamp(0, report.length);
        await _ack(0x61, [
          ..._u16(report.length),
          ..._u16(offset),
          ...report.sublist(offset, end),
        ]);
      }
      final actual = await _snapshot();
      validatePeerBoard(
        actual.json,
        appVersion: PeerIdentity.current.version,
        enabled: enforce,
      );
      if (!actual.peer.known ||
          actual.json['board_id'] is! String ||
          !RegExp(r'^[0-9a-fA-F]{32}$').hasMatch('${actual.json['boot_id']}') ||
          expectedBoard.isNotEmpty &&
              actual.json['board_id'] != expectedBoard) {
        throw const FormatException(
          'USB board identity/capabilities do not match the selected board',
        );
      }
      if (!actual.peer.table || !actual.peer.tableSelection) {
        throw const FormatException(
          'This Android USB build supports the table-selection board role only',
        );
      }
      latest = actual;
      _renew = Timer.periodic(const Duration(seconds: 1), (_) {
        if (!_busy && ready && latest!.peer.has('usb-demand')) {
          unawaited(
            _renewDemand().catchError((Object e) {
              _fail(e);
            }),
          );
        }
      });
      await _renewDemand();
    } on Object {
      await disconnect();
      rethrow;
    }
  }

  static List<int> _u16(int n) => [n & 255, n >> 8 & 255];
  static int _le(List<int> p, int at, int length) {
    var n = 0;
    for (var i = 0; i < length; i++) {
      n |= p[at + i] << (i * 8);
    }
    return n;
  }

  void _receive(dynamic raw) {
    if (raw is! Map || raw['session'] != _session) {
      return;
    }
    if (raw['type'] == 'disconnected') {
      _fail(
        const FormatException(
          'USB device detached; commands were not replayed',
        ),
      );
      return;
    }
    final bytes = raw['bytes'];
    if (bytes is! List<int>) {
      return;
    }
    for (final byte in bytes) {
      if (byte != 0) {
        if (_encoded.length < 55 && !_overflow) {
          _encoded.add(byte);
        } else {
          _encoded.clear();
          _overflow = true;
        }
        continue;
      }
      final frame = _overflow ? null : UsbFrame.decode(_encoded);
      _encoded.clear();
      _overflow = false;
      if (frame == null) {
        continue;
      }
      final pending = _pending[frame.sequence];
      if (pending != null &&
          (_expected[frame.sequence]!.contains(frame.opcode) ||
              frame.opcode == 0x82)) {
        _pending.remove(frame.sequence);
        _expected.remove(frame.sequence);
        pending.complete(frame);
      } else if (ready &&
          frame.opcode == 0xb8 &&
          (_captureAge?.elapsedMilliseconds ?? 8000) < 8000) {
        final value = _panelEvent(frame.payload);
        if (value != null) {
          events.add(value);
        }
      } else if (ready && (frame.sequence == 0 || frame.sequence == 254)) {
        if (frame.opcode == 0x90 ||
            frame.opcode == 0xa5 ||
            frame.opcode == 0xa3) {
          events.add({
            'type': 'changed',
            'board_id': latest!.json['board_id'],
            'boot_id': latest!.json['boot_id'],
          });
        } else if (frame.opcode == 0xb8 || frame.opcode == 0xb0) {
          events.add({
            'type': 'usb-peripheral',
            'opcode': frame.opcode,
            'payload': frame.payload,
            'board_id': latest!.json['board_id'],
            'boot_id': latest!.json['boot_id'],
          });
        }
      }
    }
  }

  Map<String, dynamic>? _panelEvent(List<int> payload) {
    const kinds = [
      'rest',
      'cell',
      'mode',
      'effect',
      'pixel',
      'network',
      'progress',
      'fault',
      'complete',
      'output',
      'menu',
      'scan',
      'state',
    ];
    if (payload.length < 16 ||
        payload[15] > 32 ||
        payload.length != 16 + payload[15] ||
        payload[8] >= kinds.length ||
        payload[9] > 2 ||
        payload.skip(16).any((b) => b < 32 || b > 126)) {
      return null;
    }
    final text = ascii.decode(payload.sublist(16));
    final event = <String, dynamic>{
      'type': 'panel-event',
      'board_id': latest!.json['board_id'],
      'boot_id': latest!.json['boot_id'],
      'event_id': _le(payload, 0, 4),
      'uptime_ms': _le(payload, 4, 4),
      'kind': kinds[payload[8]],
      'value': _le(payload, 13, 2),
      'text': text,
      'hardware_confirmed': false,
      'presentation': {
        'text': text.padRight(4).substring(0, 4),
        'lifetime_ms': _le(payload, 11, 2),
        'blink': ['steady', 'slow', 'double-fast'][payload[9]],
        'marquee': text.length > 4 ? 'once' : 'none',
        'priority': payload[10],
      },
    };
    return PanelEvent.decode(event) == null ? null : event;
  }

  Future<UsbFrame> _exchange(
    int op,
    List<int> payload,
    Set<int> replies,
  ) async {
    final session = _session, owner = _epoch;
    if (session == null || _pending.length >= 4) {
      throw const FormatException(
        'USB connection unavailable or busy; nothing sent',
      );
    }
    _sequence = _sequence % 253 + 1;
    final sequence = _sequence;
    final completer = Completer<UsbFrame>();
    _pending[sequence] = completer;
    _expected[sequence] = replies;
    // Observe a detach immediately, including while the native write is pending.
    final observed = completer.future;
    unawaited(observed.then<void>((_) {}, onError: (Object _) {}));
    try {
      final bytes = UsbFrame(op, sequence, payload).encode();
      final count = await channel
          .invokeMethod<int>('write', {'session': session, 'bytes': bytes})
          .timeout(const Duration(seconds: 2));
      if (count != bytes.length || owner != _epoch) {
        const error = FormatException(
          'USB write incomplete; outcome unknown, no replay',
        );
        _fail(error);
        throw error;
      }
      final frame = await observed.timeout(const Duration(seconds: 2));
      if (owner != _epoch) {
        throw const FormatException('USB owner retired');
      }
      if (frame.opcode == 0x82) {
        throw FormatException(
          'Board rejected USB request 0x${op.toRadixString(16)} (code ${frame.payload.length > 1 ? frame.payload[1] : "unknown"}); not retried',
        );
      }
      return frame;
    } finally {
      _pending.remove(sequence);
      _expected.remove(sequence);
    }
  }

  Future<void> _ack(int op, List<int> p) async {
    final frame = await _exchange(op, p, {0x80});
    if (frame.payload.length != 2 ||
        frame.payload[0] != op ||
        frame.payload[1] != 0) {
      throw const FormatException(
        'USB acknowledgement does not match; outcome unverified, no replay',
      );
    }
  }

  Future<DeviceStatus> _snapshot() async {
    _nonce = (_nonce + 1) & 255;
    final bytes = <int>[], started = Stopwatch()..start();
    var token = 0, total = 0;
    while (total == 0 || bytes.length < total) {
      if (started.elapsed > const Duration(seconds: 8)) {
        throw const FormatException('Immutable USB snapshot expired');
      }
      // Lease renewal has its own correlated sequence and runs BETWEEN chunks.
      if (ready &&
          latest!.peer.has('usb-demand') &&
          _demandAge.elapsedMilliseconds >= 900) {
        await _renewDemand();
      }
      final f = await _exchange(
        0x5d,
        [
          _nonce,
          ..._u16(bytes.length),
          ...List.generate(4, (i) => token >> (8 * i) & 255),
        ],
        {0xb6},
      );
      final p = f.payload;
      if (p.length < 10 || p[0] != _nonce || _le(p, 7, 2) != bytes.length) {
        throw const FormatException('USB snapshot chunk does not match');
      }
      final nextToken = _le(p, 1, 4), nextTotal = _le(p, 5, 2);
      if (nextToken == 0 ||
          nextTotal < 1 ||
          nextTotal > 16384 ||
          token != 0 && (token != nextToken || total != nextTotal)) {
        throw const FormatException('USB snapshot capture changed');
      }
      token = nextToken;
      total = nextTotal;
      bytes.addAll(p.skip(9));
      if (bytes.length > total) {
        throw const FormatException('USB snapshot overrun');
      }
    }
    final value = jsonDecode(utf8.decode(bytes));
    if (value is! Map<String, dynamic> ||
        value['status'] is! Map<String, dynamic>) {
      throw const FormatException('Malformed USB table snapshot');
    }
    final status = DeviceStatus(Map<String, dynamic>.from(value['status']));
    if (latest != null &&
        (latest!.json['board_id'] != status.json['board_id'] ||
            latest!.json['boot_id'] != status.json['boot_id'])) {
      throw const FormatException(
        'USB board rebooted or was replaced; reconnect explicitly',
      );
    }
    // Mapping/cell/RGB remain actual captured values, never guessed defaults.
    CellMap.fromJson(value);
    final cells = status.json['cells'];
    if (cells is! List ||
        cells.length != 80 ||
        cells.any((v) => !['white', 'red', 'off'].contains(v))) {
      throw const FormatException('USB table state incomplete');
    }
    if (value['pixels'] is Map<String, dynamic>) {
      status.json['pixel_frame'] = value['pixels'];
    }
    table = value;
    latest = status;
    _captureAge = started;
    return status;
  }

  Future<T> _exclusive<T>(Future<T> Function() action) async {
    if (_busy) {
      throw const FormatException(
        'USB operation already active; nothing queued or replayed',
      );
    }
    _busy = true;
    try {
      return await action();
    } finally {
      _busy = false;
      _diagnosticPixels = false;
    }
  }

  Future<DeviceStatus> status() => _exclusive(_snapshot);
  final _demandAge = Stopwatch()..start();
  Future<void> _renewDemand() async {
    if (!ready || !latest!.peer.has('usb-demand')) {
      return;
    }
    final flags = _foreground ? _demand | (_diagnosticPixels ? 8 : 0) : 0;
    final response = await _exchange(
      0x64,
      [..._u16(flags), ..._u16(flags == 0 ? 0 : 3000)],
      {0xba},
    );
    if (response.payload.length != 4 ||
        _le(response.payload, 0, 2) != flags ||
        (flags == 0
            ? _le(response.payload, 2, 2) != 0
            : _le(response.payload, 2, 2) == 0) ||
        _le(response.payload, 2, 2) > 3000) {
      throw const FormatException('USB observation lease readback mismatch');
    }
    _demandAge.reset();
  }

  void demand(int flags, {required bool foreground}) {
    if (flags & ~15 != 0) {
      throw ArgumentError.value(flags);
    }
    if (_demand == flags && _foreground == foreground) {
      return;
    }
    _demand = flags;
    _foreground = foreground;
    if (ready && !_busy) {
      unawaited(
        _renewDemand().catchError((Object e) {
          _fail(e);
        }),
      );
    }
  }

  Future<DeviceStatus> command(
    String path,
    Map<String, String> values, {
    required bool Function() authorize,
  }) => _exclusive(() async {
    if (!ready || !authorize()) {
      throw const FormatException('USB board not ready; nothing sent');
    }
    final before = latest!, features = before.peer;
    if (!features.tableSelection || before.json['programming_latch'] == true) {
      throw const FormatException(
        'USB table controls unavailable for this board role or during programming',
      );
    }
    int op;
    List<int> payload;
    int integer(String key, int low, int high) {
      final n = int.tryParse(values[key] ?? '');
      if (n == null || n < low || n > high) {
        throw FormatException('Invalid $key');
      }
      return n;
    }

    int state(String? v) => switch (v) {
      'white' => 0,
      'red' => 1,
      'off' => 2,
      _ => throw const FormatException('Invalid cell state'),
    };
    switch (path) {
      case '/api/cell':
        if (!features.has('leds')) {
          throw const FormatException('USB LEDs not acknowledged');
        }
        op = 0x4c;
        payload = [
          integer('id', 1, 80),
          state(values['state']),
          _effect(values['effect']),
        ];
      case '/api/cells':
        if (!features.has('cell-batch')) {
          throw const FormatException('USB cell-batch not acknowledged');
        }
        final ids = _ids(values['ids']);
        final bitmap = List.filled(10, 0);
        for (final id in ids) {
          bitmap[(id - 1) ~/ 8] |= 1 << ((id - 1) % 8);
        }
        op = 0x5f;
        payload = [
          state(values['state']),
          _effect(values['effect']),
          ...bitmap,
        ];
      case '/api/cells/assignments':
        if (!features.has('cell-assignments')) {
          throw const FormatException('USB assignments not acknowledged');
        }
        final assignments = CellAssignments.fromRows(
          jsonDecode(values['assignments'] ?? 'null'),
        );
        final bitmap = List.filled(10, 0), packed = List.filled(20, 0);
        for (final entry in assignments.values.entries) {
          final i = entry.key - 1;
          bitmap[i ~/ 8] |= 1 << (i % 8);
          packed[i ~/ 4] |= state(entry.value) << ((i % 4) * 2);
        }
        op = 0x62;
        payload = [...bitmap, ...packed];
      case '/api/cells/lock':
        if (!features.has('cell-lock')) {
          throw const FormatException('USB lock not acknowledged');
        }
        op = 0x63;
        payload = [_boolean(values['enabled'])];
      case '/api/brightness':
        op = 0x51;
        payload = [integer('value', 1, 255)];
      case '/api/effect':
        op = 0x52;
        payload = [
          switch (values['value']) {
            'none' => 0,
            'rainbow' => 1,
            'fire' => 2,
            'cell-chase' => 3,
            _ => throw const FormatException('Unknown effect'),
          },
        ];
      case '/api/mode':
        if (values['value'] != before.mode) {
          throw const FormatException('This board has one baked-in game mode');
        }
        op = 0x4b;
        payload = [
          switch (values['value']) {
            'cell' => 1,
            'first-wins' => 2,
            'snake' => 3,
            _ => throw const FormatException('Unsupported mode'),
          },
        ];
      case '/api/led/output':
        op = 0x5c;
        payload = values.containsKey('mode')
            ? [
                2,
                switch (values['mode']) {
                  'static' => 0,
                  'effects' => 1,
                  _ => throw const FormatException('Unknown LED mode'),
                },
              ]
            : [_boolean(values['enabled'])];
      case '/api/led/diagnostic':
        if (!features.has('usb-pixel-diagnostic') ||
            !features.has('usb-demand')) {
          throw const FormatException('USB pixel diagnostics not acknowledged');
        }
        final index = integer('index', 0, 479),
            count = integer('count', 1, 6),
            rgb = integer('rgb', 0, 0xffffff);
        if (count != 1 && count != 6 ||
            index + count > 480 ||
            count == 6 && index % 6 != 0) {
          throw const FormatException('Invalid pixel range');
        }
        _diagnosticPixels = true;
        await _renewDemand();
        op = 0x65;
        payload = [
          ..._u16(index),
          count,
          rgb >> 16,
          (rgb >> 8) & 255,
          rgb & 255,
          _boolean(values['off']),
        ];
      default:
        throw const FormatException(
          'This operation is not available through USB; no network fallback attempted',
        );
    }
    final boot = '${before.json['boot_id']}';
    final fence = [
      for (var i = 0; i < 32; i += 2)
        int.parse(boot.substring(i, i + 2), radix: 16),
    ];
    if (!authorize()) {
      throw const FormatException(
        'USB command admission retired; nothing sent',
      );
    }
    // One physical dispatch only. ACK is not accepted as an applied state.
    final reply = await _exchange(0x5e, [...fence, op, ...payload], {0x80});
    if (reply.payload.length != 2 ||
        reply.payload[0] != op ||
        reply.payload[1] != 0) {
      throw const FormatException('USB command receipt mismatch; no replay');
    }
    var actual = await _snapshot();
    if (op == 0x65) {
      final start = Stopwatch()..start();
      while ((actual.json['pixel_frame'] as Map?)?['pending'] == true &&
          start.elapsed < const Duration(seconds: 5)) {
        await Future<void>.delayed(const Duration(milliseconds: 30));
        actual = await _snapshot();
      }
      final probe = PixelProbe(
        index: payload[0] | payload[1] << 8,
        group: payload[2] == 6,
        rgb: payload[3] << 16 | payload[4] << 8 | payload[5],
      );
      if (!matchesDiagnosticFrame(actual, probe.frame(off: payload[6] == 1))) {
        throw const FormatException(
          'USB diagnostic published RGB differs or is pending; no replay',
        );
      }
    }
    if (!authorize()) {
      throw const FormatException(
        'USB command owner retired; outcome not claimed',
      );
    }
    if (op == 0x4c && actual.json['cells'][payload[0] - 1] != values['state'] ||
        op == 0x5f &&
            _ids(values['ids'])
                .any((id) => actual.json['cells'][id - 1] != values['state']) ||
        op == 0x63 && actual.json['red_lock'] != (payload[0] == 1) ||
        op == 0x51 && actual.brightness != payload[0] ||
        op == 0x52 && actual.effect != values['value'] ||
        op == 0x4b && actual.mode != values['value'] ||
        op == 0x5c &&
            (payload.length == 1
                ? actual.ledOutputEnabled != (payload[0] == 1)
                : actual.ledOutputMode != values['mode']) ||
        op == 0x62 &&
            CellAssignments.fromRows(jsonDecode(values['assignments']!))
                .values
                .entries
                .any((e) => actual.json['cells'][e.key - 1] != e.value)) {
      throw const FormatException(
        'USB post-ACK readback differs; nothing replayed',
      );
    }
    return actual;
  });
  static int _boolean(String? value) => switch (value) {
    'true' || '1' => 1,
    'false' || '0' => 0,
    _ => throw const FormatException('Invalid boolean'),
  };
  static int _effect(String? value) => switch (value) {
    null || 'immediate' => 0,
    'fade' => 1,
    'pulse' => 2,
    _ => throw const FormatException('Unknown effect'),
  };
  static Set<int> _ids(String? value) {
    final result = <int>{};
    for (final token in (value ?? '').split(',')) {
      final range = token.split('-');
      final first = int.tryParse(range.first), last = int.tryParse(range.last);
      if (range.length > 2 ||
          first == null ||
          last == null ||
          first < 1 ||
          last > 80 ||
          last < first) {
        throw const FormatException('Invalid cell selection');
      }
      for (var id = first; id <= last; id++) {
        result.add(id);
      }
    }
    if (result.isEmpty) {
      throw const FormatException('Select cells');
    }
    return result;
  }

  Future<UsbFrame> contract(int opcode, List<int> payload) =>
      _exclusive(() async {
        if (!ready) {
          throw const FormatException('USB board not ready');
        }
        return _exchange(0x56, [opcode, ...payload], {0xb0});
      });
  Future<void> cue(String cue) => _exclusive(() async {
    const names = [
      'welcome',
      'update-start',
      'update-success',
      'error',
      'winner',
      'finish',
      'lost',
      'incorrect',
      'rapid-error',
      'fault',
      'increase',
      'decrease',
      'welcome-puzzleboard',
      'welcome-40led',
      'attention',
    ];
    final index = names.indexOf(cue);
    if (index < 0 || latest?.audioCues.contains(cue) != true) {
      throw const FormatException('Melody not acknowledged by this USB board');
    }
    await _ack(0x11, [index]);
  });
  Future<BoardSound> boardSound({bool? silent}) => _exclusive(() async {
    if (latest?.avrOnline != true ||
        latest?.peer.has('settings') != true ||
        latest?.peer.has('buzzer') != true) {
      throw const FormatException('USB AVR sound settings unavailable');
    }
    Future<BoardSound> read() async {
      final f = await _exchange(0x56, [4], {0xb0});
      if (f.payload.length != 23 || f.payload.first != 0x91) {
        throw const FormatException('USB AVR settings response does not match');
      }
      return BoardSound.parse(BoardSound.encode(f.payload.skip(1)));
    }

    var result = await read();
    if (silent != null && (!result.persisted || result.silent != silent)) {
      if (latest?.json['programming_latch'] == true) {
        throw const FormatException('USB programming active');
      }
      final expected = result.withSilent(silent);
      final bytes = [
        for (var i = 0; i < expected.length; i += 2)
          int.parse(expected.substring(i, i + 2), radix: 16),
      ];
      final reply = await _exchange(0x56, [5, ...bytes], {0xb0});
      if (reply.payload.length != 3 ||
          reply.payload[0] != 0x80 ||
          reply.payload[1] != 5 ||
          reply.payload[2] != 0) {
        throw const FormatException('USB AVR settings ACK mismatch; no replay');
      }
      result = await read();
      if (!result.persisted || BoardSound.encode(result.bytes) != expected) {
        throw const FormatException('USB Silent readback differs; no replay');
      }
    }
    return result;
  });
  Future<void> display(Map<String, String> form) => _exclusive(() async {
    if (latest?.peer.has('text') != true || latest?.avrOnline != true) {
      throw const FormatException('USB text adapter not acknowledged');
    }
    final text = ascii.encode(form['text'] ?? '');
    if (text.length > 40 || text.any((b) => b < 32 || b > 126)) {
      throw const FormatException(
        'Display supports 40 printable ASCII characters',
      );
    }
    final speed = int.tryParse(form['speed_ms'] ?? '260'),
        hold = int.tryParse(form['hold_ms'] ?? '1500'),
        interval = int.tryParse(form['interval_seconds'] ?? '5');
    final repeat = switch (form['repeat'] ?? 'once') {
      'once' => 0,
      'loop' => 1,
      'interval' => 2,
      _ => -1,
    };
    if (speed == null ||
        speed < 80 ||
        speed > 5000 ||
        hold == null ||
        hold < 0 ||
        hold > 60000 ||
        interval == null ||
        interval < 1 ||
        interval > 255 ||
        repeat < 0) {
      throw const FormatException('Invalid display timing');
    }
    await _ack(0x38, [
      5,
      ..._u16(speed),
      text.length,
      repeat | (form['force_scroll'] == 'true' ? 128 : 0),
      ..._u16(hold),
      interval,
      ...text,
    ]);
  });
  void _fail(Object error) {
    final session = _session;
    ++_epoch;
    _session = null;
    latest = null;
    table = null;
    _captureAge = null;
    _renew?.cancel();
    _renew = null;
    for (final pending in _pending.values.toList()) {
      if (!pending.isCompleted) {
        pending.completeError(error);
      }
    }
    _pending.clear();
    _expected.clear();
    if (!events.isClosed) {
      events.addError(error);
    }
    if (session != null) {
      unawaited(
        channel
            .invokeMethod<void>('disconnect', {'session': session})
            .catchError((Object _) {}),
      );
    }
  }

  Future<void> disconnect() async {
    ++_epoch;
    final session = _session;
    final nativeOwner = _nativeOwner;
    _nativeOwner = null;
    _session = null;
    latest = null;
    table = null;
    _captureAge = null;
    _renew?.cancel();
    _renew = null;
    _encoded.clear();
    _overflow = false;
    for (final pending in _pending.values.toList()) {
      if (!pending.isCompleted) {
        pending.completeError(
          const FormatException('USB owner retired; no replay'),
        );
      }
    }
    _pending.clear();
    _expected.clear();
    await _subscription?.cancel();
    _subscription = null;
    if (session != null || nativeOwner != null) {
      try {
        await channel
            .invokeMethod<void>('disconnect', {
              'session': session,
              'owner': nativeOwner,
            })
            .timeout(const Duration(seconds: 1));
      } on Object {
        /* Physical owner is already retired. */
      }
    }
  }

  Future<void> close() async {
    await disconnect();
    await events.close();
  }
}
