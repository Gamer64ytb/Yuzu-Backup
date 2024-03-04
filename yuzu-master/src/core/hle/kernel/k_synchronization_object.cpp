// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/common_types.h"
#include "common/scratch_buffer.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_scoped_scheduler_lock_and_sleep.h"
#include "core/hle/kernel/k_synchronization_object.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/k_thread_queue.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel {

namespace {

class ThreadQueueImplForKSynchronizationObjectWait final : public KThreadQueueWithoutEndWait {
public:
    ThreadQueueImplForKSynchronizationObjectWait(KernelCore& kernel, KSynchronizationObject** o,
                                                 KSynchronizationObject::ThreadListNode* n, s32 c)
        : KThreadQueueWithoutEndWait(kernel), m_objects(o), m_nodes(n), m_count(c) {}

    void NotifyAvailable(KThread* waiting_thread, KSynchronizationObject* signaled_object,
                         Result wait_result) override {
        // Determine the sync index, and unlink all nodes.
        s32 sync_index = -1;
        for (auto i = 0; i < m_count; ++i) {
            // Check if this is the signaled object.
            if (m_objects[i] == signaled_object && sync_index == -1) {
                sync_index = i;
            }

            // Unlink the current node from the current object.
            m_objects[i]->UnlinkNode(std::addressof(m_nodes[i]));
        }

        // Set the waiting thread's sync index.
        waiting_thread->SetSyncedIndex(sync_index);

        // Set the waiting thread as not cancellable.
        waiting_thread->ClearCancellable();

        // Invoke the base end wait handler.
        KThreadQueue::EndWait(waiting_thread, wait_result);
    }

    void CancelWait(KThread* waiting_thread, Result wait_result, bool cancel_timer_task) override {
        // Remove all nodes from our list.
        for (auto i = 0; i < m_count; ++i) {
            m_objects[i]->UnlinkNode(std::addressof(m_nodes[i]));
        }

        // Set the waiting thread as not cancellable.
        waiting_thread->ClearCancellable();

        // Invoke the base cancel wait handler.
        KThreadQueue::CancelWait(waiting_thread, wait_result, cancel_timer_task);
    }

private:
    KSynchronizationObject** m_objects;
    KSynchronizationObject::ThreadListNode* m_nodes;
    s32 m_count;
};

} // namespace

void KSynchronizationObject::Finalize() {
    this->OnFinalizeSynchronizationObject();
    KAutoObject::Finalize();
}

Result KSynchronizationObject::Wait(KernelCore& kernel, s32* out_index,
                                    KSynchronizationObject** objects, const s32 num_objects,
                                    s64 timeout) {
    // Allocate space on stack for thread nodes.
    std::array<ThreadListNode, Svc::ArgumentHandleCountMax> thread_nodes;

    // Prepare for wait.
    KThread* thread = GetCurrentThreadPointer(kernel);
    KHardwareTimer* timer{};
    ThreadQueueImplForKSynchronizationObjectWait wait_queue(kernel, objects, thread_nodes.data(),
                                                            num_objects);

    {
        // Setup the scheduling lock and sleep.
        KScopedSchedulerLockAndSleep slp(kernel, std::addressof(timer), thread, timeout);

        // Check if the thread should terminate.
        if (thread->IsTerminationRequested()) {
            slp.CancelSleep();
            R_THROW(ResultTerminationRequested);
        }

        // Check if any of the objects are already signaled.
        for (auto i = 0; i < num_objects; ++i) {
            ASSERT(objects[i] != nullptr);

            if (objects[i]->IsSignaled()) {
                *out_index = i;
                slp.CancelSleep();
                R_THROW(ResultSuccess);
            }
        }

        // Check if the timeout is zero.
        if (timeout == 0) {
            slp.CancelSleep();
            R_THROW(ResultTimedOut);
        }

        // Check if waiting was canceled.
        if (thread->IsWaitCancelled()) {
            slp.CancelSleep();
            thread->ClearWaitCancelled();
            R_THROW(ResultCancelled);
        }

        // Add the waiters.
        for (auto i = 0; i < num_objects; ++i) {
            thread_nodes[i].thread = thread;
            thread_nodes[i].next = nullptr;

            objects[i]->LinkNode(std::addressof(thread_nodes[i]));
        }

        // Mark the thread as cancellable.
        thread->SetCancellable();

        // Clear the thread's synced index.
        thread->SetSyncedIndex(-1);

        // Wait for an object to be signaled.
        wait_queue.SetHardwareTimer(timer);
        thread->BeginWait(std::addressof(wait_queue));
        thread->SetWaitReasonForDebugging(ThreadWaitReasonForDebugging::Synchronization);
    }

    // Set the output index.
    *out_index = thread->GetSyncedIndex();

    // Get the wait result.
    R_RETURN(thread->GetWaitResult());
}

KSynchronizationObject::KSynchronizationObject(KernelCore& kernel) : KAutoObjectWithList{kernel} {}

KSynchronizationObject::~KSynchronizationObject() = default;

void KSynchronizationObject::NotifyAvailable(Result result) {
    KScopedSchedulerLock sl(m_kernel);

    // If we're not signaled, we've nothing to notify.
    if (!this->IsSignaled()) {
        return;
    }

    // Iterate over each thread.
    for (auto* cur_node = m_thread_list_head; cur_node != nullptr; cur_node = cur_node->next) {
        cur_node->thread->NotifyAvailable(this, result);
    }
}

std::vector<KThread*> KSynchronizationObject::GetWaitingThreadsForDebugging() const {
    std::vector<KThread*> threads;

    // If debugging, dump the list of waiters.
    {
        KScopedSchedulerLock lock(m_kernel);
        for (auto* cur_node = m_thread_list_head; cur_node != nullptr; cur_node = cur_node->next) {
            threads.emplace_back(cur_node->thread);
        }
    }

    return threads;
}
} // namespace Kernel
