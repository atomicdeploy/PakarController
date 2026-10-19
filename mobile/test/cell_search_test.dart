import 'package:flutter_test/flutter_test.dart';
import 'package:flutter/services.dart';
import 'package:pakarcontroller/cell_search.dart';

void main() {
  test('Logical IDs lists ranges and duplicates are exact', () {
    expect(parseCellSearch(''), isEmpty);
    expect(parseCellSearch('12, 3–5, 12'), {3, 4, 5, 12});
    expect(parseCellSearch('1-80').length, 80);
  });
  test('Invalid searches never silently match a different cell', () {
    for (final text in ['0', '81', '8-3', '2--4', ',', 'physical 12', '-1']) {
      expect(() => parseCellSearch(text), throwsFormatException);
    }
  });
  test('RayanLamp numeric normalization and exact shared range formatting', () {
    expect(parseCellSearch('۱،٢;۳–۵ #۸۰'), {1, 2, 3, 4, 5, 80});
    expect(formatCellSelection(parseCellSearch('1,2,3')), '1-3');
    for (final cells in [
      <int>{},
      {1},
      {2, 3, 4, 12, 80},
      parseCellSearch('1–80'),
    ]) {
      expect(parseCellSearch(formatCellSelection(cells)), cells);
    }
    expect(() => parseCellSearch('1' * 257), throwsFormatException);
    expect(latinDigits('۱۲٣۴ name'), '1234 name');
  });
  test('Input alphabet normalizes localized digits and separators without merging invalid IDs', () {
    expect(normalizeCellSearchInput(' ۰۱ ، ٢ − ۴ ; ８０ '), ',01,2-4,80,');
    expect(
      formatCellSelection(parseCellSearch(' ۰۱ ، ٢ − ۴ ; ８０ ')),
      '1-4, 80',
    );
    expect(formatCellSelection(parseCellSearch('3,01,2,3')), '1-3');
    for (final invalid in ['1a2', '1.2', '1/2', '1#2', '##2']) {
      expect(() => parseCellSearch(invalid), throwsFormatException);
    }
  });
  test(
    'Formatter keeps partial drafts/caret and rejects invalid paste as a whole',
    () {
      const formatter = CellSearchInputFormatter();
      TextEditingValue edit(String value, [int? caret]) => TextEditingValue(
        text: value,
        selection: TextSelection.collapsed(offset: caret ?? value.length),
      );
      final previous = edit('1');
      expect(formatter.formatEditUpdate(previous, edit('1a2')), previous);
      expect(formatter.formatEditUpdate(previous, edit('1-')).text, '1-');
      expect(formatter.formatEditUpdate(previous, edit('1,2,3')).text, '1,2,3');
      final localized = formatter.formatEditUpdate(
        previous,
        edit('۱۲ − ۱۴', 2),
      );
      expect(localized.text, '12-14');
      expect(localized.selection.baseOffset, 2);
      final composing = edit('۱')
          .copyWith(composing: const TextRange(start: 0, end: 1));
      expect(formatter.formatEditUpdate(previous, composing), composing);
      expect(formatter.formatEditUpdate(composing, edit('۱')).text, '1');
      expect(formatter.formatEditUpdate(previous, edit('1' * 257)), previous);
    },
  );
}
