import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';

import 'api.dart';
import 'command_failure.dart';
import 'command_readback.dart';
import 'command_wire.dart';
import 'device_registry.dart';
import 'firmware.dart';
import 'models.dart';
import 'peer_identity.dart';
import 'peer_policy.dart';

/// Shared admission for MQTT, outbound WebSocket and Android loopback clients.
/// IDs are consumed before dispatch; lost acknowledgments never trigger replay.
class BridgeAdmission {
  BridgeAdmission(this.session);
  final String session;
  final seen = <String>{};
  void admit(
    Map<String, dynamic> value,
    DeviceStatus status, {
    bool retained = false,
  }) {
    final now = DateTime.now().millisecondsSinceEpoch;
    final id = value['id'], expires = value['expires_at_ms'];
    if (retained ||
        value['bridge_session'] != session ||
        id is! String ||
        !RegExp(r'^[A-Za-z0-9_-]{1,64}$').hasMatch(id) ||
        expires is! int ||
        expires <= now ||
        expires > now + 60000 ||
        value['board_id'] is! String ||
        value['boot_id'] is! String ||
        (status.json['board_id'] as String? ?? '').isEmpty ||
        (status.json['boot_id'] as String? ?? '').isEmpty ||
        value['board_id'] != status.json['board_id'] ||
        value['boot_id'] != status.json['boot_id']) {
      throw const FormatException(
        'Rejected retained, expired or wrong-session/peer request',
      );
    }
    if (seen.length >= 1024 || !seen.add(id)) {
      throw const FormatException(
        'Duplicate request or session budget reached; nothing replayed',
      );
    }
  }
}

class BridgeOptions {
  BridgeOptions({
    this.broker = '',
    this.topic = 'pakar/bridge',
    this.user = '',
    this.password = '',
    this.remoteWebSocket = '',
    this.remoteToken = '',
    this.local = true,
    this.commands = false,
    this.firmwareStage = false,
    this.telemetrySeconds = 5,
    this.qos = 0,
  });
  final String broker, topic, user, password, remoteWebSocket, remoteToken;
  final bool local, commands, firmwareStage;
  final int telemetrySeconds, qos;
  void validate() {
    if (telemetrySeconds < 1 ||
        telemetrySeconds > 3600 ||
        qos < 0 ||
        qos > 1 ||
        topic.isEmpty ||
        topic.length > 128 ||
        RegExp(r'[+#\x00-\x20]').hasMatch(topic)) {
      throw const FormatException('Invalid interval, QoS or exact base topic');
    }
    if (broker.isNotEmpty) {
      final u = Uri.tryParse(broker);
      if (u == null ||
          !['mqtts', 'wss'].contains(u.scheme) ||
          u.host.isEmpty ||
          u.userInfo.isNotEmpty ||
          u.hasFragment ||
          u.hasQuery ||
          (u.scheme == 'mqtts' && u.path.isNotEmpty && u.path != '/')) {
        throw const FormatException(
          'Use mqtts://hostname:8883 or wss://hostname/mqtt; TLS is required',
        );
      }
    }
    if (remoteWebSocket.isNotEmpty) {
      final u = Uri.tryParse(remoteWebSocket);
      if (u == null ||
          u.scheme != 'wss' ||
          u.host.isEmpty ||
          u.userInfo.isNotEmpty ||
          u.hasFragment ||
          u.hasQuery ||
          RegExp(r'[\r\n]').hasMatch(remoteToken)) {
        throw const FormatException(
          'Use a direct wss:// endpoint without credentials/query',
        );
      }
    }
  }
}

/// A bounded, session-local event cursor, not a durable command queue. A gap
/// requires a fresh snapshot; it never authorizes replaying a mutation.
class BridgeEventJournal {
  static const capacity = 64;
  final _events = <Map<String, dynamic>>[];
  int cursor = 0;

