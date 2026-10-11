#include "Tasks.h"

namespace BoardCore {

Tasks::Tasks() { clear(); }

void Tasks::update(uint32_t now) {
  if (itemCount_ == 0 || updating_) {
    return;
  }
  // Snapshot eligibility, not callbacks. cancel/clear must still suppress later
  // due callbacks; newly added/replaced slots wait until the next outer pass.
  updating_ = true;
  due_ = 0;
  for (uint8_t i = 0; i < kMaxTasks; ++i) {
    if (tasks_[i].active && static_cast<int32_t>(now - tasks_[i].dueAt) >= 0)
      due_ |= static_cast<uint16_t>(1U << i);
  }
  for (uint8_t i = 0; i < kMaxTasks; ++i) {
    if (!(due_ & (1U << i))) continue;
    TaskItem &task = tasks_[i];
    const TaskCallback callback = task.callback;
    void *const context = task.context;
    cancelTask(static_cast<int8_t>(i));
    if (callback != nullptr) {
      callback(context);
    }
  }
  due_ = 0;
  updating_ = false;
}

int8_t Tasks::addTask(uint32_t delayMs, TaskCallback callback, void *context, uint32_t now) {
  // Signed deadline comparisons are unambiguous only within half the clock.
  if (callback == nullptr || delayMs > INT32_MAX || itemCount_ >= kMaxTasks) {
    return -1;
  }
  for (uint8_t i = 0; i < kMaxTasks; ++i) {
    if (tasks_[i].active) {
      continue;
    }
    due_ &= static_cast<uint16_t>(~(1U << i));
    tasks_[i].dueAt = now + delayMs;
    tasks_[i].callback = callback;
    tasks_[i].context = context;
    tasks_[i].active = true;
    ++itemCount_;
    return static_cast<int8_t>(i);
  }
  return -1;
}

void Tasks::cancelTask(int8_t taskIndex) {
  if (taskIndex < 0 || taskIndex >= static_cast<int8_t>(kMaxTasks) ||
      !tasks_[taskIndex].active) {
    return;
  }
  tasks_[taskIndex].active = false;
  tasks_[taskIndex].callback = nullptr;
  tasks_[taskIndex].context = nullptr;
  due_ &= static_cast<uint16_t>(~(1U << taskIndex));
  --itemCount_;
}

void Tasks::clear() {
  for (uint8_t i = 0; i < kMaxTasks; ++i) {
    tasks_[i].dueAt = 0;
    tasks_[i].callback = nullptr;
    tasks_[i].context = nullptr;
    tasks_[i].active = false;
  }
  itemCount_ = 0;
  due_ = 0; // Do not release updating_ during callback-driven clear().
}

uint8_t Tasks::count() const { return itemCount_; }

}  // namespace BoardCore
