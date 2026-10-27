import 'models.dart';
import 'pixel_corrections_editor.dart';

/// The names/order and physical spans must describe the same observed layout.
/// An absent or malformed advertised layout never falls back to six pixels.
bool pixelCorrectionMapMatches(DeviceStatus? status, CellMap? mapping) {
  if (status == null ||
      !status.peer.table ||
      mapping == null ||
      !mapping.valid) {
    return false;
  }
  final layout = status.pixelGroups;
  return layout != null &&
      (!status.peer.has('pixel-group-layout') ||
          mapping.pixelGroups?.sameAs(layout) == true);
}

/// A rule's address is in the preceding rule's output, NOT a raw scene index.
/// This label projects that slot back through the prior draft rules first. The
/// resulting logical label is approximate: software cannot verify real wiring.
String pixelCorrectionAddress({
  required int wire,
  required int pixels,
  required List<Map<String, dynamic>> precedingRules,
  required bool enabled,
  required bool current,
  required DeviceStatus? status,
  required CellMap? mapping,
}) {
  final prefix = 'Wire ${wire + 1}';
  if (!current || status == null) {
    return '$prefix · logical address not current';
  }
  final layout = status.pixelGroups;
  if (layout == null || layout.capacity != pixels) {
    return '$prefix · logical address unavailable';
  }
  final int raw;
  try {
    raw = pixelCorrectionSources(pixels, enabled, precedingRules)[wire];
  } on FormatException {
    return '$prefix · complete prior rules to locate source';
  } on RangeError {
    return '$prefix · logical address unavailable';
  }
  if (raw < 0) return '$prefix · black from prior rule · no logical source';
  final group = layout.groupAt(raw);
  final source = '$prefix · raw ${raw + 1}';
  if (group == null) return '$source · outside assigned groups';
  final within = raw - layout.start(group) + 1;
  final position = 'pixel $within/${layout.count(group)} · group $group';
  if (status.peer.role == 'first-wins') {
    final box = (group + 1) ~/ 2;
    final part = group.isOdd ? 'button' : 'indicator';
    return '$source · ≈ Box $box $part · $position';
  }
  if (!pixelCorrectionMapMatches(status, mapping)) {
    return '$source · $position · logical cell unavailable';
  }
  final logical = mapping!.physical.indexOf(group);
  if (logical < 0) return '$source · $position · logical cell unavailable';
  final name = mapping.names[logical];
  final label = name == '${logical + 1}' || name.isEmpty
      ? 'Cell ${logical + 1}'
      : 'Cell ${logical + 1} ($name)';
  return '$source · ≈ $label · $position';
}
