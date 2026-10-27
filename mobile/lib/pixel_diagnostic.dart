import 'dart:typed_data';

import 'models.dart';
import 'pixel_groups.dart';

/// Adapted from desktop/pixel_diagnostic.rs and pixel_packet in main.rs.
/// PCController AddressableLeds' stage/show discipline is retained: each
/// deliberate action publishes a complete frame, never patches a stale tail.
/// Capable boards compose temporary diagnostic overlays on their own scene.
const diagnosticPixels = 480;
const diagnosticEffects = ['solid', 'blink', 'breathe', 'rainbow'];
int diagnosticGroupSize(DeviceStatus status) =>
    status.peer.role == 'first-wins' ? 12 : 6;
int diagnosticPixelCount(DeviceStatus status) =>
    status.peer.role == 'first-wins' ? 216 : diagnosticPixels;

/// Board-owned semantic observation, never inferred from a requested cursor.
/// All clients consume this same revision through ordinary WS status pushes.
class PixelDiagnosticObservation {
  PixelDiagnosticObservation(
    this.board,
    this.boot,
    this.revision,
    this.active,
    this.probe,
    this.off,
  );
  final String board, boot;
  final int revision;
  final bool active, off;
  final PixelProbe probe;
  static PixelDiagnosticObservation? read(DeviceStatus status) {
    if (!supportsPixelDiagnostic(status) ||
        !status.peer.has('pixel-diagnostic')) {
      return null;
    }
    final output = status.json['led_output'];
    final pixels = diagnosticPixelCount(status);
    final groupSize = diagnosticGroupSize(status);
    final layout = status.pixelGroups!;
    final value = output is Map ? output['diagnostic'] : null;
    if (value is! Map) return null;
    final revision = value['revision'], index = value['index'];
    final count = value['count'], rgb = value['rgb'];
    if (revision is! int ||
        revision < 0 ||
        revision > 0xffffffff ||
        index is! int ||
        index < 0 ||
        count is! int ||
        !layout.span(index, count) ||
        status.peer.has('pixel-group-layout') &&
            value['group_layout_revision'] != layout.revision ||
        rgb is! int ||
        rgb < 0 ||
        rgb > 0xffffff ||
        value['active'] is! bool ||
        value['off'] is! bool ||
        (value['active'] == true && revision == 0)) {
      return null;
    }
    final extended =
        value.containsKey('effect') || value.containsKey('composite');
    final probe = PixelProbe(
      index: index,
      group: count > 1,
      rgb: rgb,
      pixels: pixels,
      groupSize: groupSize,
      layout: status.peer.has('pixel-group-layout') ? layout : null,
      effect: extended && value['effect'] is String
          ? value['effect'] as String
          : 'solid',
      overlay: value['composite'] == 'overlay',
      periodMs: extended && value['period_ms'] is int
          ? value['period_ms'] as int
          : 1000,
      dutyPercent: extended && value['duty_percent'] is int
          ? value['duty_percent'] as int
          : 50,
      minBrightness: extended && value['min_brightness'] is int
          ? value['min_brightness'] as int
          : 0,
      maxBrightness: extended && value['max_brightness'] is int
          ? value['max_brightness'] as int
          : 255,
      enabled: !extended || value['enabled'] == true,
    );
    if (!probe.valid ||
        extended &&
            (value['effect'] is! String ||
                !['clear', 'overlay'].contains(value['composite']) ||
                value['period_ms'] is! int ||
                value['duty_percent'] is! int ||
                value['min_brightness'] is! int ||
                value['max_brightness'] is! int ||
                value['enabled'] is! bool)) {
      return null;
    }
    return PixelDiagnosticObservation(
      status.json['board_id'] as String,
      status.json['boot_id'] as String,
      revision,
      value['active'] as bool,
      probe,
      value['off'] as bool,
    );
  }

  String get label => !active
      ? 'Board diagnostic inactive'
      : off
      ? probe.overlay
            ? 'Board reports: ${probe.range} masked off'
            : 'Board reports: all ${probe.pixels} pixels off'
      : 'Board reports: ${probe.range} · #${probe.hex}${probe.extended ? ' · ${probe.effectLabel}${probe.overlay ? ' · overlay' : ''}' : ''}';
}

