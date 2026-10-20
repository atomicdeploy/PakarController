import 'dart:typed_data';

import 'models.dart';

/// Adapted from desktop/pixel_diagnostic.rs and pixel_packet in main.rs.
/// PCController AddressableLeds' clear/stage/show discipline is retained: each
/// deliberate action replaces the entire frame, never patches a stale tail.
const diagnosticPixels = 480;
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
    final value = output is Map ? output['diagnostic'] : null;
    if (value is! Map) return null;
    final revision = value['revision'], index = value['index'];
    final count = value['count'], rgb = value['rgb'];
    if (revision is! int ||
        revision < 0 ||
        revision > 0xffffffff ||
        index is! int ||
        index < 0 ||
        index >= pixels ||
        (count != 1 && count != groupSize) ||
        index + (count as int) > pixels ||
        (count == groupSize && index % groupSize != 0) ||
        rgb is! int ||
        rgb < 0 ||
        rgb > 0xffffff ||
        value['active'] is! bool ||
        value['off'] is! bool ||
        (value['active'] == true && revision == 0)) {
      return null;
    }
    return PixelDiagnosticObservation(
      status.json['board_id'] as String,
      status.json['boot_id'] as String,
      revision,
      value['active'] as bool,
      PixelProbe(index: index, group: count == groupSize, rgb: rgb,
          pixels: pixels, groupSize: groupSize),
      value['off'] as bool,
    );
  }

  String get label => !active
      ? 'Board diagnostic inactive'
      : off
      ? 'Board reports: all ${probe.pixels} pixels off'
      : 'Board reports: ${probe.range} · #${probe.hex}';
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
    if (supportsPixelDiagnostic(status)) {
      final pixels = diagnosticPixelCount(status);
      final groupSize = diagnosticGroupSize(status);
      if (_probe.pixels != pixels || _probe.groupSize != groupSize) {
        _probe = PixelProbe(index: _probe.index.clamp(0, pixels - 1),
            group: _probe.group, rgb: _probe.rgb, pixels: pixels, groupSize: groupSize);
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
      if (retired) notifyListeners();
      return retired;
    }
    observation = next;
    if (next.active && !next.off) {
      _probe = next.probe;
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
    if (!value.valid ||
        (value.index == _probe.index &&
            value.group == _probe.group &&
            value.rgb == _probe.rgb)) {
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
  };
}

bool supportsPixelDiagnostic(DeviceStatus? status) =>
    status != null &&
    status.json['pixels'] == diagnosticPixelCount(status) &&
    status.peer.has('leds') &&
    (status.peer.has('streaming') || status.peer.has('pixel-diagnostic')) &&
    status.json['board_id'] is String &&
    (status.json['board_id'] as String).isNotEmpty &&
    status.json['boot_id'] is String &&
    (status.json['boot_id'] as String).isNotEmpty;

class PixelProbe {
  const PixelProbe({this.index = 0, this.group = false, this.rgb = 0xffffff,
    this.pixels = diagnosticPixels, this.groupSize = 6});
  final int index, rgb, pixels, groupSize;
  final bool group;
  PixelProbe copyWith({int? index, bool? group, int? rgb}) =>
      PixelProbe(index: index ?? this.index, group: group ?? this.group,
          rgb: rgb ?? this.rgb, pixels: pixels, groupSize: groupSize);
  int get count => group ? groupSize : 1;
  int get first => group ? index ~/ groupSize * groupSize : index;
  int get last => first + count - 1;
  bool get valid =>
      (pixels == 480 && groupSize == 6 || pixels == 216 && groupSize == 12) &&
      index >= 0 && index < pixels && last < pixels && rgb >= 0 && rgb <= 0xffffff;
  bool get canPrevious => first > 0;
  bool get canNext => last < pixels - 1;
  PixelProbe move(int direction) {
    if (!valid || ![-1, 1].contains(direction)) {
      throw const FormatException('Invalid pixel move');
    }
    final next = first + direction * count;
    if (next < 0 || next + count > pixels) {
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
        ..setRange(10, 10 + (frame.length ~/ 3 - offset).clamp(0, 240) * 3, frame, offset * 3),
  ];
}

bool diagnosticReady(DeviceStatus status) =>
    supportsPixelDiagnostic(status) &&
    (status.mode == 'cell' || status.peer.role == 'first-wins' && status.mode == 'first-wins') &&
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
