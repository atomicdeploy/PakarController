import 'models.dart';

// Published physical RGB, not a client-generated approximation of the effect.
List<int>? keyPixelGroup(DeviceStatus? status, int key, int group) {
  if (status?.peer.role != 'first-wins' ||
      key < 1 ||
      key > 9 ||
      group < 0 ||
      group > 1) {
    return null;
  }
  final frame = status!.json['pixel_frame'];
  if (frame is! Map || frame['pending'] == true || frame['revision'] is! int) {
    return null;
  }
  final rgb = frame['rgb'], physical = frame['physical'];
  if (rgb is! String ||
      !RegExp(r'^[0-9a-fA-F]{1296}$').hasMatch(rgb) ||
      physical is! List ||
      physical.length != 18 ||
      physical.any((p) => p is! int || p < 1 || p > 18) ||
      physical.toSet().length != 18) {
    return null;
  }
  final layout = status.frameGroups(frame);
  if (layout == null) return null;
  final id = physical[(key - 1) * 2 + group] as int;
  final first = layout.start(id), count = layout.count(id);
  return [
    for (var p = first; p < first + count; p++)
      int.parse(rgb.substring(p * 6, p * 6 + 6), radix: 16),
  ];
}
