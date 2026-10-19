import 'models.dart';
import 'pixel_diagnostic.dart';

/// Presentation only. Like desktop/ui/cell_pixels::Frame, a published sample
/// owns its physical mapping; cell/game state must never masquerade as RGB.
class CellPixelPreview {
  const CellPixelPreview(this.colors, this.description);
  final List<int> colors;
  final String description;
}

CellPixelPreview? cellPixelPreview(
  DeviceStatus? status,
  CellMap mapping,
  int id, {
  bool mappingCurrent = true,
}) {
  if (status == null || !status.peer.table || id < 1 || id > 80) return null;
  final frame = status.json['pixel_frame'];
  if (frame is Map && frame['pending'] != true) {
    final rgb = frame['rgb'], physical = frame['physical'];
    if (frame['revision'] is int &&
        rgb is String &&
        RegExp(r'^[0-9a-fA-F]{2880}$').hasMatch(rgb) &&
        physical is List &&
        physical.length == 80 &&
        physical.every((v) => v is int && v >= 1 && v <= 80) &&
        physical.toSet().length == 80) {
      final first = ((physical[id - 1] as int) - 1) * 6;
      return CellPixelPreview([
        for (var pixel = first; pixel < first + 6; ++pixel)
          int.parse(rgb.substring(pixel * 6, pixel * 6 + 6), radix: 16),
      ], 'Last published RGB · pixels ${first + 1}–${first + 6}');
    }
  }
  // Diagnostics override semantic cell colours. This is explicitly labelled
  // board diagnostic state, rather than claiming an optical/output readback.
  final diagnostic = PixelDiagnosticObservation.read(status);
  if (diagnostic?.active == true) {
    if (!mappingCurrent || !mapping.valid) return null;
    final first = (mapping.physical[id - 1] - 1) * 6;
    return CellPixelPreview([
      for (var pixel = first; pixel < first + 6; ++pixel)
        !diagnostic!.off &&
                pixel >= diagnostic.probe.first &&
                pixel <= diagnostic.probe.last
            ? diagnostic.probe.rgb
            : 0,
    ], 'Board diagnostic · pixels ${first + 1}–${first + 6}');
  }
  // A rainbow/stream/fade is not six copies of the old semantic white cell.
  // Wait for sampled RGB instead of manufacturing an animated preview.
  if (status.mode != 'cell' ||
      status.effect != 'none' ||
      status.ledOutputEnabled == false) {
    return null;
  }
  final color = switch (status.cells.elementAtOrNull(id - 1)) {
    'white' => 0xffffff,
    'red' => 0xff0000,
    'off' => 0,
    _ => null,
  };
  return color == null
      ? null
      : CellPixelPreview(List.filled(6, color), 'Board cell state');
}
