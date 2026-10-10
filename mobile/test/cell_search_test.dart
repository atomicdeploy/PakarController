import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/cell_search.dart';

void main() {
  test('Logical IDs lists ranges and duplicates are exact', () {
    expect(parseCellSearch(''), isEmpty);
    expect(parseCellSearch('12, 3–5, 12'), {3, 4, 5, 12});
    expect(parseCellSearch('1-80').length, 80);
  });
  test('Invalid searches never silently match a different cell', () {
    for (final text in ['0', '81', '8-3', '2--4', '1,', 'physical 12', '-1']) {
      expect(() => parseCellSearch(text), throwsFormatException);
    }
  });
}
