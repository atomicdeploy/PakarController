import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';

import 'device_registry.dart';
import 'firmware.dart';

// Adapts the fully reviewed PCController hostbridge/manager.go handshake,
// namespace, ping/pong and exact event/data tuple. No second board connection.
class FirmwareFeedOptions {
  const FirmwareFeedOptions({
    required this.endpoint,
    required this.origin,
    this.socketIo = false,
    this.namespace = '/',
    this.event = 'firmware-offer',
    this.token = '',
  });
  final String endpoint, origin, namespace, event, token;
  final bool socketIo;
  void validate() {
    final u = Uri.tryParse(endpoint), d = Uri.tryParse(origin);
    if (u == null ||
        u.host.isEmpty ||
        u.userInfo.isNotEmpty ||
        u.hasQuery ||
        u.hasFragment ||
        (u.scheme != 'wss' &&
            !(u.scheme == 'ws' &&
                {'127.0.0.1', 'localhost', '::1'}.contains(u.host))) ||
        d == null ||
        d.scheme != 'https' ||
        d.host.isEmpty ||
        d.userInfo.isNotEmpty ||
        d.hasQuery ||
        d.hasFragment ||
        !(d.path.isEmpty || d.path == '/') ||
        !RegExp(r'^/[A-Za-z0-9/_-]{0,95}$').hasMatch(namespace) ||
        !RegExp(r'^[A-Za-z0-9._-]{1,64}$').hasMatch(event) ||
        endpoint.length > 2048 ||
        RegExp(r'[\x00-\x20\x7f-\uffff\\]').hasMatch(endpoint) ||
        token.length > 256 ||
        RegExp(r'[^!-~]').hasMatch(token)) {
      throw const FormatException(
        'Use a WSS endpoint, exact namespace/event and trusted HTTPS download origin. Plain WS is loopback-test only.',
      );
    }
  }
}

class FirmwareOffer {
  FirmwareOffer(this.json);
  final Map<String, dynamic> json;
  String get id => json['id'];
  String get url => json['url'];
  String get hash => json['sha256'];
  String get target => json['target'];
  String get kind => json['kind'];
  static FirmwareOffer parse(
    Object? raw,
    FirmwareFeedOptions options,
    String session,
    String board,
    String boot, {
    int? now,
  }) {
    final at = now ?? DateTime.now().millisecondsSinceEpoch;
    if (raw is! Map<String, dynamic>) {
      throw const FormatException('Invalid firmware offer');
    }
    final id = raw['id'], deadline = raw['expires_at_ms'], url = raw['url'];
    final u = url is String ? Uri.tryParse(url) : null,
        origin = Uri.parse(options.origin);
    if (id is! String ||
        !RegExp(r'^[A-Za-z0-9_-]{1,64}$').hasMatch(id) ||
        raw['session'] != session ||
        raw['board_id'] != board ||
        raw['boot_id'] != boot ||
        deadline is! int ||
        deadline <= at ||
        deadline > at + 60000 ||
        u == null ||
        u.scheme != origin.scheme ||
        u.host != origin.host ||
        u.port != origin.port ||
        u.userInfo.isNotEmpty ||
        u.hasFragment ||
        (url as String).length > 2048 ||
        RegExp(r'[\x00-\x20\x7f-\uffff\\]').hasMatch(url) ||
        raw['sha256'] is! String ||
        !firmwareHash(raw['sha256']) ||
        !{'esp', 'avr', 'all'}.contains(raw['target']) ||
        !{'binary', 'manifest'}.contains(raw['kind']) ||
        (raw['kind'] == 'binary' && raw['target'] == 'all')) {
      throw const FormatException(
        'Rejected stale, wrong-session/peer/origin or malformed firmware offer',
      );
    }
    return FirmwareOffer(Map.unmodifiable(raw));
  }
}

class FirmwareFeedWire {
  FirmwareFeedWire(this.options);
  final FirmwareFeedOptions options;
  bool opened = false, connected = false;
  Duration heartbeat = const Duration(seconds: 15);
  String get ns => options.namespace == '/' ? '' : '${options.namespace},';
  String event(String name, Object data) => '42$ns${jsonEncode([name, data])}';
  // reply: send packet; ready: send subscription; offer: data; ignore: no actuation.
  (String, Object?) receive(String text) {
    if (utf8.encode(text).length > 8192) {
      throw const FormatException('Oversized firmware event');
    }
    if (!options.socketIo) {
      final v = jsonDecode(text);
      return v is Map && v['type'] == options.event
          ? ('offer', v)
          : ('ignore', null);
    }
    if (text.startsWith('0') && !opened) {
      final v = jsonDecode(text.substring(1));
      if (v is! Map ||
          v['sid'] is! String ||
          (v['sid'] as String).isEmpty ||
          v['pingInterval'] is! int ||
          v['pingTimeout'] is! int ||
          v['pingInterval'] <= 0 ||
          v['pingInterval'] > 120000 ||
          v['pingTimeout'] <= 0 ||
          v['pingTimeout'] > 60000) {
        throw const FormatException('Invalid Engine.IO handshake');
      }
      opened = true;
      heartbeat = Duration(
        milliseconds: (v['pingInterval'] as int) + (v['pingTimeout'] as int),
      );
      return (
        'reply',
        '40$ns${options.token.isEmpty ? '' : jsonEncode({'token': options.token})}',
      );
    }
    if (!opened) {
      throw const FormatException('Socket.IO packet before Engine.IO open');
    }
    if (text.startsWith('2')) return ('reply', '3${text.substring(1)}');
    if (!connected && (text == '40$ns' || text.startsWith('40$ns{'))) {
      connected = true;
      return ('ready', null);
    }
    if (text.startsWith('1') ||
        text.startsWith('41$ns') ||
        text.startsWith('44$ns')) {
      throw const FormatException('Remote namespace closed/refused');
    }
    if (!text.startsWith('42$ns')) return ('ignore', null);
    final raw = text.substring(2 + ns.length).replaceFirst(RegExp(r'^\d+'), '');
    if (!raw.startsWith('[')) return ('ignore', null);
    if (!connected) {
      throw const FormatException('Event before namespace acknowledgment');
    }
    final a = jsonDecode(raw);
    if (a is! List || a.length != 2) {
      throw const FormatException('Invalid Socket.IO event tuple');
    }
    return a.first == options.event ? ('offer', a[1]) : ('ignore', null);
  }
}

