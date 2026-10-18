import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'device_registry.dart';
import 'models.dart';
import 'panel_event.dart';

abstract class ToneOutput {
  Future<void> open(String session, double volume);
  Future<void> note(String session, int frequency, int durationMs);
  Future<void> lease(String session);
  Future<void> stop(String session);
}

/// Frequency-driven PCM, not Android's fixed ToneGenerator cue catalog.
class AndroidToneOutput implements ToneOutput {
  static const channel = MethodChannel('ir.digitalogic.pakar/tone-mirror');
  @override
  Future<void> open(String session, double volume) =>
      channel.invokeMethod('open', {'session': session, 'volume': volume});
  @override
  Future<void> note(String session, int frequency, int durationMs) =>
      channel.invokeMethod('note', {
        'session': session,
        'frequency': frequency,
        'duration_ms': durationMs,
      });
  @override
  Future<void> lease(String session) =>
      channel.invokeMethod('lease', {'session': session});
  @override
  Future<void> stop(String session) =>
      channel.invokeMethod('stop', {'session': session});
}

class MirrorTone {
  const MirrorTone(
    this.frequency,
    this.durationMs,
    this.deviceMicros,
    this.deviceNowMicros, {
    this.muted = false,
  });
  final int frequency, durationMs, deviceMicros, deviceNowMicros;
  final bool muted;
  static MirrorTone? decode(Object? value) {
    if (value is! String || !RegExp(r'^[0-9a-f]{26}$').hasMatch(value)) {
      return null;
    }
    final bytes = Uint8List.fromList(
      List.generate(
        13,
        (i) => int.parse(value.substring(i * 2, i * 2 + 2), radix: 16),
      ),
    );
    final data = ByteData.sublistView(bytes);
    final hz = data.getUint16(0, Endian.little),
        ms = data.getUint16(2, Endian.little);
    if (bytes[4] > 1 || (hz != 0 && (hz < 20 || hz > 20000 || ms == 0))) {
      return null;
    }
    return MirrorTone(
      hz,
      ms,
      data.getUint32(5, Endian.little),
      data.getUint32(9, Endian.little),
      muted: bytes[4] == 1,
    );
  }
}

/// Actual PCController buzzerPlaybackTimeline adaptation: wrap-safe MCU cadence,
/// 50ms playout cushion, bounded skew, absolute end and no expired-note replay.
class ToneTimeline {
  int? _device, _start;
  void clear() {
    _device = _start = null;
  }

  (int, int)? plan(MirrorTone tone, int observedUs) {
    final age = (tone.deviceNowMicros - tone.deviceMicros) & 0xffffffff;
    if (age > 65535000) return null;
    var start = observedUs + 50000 - age;
    if (_device != null) {
      final delta = (tone.deviceMicros - _device!) & 0xffffffff;
      final candidate = _start! + delta;
      if (delta <= 300000000 &&
          candidate >= observedUs - age &&
          (candidate - (observedUs - age)).abs() <= 250000) {
        start = candidate;
      }
    }
    _device = tone.deviceMicros;
    _start = start;
    return (start, start + tone.durationMs * 1000);
  }
}

/// One selected peer, independent mirror domains. No board mutation, no
/// capability inference, no HTTP polling of notes, no replay after reconnect.
class PeripheralMirror extends ChangeNotifier {
  PeripheralMirror({ToneOutput? output, this.now})
    : output = output ?? AndroidToneOutput();
  final ToneOutput output;
  final int Function()? now;
  int get elapsedUs => now?.call() ?? clock.elapsedMicroseconds;
  final clock = Stopwatch()..start();
  final timeline = ToneTimeline();
  final List<Timer> pending = [];
  DeviceStatus? status;
  List<int> segments = const [];
  int brightness = 0;
  final List<PanelEvent> _panelHistory = [];
  List<PanelEvent> get panelHistory => List.unmodifiable(_panelHistory);
  int? _lastPanelEvent, _panelAt;
  Timer? _panelExpiry;
  PanelEvent? get panelPresentation {
    if (status == null || _panelHistory.isEmpty || _panelAt == null) {
      return null;
    }
    final event = _panelHistory.last;
    return event.lifetimeMs > 0 &&
            elapsedUs - _panelAt! < event.lifetimeMs * 1000
        ? event
        : null;
  }

