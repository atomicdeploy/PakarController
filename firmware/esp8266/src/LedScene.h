#pragma once
#include "LedGrid.h"
#include "SceneStore.h"

inline SceneState captureLedScene(const SceneState &previous, const LedGrid &grid,
    uint8_t mode, uint8_t selectedCell, uint8_t selectedState, uint8_t winner) {
  SceneState next = previous;
  next.mode = mode; next.pattern = grid.pattern(); next.brightness = grid.brightness();
  next.flags = (grid.outputEnabled() ? 1 : 0) | (grid.staticMode() ? 2 : 0) | (grid.redLock() ? 4 : 0);
  next.selectedCell = selectedCell; next.selectedState = selectedState; next.winner = winner;
  // Game/chase frames are not flash edits. Raw pixels never change state(id).
  if (mode != BoardCore::Protocol::kModeSnake && grid.pattern() != LedGrid::kPatternCellChase)
    for (uint8_t id = 1; id <= LedGrid::kCells; ++id) next.cells[id-1] = grid.state(id);
  return next;
}
inline void restoreLedScene(LedGrid &grid, const SceneState &saved) {
  grid.setRedLock(false);
  grid.setBrightness(saved.brightness); grid.setStaticMode(saved.flags & 2);
  for (uint8_t id = 1; id <= LedGrid::kCells; ++id)
    grid.setCell(id, static_cast<BoardCore::Protocol::CellState>(saved.cells[id-1]),
                 BoardCore::Protocol::kEffectImmediate);
  grid.setRedLock(saved.flags & 4);
  grid.setPattern(static_cast<LedGrid::Pattern>(saved.pattern));
  grid.setOutputEnabled(PAKAR_LED_START_ENABLED && (saved.flags & 1));
}
