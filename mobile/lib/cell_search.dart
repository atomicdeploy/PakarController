import 'dart:convert';

import 'package:flutter/services.dart';

// Adapted from RayanLamp internal/i18n/digits.go ShapeLatinDigits.
// Input-only normalization: never shape hostnames, passwords or peer names.
String latinDigits(String text) => String.fromCharCodes(
  text.runes.map((rune) {
    if (rune >= 0x06f0 && rune <= 0x06f9) return rune - 0x06f0 + 48;
    if (rune >= 0x0660 && rune <= 0x0669) return rune - 0x0660 + 48;
    return rune;
  }),
);

/// Normalize only the cell-expression alphabet, never arbitrary application
/// text. Reject an invalid edit whole: deleting letters from `1a2` would silently
/// select cell12. Preserve partial ranges/lists and composing IME text while
/// editing; semantic sort/dedup/range compaction belongs to submit/focus loss.
String normalizeCellSearchInput(String text) {
  var value = latinDigits(text)
      .replaceAllMapped(
        RegExp('[０-９]'),
        (m) => '${m[0]!.codeUnitAt(0) - 0xff10}',
      )
      .replaceAll(RegExp('[‐‑‒–—−﹣－]'), '-')
      .replaceAll(RegExp('[،，;؛]'), ',');
  if (!RegExp(r'^[0-9#,\s-]*$').hasMatch(value)) {
    throw const FormatException('Use numbers, dashes and commas only');
  }
  value = value.replaceAllMapped(RegExp(r'(^|[,\s-])#(?=[0-9])'), (m) => m[1]!);
  if (value.contains('#')) {
    throw const FormatException('Use numbers, dashes and commas only');
  }
  return value
      .replaceAll(RegExp(r'\s*-\s*'), '-')
      .replaceAll(RegExp(r'\s*,\s*'), ',')
      .replaceAll(RegExp(r'\s+'), ',')
      .replaceAll(RegExp(r',+'), ',');
}

class CellSearchInputFormatter extends TextInputFormatter {
  const CellSearchInputFormatter();

  @override
  TextEditingValue formatEditUpdate(
    TextEditingValue oldValue,
    TextEditingValue newValue,
  ) {
    if (newValue.composing.isValid && !newValue.composing.isCollapsed) {
      return newValue;
    }
    try {
      final text = normalizeCellSearchInput(newValue.text);
      if (text.length > 256) return oldValue;
      int offset(int position) {
        if (position < 0) return text.length;
        return normalizeCellSearchInput(newValue.text.substring(0, position))
            .length
            .clamp(0, text.length);
      }

      return newValue.copyWith(
        text: text,
        selection: TextSelection(
          baseOffset: offset(newValue.selection.baseOffset),
          extentOffset: offset(newValue.selection.extentOffset),
          affinity: newValue.selection.affinity,
          isDirectional: newValue.selection.isDirectional,
        ),
        composing: TextRange.empty,
      );
    } on FormatException {
      return oldValue;
    }
  }
}

// Actual desktop/src/ui/cell_search.rs parse/format_selection contract.
// Searching changes the shared selection only, never physical output.
Set<int> parseCellSearch(String text) {
  if (utf8.encode(text).length > 256) {
    throw const FormatException('Search is too long');
  }
  final normalized = normalizeCellSearchInput(text);
  final cells = <int>{};
  int number(String text) {
    final value = text.startsWith('#') ? text.substring(1) : text;
    final id = RegExp(r'^[0-9]+$').hasMatch(value) ? int.tryParse(value) : null;
    if (id == null || id < 1 || id > 80) {
      throw const FormatException('Use cell IDs 1–80 or ranges such as 20–28');
    }
    return id;
  }

  for (final token in normalized.split(RegExp(r'[,;\s]'))) {
    if (token.isEmpty) continue;
    final split = token.indexOf('-');
    if (split < 0) {
      cells.add(number(token));
    } else {
      final start = number(token.substring(0, split));
      final end = number(token.substring(split + 1));
      if (start > end) {
        throw const FormatException('Range start must not exceed its end');
      }
      cells.addAll(Iterable.generate(end - start + 1, (i) => start + i));
    }
  }
  if (text.trim().isNotEmpty && cells.isEmpty) {
    throw const FormatException('Enter a cell ID or range');
  }
  return cells;
}

String formatCellSelection(Set<int> selection) {
  final cells = selection.toList()..sort();
  final parts = <String>[];
  for (var i = 0; i < cells.length; ++i) {
    final start = cells[i];
    var end = start;
    while (i + 1 < cells.length && cells[i + 1] == end + 1) {
      end = cells[++i];
    }
    parts.add(start == end ? '$start' : '$start-$end');
  }
  return parts.join(', ');
}