  bool segmentsEnabled = true, panelEnabled = true, buzzerEnabled = false;
  bool followBoardMute = false;
  MirrorTone? _lastTone;
  double volume = .35;
  String? failure, _owner, _session;
  int _lastEvent = 0, _epoch = 0;
  Timer? _expiry;
  bool _closed = false, _foreground = true;
  bool get panelAvailable =>
      status?.avrOnline == true &&
      status?.json['mirroring'] is Map &&
      status!.json['mirroring']['front_panel'] == true &&
      status?.json['front_panel'] is Map;
  bool get segmentsAvailable =>
      status?.avrOnline == true && status?.peer.has('display') == true;
  bool get buzzerAvailable =>
      status?.avrOnline == true &&
      status?.peer.has('buzzer') == true &&
      status?.json['mirroring'] is Map &&
      status!.json['mirroring']['avr_session'] is int &&
      status!.json['mirroring']['avr_session'] > 0 &&
      status!.json['mirroring']['buzzer_timestamps'] == true;
  Map get panel =>
      panelAvailable ? status!.json['front_panel'] as Map : const {};

  void observe(DeviceStatus? value, int revision) {
    final raw = value?.json, manifest = raw?['mirroring'];
    final board = raw?['board_id'], boot = raw?['boot_id'];
    final avr = manifest is Map ? manifest['avr_session'] : null;
    final valid =
        value?.peer.known == true &&
        board is String &&
        board.isNotEmpty &&
        boot is String &&
        boot.isNotEmpty;
    final owner = valid ? '$revision:$board:$boot:$avr' : null;
    if (owner != _owner || owner == null) {
      _silence();
      _panelHistory.clear();
      _lastPanelEvent = _panelAt = null;
      _panelExpiry?.cancel();
      _lastEvent = 0;
      _owner = owner;
    }
    status = valid ? value : null;
    segments = segmentsAvailable ? value!.segments : const [];
    brightness = segmentsAvailable ? value!.segmentBrightness : 0;
    if (!buzzerAvailable && buzzerEnabled) _silence();
    _expiry?.cancel();
    if (status != null) {
      _expiry = Timer(
        const Duration(seconds: 6),
        () => observe(null, revision),
      );
    }
    final session = _session;
    if (session != null && buzzerEnabled) _call(output.lease(session), session);
    if (!_closed) notifyListeners();
  }

  void foreground(bool active) {
    if (_closed) return;
    _foreground = active;
    if (!active) {
      _silence();
      notifyListeners();
    }
  }

  void setSegments(bool value) {
    segmentsEnabled = value;
    notifyListeners();
  }

  void setPanel(bool value) {
    panelEnabled = value;
    notifyListeners();
  }

  Future<void> setBuzzer(bool value) async {
    if (_closed) return;
    _silence();
    failure = null;
    if (!value || !buzzerAvailable || !_foreground || _closed) {
      notifyListeners();
      return;
    }
    final epoch = _epoch, session = randomIdentifier();
    _session = session;
    try {
      await output.open(session, volume);
      if (_closed || epoch != _epoch || !_foreground || !buzzerAvailable) {
        await output.stop(session);
        return;
      }
      buzzerEnabled = true;
      notifyListeners();
    } catch (error) {
      _fail(session, error);
    }
  }

  void setVolume(double value) {
    volume = value.clamp(0, 1);
    // Changing audio configuration invalidates pending playback, never restarts it.
    if (buzzerEnabled || _session != null) _silence();
    notifyListeners();
  }

  void setFollowBoardMute(bool value) {
    if (_closed || value == followBoardMute) return;
    followBoardMute = value;
    _flushNotes(); // A policy change never replays buffered/interrupted notes.
    notifyListeners();
  }

  void _flushNotes() {
    ++_epoch;
    for (final timer in pending) {
      timer.cancel();
    }
    pending.clear();
    timeline.clear();
    final session = _session;
    if (session != null && buzzerEnabled) {
      _call(output.note(session, 0, 0), session);
    }
  }

