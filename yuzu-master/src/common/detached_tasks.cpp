// SPDX-FileCopyrightText: 2018 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <thread>
#include "common/assert.h"
#include "common/detached_tasks.h"

namespace Common {

DetachedTasks* DetachedTasks::instance = nullptr;

DetachedTasks::DetachedTasks() {
    ASSERT(instance == nullptr);
    instance = this;
}

void DetachedTasks::WaitForAllTasks() {
    std::unique_lock lock{mutex};
    cv.wait(lock, [this]() { return count == 0; });
}

DetachedTasks::~DetachedTasks() {
    WaitForAllTasks();

    std::unique_lock lock{mutex};
    ASSERT(count == 0);
    instance = nullptr;
}

void DetachedTasks::AddTask(std::function<void()> task) {
    std::unique_lock lock{instance->mutex};
    ++instance->count;
    std::thread([task_{std::move(task)}]() {
        task_();
        std::unique_lock thread_lock{instance->mutex};
        --instance->count;
        std::notify_all_at_thread_exit(instance->cv, std::move(thread_lock));
    }).detach();
}

} // namespace Common
