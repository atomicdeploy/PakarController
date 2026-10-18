// Presentation only. API IDs remain logical row-major 10x8, and physical
// chain mapping remains the board's independent 80-entry mapping.
const tableDisplayColumns = 20;
const tableDisplayWidth = tableDisplayColumns * 1.0;
// Rows share exact column centers. Only neighboring columns stagger vertically.
const tableDisplayRowPitch = 1.0;
const tableDisplayHeight = 3 * tableDisplayRowPitch + 1.5;

({int column, int row, double x, double y}) tableCellPosition(int id) {
  if (id < 1 || id > 80) throw RangeError.range(id, 1, 80, 'id');
  final slot = id - 1;
  final column = 2 * (slot % 10) + ((slot ~/ 10) % 2);
  final row = slot ~/ 20;
  return (
    column: column,
    row: row,
    x: column.toDouble(),
    // Physical first column is the upper one: cell 1 is never shifted down.
    y: row * tableDisplayRowPitch + (column.isOdd ? .5 : 0.0),
  );
}
