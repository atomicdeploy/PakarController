/// State identity must never be inferred from the currently rendered RGB.
String? semanticCellState(Object? value) => switch (value) {
  'idle' || 'white' => 'idle',
  'lost' || 'red' => 'lost',
  'won' => 'won',
  'off' => 'off',
  _ => null,
};

List<String> semanticCells(Map<String, dynamic> status) {
  final values = status['cell_states'] ?? status['cells'];
  if (values is! List) return const [];
  return [for (final value in values) semanticCellState(value) ?? 'unknown'];
}

bool sameCellState(Object? a, Object? b) {
  final state = semanticCellState(a);
  return state != null && state == semanticCellState(b);
}

/// Only a complete authoritative observation can mark an action as applied.
/// An empty selection, missing cell or rendered RGB is not state confirmation.
bool selectionHasCellState(
  Map<String, dynamic> status,
  Iterable<int> selection,
  String target,
) {
  if (selection.isEmpty || semanticCellState(target) == null) return false;
  final cells = semanticCells(status);
  return selection.every(
    (id) =>
        id > 0 && id <= cells.length && sameCellState(cells[id - 1], target),
  );
}