  Map<String, dynamic> add(Map<String, dynamic> value) {
    // Freeze nested values too: later UI/status updates cannot rewrite history.
    final frozen = jsonDecode(jsonEncode(value)) as Map<String, dynamic>;
    frozen['cursor'] = ++cursor;
    _events.add(frozen);
    if (_events.length > capacity) _events.removeAt(0);
    return frozen;
  }

  Map<String, dynamic> since(int after) {
    if (after < 0) throw const FormatException('Invalid event cursor');
    final oldest = _events.isEmpty
        ? cursor + 1
        : _events.first['cursor'] as int;
    return {
      'cursor': cursor,
      'oldest_cursor': oldest,
      'gap': after < oldest - 1 || after > cursor,
      'events': _events.where((e) => (e['cursor'] as int) > after).toList(),
    };
  }
}

/// Foreground-only bridge. Never promises Android background/lockscreen uptime.
/// No remote peer receives the ESP password, Wi-Fi settings or arbitrary status JSON.
class AndroidBridge extends ChangeNotifier {
  AndroidBridge(
    this.api,
    this.observed, {
    this.enforceVersions = enforcePeerVersions,
  }) : owner = api.connectionRevision;
  final bool enforceVersions;
  final peerRegistry = PeerRegistry();
  final PakarApi api;
  final DeviceStatus? Function() observed;
  final int owner;
  BridgeOptions? options;
  BridgeAdmission? admission;
  HttpServer? server;
  MqttServerClient? mqtt;
  WebSocket? remote;
  final localSockets = <WebSocket>{};
  StreamSubscription<dynamic>? mqttEvents, remoteEvents;
  StreamSubscription<DeviceStatus>? boardEvents, rgbEvents;
  Timer? timer, watchdog, liveFlush;
  DeviceStatus? pendingStatus;
  String? lastBoardSnapshot;
  BridgeEventJournal journal = BridgeEventJournal();
  String token = '', state = 'Stopped', error = '';
  final incidents = <String, int>{};
  String? peerBoard, peerBoot;
  int received = 0, rejected = 0, sent = 0, epoch = 0;
  bool running = false, busy = false, uncertain = false, closed = false;
  bool subscriptionAcknowledged = false;
  bool get mqttConnected =>
      mqtt?.connectionStatus?.state == MqttConnectionState.connected;
  bool get webSocketConnected => remote?.readyState == WebSocket.open;
  DeviceStatus current() {
    final value = observed();
    if (!running ||
        closed ||
        owner != api.connectionRevision ||
        value == null ||
        !value.peer.known ||
        value.json['board_id'] != peerBoard ||
        value.json['boot_id'] != peerBoot) {
      throw const StaleConnection();
    }
    return value;
  }