/// Cursor/colour are user-owned workspace state, not a claim about board output.
class PixelDiagnosticWorkspace {
  PixelDiagnosticWorkspace({Map<String, dynamic> saved = const {}}) {
    final index = saved['index'], rgb = saved['rgb'];
    _probe = PixelProbe(
      index: index is int && index >= 0 && index < diagnosticPixels ? index : 0,
      group: saved['group'] == true,
      rgb: rgb is int && rgb >= 0 && rgb <= 0xffffff ? rgb : 0xffffff,
    );
    int bounded(String key, int minimum, int maximum, int fallback) {
      final value = saved[key];
      return value is int && value >= minimum && value <= maximum
          ? value
          : fallback;
    }

    final candidate = _probe.copyWith(
      effect: diagnosticEffects.contains(saved['effect'])
          ? saved['effect'] as String
          : 'solid',
      overlay: saved['overlay'] == true,
      periodMs: bounded('period_ms', 100, 10000, 1000),
      dutyPercent: bounded('duty_percent', 1, 99, 50),
      minBrightness: bounded('min_brightness', 0, 255, 0),
      maxBrightness: bounded('max_brightness', 0, 255, 255),
    );
    _probe = candidate.valid
        ? candidate
        : candidate.copyWith(minBrightness: 0, maxBrightness: 255);
    final text = saved['position'];
    _position = text is String && text.length <= 8
        ? text
        : '${_probe.first + 1}';
    final offset = saved['vertical'];
    _vertical = offset is num && offset.isFinite && offset >= 0
        ? offset.toDouble().clamp(0, 100000)
        : 0;
  }
  PixelProbe _probe = const PixelProbe();
  PixelDiagnosticObservation? observation;
  bool observe(DeviceStatus status) {
    var layoutChanged = false;
    if (supportsPixelDiagnostic(status)) {
      final pixels = diagnosticPixelCount(status);
      final groupSize = diagnosticGroupSize(status);
      final layout = status.peer.has('pixel-group-layout')
          ? status.pixelGroups
          : null;
      if (_probe.pixels != pixels ||
          _probe.groupSize != groupSize ||
          layout != null && _probe.layout == null ||
          layout?.sameAs(
                _probe.layout ?? PixelGroupLayout.defaults(keys: pixels == 216),
              ) ==
              false ||
          layout == null && _probe.layout != null) {
        _probe = _probe.copyWith(
          index: _probe.index.clamp(0, (layout?.usedPixels ?? pixels) - 1),
          pixels: pixels,
          groupSize: groupSize,
          layout: layout,
          clearLayout: layout == null,
        );
        observation = null;
        layoutChanged = true;
        _position = '${_probe.first + 1}';
      }
    }
    // PCController PeerSession._install retires output observations when the
    // selected host/board generation changes, independently of whether the new
    // snapshot contains that peripheral. Keep the user's cursor/draft intact.
    final previous = observation;
    final board = status.json['board_id'], boot = status.json['boot_id'];
    final retired =
        previous != null &&
        board is String &&
        board.isNotEmpty &&
        boot is String &&
        boot.isNotEmpty &&
        (previous.board != board || previous.boot != boot);
    if (retired) observation = null;
    final next = PixelDiagnosticObservation.read(status);
    final old = observation;
    if (next == null ||
        (old != null &&
            old.board == next.board &&
            old.boot == next.boot &&
            next.revision <= old.revision)) {
      if (retired || layoutChanged) notifyListeners();
      return retired || layoutChanged;
    }
    observation = next;
    if (next.active && !next.off) {
      // A one-pixel group has the same wire span as a single-pixel command.
      // Retain the user's group-navigation mode for that ambiguous observation.
      _probe = next.probe.copyWith(
        group:
            next.probe.group ||
            _probe.group && next.probe.currentGroupSize == 1,
      );
      _position = '${_probe.first + 1}';
    }
    notifyListeners();
    return true;
  }

  // Keep this transport module usable by the standalone Dart pixel probe;
  // importing Flutter's ChangeNotifier here would pull dart:ui into that tool.
  final _listeners = <void Function()>{};
  void addListener(void Function() listener) => _listeners.add(listener);
  void removeListener(void Function() listener) => _listeners.remove(listener);
  void notifyListeners() {
    for (final listener in List.of(_listeners)) {
      listener();
    }
  }

  void dispose() => _listeners.clear();
  String _position = '1';
  double _vertical = 0;
  String get position => _position;
  set position(String value) {
    if (value.length > 8 || value == _position) return;
    _position = value;
    notifyListeners();
  }

  double get vertical => _vertical;
  set vertical(double value) {
    if (!value.isFinite || value < 0 || value == _vertical) return;
    _vertical = value.clamp(0, 100000);
    notifyListeners();
  }

  PixelProbe get probe => _probe;
  set probe(PixelProbe value) {
    if (!value.valid || value.sameAs(_probe)) {
      return;
    }
    _probe = value;
    _position = '${value.first + 1}';
    notifyListeners();
  }

