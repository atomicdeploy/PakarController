import 'dart:typed_data';

import 'models.dart';

/// Adapted from desktop/pixel_diagnostic.rs and pixel_packet in main.rs.
/// PCController AddressableLeds' clear/stage/show discipline is retained: each
/// deliberate action replaces the entire frame, never patches a stale tail.
const diagnosticPixels = 480;

/// Cursor/colour are user-owned workspace state, not a claim about board output.
class PixelDiagnosticWorkspace {
  PixelProbe probe = const PixelProbe();
}

bool supportsPixelDiagnostic(DeviceStatus? status) =>
    status != null &&
    status.json['pixels'] == diagnosticPixels &&
    status.peer.has('leds') &&
    (status.peer.has('streaming') || status.peer.has('pixel-diagnostic')) &&
    status.json['board_id'] is String &&
    (status.json['board_id'] as String).isNotEmpty &&
    status.json['boot_id'] is String &&
    (status.json['boot_id'] as String).isNotEmpty;

class PixelProbe {
  const PixelProbe({this.index = 0, this.group = false, this.rgb = 0xffffff});
  final int index, rgb;
  final bool group;
  int get count => group ? 6 : 1;
  int get first => group ? index ~/ 6 * 6 : index;
  int get last => first + count - 1;
  bool get valid =>
      index >= 0 && index < diagnosticPixels && rgb >= 0 && rgb <= 0xffffff;
  bool get canPrevious => first > 0;
  bool get canNext => last < diagnosticPixels - 1;
  PixelProbe move(int direction) {
    if (!valid || ![-1, 1].contains(direction)) {
      throw const FormatException('Invalid pixel move');
    }
    final next = first + direction * count;
    if (next < 0 || next + count > diagnosticPixels) {
      throw const FormatException('End of strip');
    }
    return PixelProbe(index: next, group: group, rgb: rgb);
  }

  String get range =>
      group ? 'Pixels ${first + 1}–${last + 1}' : 'Pixel ${first + 1}';
  String get hex => rgb.toRadixString(16).padLeft(6, '0').toUpperCase();
  Uint8List frame({bool off = false}) {
    if (!valid) {
      throw const FormatException('Pixel outside 1–480 or invalid RGB');
    }
    final bytes = Uint8List(diagnosticPixels * 3);
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
  if (frame.length != diagnosticPixels * 3) {
    throw const FormatException('Incomplete diagnostic frame');
  }
  return [
    for (final offset in [0, 240])
      Uint8List(10 + 240 * 3)
        ..setRange(0, 4, [80, 75, 65, 82]) // PKAR; reserved bytes remain zero.
        ..[6] = offset & 255
        ..[7] = offset >> 8
        ..[8] = 240
        ..[9] = offset == 240 ? 1 : 0
        ..setRange(10, 730, frame, offset * 3),
  ];
}

bool diagnosticReady(DeviceStatus status) =>
    supportsPixelDiagnostic(status) &&
    status.mode == 'cell' &&
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
