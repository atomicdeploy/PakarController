/// Product compatibility is an admission policy, never an API selector.
/// Unknown additive JSON/capability fields remain tolerated.
const enforcePeerVersions = bool.fromEnvironment(
  'PAKAR_ENFORCE_PEER_VERSIONS',
  defaultValue: true,
);
const minimumPeerApp = '0.2.8', minimumPeerBoard = '0.2.8';
const peerVersionChecksEnabled = enforcePeerVersions;

class UnsupportedPeerVersion implements Exception {
  const UnsupportedPeerVersion(this.message);
  final String message;
  @override
  String toString() => message;
}

class PeerVersion implements Comparable<PeerVersion> {
  PeerVersion._(this.parts, this.pre);
  final List<int> parts;
  final List<String> pre;
  static PeerVersion? parse(Object? raw) {
    if (raw is! String || raw.length > 63) return null;
    final match = RegExp(
      r'^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$',
    ).firstMatch(raw);
    if (match == null || match[0] != raw) return null;
    final suffix = raw.indexOf(RegExp(r'[-+]'));
    if (suffix >= 0 && raw.length - suffix - 1 > 63) return null;
    final parts = [for (var i = 1; i <= 3; i++) int.tryParse(match[i]!)];
    final pre = match[4]?.split('.') ?? <String>[];
    if (parts.any((p) => p == null || p > 0xffffffff) ||
        pre.any((p) => RegExp(r'^0[0-9]+$').hasMatch(p))) {
      return null;
    }
    return PeerVersion._(parts.cast<int>(), pre);
  }

  @override
  int compareTo(PeerVersion other) {
    for (var i = 0; i < 3; i++) {
      final order = parts[i].compareTo(other.parts[i]);
      if (order != 0) return order;
    }
    if (pre.isEmpty || other.pre.isEmpty) {
      return pre.isEmpty ? (other.pre.isEmpty ? 0 : 1) : -1;
    }
    for (var i = 0; i < pre.length && i < other.pre.length; i++) {
      final a = pre[i], b = other.pre[i];
      final an = RegExp(r'^[0-9]+$').hasMatch(a) ? BigInt.parse(a) : null;
      final bn = RegExp(r'^[0-9]+$').hasMatch(b) ? BigInt.parse(b) : null;
      final order = an != null && bn != null
          ? an.compareTo(bn)
          : an != null
          ? -1
          : bn != null
          ? 1
          : a.compareTo(b);
      if (order != 0) return order;
    }
    return pre.length.compareTo(other.pre.length);
  }
}

Map<String, dynamic> peerCompatibility({bool enabled = enforcePeerVersions}) =>
    {
      'enforced': enabled,
      'minimum_app': minimumPeerApp,
      'minimum_board': minimumPeerBoard,
    };

void _range(Object? value, Object? minimum, Object? maximum, String party) {
  final actual = PeerVersion.parse(value), low = PeerVersion.parse(minimum);
  final high = maximum == null ? null : PeerVersion.parse(maximum);
  if (actual == null ||
      low == null ||
      maximum != null && high == null ||
      high != null && high.compareTo(low) <= 0) {
    throw UnsupportedPeerVersion(
      '$party version or compatibility range is missing/invalid. Update this peer or disable version enforcement in its build.',
    );
  }
  if (actual.compareTo(low) < 0 ||
      high != null && actual.compareTo(high) >= 0) {
    throw UnsupportedPeerVersion(
      '$party $value is unsupported; requires >=$minimum${maximum == null ? '' : ' and <$maximum'}. Update the unsupported peer.',
    );
  }
}

void validatePeerBoard(
  Map<String, dynamic> board, {
  required String appVersion,
  bool enabled = enforcePeerVersions,
}) {
  if (!enabled) return;
  final raw = board['compatibility'];
  if (raw != null && raw is! Map) {
    throw const UnsupportedPeerVersion(
      'Board compatibility policy is invalid.',
    );
  }
  final policy = raw is Map ? raw : const {};
  if (policy.containsKey('enforced') && policy['enforced'] is! bool) {
    throw const UnsupportedPeerVersion(
      'Board compatibility policy is invalid.',
    );
  }
  _range(board['version'], minimumPeerBoard, null, 'Board');
  // A peer may independently disable its own version policy; it cannot disable
  // our board minimum or grant identity/auth/capabilities.
  if (policy['enforced'] != false) {
    _range(
      appVersion,
      policy['minimum_app'] ?? minimumPeerApp,
      policy['maximum_app'],
      'Application',
    );
  }
}

void validatePeerApp(Object? version, {bool enabled = enforcePeerVersions}) {
  if (enabled) _range(version, minimumPeerApp, null, 'Application');
}
