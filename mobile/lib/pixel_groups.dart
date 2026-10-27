/// Board-owned physical group spans. Logical names/order never change these
/// addresses. No Flutter dependency: diagnostics CLI uses this contract too.
class PixelGroupLayout {
  PixelGroupLayout._(
    this.revision,
    this.defaultCount,
    this.capacity,
    List<int> counts,
    Map<int, int> overrides,
  ) : counts = List.unmodifiable(counts),
      overrides = Map.unmodifiable(overrides),
      starts = List.unmodifiable(_starts(counts)),
      usedPixels = counts.fold(0, (sum, count) => sum + count);

  final int revision, defaultCount, capacity, usedPixels;
  final List<int> counts, starts;
  final Map<int, int> overrides;
  int get groupCount => counts.length;
  bool get custom => counts.any((count) => count != defaultCount);

  static List<int> _starts(List<int> counts) {
    var next = 0;
    return [
      for (final count in counts)
        (() {
          final start = next;
          next += count;
          return start;
        })(),
    ];
  }

  factory PixelGroupLayout.defaults({bool keys = false}) => PixelGroupLayout._(
    0,
    keys ? 12 : 6,
    keys ? 216 : 480,
    List.filled(keys ? 18 : 80, keys ? 12 : 6),
    const {},
  );

  factory PixelGroupLayout.fromJson(Object? value, {required bool keys}) {
    if (value is! Map) {
      throw const FormatException('Pixel group layout is unavailable');
    }
    final revision = value['revision'], defaultCount = value['default_count'];
    final capacity = keys ? 216 : 480, groups = keys ? 18 : 80;
    final counts = value['counts'];
    final partial = value['partial'] == true;
    final starts =
        partial &&
            !value.containsKey('starts') &&
            counts is List &&
            counts.every((v) => v is int)
        ? _starts(counts.cast<int>())
        : value['starts'];
    if (revision is! int ||
        revision < 0 ||
        revision > 0xffffffff ||
        defaultCount is! int ||
        defaultCount != (keys ? 12 : 6) ||
        value['capacity'] != capacity ||
        value['group_count'] != groups ||
        value['index_base'] != 0 ||
        counts is! List ||
        counts.length != groups ||
        counts.any((v) => v is! int || v < 1 || v > 255) ||
        starts is! List ||
        starts.length != groups ||
        starts.any((v) => v is! int || v < 0) ||
        (!partial || value.containsKey('overrides')) &&
            value['overrides'] is! List) {
      throw const FormatException('Invalid pixel group layout');
    }
    final overrides = <int, int>{};
    for (final row
        in value['overrides'] is List ? value['overrides'] as List : const []) {
      if (row is! Map ||
          row['group'] is! int ||
          row['count'] is! int ||
          (row['group'] as int) < 1 ||
          (row['group'] as int) > groups ||
          (row['count'] as int) < 0 ||
          (row['count'] as int) > 255 ||
          overrides.containsKey(row['group'])) {
        throw const FormatException('Invalid pixel group override');
      }
      overrides[row['group'] as int] = row['count'] as int;
    }
    final result = PixelGroupLayout._(
      revision,
      defaultCount,
      capacity,
      counts.cast<int>(),
      overrides,
    );
    if (result.usedPixels > capacity ||
        value['used_pixels'] != result.usedPixels ||
        !_equal(starts, result.starts) ||
        value['overrides'] is List &&
            List.generate(groups, (index) => index).any((index) {
              final override = overrides[index + 1] ?? 0;
              return counts[index] != (override == 0 ? defaultCount : override);
            })) {
      throw const FormatException('Inconsistent pixel group addresses');
    }
    return result;
  }

  /// A frame owns its capture-time spans, independent of newer status settings.
  static PixelGroupLayout? fromFrame(Object? value, {required bool keys}) {
    if (value is! Map) return null;
    final counts = value['group_counts'], starts = value['group_starts'];
    final revision = value['group_layout_revision'];
    if (counts is! List || starts is! List || revision is! int) return null;
    final defaultCount = keys ? 12 : 6;
    try {
      return PixelGroupLayout.fromJson({
        'revision': revision,
        'default_count': defaultCount,
        'capacity': keys ? 216 : 480,
        'group_count': keys ? 18 : 80,
        'index_base': 0,
        'counts': counts,
        'starts': starts,
        'used_pixels': counts.whereType<int>().fold<int>(
          0,
          (sum, value) => sum + value,
        ),
        'overrides': [
          for (var i = 0; i < counts.length; i++)
            if (counts[i] != defaultCount) {'group': i + 1, 'count': counts[i]},
        ],
      }, keys: keys);
    } on FormatException {
      return null;
    }
  }

  PixelGroupLayout draft(Map<int, int> overrides) {
    final counts = [
      for (var group = 1; group <= groupCount; group++)
        (overrides[group] ?? 0) == 0 ? defaultCount : overrides[group]!,
    ];
    return PixelGroupLayout.fromJson({
      'revision': revision,
      'default_count': defaultCount,
      'capacity': capacity,
      'group_count': groupCount,
      'index_base': 0,
      'counts': counts,
      'starts': _starts(counts),
      'used_pixels': counts.fold(0, (sum, n) => sum + n),
      'overrides': [
        for (final row in overrides.entries)
          {'group': row.key, 'count': row.value},
      ],
    }, keys: groupCount == 18);
  }

  int? groupAt(int pixel) {
    if (pixel < 0 || pixel >= usedPixels) return null;
    for (var i = counts.length - 1; i >= 0; i--) {
      if (pixel >= starts[i]) return i + 1;
    }
    return null;
  }

  int start(int group) => starts[group - 1];
  int count(int group) => counts[group - 1];
  bool span(int index, int count) {
    final group = groupAt(index);
    return index >= 0 &&
        count >= 1 &&
        index + count <= usedPixels &&
        (count == 1 ||
            group != null &&
                start(group) == index &&
                this.count(group) == count);
  }

  bool sameAs(PixelGroupLayout other) =>
      revision == other.revision &&
      capacity == other.capacity &&
      _equal(counts, other.counts) &&
      _equal(starts, other.starts);
  static bool _equal(List a, List b) =>
      a.length == b.length &&
      List.generate(a.length, (i) => i).every((i) => a[i] == b[i]);
  Map<String, dynamic> configuration() => {
    'revision': revision,
    'overrides': [
      for (final row in overrides.entries)
        {'group': row.key, 'count': row.value},
    ],
  };
}