  Future<void> start(BridgeOptions selected) async {
    selected.validate();
    await stop();
    if (closed) throw const StaleConnection();
    final value = observed();
    if (owner != api.connectionRevision ||
        value == null ||
        !value.peer.known ||
        value.json['board_id'] is! String ||
        !RegExp(r'^[!-~]{1,80}$').hasMatch(value.json['board_id']) ||
        value.json['boot_id'] is! String ||
        !RegExp(r'^[!-~]{1,80}$').hasMatch(value.json['boot_id'])) {
      throw const FormatException(
        'Connect to a live peer with board/boot identity before starting a bridge',
      );
    }
    peerBoard = value.json['board_id'];
    peerBoot = value.json['boot_id'];
    options = selected;
    admission = BridgeAdmission(randomIdentifier());
    token = randomIdentifier() + randomIdentifier();
    final generation = ++epoch;
    running = true;
    uncertain = false;
    state = 'Starting';
    error = '';
    incidents.clear();
    journal = BridgeEventJournal();
    lastBoardSnapshot = null;
    notifyListeners();
    try {
      if (selected.local) {
        final active = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
        if (generation != epoch || closed) {
          await active.close(force: true);
          throw const StaleConnection();
        }
        server = active;
        active.listen((r) => unawaited(_local(r, generation)));
      }
      if (selected.broker.isNotEmpty) {
        final u = Uri.parse(selected.broker),
            ws = Uri.parse(selected.broker).scheme == 'wss';
        final active = MqttServerClient.withPort(
          ws ? selected.broker : u.host,
          'pakar-${admission!.session.substring(0, 16)}',
          u.hasPort
              ? u.port
              : ws
              ? 443
              : 8883,
        );
        mqtt = active;
        active.useWebSocket = ws;
        if (ws) active.websocketHeader = PeerIdentity.current.webSocketHeaders;
        active.secure = !ws;
        active.logging(on: false);
        active.setProtocolV311();
        active.keepAlivePeriod = 20;
        active.autoReconnect =
            false; // No offline command backlog/session resumption.
        active.connectTimeoutPeriod = 8000;
        active.connectionMessage = MqttConnectMessage()
            .withClientIdentifier(
              'pakar-${admission!.session.substring(0, 16)}',
            )
            .withWillTopic('${selected.topic}/availability')
            .withWillMessage(
              jsonEncode({
                'online': false,
                'bridge_session': admission!.session,
                'board_id': peerBoard,
                'boot_id': peerBoot,
              }),
            )
            .withWillQos(MqttQos.atMostOnce)
            .withWillRetain()
            .startClean();
        active.onSubscribed = (topic) {
          if (running &&
              !closed &&
              generation == epoch &&
              topic == '${selected.topic}/request') {
            subscriptionAcknowledged = true;
            notifyListeners();
          }
        };
        active.onDisconnected = () {
          if (running && !closed) {
            subscriptionAcknowledged = false;
            _failure('MQTT disconnected; reconnect explicitly');
          }
        };
        active.onSubscribeFail = (_) {
          if (running && !closed) {
            _failure('MQTT command subscription rejected');
          }
        };
        await active.connect(
          selected.user.isEmpty ? null : selected.user,
          selected.password.isEmpty ? null : selected.password,
        );
        if (generation != epoch || !mqttConnected) {
          throw const StaleConnection();
        }
        current();
        mqttEvents = active.updates!.listen((messages) {
          for (final m in messages) {
            if (m.topic != '${selected.topic}/request' ||
                m.payload is! MqttPublishMessage) {
              continue;
            }
            final publish = m.payload as MqttPublishMessage;
            if (publish.payload.message.length > 8192) {
              rejected++;
              continue;
            }
            unawaited(
              _incoming(
                utf8.decode(publish.payload.message, allowMalformed: true),
                retained: publish.header?.retain == true,
                generation: generation,
              ),
            );
          }
        });
        active.subscribe('${selected.topic}/request', MqttQos.atMostOnce);
      }
      if (selected.remoteWebSocket.isNotEmpty) {
        final active = await WebSocket.connect(
          selected.remoteWebSocket,
          headers: {
            ...PeerIdentity.current.webSocketHeaders,
            if (selected.remoteToken.isNotEmpty)
              'Authorization': 'Bearer ${selected.remoteToken}',
          },
        ).timeout(const Duration(seconds: 8));
        if (generation != epoch || closed) {
          await active.close();
          throw const StaleConnection();
        }
        remote = active;
        active.pingInterval = const Duration(seconds: 20);
        current();
        remoteEvents = active.listen(
          (message) {
            if (message is String && message.length <= 8192) {
              unawaited(_incoming(message, generation: generation));
            }
          },
          onError: (Object _) => _failure('Remote WebSocket disconnected'),
          onDone: () {
            if (running && !closed) _failure('Remote WebSocket disconnected');
          },
        );
      }
      if (generation != epoch) throw const StaleConnection();
      current();
      state = 'Running · foreground only';
      _publish('${selected.topic}/availability', {
        'online': true,
        'bridge_session': admission!.session,
        'board_id': peerBoard,
        'boot_id': peerBoot,
      }, retain: true);
      notifyListeners();
      telemetry();
      // Subscribe to the existing board event owner, never open a competing
      // socket/poller. Coalesce one event-loop turn, not the heartbeat interval.
      boardEvents = api.statusUpdates.listen(
        (status) => _boardChanged(status, generation),
      );
      rgbEvents = api.rgbUpdates.listen(
        (status) => _boardChanged(status, generation),
      );
      timer = Timer.periodic(
        Duration(seconds: selected.telemetrySeconds),
        (_) => telemetry(),
      );
      // Freshness must not depend on a user-selected long telemetry interval.
      watchdog = Timer.periodic(const Duration(seconds: 1), (_) {
        if (!running || closed) return;
        try {
          current();
        } on Object {
          unawaited(stop());
          _failure(
            'Peer expired/changed; bridge stopped and requests discarded',
          );
        }
      });
    } on Object {
      await stop();
      _failure('Bridge startup failed; no commands retried');
      rethrow;
    }
  }

