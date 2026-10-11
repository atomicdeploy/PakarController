// Actual production scheduler, clock double; not AVR physical timing/reset proof.
#include "Tasks.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
uint32_t taskTestClock = 0;
using BoardCore::Tasks;
void increment(void *p) { ++*static_cast<unsigned *>(p); }
struct Changes { Tasks *tasks; unsigned calls = 0, later = 0; int8_t victim = -1; };
void replaceLater(void *p) {
  auto &c = *static_cast<Changes *>(p); ++c.calls;
  c.tasks->cancelTask(c.victim);
  assert(c.tasks->addTask(0, increment, &c.later) >= 0);
  c.tasks->update(); // Recursive service cannot consume newly added work.
}
void clearAndAdd(void *p) {
  auto &c = *static_cast<Changes *>(p); ++c.calls;
  c.tasks->clear(); assert(c.tasks->addTask(0, increment, &c.later) == 0);
  c.tasks->update();
}
void refillAndAdd(void *p) {
  auto &c = *static_cast<Changes *>(p); ++c.calls;
  assert(c.tasks->addTask(0, increment, &c.later) == 0);
  assert(c.tasks->addTask(0, increment, &c.later) == 1);
}
int main() {
  Tasks tasks; unsigned calls = 0;
  assert(tasks.count() == 0 && Tasks::capacity() == 10);
  assert(tasks.addTask(0, nullptr) == -1);
  assert(tasks.addTask(0x80000000UL, increment, &calls) == -1);
  assert(tasks.addTask(UINT32_MAX, increment, &calls) == -1);
  assert(tasks.addTask(0, increment, &calls) == 0); // Zero is a valid deadline.
  tasks.update(); tasks.update(); assert(calls == 1 && tasks.count() == 0);
  assert(tasks.addTask(100, increment, &calls) == 0);
  tasks.update(99); assert(calls == 1); tasks.update(100); assert(calls == 2);
  tasks.cancelTask(-1); tasks.cancelTask(10); tasks.cancelTask(127);
  for (uint8_t i = 0; i < Tasks::capacity(); ++i) assert(tasks.addTask(100, increment, &calls) == i);
  assert(tasks.addTask(0, increment, &calls) == -1 && tasks.count() == 10);
  tasks.cancelTask(3); tasks.cancelTask(3); assert(tasks.count() == 9);
  assert(tasks.addTask(0, increment, &calls) == 3); tasks.update(0);
  assert(calls == 3 && tasks.count() == 9); tasks.clear(); assert(tasks.count() == 0);
  taskTestClock = UINT32_MAX - 5;
  assert(tasks.addTask(6, increment, &calls) == 0); // Wrap deadline to zero.
  tasks.update(UINT32_MAX); assert(calls == 3); tasks.update(0); assert(calls == 4);
  taskTestClock = 0;
  assert(tasks.addTask(INT32_MAX, increment, &calls) == 0);
  tasks.update(INT32_MAX - 1); assert(calls == 4);
  tasks.update(INT32_MAX); assert(calls == 5);
  Changes change{&tasks};
  assert(tasks.addTask(0, replaceLater, &change) == 0);
  change.victim = tasks.addTask(0, increment, &calls);
  tasks.update(); assert(change.calls == 1 && change.later == 0 && calls == 5);
  tasks.update(); assert(change.later == 1 && tasks.count() == 0);
  assert(tasks.addTask(0, clearAndAdd, &change) == 0);
  assert(tasks.addTask(0, increment, &calls) == 1);
  tasks.update(); assert(change.calls == 2 && change.later == 1 && calls == 5);
  tasks.update(); assert(change.later == 2 && tasks.count() == 0);
  assert(tasks.addTask(0, refillAndAdd, &change) == 0);
  tasks.update(); assert(change.calls == 3 && change.later == 2 && tasks.count() == 2);
  tasks.update(); assert(change.later == 4 && tasks.count() == 0);
  // Reset-owner use: repeated scheduling retains one delayed callback; entering
  // Prog cancels it rather than replaying when the latch is released.
  int8_t reset = tasks.addTask(100, increment, &calls, 10);
  tasks.cancelTask(reset); reset = tasks.addTask(100, increment, &calls, 20);
  assert(tasks.count() == 1); tasks.update(119); assert(calls == 5);
  tasks.cancelTask(reset); tasks.update(120); tasks.update(200); assert(calls == 5);
  puts("Production Tasks: ten slots, zero/exact/wrapped deadlines, half-range bounds, cancellation, clear, context, nested/replaced/new callback deferral and reset-owner cancellation passed; clock double, not hardware.");
}