  Map<String, dynamic> snapshot() => {
    'index': probe.index,
    'group': probe.group,
    'rgb': probe.rgb,
    'position': position,
    'vertical': vertical,
    if (probe.extended) ...{
      'effect': probe.effect,
      'overlay': probe.overlay,
      'period_ms': probe.periodMs,
      'duty_percent': probe.dutyPercent,
      'min_brightness': probe.minBrightness,
      'max_brightness': probe.maxBrightness,
    },
  };
}

bool supportsPixelDiagnostic(DeviceStatus? status) =>
    status != null &&
    status.json['pixels'] == diagnosticPixelCount(status) &&
    status.pixelGroups != null &&
    status.peer.has('leds') &&
    (status.peer.has('streaming') || status.peer.has('pixel-diagnostic')) &&
    status.json['board_id'] is String &&
    (status.json['board_id'] as String).isNotEmpty &&
    status.json['boot_id'] is String &&
    (status.json['boot_id'] as String).isNotEmpty;

class PixelProbe {
  const PixelProbe({
    this.index = 0,
    this.group = false,
    this.rgb = 0xffffff,
    this.pixels = diagnosticPixels,
    this.groupSize = 6,
    this.layout,
    this.effect = 'solid',
    this.overlay = false,
    this.periodMs = 1000,
    this.dutyPercent = 50,
    this.minBrightness = 0,
    this.maxBrightness = 255,
    this.enabled = true,
  });
  factory PixelProbe.forPhysicalGroup(
    DeviceStatus status, {
    required int physicalGroup,
    bool group = true,
    int rgb = 0xffffff,
  }) {
    final layout = status.pixelGroups;
    if (layout == null ||
        physicalGroup < 1 ||
        physicalGroup > layout.groupCount) {
      throw const FormatException('Physical group is unavailable');
    }
    final result = PixelProbe(
      index: layout.start(physicalGroup),
      group: group,
      rgb: rgb,
      pixels: diagnosticPixelCount(status),
      groupSize: diagnosticGroupSize(status),
      layout: status.peer.has('pixel-group-layout') ? layout : null,
    );
    if (!supportsPixelDiagnostic(status) || !result.valid) {
      throw const FormatException('Physical group is unavailable');
    }
    return result;
  }
  factory PixelProbe.fromCommandFields(
    DeviceStatus status,
    Map<String, String> values,
  ) {
    if (values.containsKey('composite') &&
            !status.peer.has('pixel-diagnostic-overlay') ||
        [
              'effect',
              'period_ms',
              'duty_percent',
              'min_brightness',
              'max_brightness',
              'enabled',
            ].any(values.containsKey) &&
            !status.peer.has('pixel-diagnostic-effects')) {
      throw const FormatException(
        'Diagnostic options are not advertised by this board',
      );
    }
    int number(String key, int fallback) => values.containsKey(key)
        ? int.tryParse(values[key]!) ?? (throw FormatException('Invalid $key'))
        : fallback;
    bool boolean(String key, bool fallback) => values.containsKey(key)
        ? switch (values[key]) {
            'true' => true,
            'false' => false,
            _ => throw FormatException('Invalid $key'),
          }
        : fallback;
    final size = diagnosticGroupSize(status),
        count = number('count', 1),
        index = number('index', 0);
    final layout = status.pixelGroups;
    if (layout == null ||
        !layout.span(index, count) ||
        status.peer.has('pixel-group-layout') &&
            number('group_layout_revision', -1) != layout.revision ||
        values.containsKey('composite') &&
            !['clear', 'overlay'].contains(values['composite'])) {
      throw const FormatException('Invalid diagnostic range or composition');
    }
    final result = PixelProbe(
      index: index,
      group: count > 1,
      rgb: number('rgb', 0xffffff),
      pixels: diagnosticPixelCount(status),
      groupSize: size,
      layout: status.peer.has('pixel-group-layout') ? layout : null,
      effect: values['effect'] ?? 'solid',
      overlay: values['composite'] == 'overlay',
      periodMs: number('period_ms', 1000),
      dutyPercent: number('duty_percent', 50),
      minBrightness: number('min_brightness', 0),
      maxBrightness: number('max_brightness', 255),
      enabled: boolean('enabled', true),
    );
    final problem = result.unavailableReason(status);
    if (problem != null) throw FormatException(problem);
    return result;
  }
  final int index,
      rgb,
      pixels,
      groupSize,
      periodMs,
      dutyPercent,
      minBrightness,
      maxBrightness;
  final bool group, overlay, enabled;
  final String effect;
  final PixelGroupLayout? layout;
  PixelProbe copyWith({
    int? index,
    bool? group,
    int? rgb,
    int? pixels,
    int? groupSize,
    PixelGroupLayout? layout,
    bool clearLayout = false,
    String? effect,
    bool? overlay,
    int? periodMs,
    int? dutyPercent,
    int? minBrightness,
    int? maxBrightness,
    bool? enabled,
  }) => PixelProbe(
    index: index ?? this.index,
    group: group ?? this.group,
    rgb: rgb ?? this.rgb,
    pixels: pixels ?? this.pixels,
    groupSize: groupSize ?? this.groupSize,
    layout: clearLayout ? null : layout ?? this.layout,
    effect: effect ?? this.effect,
    overlay: overlay ?? this.overlay,
    periodMs: periodMs ?? this.periodMs,
    dutyPercent: dutyPercent ?? this.dutyPercent,
    minBrightness: minBrightness ?? this.minBrightness,
    maxBrightness: maxBrightness ?? this.maxBrightness,
    enabled: enabled ?? this.enabled,
  );
  bool sameAs(PixelProbe other) =>
      index == other.index &&
      group == other.group &&
      rgb == other.rgb &&
      pixels == other.pixels &&
      groupSize == other.groupSize &&
      (layout == null && other.layout == null ||
          layout != null &&
              other.layout != null &&
              layout!.sameAs(other.layout!)) &&
      effect == other.effect &&
      overlay == other.overlay &&
      periodMs == other.periodMs &&
      dutyPercent == other.dutyPercent &&
      minBrightness == other.minBrightness &&
      maxBrightness == other.maxBrightness &&
      enabled == other.enabled;
  bool get effectOptions =>
      effect != 'solid' ||
      periodMs != 1000 ||
      dutyPercent != 50 ||
      minBrightness != 0 ||
      maxBrightness != 255 ||
      !enabled;
  bool get extended => effectOptions || overlay;
  String get effectLabel =>
      const {
        'solid': 'Solid',
        'blink': 'Flash',
        'breathe': 'Breathing',
        'rainbow': 'Rainbow',
      }[effect] ??
      effect;
  String? unavailableReason(DeviceStatus? status) {
    if (!valid) return 'Invalid pixel diagnostic values.';
    if (status != null &&
        status.peer.has('pixel-group-layout') &&
        (layout == null || status.pixelGroups?.sameAs(layout!) != true)) {
      return 'Pixel group layout changed or is unavailable. Select the current group again.';
    }
    if (effectOptions && status?.peer.has('pixel-diagnostic-effects') != true) {
      return 'This board does not support diagnostic effects. Choose Solid with default timing, or update its firmware.';
    }
    if (overlay && status?.peer.has('pixel-diagnostic-overlay') != true) {
      return 'This board does not support an overlay mask. Choose Solo, or update its firmware.';
    }
    return null;
  }

