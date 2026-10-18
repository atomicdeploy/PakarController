import 'dart:typed_data';

// PCController's MCU-owned Silent flag, not the phone's notification volume.
// Keep the complete Pakar record when editing its single flag.
class BoardSound {
  BoardSound._(this.bytes, this.persisted);
  final Uint8List bytes;
  final bool persisted;
  bool get silent => bytes[1] & 1 != 0;
  factory BoardSound.parse(String hex) {
    if (!RegExp(r'^[0-9a-fA-F]{44}$').hasMatch(hex)) {
      throw const FormatException('Malformed AVR settings response');
    }
    final p = Uint8List.fromList([
      for (var i = 0; i < hex.length; i += 2)
        int.parse(hex.substring(i, i + 2), radix: 16),
    ]);
    final period = p[3] | p[4] << 8, threshold = p[5] | p[6] << 8;
    if (p[0] != 0x50 ||
        p[1] & ~7 != 0 ||
        p[2] > 7 ||
        (period != 0 && period < 100) ||
        threshold < 1 ||
        threshold > 1023 ||
        p[7] >= 6 ||
        p[8] == 0 ||
        p[8] & ~63 != 0 ||
        p[8] & (1 << p[7]) == 0 ||
        p[12] > 8 ||
        p[21] > 1) {
      throw const FormatException('Invalid AVR settings');
    }
    var seen = 0;
    for (var rank = 0; rank < 6; rank++) {
      final page = (p[9 + rank ~/ 2] >> (4 * (rank % 2))) & 15;
      if (page >= 6 || seen & (1 << page) != 0) {
        throw const FormatException('Invalid AVR menu order');
      }
      seen |= 1 << page;
    }
    for (var i = 0; i < 8; i++) {
      if (i < p[12] ? p[13 + i] < 32 || p[13 + i] > 126 : p[13 + i] != 0) {
        throw const FormatException('Invalid AVR name');
      }
    }
    return BoardSound._(Uint8List.fromList(p.take(21).toList()), p[21] == 1);
  }
  String withSilent(bool value) {
    final next = Uint8List.fromList(bytes);
    next[1] = (next[1] & ~1) | (value ? 1 : 0);
    return encode(next);
  }

  static String encode(Iterable<int> bytes) =>
      bytes.map((b) => b.toRadixString(16).padLeft(2, '0')).join();
}
