#include "LedScene.h"
#include <cassert>
#include <cstdio>
int main() {
  using namespace BoardCore::Protocol;
  SceneState tableScene;tableScene.mode=1;tableScene.flags=7;tableScene.cells[0]=kCellRed;
  LedGrid grid;grid.begin(false);restoreLedScene(grid,tableScene);
  assert(grid.state(1)==kCellRed);
  assert(grid.redLock()==bool(PAKAR_TABLE_SELECTION));
  if(PAKAR_TABLE_SELECTION) {
    assert(!grid.resetWhite());grid.setRedLock(false);assert(grid.resetWhite());
    grid.showWinner(9);assert(grid.state(80)==kCellWhite); // Other game's renderer is compiled out.
  }else{
    assert(grid.resetWhite()); // A locked table scene never traps first-wins reset.
    grid.setRedLock(true);assert(!grid.redLock());
    grid.showWinner(9);assert(grid.state(80)==kCellRed&&grid.state(1)==kCellOff);
  }
  puts("Baked role renderer: game exclusion and saved table-lock migration passed; host doubles only.");
}