  void _failure(String reason) {
    if (closed) return;
    incidents[reason] = (incidents[reason] ?? 0) + 1;
    error = reason;
    notifyListeners();
  }

  Map<String, dynamic> snapshot([DeviceStatus? eventStatus]) {
    final currentStatus = current();
    final s = eventStatus ?? currentStatus;
    if (s.json['board_id'] != peerBoard || s.json['boot_id'] != peerBoot) {
      throw const StaleConnection();
    }
    return {
      'type': 'telemetry',
      'bridge_session': admission!.session,
      'board_id': peerBoard,
      'boot_id': peerBoot,
      'observed_at_ms': DateTime.now().millisecondsSinceEpoch,
      'client': {
        ...PeerIdentity.current.report(enforce: enforceVersions),
        'platform': Platform.operatingSystem,
        'bridge': 'foreground',
        'commands_enabled': options!.commands,
        'firmware_stage_enabled': options!.firmwareStage,
      },
      'board': {
        for (final key in [
          'board_id',
          'boot_id',
          'hostname',
          'version',
          'avr_online',
          'tm1637_ack',
          'mode',
          'effect',
          'brightness',
          'winner',
          'active_inputs',
          'input_9',
          'active_outputs',
          'key_mask_applied',
          'game_audio_applied',
          'auto_reset_state',
          'ntp_synced',
          'station_rssi',
          'selected_cell',
          'red_lock',
          'command_clock_ms',
        ])
          if (s.json[key] is bool ||
              s.json[key] is num ||
              (s.json[key] is String && (s.json[key] as String).length <= 128))
            key: s.json[key],
        if (s.cells.length == 80 &&
            s.cells.every((cell) => {'red', 'white', 'off'}.contains(cell)))
          'cells': s.cells,
        if (s.segments.length == 4) 'tm1637_segments': s.segments,
        'tm1637_brightness': s.segmentBrightness,
        if (s.peer.role == 'first-wins' && s.json['key_game'] is Map)
          'key_game': _safeFields(s.json['key_game'] as Map, [
            'enabled_mask', 'enabled_count', 'active_pixels', 'span_pixels',
            'capacity_pixels', 'idle_rgb', 'win_rgb', 'lose_rgb',
            'extended_effects', 'auto_reset_ms', 'chase_period_ms',
            'chase_neighbors', 'chase_invert', 'winner_cue', 'reset_cue',
            'presentation',
          ]),
        if (s.json['led_output'] is Map)
          'led_output': {
            ..._safeFields(s.json['led_output'] as Map, [
              'enabled',
              'mode',
              'backend',
              'rmt',
              'dma',
              'backend_saved',
              'backend_settings_error',
            ]),
            if ((s.json['led_output'] as Map)['driver'] is Map)
              'driver': _safeFields(
                (s.json['led_output'] as Map)['driver'] as Map,
                ['backend', 'available_backends', 'requested_backend',
                  'switch_pending', 'switch_error', 'error_code', 'ready',
                  'rmt', 'dma'],
              ),
            if ((s.json['led_output'] as Map)['diagnostic'] is Map)
              'diagnostic': _safeFields(
                (s.json['led_output'] as Map)['diagnostic'] as Map,
                ['revision', 'index', 'count', 'rgb', 'active', 'off'],
              ),
          },
        if (s.upgrade != null)
          'upgrade': _safeFields(s.upgrade!.json, [
            'revision',
            'operation_id',
            'source',
            'phase',
            'received_bytes',
            'expected_bytes',
            'active',
            'progress_known',
            'progress_percent',
            'completion_verified',
          ]),
        if (s.json['status_rgb'] is Map)
          'status_rgb': {
            ..._safeFields(s.json['status_rgb'] as Map, [
              'revision',
              'available',
              'have_output',
              'effect',
              'brightness',
            ]),
            if ((s.json['status_rgb'] as Map)['rgb'] is List &&
                ((s.json['status_rgb'] as Map)['rgb'] as List).length == 3 &&
                ((s.json['status_rgb'] as Map)['rgb'] as List).every(
                  (v) => v is int && v >= 0 && v <= 255,
                ))
              'rgb': (s.json['status_rgb'] as Map)['rgb'],
          },
      },
    };
  }