  Map<String, String> commandFields(DeviceStatus status, {bool off = false}) {
    final problem = unavailableReason(status);
    if (problem != null) throw FormatException(problem);
    return {
      'index': '$first',
      'count': '$count',
      'rgb': '$rgb',
      'off': '$off',
      if (status.peer.has('pixel-group-layout'))
        'group_layout_revision': '${layout!.revision}',
      if (extended) ...{
        if (status.peer.has('pixel-diagnostic-effects')) ...{
          'effect': effect,
          'period_ms': '$periodMs',
          'duty_percent': '$dutyPercent',
          'min_brightness': '$minBrightness',
          'max_brightness': '$maxBrightness',
          'enabled': '$enabled',
        },
        if (status.peer.has('pixel-diagnostic-overlay'))
          'composite': overlay ? 'overlay' : 'clear',
      },
    };
  }

  int get physicalGroup => layout?.groupAt(index) ?? index ~/ groupSize + 1;
  int get groupFirst =>
      layout?.start(physicalGroup) ?? index ~/ groupSize * groupSize;
  int get currentGroupSize => layout?.count(physicalGroup) ?? groupSize;
  int get groupCount => layout?.groupCount ?? pixels ~/ groupSize;
  int get limit => layout?.usedPixels ?? pixels;
  int get count => group ? currentGroupSize : 1;
  int get first => group ? groupFirst : index;
  int get last => first + count - 1;
  bool get valid =>
      (pixels == 480 && groupSize == 6 || pixels == 216 && groupSize == 12) &&
      (layout == null ||
          layout!.capacity == pixels && layout!.defaultCount == groupSize) &&
      index >= 0 &&
      index < limit &&
      last < limit &&
      rgb >= 0 &&
      rgb <= 0xffffff &&
      diagnosticEffects.contains(effect) &&
      periodMs >= 100 &&
      periodMs <= 10000 &&
      dutyPercent >= 1 &&
      dutyPercent <= 99 &&
      minBrightness >= 0 &&
      minBrightness <= maxBrightness &&
      maxBrightness <= 255;
  bool get canPrevious => first > 0;
  bool get canNext => last < limit - 1;
  PixelProbe move(int direction) {
    if (!valid || ![-1, 1].contains(direction)) {
      throw const FormatException('Invalid pixel move');
    }
    final nextGroup = physicalGroup + direction;
    final next = group && layout != null
        ? nextGroup < 1 || nextGroup > groupCount
              ? -1
              : layout!.start(nextGroup)
        : first + direction * count;
    if (next < 0 || next >= limit) {
      throw const FormatException('End of strip');
    }
    return copyWith(index: next);
  }

