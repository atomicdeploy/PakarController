#pragma once

#include <Arduino.h>

namespace BoardCore {

using TaskCallback = void (*)(void *context);

// Adapt actual PCController LocalLib/Tasks.h/.cpp; retain Puzzles' ten slots.
// Cooperative loop-only API, not ISR/thread-safe. Callbacks must not block.
class Tasks {
 public:
  Tasks();
  void update(uint32_t now = millis());
  int8_t addTask(uint32_t delayMs, TaskCallback callback,
                 void *context = nullptr, uint32_t now = millis());
  void cancelTask(int8_t taskIndex);
  void clear();
  uint8_t count() const;
  static constexpr uint8_t capacity() { return kMaxTasks; }

 private:
  struct TaskItem {
    uint32_t dueAt;
    TaskCallback callback;
    void *context;
    bool active;
  };

  static constexpr uint8_t kMaxTasks = 10;
  TaskItem tasks_[kMaxTasks];
  uint8_t itemCount_ = 0;
  uint16_t due_ = 0;
  bool updating_ = false;
};

}  // namespace BoardCore