  void event(Map<String, dynamic> event) {
    final raw = status?.json, manifest = raw?['mirroring'];
    if (event['type'] == 'panel-event') {
      if (_closed ||
          _owner == null ||
          raw == null ||
          event['board_id'] != raw['board_id'] ||
          event['boot_id'] != raw['boot_id']) {
        return;
      }
      final next = PanelEvent.decode(event);
      if (next == null) return;
      final delta = _lastPanelEvent == null
          ? 1
          : (next.id - _lastPanelEvent!) & 0xffffffff;
      if (delta == 0 || delta > 0x7fffffff) return;
      _lastPanelEvent = next.id;
      if (_panelHistory.length == 32) _panelHistory.removeAt(0);
      _panelHistory.add(next);
      _panelAt = elapsedUs;
      _panelExpiry?.cancel();
      if (next.lifetimeMs > 0) {
        _panelExpiry = Timer(Duration(milliseconds: next.lifetimeMs), () {
          if (!_closed) notifyListeners();
        });
      }
      notifyListeners();
      return; // Separate sequence; never affects AVR tone gap/Stop handling.
    }
    if (_closed ||
        _owner == null ||
        raw == null ||
        manifest is! Map ||
        event['board_id'] != raw['board_id'] ||
        event['boot_id'] != raw['boot_id'] ||
        event['avr_session'] != manifest['avr_session']) {
      return;
    }
    final id = event['event_id'];
    if (id is! int || id <= _lastEvent || id > 0xffffffff) return;
    if (_lastEvent != 0 && id != _lastEvent + 1) {
      // A missing ToneStop must not leave old audio running. The first event
      // after subscription may legitimately start at any global sequence ID.
      observe(null, 0);
      return;
    }
    _lastEvent = id;
    final payload = event['payload_hex'];
    if (event['response_opcode'] == 0x9c &&
        segmentsAvailable &&
        payload is String &&
        RegExp(r'^[0-9a-f]{10}$').hasMatch(payload)) {
      final flags = int.parse(payload.substring(8), radix: 16);
      if (flags & 0xc0 != 0xc0) {
        segments = const [];
      } else {
        segments = List.generate(
          4,
          (i) => int.parse(payload.substring(i * 2, i * 2 + 2), radix: 16),
        );
        brightness = flags & 7;
      }
      notifyListeners();
    }
    if (event['response_opcode'] != 0x9d ||
        !buzzerAvailable ||
        !buzzerEnabled ||
        !_foreground) {
      return;
    }
    final tone = MirrorTone.decode(payload), session = _session;
    if (tone == null || session == null) return;
    if (tone.frequency == 0 && tone.durationMs == 0) {
      _lastTone = null;
      _flushNotes();
      return;
    }
    final previous = _lastTone;
    _lastTone = tone;
    if (followBoardMute && tone.muted) {
      _flushNotes();
      return;
    }
    // Mute/settings notifications can repeat the same musical step with a new
    // event ID. They must not restart PCM or revive an interrupted note.
    if (previous != null &&
        previous.deviceMicros == tone.deviceMicros &&
        previous.frequency == tone.frequency &&
        previous.durationMs == tone.durationMs) {
      return;
    }
    final plan = timeline.plan(tone, elapsedUs);
    if (plan == null || plan.$2 <= elapsedUs) return;
    if (pending.length >= 32) {
      _fail(
        session,
        StateError('Tone mirror queue full; stopped without replay'),
      );
      return;
    }
    final epoch = _epoch;
    late final Timer timer;
    timer = Timer(
      Duration(microseconds: (plan.$1 - elapsedUs).clamp(0, 300000)),
      () {
        pending.remove(timer);
        if (_closed ||
            epoch != _epoch ||
            !buzzerEnabled ||
            !buzzerAvailable ||
            !_foreground) {
          return;
        }
        final remaining = plan.$2 - elapsedUs;
        if (remaining <= 0) return;
        _call(
          output.note(session, tone.frequency, (remaining / 1000).ceil()),
          session,
        );
      },
    );
    pending.add(timer);
  }

  void _call(Future<void> operation, String session) {
    unawaited(operation.catchError((Object error) => _fail(session, error)));
  }

  void _fail(String session, Object error) {
    if (_closed || _session != session) return;
    failure = 'Phone audio unavailable: $error';
    _silence();
    notifyListeners();
  }

  void _silence() {
    ++_epoch;
    for (final timer in pending) {
      timer.cancel();
    }
    pending.clear();
    timeline.clear();
    _lastTone = null;
    buzzerEnabled = false;
    final session = _session;
    _session = null;
    if (session != null) {
      unawaited(output.stop(session).catchError((Object _) {}));
    }
  }

  @override
  void dispose() {
    if (_closed) return;
    _closed = true;
    _expiry?.cancel();
    _panelExpiry?.cancel();
    _silence();
    clock.stop();
    super.dispose();
  }
}
