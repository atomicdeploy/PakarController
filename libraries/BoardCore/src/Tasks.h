#pragma once

#include <Arduino.h>

namespace BoardCore {

using TaskCallback = void (*)(void *context);

class Tasks {
 public:
  Tasks();
  void update(uint32_t now = millis());
  int8_t addTask(uint32_t delayMs, TaskCallback callback,
                 void *context = nullptr);
  void cancelTask(int8_t taskIndex);
  void clear();
  uint8_t count() const;

 private:
  struct TaskItem {
    uint32_t dueAt;
    TaskCallback callback;
    void *context;
    bool active;
  };

  static constexpr uint8_t kMaxTasks = 8;
  TaskItem tasks_[kMaxTasks];
  uint8_t itemCount_ = 0;
};

}  // namespace BoardCore