  String get range =>
      group ? 'Pixels ${first + 1}–${last + 1}' : 'Pixel ${first + 1}';
  String get hex => rgb.toRadixString(16).padLeft(6, '0').toUpperCase();
  Uint8List frame({bool off = false}) {
    if (!valid) {
      throw FormatException('Pixel outside 1–$pixels or invalid RGB');
    }
    if (extended) {
      throw const FormatException(
        'Diagnostic effects and overlays must be rendered by the board; no raw frame generated',
      );
    }
    final bytes = Uint8List(pixels * 3);
    if (!off) {
      for (var pixel = first; pixel <= last; pixel++) {
        bytes[pixel * 3] = rgb >> 16 & 255;
        bytes[pixel * 3 + 1] = rgb >> 8 & 255;
        bytes[pixel * 3 + 2] = rgb & 255;
      }
    }
    return bytes;
  }
}

/// Animated/overlay output has no single expected RGB frame. Confirm the exact
/// accepted diagnostic specification and its published revision instead, never
/// render animation in the client or infer physical output from an ACK.
bool matchesDiagnosticObservation(
  DeviceStatus actual,
  PixelProbe requested, {
  bool off = false,
  int? afterRevision,
}) {
  final observed = PixelDiagnosticObservation.read(actual);
  if (observed == null ||
      observed.active != requested.enabled ||
      observed.off != off ||
      observed.probe.first != requested.first ||
      observed.probe.count != requested.count ||
      !observed.probe
          .copyWith(index: requested.index, group: requested.group)
          .sameAs(requested)) {
    return false;
  }
  if (afterRevision != null && observed.revision <= afterRevision) return false;
  final frame = actual.json['pixel_frame'];
  return frame is Map && frame['revision'] is int && frame['pending'] == false;
}

List<Uint8List> diagnosticPackets(Uint8List frame) {
  if (frame.length != diagnosticPixels * 3 && frame.length != 216 * 3) {
    throw const FormatException('Incomplete diagnostic frame');
  }
  return [
    for (var offset = 0; offset < frame.length ~/ 3; offset += 240)
      Uint8List(10 + (frame.length ~/ 3 - offset).clamp(0, 240) * 3)
        ..setRange(0, 4, [80, 75, 65, 82]) // PKAR; reserved bytes remain zero.
        ..[6] = offset & 255
        ..[7] = offset >> 8
        ..[8] = (frame.length ~/ 3 - offset).clamp(0, 240)
        ..[9] = offset + 240 >= frame.length ~/ 3 ? 1 : 0
        ..setRange(
          10,
          10 + (frame.length ~/ 3 - offset).clamp(0, 240) * 3,
          frame,
          offset * 3,
        ),
  ];
}

bool diagnosticReady(DeviceStatus status) =>
    supportsPixelDiagnostic(status) &&
    (status.mode == 'cell' ||
        status.peer.role == 'first-wins' && status.mode == 'first-wins') &&
    status.effect == 'none' &&
    status.ledOutputEnabled == true &&
    status.ledOutputMode == 'effects' &&
    status.json['programming_latch'] != true &&
    (status.json['remote_firmware'] as Map?)?['busy'] != true;

bool matchesDiagnosticFrame(DeviceStatus status, Uint8List expected) {
  final frame = status.json['pixel_frame'];
  if (frame is! Map ||
      frame['revision'] is! int ||
      frame['rgb'] is! String ||
      frame['pending'] == true) {
    return false;
  }
  final hex = frame['rgb'] as String;
  if (hex.length != expected.length * 2) return false;
  for (var index = 0; index < expected.length; index++) {
    if (int.tryParse(hex.substring(index * 2, index * 2 + 2), radix: 16) !=
        expected[index]) {
      return false;
    }
  }
  return true;
}