  static Map<String, dynamic> _safeFields(Map value, List<String> keys) => {
    for (final key in keys)
      if (value[key] is bool ||
          value[key] is num ||
          (value[key] is String && (value[key] as String).length <= 128))
        key: value[key],
  };

  void _boardChanged(DeviceStatus value, int generation) {
    if (!running || closed || generation != epoch) return;
    if (value.json['board_id'] != peerBoard ||
        value.json['boot_id'] != peerBoot) {
      unawaited(stop());
      _failure('Peer changed; bridge stopped and requests discarded');
      return;
    }
    pendingStatus = value;
    liveFlush ??= Timer(Duration.zero, () {
      liveFlush = null;
      final latest = pendingStatus;
      pendingStatus = null;
      if (latest != null && generation == epoch && running && !closed) {
        telemetry(latest, true);
      }
    });
  }

  void telemetry([DeviceStatus? value, bool changesOnly = false]) {
    if (!running || closed) return;
    try {
      final observation = snapshot(value);
      final signature = jsonEncode(observation['board']);
      if (changesOnly && signature == lastBoardSnapshot) return;
      lastBoardSnapshot = signature;
      _emit(observation);
    } on Object {
      unawaited(stop());
      _failure('Peer expired/changed; bridge stopped and requests discarded');
    }
  }

  Map<String, dynamic> _emit(Map<String, dynamic> value) {
    final event = journal.add(value);
    final encoded = jsonEncode(event);
    _publish(
      '${options!.topic}/${value['type'] == 'command-result' ? 'reply' : 'telemetry'}',
      event,
    );
    if (webSocketConnected) {
      try {
        remote!.add(encoded);
      } on Object {
        _failure('Remote WebSocket publish failed; nothing queued or replayed');
      }
    }
    for (final socket in localSockets.toList()) {
      if (socket.readyState == WebSocket.open) {
        try {
          socket.add(encoded);
        } on Object {
          localSockets.remove(socket);
          unawaited(socket.close());
        }
      }
    }
    sent++;
    if (!closed) notifyListeners();
    return event;
  }

  Map<String, dynamic> _result(
    int generation,
    String? id,
    bool accepted,
    String outcome, {
    Map<String, dynamic> extra = const {},
    required String? session,
    required String? board,
    required String? boot,
  }) {
    final value = <String, dynamic>{
      'type': 'command-result',
      'id': id,
      'request_id': id, // Correlation alias, not a second dispatch identifier.
      'bridge_session': session,
      'board_id': board,
      'boot_id': boot,
      'accepted': accepted,
      'code': accepted
          ? 'accepted'
          : outcome.startsWith('uncertain')
          ? 'outcome_uncertain'
          : 'rejected',
      'outcome': outcome,
      'replayed': false,
      ...extra,
    };
    return generation == epoch && running && !closed ? _emit(value) : value;
  }

