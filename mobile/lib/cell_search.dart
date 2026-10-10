// Same strict logical-ID grammar as desktop/src/ui/cell_search.rs.
Set<int> parseCellSearch(String text) {
  if (text.trim().isEmpty) return {};
  final matches = <int>{};
  for (final term in text.split(',')) {
    final match = RegExp(r'^\s*(\d+)\s*(?:[-–]\s*(\d+)\s*)?$').firstMatch(term);
    if (match == null) {
      throw const FormatException('Use cell numbers or ranges: 12, 3–8');
    }
    final first = int.parse(match.group(1)!);
    final last = int.parse(match.group(2) ?? match.group(1)!);
    if (first < 1 || last > 80 || last < first) {
      throw const FormatException(
        'Cells must be in ascending ranges from 1 to 80',
      );
    }
    for (var id = first; id <= last; ++id) {
      matches.add(id);
    }
  }
  return matches;
}
