// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/k_light_condition_variable.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_scoped_scheduler_lock_and_sleep.h"
#include "core/hle/kernel/k_thread_queue.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel {

namespace {

class ThreadQueueImplForKLightConditionVariable final : public KThreadQueue {
public:
    ThreadQueueImplForKLightConditionVariable(KernelCore& kernel, KThread::WaiterList* wl,
                                              bool term)
        : KThreadQueue(kernel), m_wait_list(wl), m_allow_terminating_thread(term) {}

    void CancelWait(KThread* waiting_thread, Result wait_result, bool cancel_timer_task) override {
        // Only process waits if we're allowed to.
        if (ResultTerminationRequested == wait_result && m_allow_terminating_thread) {
            return;
        }

        // Remove the thread from the waiting thread from the light condition variable.
        m_wait_list->erase(m_wait_list->iterator_to(*waiting_thread));

        // Invoke the base cancel wait handler.
        KThreadQueue::CancelWait(waiting_thread, wait_result, cancel_timer_task);
    }

private:
    KThread::WaiterList* m_wait_list;
    bool m_allow_terminating_thread;
};

} // namespace

void KLightConditionVariable::Wait(KLightLock* lock, s64 timeout, bool allow_terminating_thread) {
    // Create thread queue.
    KThread* owner = GetCurrentThreadPointer(m_kernel);
    KHardwareTimer* timer{};

    ThreadQueueImplForKLightConditionVariable wait_queue(m_kernel, std::addressof(m_wait_list),
                                                         allow_terminating_thread);

    // Sleep the thread.
    {
        KScopedSchedulerLockAndSleep lk(m_kernel, std::addressof(timer), owner, timeout);

        if (!allow_terminating_thread && owner->IsTerminationRequested()) {
            lk.CancelSleep();
            return;
        }

        lock->Unlock();

        // Add the thread to the queue.
        m_wait_list.push_back(*owner);

        // Begin waiting.
        wait_queue.SetHardwareTimer(timer);
        owner->BeginWait(std::addressof(wait_queue));
    }

    // Re-acquire the lock.
    lock->Lock();
}

void KLightConditionVariable::Broadcast() {
    KScopedSchedulerLock lk(m_kernel);

    // Signal all threads.
    for (auto it = m_wait_list.begin(); it != m_wait_list.end(); it = m_wait_list.erase(it)) {
        it->EndWait(ResultSuccess);
    }
}

} // namespace Kernel