  void _publish(
    String topic,
    Map<String, dynamic> value, {
    bool retain = false,
  }) {
    if (!mqttConnected) return;
    try {
      final builder = MqttClientPayloadBuilder()..addString(jsonEncode(value));
      mqtt!.publishMessage(
        topic,
        options!.qos == 1 ? MqttQos.atLeastOnce : MqttQos.atMostOnce,
        builder.payload!,
        retain: retain,
      );
    } on Object {
      _failure('MQTT publish failed; not queued or replayed');
    }
  }

  Future<Map<String, dynamic>> _incoming(
    String raw, {
    bool retained = false,
    required int generation,
    String? clientVersion,
  }) async {
    received++;
    String? id;
    var dispatched = false;
    final requestSession = admission?.session,
        requestBoard = peerBoard,
        requestBoot = peerBoot;
    Map<String, dynamic> result(
      bool accepted,
      String outcome, {
      Map<String, dynamic> extra = const {},
    }) => _result(
      generation,
      id != null && id.length <= 64 ? id : null,
      accepted,
      outcome,
      extra: extra,
      session: requestSession,
      board: requestBoard,
      boot: requestBoot,
    );
    try {
      if (!running || closed || generation != epoch || raw.length > 8192) {
        throw const StaleConnection();
      }
      final decoded = jsonDecode(raw);
      if (decoded is! Map<String, dynamic>) {
        throw const FormatException('Invalid request');
      }
      // A remote repeater may reflect observations; they are never commands
      // and must not produce another result that could circulate indefinitely.
      if (decoded['type'] == 'telemetry' ||
          decoded['type'] == 'command-result') {
        return {'accepted': false, 'code': 'observation_not_command'};
      }
      id = decoded['id'] is String ? decoded['id'] : null;
      if (busy || uncertain) throw const StaleConnection();
      final peer = decoded['peer'];
      validatePeerApp(
        peer is Map
            ? peer['version']
            : decoded['client_version'] ?? clientVersion,
        enabled: enforceVersions,
      );
      final s = current();
      admission!.admit(decoded, s, retained: retained);
      if (peer != null) peerRegistry.observe(peer);
      busy = true;
      try {
        if (decoded['type'] == 'command' && options!.commands) {
          final path = decoded['path'], rawValues = decoded['values'];
          if (path is! String ||
              rawValues is! Map ||
              rawValues.length > 16 ||
              rawValues.keys.any((k) => k is! String) ||
              rawValues.values.any((v) => v is! String || v.length > 512)) {
            throw const FormatException('Invalid command');
          }
          final values = Map<String, String>.from(rawValues);
          if (!s.peer.allows(path, values)) {
            throw const FormatException('Unacknowledged capability or path');
          }
          if (s.peer.role == 'first-wins' && commandNames.containsKey(path)) {
            if (!s.peer.has('command-ack') ||
                (path == '/api/game/reset' && !s.peer.has('reset-ack'))) {
              throw const FormatException('Correlated game command unavailable');
            }
            dispatched = true;
            final actual = await api.fastCommand(path, values,
                authorize: () => generation == epoch && running &&
                    !closed && current().peer.allows(path, values));
            if (actual == null || !commandReadbackMatches(actual, path, values)) {
              throw const OperationUncertain(
                'Board command readback unavailable; no intent replayed.',
              );
            }
            if (generation != epoch) {
              return result(false, 'uncertain: bridge session retired after dispatch');
            }
            current();
            return result(true, 'Correlated board receipt and matching state readback',
                extra: {'board': snapshot(actual)['board']});
          }
          dispatched = true;
          await api.post(path, values);
          if (generation != epoch) {
            return result(
              false,
              'uncertain: bridge session retired after dispatch',
            );
          }
          current();
          return result(
            true,
            'HTTP acknowledged; peripheral completion is separate',
          );
        }
        if (decoded['type'] == 'firmware-stage' && options!.firmwareStage) {
          if (decoded['url'] is! String ||
              decoded['sha256'] is! String ||
              decoded['targets'] is! int) {
            throw const FormatException('Invalid pinned firmware request');
          }
          final controller = FirmwareController(
            api,
            authorized: (f) => current().peer.has(f),
          );
          try {
            dispatched = true;
            final job = await controller.stage(
              decoded['url'],
              decoded['sha256'],
              decoded['targets'],
              '',
            );
            if (generation != epoch) {
              return result(
                false,
                'uncertain: bridge session retired after dispatch',
              );
            }
            current();
            return result(
              true,
              'Staging only. Apply requires the firmware page confirmation.',
              extra: {
                'job': _safeFields(job.json, [
                  'job_id',
                  'boot_id',
                  'manifest_sha256',
                  'phase',
                  'targets',
                  'busy',
                  'cancellable',
                  'received_bytes',
                  'expected_bytes',
                ]),
              },
            );
          } finally {
            controller.dispose();
          }
        }
        throw const FormatException(
          'Request type disabled or unsupported; no arbitrary proxy',
        );
      } finally {
        busy = false;
      }
    } on OperationUncertain {
      uncertain = true;
      _failure(
        'Command outcome uncertain. Inspect board before restarting bridge.',
      );
      return result(false, 'uncertain');
    } on DeviceCommandRejected {
      rejected++;
      return result(false, 'Board rejected command; nothing replayed');
    } on Object {
      if (dispatched) {
        uncertain = true;
        _failure(
          'Command outcome uncertain. Inspect board before restarting bridge.',
        );
        return result(false, 'uncertain');
      }
      rejected++;
      if (!closed) notifyListeners();
      return result(
        false,
        'Rejected by session/identity/deadline/capability policy; nothing replayed',
      );
    }
  }

