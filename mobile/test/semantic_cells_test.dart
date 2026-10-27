import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/semantic_cells.dart';
import 'package:pakarcontroller/cell_assignments.dart';

void main() {
  test('state identity ignores rendered colors and prefers semantic observation', () {
    expect(semanticCells({'cells': ['red'], 'cell_states': ['won']}), ['won']);
    expect(sameCellState('lost', 'red'), isTrue);
    expect(sameCellState('idle', 'white'), isTrue);
    expect(sameCellState('lost', 'won'), isFalse);
    expect(sameCellState(null, null), isFalse);
    expect(semanticCellState('#ff0000'), isNull);
  });
  test('import normalizes state while preserving stable wire codes', () {
    final value = CellAssignments.fromRows([
      {'cell': 1, 'state': 'lost'}, {'cell': 2, 'state': 'won'},
      {'cell': 3, 'state': 'white'},
    ]);
    expect(value.values, {1: 'lost', 2: 'won', 3: 'idle'});
    expect(value.wireAssignments, [
      {'cell': 1, 'state': 'red'}, {'cell': 2, 'state': 'won'},
      {'cell': 3, 'state': 'white'},
    ]);
  });
}