class FirmwareFeed extends ChangeNotifier {
  WebSocket? socket;
  StreamSubscription<dynamic>? events;
  Timer? timer;
  String session = '', state = 'Stopped';
  final seen = <String>{};
  FirmwareOffer? pending;
  bool running = false, dispatching = false, closed = false;
  int epoch = 0;
  Future<void> start(
    FirmwareFeedOptions options,
    String board,
    String boot, {
    required bool armed,
    required Future<void> Function(FirmwareOffer) deliver,
  }) async {
    options.validate();
    await stop();
    if (closed) return;
    session = randomIdentifier();
    seen.clear();
    pending = null;
    running = true;
    state = 'Connecting';
    notifyListeners();
    final generation = ++epoch;
    try {
      final uri = Uri.parse(options.endpoint);
      final ws = await WebSocket.connect(
        options.socketIo
            ? uri
                  .replace(
                    queryParameters: {'EIO': '4', 'transport': 'websocket'},
                  )
                  .toString()
            : options.endpoint,
        headers: options.token.isEmpty
            ? {}
            : {'Authorization': 'Bearer ${options.token}'},
      ).timeout(const Duration(seconds: 8));
      if (generation != epoch || closed) {
        await ws.close();
        return;
      }
      socket = ws;
      final wire = FirmwareFeedWire(options);
      var last = DateTime.now();
      final subscription = {
        'type': 'subscribe',
        'events': [options.event],
        'session': session,
        'board_id': board,
        'boot_id': boot,
      };
      if (!options.socketIo) {
        ws.pingInterval = const Duration(seconds: 20);
        ws.add(jsonEncode(subscription));
        state = 'Listening';
        notifyListeners();
      }
      events = ws.listen(
        (raw) {
          if (generation != epoch || closed) return;
          try {
            if (raw is! String) {
              throw const FormatException(
                'Use a pinned HTTP file offer, not binary socket data',
              );
            }
            last = DateTime.now();
            final decoded = wire.receive(raw);
            switch (decoded.$1) {
              case 'reply':
                ws.add(decoded.$2 as String);
                break;
              case 'ready':
                ws.add(wire.event('subscribe', subscription));
                state = 'Listening';
                notifyListeners();
                break;
              case 'offer':
                final offer = FirmwareOffer.parse(
                  decoded.$2,
                  options,
                  session,
                  board,
                  boot,
                );
                if (seen.contains(offer.id)) return;
                if (seen.length >= 1024) {
                  throw const FormatException(
                    'Session budget reached; restart explicitly',
                  );
                }
                seen.add(offer.id);
                if (armed) {
                  if (dispatching) {
                    state = 'Offer discarded: a delivery is in progress';
                    notifyListeners();
                    return;
                  }
                  dispatching = true;
                  unawaited(
                    deliver(offer)
                        .catchError((Object _) {
                          if (!closed) {
                            state = 'Delivery outcome needs inspection; not replayed';
                            notifyListeners();
                          }
                        })
                        .whenComplete(() => dispatching = false),
                  );
                } else if (pending == null) {
                  pending = offer;
                  notifyListeners();
                }
                break;
              default:
                break;
            }
          } on Object {
            state = 'Feed packet rejected; listener stopped';
            unawaited(stop(keepState: true));
          }
        },
        onError: (Object _) {
          state = 'Remote feed disconnected; not replayed';
          unawaited(stop(keepState: true));
        },
        onDone: () {
          if (generation == epoch) {
            state = 'Remote feed closed; not replayed';
            unawaited(stop(keepState: true));
          }
        },
      );
      timer = Timer.periodic(const Duration(seconds: 1), (_) {
        if (generation != epoch) return;
        if (options.socketIo &&
            DateTime.now().difference(last) > wire.heartbeat) {
          state = 'Remote feed heartbeat expired';
          unawaited(stop(keepState: true));
        }
      });
    } on Object {
      state = 'Feed connection failed; no insecure fallback';
      await stop(keepState: true);
      rethrow;
    }
  }

  Future<void> stop({bool keepState = false}) async {
    ++epoch;
    running = false;
    pending = null;
    timer?.cancel();
    timer = null;
    await events?.cancel();
    events = null;
    final prior = socket;
    socket = null;
    if (!keepState) state = 'Stopped';
    if (!closed) notifyListeners();
    try {
      await prior?.close().timeout(const Duration(milliseconds: 500));
    } on Object {
      /* bounded stop */
    }
  }

  @override
  void dispose() {
    closed = true;
    unawaited(stop());
    super.dispose();
  }
}