  Future<void> _local(HttpRequest r, int generation) async {
    identifyResponse(r.response);
    try {
      if (generation != epoch ||
          !running ||
          r.headers.value('authorization') != 'Bearer $token' ||
          r.headers.value('origin') != null) {
        r.response.statusCode = 403;
        await r.response.close();
        return;
      }
      if (r.method == 'GET' && r.uri.path == '/api/peer/info') {
        r.response.headers.contentType = ContentType.json;
        r.response.write(
          jsonEncode({
            'product': 'PakarController',
            'kind': 'android-app',
            'version': PeerIdentity.current.version,
            'build': PeerIdentity.current.build,
            'compatibility': peerCompatibility(enabled: enforceVersions),
            'peer': PeerIdentity.current.report(enforce: enforceVersions),
          }),
        );
        await r.response.close();
        return;
      }
      validatePeerApp(
        r.headers.value('x-pakar-version'),
        enabled: enforceVersions,
      );
      if (r.method == 'GET' && r.uri.path == '/api/clients') {
        r.response.headers.contentType = ContentType.json;
        r.response.write(jsonEncode({'clients': peerRegistry.peers}));
        await r.response.close();
        return;
      }
      if (r.method == 'GET' &&
          r.uri.path == '/ws' &&
          WebSocketTransformer.isUpgradeRequest(r)) {
        if (localSockets.length >= 8) {
          r.response.statusCode = 429;
          await r.response.close();
          return;
        }
        final s = await WebSocketTransformer.upgrade(r);
        if (generation != epoch || !running) {
          await s.close();
          return;
        }
        localSockets.add(s);
        s.pingInterval = const Duration(seconds: 20);
        s.listen(
          (m) {
            if (m is String && m.length <= 8192) {
              unawaited(
                _incoming(
                  m,
                  generation: generation,
                  clientVersion: r.headers.value('x-pakar-version'),
                ),
              );
            }
          },
          onDone: () => localSockets.remove(s),
          onError: (Object _) => localSockets.remove(s),
        );
        s.add(jsonEncode(snapshot()));
        return;
      }
      if (r.method == 'GET' &&
          r.uri.path == '/api/firmware/download' &&
          options!.firmwareStage) {
        final target = r.uri.queryParameters['target'];
        if (!{'esp', 'avr', 'eeprom'}.contains(target)) {
          throw const FormatException('Explicit firmware target required');
        }
        current();
        final controller = FirmwareController(
          api,
          authorized: (f) => current().peer.has(f),
        );
        try {
          final bytes = target == 'esp'
              ? await controller.espBytes()
              : await controller.backupBytes(target == 'eeprom');
          current();
          if (generation != epoch || !running) throw const StaleConnection();
          r.response.headers.contentType = ContentType.binary;
          r.response.headers.set('Cache-Control', 'no-store');
          r.response.contentLength = bytes.length;
          r.response.add(bytes);
          await r.response.close();
          return;
        } finally {
          controller.dispose();
        }
      }
      r.response.headers.contentType = ContentType.json;
      r.response.headers.set('Cache-Control', 'no-store');
      Map<String, dynamic> value;
      if (r.method == 'GET' && r.uri.path == '/api/status') {
        value = snapshot();
      } else if (r.method == 'GET' && r.uri.path == '/api/events') {
        current();
        final cursor = int.tryParse(r.uri.queryParameters['after'] ?? '0');
        if (cursor == null ||
            r.uri.queryParameters['bridge_session'] != admission!.session) {
          throw const FormatException(
            'Current bridge session and cursor required',
          );
        }
        value = {
          'board_id': peerBoard,
          'boot_id': peerBoot,
          'bridge_session': admission!.session,
          ...journal.since(cursor),
        };
      } else if (r.method == 'POST' && r.uri.path == '/api/request') {
        final elapsed = Stopwatch()..start();
        final chunks = <int>[];
        final reader = StreamIterator(r);
        try {
          while (true) {
            final remaining = const Duration(seconds: 8) - elapsed.elapsed;
            if (remaining <= Duration.zero) {
              throw TimeoutException('Request deadline');
            }
            if (!await reader.moveNext().timeout(remaining)) break;
            if (chunks.length + reader.current.length > 8192) {
              throw const FormatException('Request limit');
            }
            chunks.addAll(reader.current);
          }
        } finally {
          await reader.cancel();
        }
        if (generation != epoch || !running) {
          throw const StaleConnection();
        }
        value = await _incoming(
          utf8.decode(chunks),
          generation: generation,
          clientVersion: r.headers.value('x-pakar-version'),
        );
      } else {
        r.response.statusCode = 404;
        value = {'accepted': false};
      }
      r.response.write(jsonEncode(value));
      await r.response.close();
    } on Object catch (error) {
      r.response.statusCode = error is UnsupportedPeerVersion ? 426 : 400;
      await r.response.close();
    }
  }

  Future<void> stop() async {
    if (running && options != null) {
      _publish('${options!.topic}/availability', {
        'online': false,
        'bridge_session': admission?.session,
        'board_id': peerBoard,
        'boot_id': peerBoot,
      }, retain: true);
    }
    ++epoch;
    running = false;
    timer?.cancel();
    watchdog?.cancel();
    liveFlush?.cancel();
    liveFlush = null;
    pendingStatus = null;
    watchdog = null;
    timer = null;
    final m = mqtt, w = remote, h = server;
    mqtt = null;
    subscriptionAcknowledged = false;
    remote = null;
    server = null;
    m?.disconnect();
    await mqttEvents?.cancel();
    await remoteEvents?.cancel();
    await boardEvents?.cancel();
    await rgbEvents?.cancel();
    boardEvents = rgbEvents = null;
    mqttEvents = remoteEvents = null;
    for (final s in localSockets.toList()) {
      await s.close();
    }
    localSockets.clear();
    await w?.close();
    await h?.close(force: true);
    token = '';
    state = 'Stopped';
    if (!closed) notifyListeners();
  }

  @override
  void dispose() {
    closed = true;
    unawaited(stop());
    super.dispose();
  }
}
