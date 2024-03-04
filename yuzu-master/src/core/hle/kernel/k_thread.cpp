// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <vector>

#include "common/assert.h"
#include "common/bit_util.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/fiber.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/cpu_manager.h"
#include "core/hardware_properties.h"
#include "core/hle/kernel/k_condition_variable.h"
#include "core/hle/kernel/k_handle_table.h"
#include "core/hle/kernel/k_memory_layout.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_resource_limit.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_scoped_scheduler_lock_and_sleep.h"
#include "core/hle/kernel/k_system_control.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/k_thread_queue.h"
#include "core/hle/kernel/k_worker_task_manager.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_results.h"
#include "core/hle/kernel/svc_types.h"
#include "core/hle/result.h"
#include "core/memory.h"

namespace {

constexpr inline s32 TerminatingThreadPriority = Kernel::Svc::SystemThreadPriorityHighest - 1;

static void ResetThreadContext32(Kernel::Svc::ThreadContext& ctx, u64 stack_top, u64 entry_point,
                                 u64 arg) {
    ctx = {};
    ctx.r[0] = arg;
    ctx.r[15] = entry_point;
    ctx.r[13] = stack_top;
    ctx.fpcr = 0;
    ctx.fpsr = 0;
}

static void ResetThreadContext64(Kernel::Svc::ThreadContext& ctx, u64 stack_top, u64 entry_point,
                                 u64 arg) {
    ctx = {};
    ctx.r[0] = arg;
    ctx.r[18] = Kernel::KSystemControl::GenerateRandomU64() | 1;
    ctx.pc = entry_point;
    ctx.sp = stack_top;
    ctx.fpcr = 0;
    ctx.fpsr = 0;
}
} // namespace

namespace Kernel {

namespace {

struct ThreadLocalRegion {
    static constexpr std::size_t MessageBufferSize = 0x100;
    std::array<u32, MessageBufferSize / sizeof(u32)> message_buffer;
    std::atomic_uint16_t disable_count;
    std::atomic_uint16_t interrupt_flag;
};

class ThreadQueueImplForKThreadSleep final : public KThreadQueueWithoutEndWait {
public:
    explicit ThreadQueueImplForKThreadSleep(KernelCore& kernel)
        : KThreadQueueWithoutEndWait(kernel) {}
};

class ThreadQueueImplForKThreadSetProperty final : public KThreadQueue {
public:
    explicit ThreadQueueImplForKThreadSetProperty(KernelCore& kernel, KThread::WaiterList* wl)
        : KThreadQueue(kernel), m_wait_list(wl) {}

    void CancelWait(KThread* waiting_thread, Result wait_result, bool cancel_timer_task) override {
        // Remove the thread from the wait list.
        m_wait_list->erase(m_wait_list->iterator_to(*waiting_thread));

        // Invoke the base cancel wait handler.
        KThreadQueue::CancelWait(waiting_thread, wait_result, cancel_timer_task);
    }

private:
    KThread::WaiterList* m_wait_list{};
};

} // namespace

KThread::KThread(KernelCore& kernel)
    : KAutoObjectWithSlabHeapAndContainer{kernel}, m_activity_pause_lock{kernel} {}
KThread::~KThread() = default;

Result KThread::Initialize(KThreadFunction func, uintptr_t arg, KProcessAddress user_stack_top,
                           s32 prio, s32 virt_core, KProcess* owner, ThreadType type) {
    // Assert parameters are valid.
    ASSERT((type == ThreadType::Main) || (type == ThreadType::Dummy) ||
           (Svc::HighestThreadPriority <= prio && prio <= Svc::LowestThreadPriority));
    ASSERT((owner != nullptr) || (type != ThreadType::User));
    ASSERT(0 <= virt_core && virt_core < static_cast<s32>(Common::BitSize<u64>()));

    // Convert the virtual core to a physical core.
    const s32 phys_core = Core::Hardware::VirtualToPhysicalCoreMap[virt_core];
    ASSERT(0 <= phys_core && phys_core < static_cast<s32>(Core::Hardware::NUM_CPU_CORES));

    // First, clear the TLS address.
    m_tls_address = {};

    // Next, assert things based on the type.
    switch (type) {
    case ThreadType::Main:
        ASSERT(arg == 0);
        [[fallthrough]];
    case ThreadType::User:
        ASSERT(((owner == nullptr) ||
                (owner->GetCoreMask() | (1ULL << virt_core)) == owner->GetCoreMask()));
        ASSERT(((owner == nullptr) || (prio > Svc::LowestThreadPriority) ||
                (owner->GetPriorityMask() | (1ULL << prio)) == owner->GetPriorityMask()));
        break;
    case ThreadType::HighPriority:
    case ThreadType::Dummy:
        break;
    case ThreadType::Kernel:
        UNIMPLEMENTED();
        break;
    default:
        ASSERT_MSG(false, "KThread::Initialize: Unknown ThreadType {}", static_cast<u32>(type));
        break;
    }
    m_thread_type = type;

    // Set the ideal core ID and affinity mask.
    m_virtual_ideal_core_id = virt_core;
    m_physical_ideal_core_id = phys_core;
    m_virtual_affinity_mask = 1ULL << virt_core;
    m_physical_affinity_mask.SetAffinity(phys_core, true);

    // Set the thread state.
    m_thread_state = (type == ThreadType::Main || type == ThreadType::Dummy)
                         ? ThreadState::Runnable
                         : ThreadState::Initialized;

    // Set TLS address.
    m_tls_address = 0;

    // Set parent and condvar tree.
    m_parent = nullptr;
    m_condvar_tree = nullptr;

    // Set sync booleans.
    m_signaled = false;
    m_termination_requested = false;
    m_wait_cancelled = false;
    m_cancellable = false;

    // Set core ID and wait result.
    m_core_id = phys_core;
    m_wait_result = ResultNoSynchronizationObject;

    // Set priorities.
    m_priority = prio;
    m_base_priority = prio;

    // Initialize sleeping queue.
    m_wait_queue = nullptr;

    // Set suspend flags.
    m_suspend_request_flags = 0;
    m_suspend_allowed_flags = static_cast<u32>(ThreadState::SuspendFlagMask);

    // We're neither debug attached, nor are we nesting our priority inheritance.
    m_debug_attached = false;
    m_priority_inheritance_count = 0;

    // We haven't been scheduled, and we have done no light IPC.
    m_schedule_count = -1;
    m_last_scheduled_tick = 0;
    m_light_ipc_data = nullptr;

    // We're not waiting for a lock, and we haven't disabled migration.
    m_waiting_lock_info = nullptr;
    m_num_core_migration_disables = 0;

    // We have no waiters, but we do have an entrypoint.
    m_num_kernel_waiters = 0;

    // Set our current core id.
    m_current_core_id = phys_core;

    // We haven't released our resource limit hint, and we've spent no time on the cpu.
    m_resource_limit_release_hint = false;
    m_cpu_time = 0;

    // Set debug context.
    m_stack_top = user_stack_top;
    m_argument = arg;

    // Clear our stack parameters.
    std::memset(static_cast<void*>(std::addressof(this->GetStackParameters())), 0,
                sizeof(StackParameters));

    // Set parent, if relevant.
    if (owner != nullptr) {
        // Setup the TLS, if needed.
        if (type == ThreadType::User) {
            R_TRY(owner->CreateThreadLocalRegion(std::addressof(m_tls_address)));
            owner->GetMemory().ZeroBlock(m_tls_address, Svc::ThreadLocalRegionSize);
        }

        m_parent = owner;
        m_parent->Open();
    }

    // Initialize thread context.
    if (m_parent != nullptr && !m_parent->Is64Bit()) {
        ResetThreadContext32(m_thread_context, GetInteger(user_stack_top), GetInteger(func), arg);
    } else {
        ResetThreadContext64(m_thread_context, GetInteger(user_stack_top), GetInteger(func), arg);
    }

    // Setup the stack parameters.
    StackParameters& sp = this->GetStackParameters();
    sp.cur_thread = this;
    sp.disable_count = 1;
    this->SetInExceptionHandler();

    // Set thread ID.
    m_thread_id = m_kernel.CreateNewThreadID();

    // We initialized!
    m_initialized = true;

    // Register ourselves with our parent process.
    if (m_parent != nullptr) {
        m_parent->RegisterThread(this);
        if (m_parent->IsSuspended()) {
            RequestSuspend(SuspendType::Process);
        }
    }

    R_SUCCEED();
}

Result KThread::InitializeThread(KThread* thread, KThreadFunction func, uintptr_t arg,
                                 KProcessAddress user_stack_top, s32 prio, s32 core,
                                 KProcess* owner, ThreadType type,
                                 std::function<void()>&& init_func) {
    // Initialize the thread.
    R_TRY(thread->Initialize(func, arg, user_stack_top, prio, core, owner, type));

    // Initialize emulation parameters.
    thread->m_host_context = std::make_shared<Common::Fiber>(std::move(init_func));

    R_SUCCEED();
}

Result KThread::InitializeDummyThread(KThread* thread, KProcess* owner) {
    // Initialize the thread.
    R_TRY(thread->Initialize({}, {}, {}, DummyThreadPriority, 3, owner, ThreadType::Dummy));

    // Initialize emulation parameters.
    thread->m_stack_parameters.disable_count = 0;

    R_SUCCEED();
}

Result KThread::InitializeMainThread(Core::System& system, KThread* thread, s32 virt_core) {
    R_RETURN(InitializeThread(thread, {}, {}, {}, IdleThreadPriority, virt_core, {},
                              ThreadType::Main, system.GetCpuManager().GetGuestActivateFunc()));
}

Result KThread::InitializeIdleThread(Core::System& system, KThread* thread, s32 virt_core) {
    R_RETURN(InitializeThread(thread, {}, {}, {}, IdleThreadPriority, virt_core, {},
                              ThreadType::Main, system.GetCpuManager().GetIdleThreadStartFunc()));
}

Result KThread::InitializeHighPriorityThread(Core::System& system, KThread* thread,
                                             KThreadFunction func, uintptr_t arg, s32 virt_core) {
    R_RETURN(InitializeThread(thread, func, arg, {}, {}, virt_core, nullptr,
                              ThreadType::HighPriority,
                              system.GetCpuManager().GetShutdownThreadStartFunc()));
}

Result KThread::InitializeUserThread(Core::System& system, KThread* thread, KThreadFunction func,
                                     uintptr_t arg, KProcessAddress user_stack_top, s32 prio,
                                     s32 virt_core, KProcess* owner) {
    system.Kernel().GlobalSchedulerContext().AddThread(thread);
    R_RETURN(InitializeThread(thread, func, arg, user_stack_top, prio, virt_core, owner,
                              ThreadType::User, system.GetCpuManager().GetGuestThreadFunc()));
}

Result KThread::InitializeServiceThread(Core::System& system, KThread* thread,
                                        std::function<void()>&& func, s32 prio, s32 virt_core,
                                        KProcess* owner) {
    system.Kernel().GlobalSchedulerContext().AddThread(thread);
    std::function<void()> func2{[&system, func_{std::move(func)}] {
        // Similar to UserModeThreadStarter.
        system.Kernel().CurrentScheduler()->OnThreadStart();

        // Run the guest function.
        func_();

        // Exit.
        Svc::ExitThread(system);
    }};

    R_RETURN(InitializeThread(thread, {}, {}, {}, prio, virt_core, owner, ThreadType::HighPriority,
                              std::move(func2)));
}

void KThread::PostDestroy(uintptr_t arg) {
    KProcess* owner = reinterpret_cast<KProcess*>(arg & ~1ULL);
    const bool resource_limit_release_hint = (arg & 1);
    const s64 hint_value = (resource_limit_release_hint ? 0 : 1);
    if (owner != nullptr) {
        owner->GetResourceLimit()->Release(LimitableResource::ThreadCountMax, 1, hint_value);
        owner->Close();
    }
}

void KThread::Finalize() {
    // If the thread has an owner process, unregister it.
    if (m_parent != nullptr) {
        m_parent->UnregisterThread(this);
    }

    // If the thread has a local region, delete it.
    if (m_tls_address != 0) {
        ASSERT(m_parent->DeleteThreadLocalRegion(m_tls_address).IsSuccess());
    }

    // Release any waiters.
    {
        ASSERT(m_waiting_lock_info == nullptr);
        KScopedSchedulerLock sl{m_kernel};

        // Check that we have no kernel waiters.
        ASSERT(m_num_kernel_waiters == 0);

        auto it = m_held_lock_info_list.begin();
        while (it != m_held_lock_info_list.end()) {
            // Get the lock info.
            auto* const lock_info = std::addressof(*it);

            // The lock shouldn't have a kernel waiter.
            ASSERT(!lock_info->GetIsKernelAddressKey());

            // Remove all waiters.
            while (lock_info->GetWaiterCount() != 0) {
                // Get the front waiter.
                KThread* const waiter = lock_info->GetHighestPriorityWaiter();

                // Remove it from the lock.
                if (lock_info->RemoveWaiter(waiter)) {
                    ASSERT(lock_info->GetWaiterCount() == 0);
                }

                // Cancel the thread's wait.
                waiter->CancelWait(ResultInvalidState, true);
            }

            // Remove the held lock from our list.
            it = m_held_lock_info_list.erase(it);

            // Free the lock info.
            LockWithPriorityInheritanceInfo::Free(m_kernel, lock_info);
        }
    }

    // Release host emulation members.
    m_host_context.reset();

    // Perform inherited finalization.
    KSynchronizationObject::Finalize();
}

bool KThread::IsSignaled() const {
    return m_signaled;
}

void KThread::OnTimer() {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));

    // If we're waiting, cancel the wait.
    if (this->GetState() == ThreadState::Waiting) {
        m_wait_queue->CancelWait(this, ResultTimedOut, false);
    }
}

void KThread::StartTermination() {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));

    // Release user exception and unpin, if relevant.
    if (m_parent != nullptr) {
        m_parent->ReleaseUserException(this);
        if (m_parent->GetPinnedThread(GetCurrentCoreId(m_kernel)) == this) {
            m_parent->UnpinCurrentThread();
        }
    }

    // Set state to terminated.
    this->SetState(ThreadState::Terminated);

    // Clear the thread's status as running in parent.
    if (m_parent != nullptr) {
        m_parent->ClearRunningThread(this);
    }

    // Clear previous thread in KScheduler.
    KScheduler::ClearPreviousThread(m_kernel, this);

    // Register terminated dpc flag.
    this->RegisterDpc(DpcFlag::Terminated);
}

void KThread::FinishTermination() {
    // Ensure that the thread is not executing on any core.
    if (m_parent != nullptr) {
        for (std::size_t i = 0; i < static_cast<std::size_t>(Core::Hardware::NUM_CPU_CORES); ++i) {
            KThread* core_thread{};
            do {
                core_thread = m_kernel.Scheduler(i).GetSchedulerCurrentThread();
            } while (core_thread == this);
        }
    }

    // Acquire the scheduler lock.
    KScopedSchedulerLock sl{m_kernel};

    // Signal.
    m_signaled = true;
    KSynchronizationObject::NotifyAvailable();

    // Close the thread.
    this->Close();
}

void KThread::DoWorkerTaskImpl() {
    // Finish the termination that was begun by Exit().
    this->FinishTermination();
}

void KThread::Pin(s32 current_core) {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));

    // Set ourselves as pinned.
    GetStackParameters().is_pinned = true;

    // Disable core migration.
    ASSERT(m_num_core_migration_disables == 0);
    {
        ++m_num_core_migration_disables;

        // Save our ideal state to restore when we're unpinned.
        m_original_physical_ideal_core_id = m_physical_ideal_core_id;
        m_original_physical_affinity_mask = m_physical_affinity_mask;

        // Bind ourselves to this core.
        const s32 active_core = this->GetActiveCore();

        this->SetActiveCore(current_core);
        m_physical_ideal_core_id = current_core;
        m_physical_affinity_mask.SetAffinityMask(1ULL << current_core);

        if (active_core != current_core ||
            m_physical_affinity_mask.GetAffinityMask() !=
                m_original_physical_affinity_mask.GetAffinityMask()) {
            KScheduler::OnThreadAffinityMaskChanged(m_kernel, this,
                                                    m_original_physical_affinity_mask, active_core);
        }
    }

    // Disallow performing thread suspension.
    {
        // Update our allow flags.
        m_suspend_allowed_flags &= ~(1 << (static_cast<u32>(SuspendType::Thread) +
                                           static_cast<u32>(ThreadState::SuspendShift)));

        // Update our state.
        this->UpdateState();
    }

    // TODO(bunnei): Update our SVC access permissions.
    ASSERT(m_parent != nullptr);
}

void KThread::Unpin() {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));

    // Set ourselves as unpinned.
    this->GetStackParameters().is_pinned = false;

    // Enable core migration.
    ASSERT(m_num_core_migration_disables == 1);
    {
        m_num_core_migration_disables--;

        // Restore our original state.
        const KAffinityMask old_mask = m_physical_affinity_mask;

        m_physical_ideal_core_id = m_original_physical_ideal_core_id;
        m_physical_affinity_mask = m_original_physical_affinity_mask;

        if (m_physical_affinity_mask.GetAffinityMask() != old_mask.GetAffinityMask()) {
            const s32 active_core = this->GetActiveCore();

            if (!m_physical_affinity_mask.GetAffinity(active_core)) {
                if (m_physical_ideal_core_id >= 0) {
                    this->SetActiveCore(m_physical_ideal_core_id);
                } else {
                    this->SetActiveCore(static_cast<s32>(
                        Common::BitSize<u64>() - 1 -
                        std::countl_zero(m_physical_affinity_mask.GetAffinityMask())));
                }
            }
            KScheduler::OnThreadAffinityMaskChanged(m_kernel, this, old_mask, active_core);
        }
    }

    // Allow performing thread suspension (if termination hasn't been requested).
    if (!this->IsTerminationRequested()) {
        // Update our allow flags.
        m_suspend_allowed_flags |= (1 << (static_cast<u32>(SuspendType::Thread) +
                                          static_cast<u32>(ThreadState::SuspendShift)));

        // Update our state.
        this->UpdateState();
    }

    // TODO(bunnei): Update our SVC access permissions.
    ASSERT(m_parent != nullptr);

    // Resume any threads that began waiting on us while we were pinned.
    for (auto it = m_pinned_waiter_list.begin(); it != m_pinned_waiter_list.end();
         it = m_pinned_waiter_list.erase(it)) {
        it->EndWait(ResultSuccess);
    }
}

u16 KThread::GetUserDisableCount() const {
    if (!this->IsUserThread()) {
        // We only emulate TLS for user threads
        return {};
    }

    auto& memory = this->GetOwnerProcess()->GetMemory();
    return memory.Read16(m_tls_address + offsetof(ThreadLocalRegion, disable_count));
}

void KThread::SetInterruptFlag() {
    if (!this->IsUserThread()) {
        // We only emulate TLS for user threads
        return;
    }

    auto& memory = this->GetOwnerProcess()->GetMemory();
    memory.Write16(m_tls_address + offsetof(ThreadLocalRegion, interrupt_flag), 1);
}

void KThread::ClearInterruptFlag() {
    if (!this->IsUserThread()) {
        // We only emulate TLS for user threads
        return;
    }

    auto& memory = this->GetOwnerProcess()->GetMemory();
    memory.Write16(m_tls_address + offsetof(ThreadLocalRegion, interrupt_flag), 0);
}

Result KThread::GetCoreMask(s32* out_ideal_core, u64* out_affinity_mask) {
    KScopedSchedulerLock sl{m_kernel};

    // Get the virtual mask.
    *out_ideal_core = m_virtual_ideal_core_id;
    *out_affinity_mask = m_virtual_affinity_mask;

    R_SUCCEED();
}

Result KThread::GetPhysicalCoreMask(s32* out_ideal_core, u64* out_affinity_mask) {
    KScopedSchedulerLock sl{m_kernel};
    ASSERT(m_num_core_migration_disables >= 0);

    // Select between core mask and original core mask.
    if (m_num_core_migration_disables == 0) {
        *out_ideal_core = m_physical_ideal_core_id;
        *out_affinity_mask = m_physical_affinity_mask.GetAffinityMask();
    } else {
        *out_ideal_core = m_original_physical_ideal_core_id;
        *out_affinity_mask = m_original_physical_affinity_mask.GetAffinityMask();
    }

    R_SUCCEED();
}

Result KThread::SetCoreMask(s32 core_id, u64 v_affinity_mask) {
    ASSERT(m_parent != nullptr);
    ASSERT(v_affinity_mask != 0);
    KScopedLightLock lk(m_activity_pause_lock);

    // Set the core mask.
    u64 p_affinity_mask = 0;
    {
        KScopedSchedulerLock sl(m_kernel);
        ASSERT(m_num_core_migration_disables >= 0);

        // If we're updating, set our ideal virtual core.
        if (core_id != Svc::IdealCoreNoUpdate) {
            m_virtual_ideal_core_id = core_id;
        } else {
            // Preserve our ideal core id.
            core_id = m_virtual_ideal_core_id;
            R_UNLESS(((1ULL << core_id) & v_affinity_mask) != 0, ResultInvalidCombination);
        }

        // Set our affinity mask.
        m_virtual_affinity_mask = v_affinity_mask;

        // Translate the virtual core to a physical core.
        if (core_id >= 0) {
            core_id = Core::Hardware::VirtualToPhysicalCoreMap[core_id];
        }

        // Translate the virtual affinity mask to a physical one.
        while (v_affinity_mask != 0) {
            const u64 next = std::countr_zero(v_affinity_mask);
            v_affinity_mask &= ~(1ULL << next);
            p_affinity_mask |= (1ULL << Core::Hardware::VirtualToPhysicalCoreMap[next]);
        }

        // If we haven't disabled migration, perform an affinity change.
        if (m_num_core_migration_disables == 0) {
            const KAffinityMask old_mask = m_physical_affinity_mask;

            // Set our new ideals.
            m_physical_ideal_core_id = core_id;
            m_physical_affinity_mask.SetAffinityMask(p_affinity_mask);

            if (m_physical_affinity_mask.GetAffinityMask() != old_mask.GetAffinityMask()) {
                const s32 active_core = GetActiveCore();

                if (active_core >= 0 && !m_physical_affinity_mask.GetAffinity(active_core)) {
                    const s32 new_core = static_cast<s32>(
                        m_physical_ideal_core_id >= 0
                            ? m_physical_ideal_core_id
                            : Common::BitSize<u64>() - 1 -
                                  std::countl_zero(m_physical_affinity_mask.GetAffinityMask()));
                    SetActiveCore(new_core);
                }
                KScheduler::OnThreadAffinityMaskChanged(m_kernel, this, old_mask, active_core);
            }
        } else {
            // Otherwise, we edit the original affinity for restoration later.
            m_original_physical_ideal_core_id = core_id;
            m_original_physical_affinity_mask.SetAffinityMask(p_affinity_mask);
        }
    }

    // Update the pinned waiter list.
    ThreadQueueImplForKThreadSetProperty wait_queue(m_kernel, std::addressof(m_pinned_waiter_list));
    {
        bool retry_update{};
        do {
            // Lock the scheduler.
            KScopedSchedulerLock sl(m_kernel);

            // Don't do any further management if our termination has been requested.
            R_SUCCEED_IF(this->IsTerminationRequested());

            // By default, we won't need to retry.
            retry_update = false;

            // Check if the thread is currently running.
            bool thread_is_current{};
            s32 thread_core;
            for (thread_core = 0; thread_core < static_cast<s32>(Core::Hardware::NUM_CPU_CORES);
                 ++thread_core) {
                if (m_kernel.Scheduler(thread_core).GetSchedulerCurrentThread() == this) {
                    thread_is_current = true;
                    break;
                }
            }

            // If the thread is currently running, check whether it's no longer allowed under the
            // new mask.
            if (thread_is_current && ((1ULL << thread_core) & p_affinity_mask) == 0) {
                // If the thread is pinned, we want to wait until it's not pinned.
                if (this->GetStackParameters().is_pinned) {
                    // Verify that the current thread isn't terminating.
                    R_UNLESS(!GetCurrentThread(m_kernel).IsTerminationRequested(),
                             ResultTerminationRequested);

                    // Wait until the thread isn't pinned any more.
                    m_pinned_waiter_list.push_back(GetCurrentThread(m_kernel));
                    GetCurrentThread(m_kernel).BeginWait(std::addressof(wait_queue));
                } else {
                    // If the thread isn't pinned, release the scheduler lock and retry until it's
                    // not current.
                    retry_update = true;
                }
            }
        } while (retry_update);
    }

    R_SUCCEED();
}

void KThread::SetBasePriority(s32 value) {
    ASSERT(Svc::HighestThreadPriority <= value && value <= Svc::LowestThreadPriority);

    KScopedSchedulerLock sl{m_kernel};

    // Change our base priority.
    m_base_priority = value;

    // Perform a priority restoration.
    RestorePriority(m_kernel, this);
}

KThread* KThread::GetLockOwner() const {
    return m_waiting_lock_info != nullptr ? m_waiting_lock_info->GetOwner() : nullptr;
}

void KThread::IncreaseBasePriority(s32 priority) {
    ASSERT(Svc::HighestThreadPriority <= priority && priority <= Svc::LowestThreadPriority);
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));
    ASSERT(!this->GetStackParameters().is_pinned);

    // Set our base priority.
    if (m_base_priority > priority) {
        m_base_priority = priority;

        // Perform a priority restoration.
        RestorePriority(m_kernel, this);
    }
}

void KThread::RequestSuspend(SuspendType type) {
    KScopedSchedulerLock sl{m_kernel};

    // Note the request in our flags.
    m_suspend_request_flags |=
        (1U << (static_cast<u32>(ThreadState::SuspendShift) + static_cast<u32>(type)));

    // Try to perform the suspend.
    this->TrySuspend();
}

void KThread::Resume(SuspendType type) {
    KScopedSchedulerLock sl{m_kernel};

    // Clear the request in our flags.
    m_suspend_request_flags &=
        ~(1U << (static_cast<u32>(ThreadState::SuspendShift) + static_cast<u32>(type)));

    // Update our state.
    this->UpdateState();
}

void KThread::WaitCancel() {
    KScopedSchedulerLock sl{m_kernel};

    // Check if we're waiting and cancellable.
    if (this->GetState() == ThreadState::Waiting && m_cancellable) {
        m_wait_cancelled = false;
        m_wait_queue->CancelWait(this, ResultCancelled, true);
    } else {
        // Otherwise, note that we cancelled a wait.
        m_wait_cancelled = true;
    }
}

void KThread::TrySuspend() {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));
    ASSERT(this->IsSuspendRequested());

    // Ensure that we have no waiters.
    if (this->GetNumKernelWaiters() > 0) {
        return;
    }
    ASSERT(this->GetNumKernelWaiters() == 0);

    // Perform the suspend.
    this->UpdateState();
}

void KThread::UpdateState() {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));

    // Set our suspend flags in state.
    const ThreadState old_state = m_thread_state.load(std::memory_order_relaxed);
    const auto new_state =
        static_cast<ThreadState>(this->GetSuspendFlags()) | (old_state & ThreadState::Mask);
    m_thread_state.store(new_state, std::memory_order_relaxed);

    // Note the state change in scheduler.
    if (new_state != old_state) {
        KScheduler::OnThreadStateChanged(m_kernel, this, old_state);
    }
}

void KThread::Continue() {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));

    // Clear our suspend flags in state.
    const ThreadState old_state = m_thread_state.load(std::memory_order_relaxed);
    m_thread_state.store(old_state & ThreadState::Mask, std::memory_order_relaxed);

    // Note the state change in scheduler.
    KScheduler::OnThreadStateChanged(m_kernel, this, old_state);
}

void KThread::CloneFpuStatus() {
    // We shouldn't reach here when starting kernel threads.
    ASSERT(this->GetOwnerProcess() != nullptr);
    ASSERT(this->GetOwnerProcess() == GetCurrentProcessPointer(m_kernel));

    m_kernel.CurrentPhysicalCore().CloneFpuStatus(this);
}

Result KThread::SetActivity(Svc::ThreadActivity activity) {
    // Lock ourselves.
    KScopedLightLock lk(m_activity_pause_lock);

    // Set the activity.
    {
        // Lock the scheduler.
        KScopedSchedulerLock sl(m_kernel);

        // Verify our state.
        const auto cur_state = this->GetState();
        R_UNLESS((cur_state == ThreadState::Waiting || cur_state == ThreadState::Runnable),
                 ResultInvalidState);

        // Either pause or resume.
        if (activity == Svc::ThreadActivity::Paused) {
            // Verify that we're not suspended.
            R_UNLESS(!this->IsSuspendRequested(SuspendType::Thread), ResultInvalidState);

            // Suspend.
            this->RequestSuspend(SuspendType::Thread);
        } else {
            ASSERT(activity == Svc::ThreadActivity::Runnable);

            // Verify that we're suspended.
            R_UNLESS(this->IsSuspendRequested(SuspendType::Thread), ResultInvalidState);

            // Resume.
            this->Resume(SuspendType::Thread);
        }
    }

    // If the thread is now paused, update the pinned waiter list.
    if (activity == Svc::ThreadActivity::Paused) {
        ThreadQueueImplForKThreadSetProperty wait_queue(m_kernel,
                                                        std::addressof(m_pinned_waiter_list));

        bool thread_is_current{};
        do {
            // Lock the scheduler.
            KScopedSchedulerLock sl(m_kernel);

            // Don't do any further management if our termination has been requested.
            R_SUCCEED_IF(this->IsTerminationRequested());

            // By default, treat the thread as not current.
            thread_is_current = false;

            // Check whether the thread is pinned.
            if (this->GetStackParameters().is_pinned) {
                // Verify that the current thread isn't terminating.
                R_UNLESS(!GetCurrentThread(m_kernel).IsTerminationRequested(),
                         ResultTerminationRequested);

                // Wait until the thread isn't pinned any more.
                m_pinned_waiter_list.push_back(GetCurrentThread(m_kernel));
                GetCurrentThread(m_kernel).BeginWait(std::addressof(wait_queue));
            } else {
                // Check if the thread is currently running.
                // If it is, we'll need to retry.
                for (auto i = 0; i < static_cast<s32>(Core::Hardware::NUM_CPU_CORES); ++i) {
                    if (m_kernel.Scheduler(i).GetSchedulerCurrentThread() == this) {
                        thread_is_current = true;
                        break;
                    }
                }
            }
        } while (thread_is_current);
    }

    R_SUCCEED();
}

Result KThread::GetThreadContext3(Svc::ThreadContext* out) {
    // Lock ourselves.
    KScopedLightLock lk{m_activity_pause_lock};

    // Get the context.
    {
        // Lock the scheduler.
        KScopedSchedulerLock sl{m_kernel};

        // Verify that we're suspended.
        R_UNLESS(this->IsSuspendRequested(SuspendType::Thread), ResultInvalidState);

        // If we're not terminating, get the thread's user context.
        if (!this->IsTerminationRequested()) {
            *out = m_thread_context;

            // Mask away mode bits, interrupt bits, IL bit, and other reserved bits.
            constexpr u32 El0Aarch64PsrMask = 0xF0000000;
            constexpr u32 El0Aarch32PsrMask = 0xFE0FFE20;

            if (m_parent->Is64Bit()) {
                out->pstate &= El0Aarch64PsrMask;
            } else {
                out->pstate &= El0Aarch32PsrMask;
            }
        }
    }

    R_SUCCEED();
}

void KThread::AddHeldLock(LockWithPriorityInheritanceInfo* lock_info) {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));

    // Set ourselves as the lock's owner.
    lock_info->SetOwner(this);

    // Add the lock to our held list.
    m_held_lock_info_list.push_front(*lock_info);
}

KThread::LockWithPriorityInheritanceInfo* KThread::FindHeldLock(KProcessAddress address_key,
                                                                bool is_kernel_address_key) {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));

    // Try to find an existing held lock.
    for (auto& held_lock : m_held_lock_info_list) {
        if (held_lock.GetAddressKey() == address_key &&
            held_lock.GetIsKernelAddressKey() == is_kernel_address_key) {
            return std::addressof(held_lock);
        }
    }

    return nullptr;
}

void KThread::AddWaiterImpl(KThread* thread) {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));
    ASSERT(thread->GetConditionVariableTree() == nullptr);

    // Get the thread's address key.
    const auto address_key = thread->GetAddressKey();
    const auto is_kernel_address_key = thread->GetIsKernelAddressKey();

    // Keep track of how many kernel waiters we have.
    if (is_kernel_address_key) {
        ASSERT((m_num_kernel_waiters++) >= 0);
        KScheduler::SetSchedulerUpdateNeeded(m_kernel);
    }

    // Get the relevant lock info.
    auto* lock_info = this->FindHeldLock(address_key, is_kernel_address_key);
    if (lock_info == nullptr) {
        // Create a new lock for the address key.
        lock_info =
            LockWithPriorityInheritanceInfo::Create(m_kernel, address_key, is_kernel_address_key);

        // Add the new lock to our list.
        this->AddHeldLock(lock_info);
    }

    // Add the thread as waiter to the lock info.
    lock_info->AddWaiter(thread);
}

void KThread::RemoveWaiterImpl(KThread* thread) {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));

    // Keep track of how many kernel waiters we have.
    if (thread->GetIsKernelAddressKey()) {
        ASSERT((m_num_kernel_waiters--) > 0);
        KScheduler::SetSchedulerUpdateNeeded(m_kernel);
    }

    // Get the info for the lock the thread is waiting on.
    auto* lock_info = thread->GetWaitingLockInfo();
    ASSERT(lock_info->GetOwner() == this);

    // Remove the waiter.
    if (lock_info->RemoveWaiter(thread)) {
        m_held_lock_info_list.erase(m_held_lock_info_list.iterator_to(*lock_info));
        LockWithPriorityInheritanceInfo::Free(m_kernel, lock_info);
    }
}

void KThread::RestorePriority(KernelCore& kernel, KThread* thread) {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(kernel));

    while (thread != nullptr) {
        // We want to inherit priority where possible.
        s32 new_priority = thread->GetBasePriority();
        for (const auto& held_lock : thread->m_held_lock_info_list) {
            new_priority =
                std::min(new_priority, held_lock.GetHighestPriorityWaiter()->GetPriority());
        }

        // If the priority we would inherit is not different from ours, don't do anything.
        if (new_priority == thread->GetPriority()) {
            return;
        }

        // Get the owner of whatever lock this thread is waiting on.
        KThread* const lock_owner = thread->GetLockOwner();

        // If the thread is waiting on some lock, remove it as a waiter to prevent violating red
        // black tree invariants.
        if (lock_owner != nullptr) {
            lock_owner->RemoveWaiterImpl(thread);
        }

        // Ensure we don't violate condition variable red black tree invariants.
        if (auto* cv_tree = thread->GetConditionVariableTree(); cv_tree != nullptr) {
            BeforeUpdatePriority(kernel, cv_tree, thread);
        }

        // Change the priority.
        const s32 old_priority = thread->GetPriority();
        thread->SetPriority(new_priority);

        // Restore the condition variable, if relevant.
        if (auto* cv_tree = thread->GetConditionVariableTree(); cv_tree != nullptr) {
            AfterUpdatePriority(kernel, cv_tree, thread);
        }

        // If we removed the thread from some lock's waiting list, add it back.
        if (lock_owner != nullptr) {
            lock_owner->AddWaiterImpl(thread);
        }

        // Update the scheduler.
        KScheduler::OnThreadPriorityChanged(kernel, thread, old_priority);

        // Continue inheriting priority.
        thread = lock_owner;
    }
}

void KThread::AddWaiter(KThread* thread) {
    this->AddWaiterImpl(thread);

    // If the thread has a higher priority than us, we should inherit.
    if (thread->GetPriority() < this->GetPriority()) {
        RestorePriority(m_kernel, this);
    }
}

void KThread::RemoveWaiter(KThread* thread) {
    this->RemoveWaiterImpl(thread);

    // If our priority is the same as the thread's (and we've inherited), we may need to restore to
    // lower priority.
    if (this->GetPriority() == thread->GetPriority() &&
        this->GetPriority() < this->GetBasePriority()) {
        RestorePriority(m_kernel, this);
    }
}

KThread* KThread::RemoveWaiterByKey(bool* out_has_waiters, KProcessAddress key,
                                    bool is_kernel_address_key_) {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));

    // Get the relevant lock info.
    auto* lock_info = this->FindHeldLock(key, is_kernel_address_key_);
    if (lock_info == nullptr) {
        *out_has_waiters = false;
        return nullptr;
    }

    // Remove the lock info from our held list.
    m_held_lock_info_list.erase(m_held_lock_info_list.iterator_to(*lock_info));

    // Keep track of how many kernel waiters we have.
    if (lock_info->GetIsKernelAddressKey()) {
        m_num_kernel_waiters -= lock_info->GetWaiterCount();
        ASSERT(m_num_kernel_waiters >= 0);
        KScheduler::SetSchedulerUpdateNeeded(m_kernel);
    }

    ASSERT(lock_info->GetWaiterCount() > 0);

    // Remove the highest priority waiter from the lock to be the next owner.
    KThread* next_lock_owner = lock_info->GetHighestPriorityWaiter();
    if (lock_info->RemoveWaiter(next_lock_owner)) {
        // The new owner was the only waiter.
        *out_has_waiters = false;

        // Free the lock info, since it has no waiters.
        LockWithPriorityInheritanceInfo::Free(m_kernel, lock_info);
    } else {
        // There are additional waiters on the lock.
        *out_has_waiters = true;

        // Add the lock to the new owner's held list.
        next_lock_owner->AddHeldLock(lock_info);

        // Keep track of any kernel waiters for the new owner.
        if (lock_info->GetIsKernelAddressKey()) {
            next_lock_owner->m_num_kernel_waiters += lock_info->GetWaiterCount();
            ASSERT(next_lock_owner->m_num_kernel_waiters > 0);

            // NOTE: No need to set scheduler update needed, because we will have already done so
            // when removing earlier.
        }
    }

    // If our priority is the same as the next owner's (and we've inherited), we may need to restore
    // to lower priority.
    if (this->GetPriority() == next_lock_owner->GetPriority() &&
        this->GetPriority() < this->GetBasePriority()) {
        RestorePriority(m_kernel, this);
        // NOTE: No need to restore priority on the next lock owner, because it was already the
        // highest priority waiter on the lock.
    }

    // Return the next lock owner.
    return next_lock_owner;
}

Result KThread::Run() {
    while (true) {
        KScopedSchedulerLock lk{m_kernel};

        // If either this thread or the current thread are requesting termination, note it.
        R_UNLESS(!this->IsTerminationRequested(), ResultTerminationRequested);
        R_UNLESS(!GetCurrentThread(m_kernel).IsTerminationRequested(), ResultTerminationRequested);

        // Ensure our thread state is correct.
        R_UNLESS(this->GetState() == ThreadState::Initialized, ResultInvalidState);

        // If the current thread has been asked to suspend, suspend it and retry.
        if (GetCurrentThread(m_kernel).IsSuspended()) {
            GetCurrentThread(m_kernel).UpdateState();
            continue;
        }

        // If we're not a kernel thread and we've been asked to suspend, suspend ourselves.
        if (KProcess* owner = this->GetOwnerProcess(); owner != nullptr) {
            if (this->IsUserThread() && this->IsSuspended()) {
                this->UpdateState();
            }
            owner->IncrementRunningThreadCount();
        }

        // Open a reference, now that we're running.
        this->Open();

        // Set our state and finish.
        this->SetState(ThreadState::Runnable);

        R_SUCCEED();
    }
}

void KThread::Exit() {
    ASSERT(this == GetCurrentThreadPointer(m_kernel));

    // Release the thread resource hint, running thread count from parent.
    if (m_parent != nullptr) {
        m_parent->GetResourceLimit()->Release(Kernel::LimitableResource::ThreadCountMax, 0, 1);
        m_resource_limit_release_hint = true;
        m_parent->DecrementRunningThreadCount();
    }

    // Perform termination.
    {
        KScopedSchedulerLock sl{m_kernel};

        // Disallow all suspension.
        m_suspend_allowed_flags = 0;
        this->UpdateState();

        // Disallow all suspension.
        m_suspend_allowed_flags = 0;

        // Start termination.
        this->StartTermination();

        // Register the thread as a work task.
        KWorkerTaskManager::AddTask(m_kernel, KWorkerTaskManager::WorkerType::Exit, this);
    }

    UNREACHABLE_MSG("KThread::Exit() would return");
}

Result KThread::Terminate() {
    ASSERT(this != GetCurrentThreadPointer(m_kernel));

    // Request the thread terminate if it hasn't already.
    if (const auto new_state = this->RequestTerminate(); new_state != ThreadState::Terminated) {
        // If the thread isn't terminated, wait for it to terminate.
        s32 index;
        KSynchronizationObject* objects[] = {this};
        R_TRY(KSynchronizationObject::Wait(m_kernel, std::addressof(index), objects, 1,
                                           Svc::WaitInfinite));
    }

    R_SUCCEED();
}

ThreadState KThread::RequestTerminate() {
    ASSERT(this != GetCurrentThreadPointer(m_kernel));

    KScopedSchedulerLock sl{m_kernel};

    // Determine if this is the first termination request.
    const bool first_request = [&]() -> bool {
        // Perform an atomic compare-and-swap from false to true.
        bool expected = false;
        return m_termination_requested.compare_exchange_strong(expected, true);
    }();

    // If this is the first request, start termination procedure.
    if (first_request) {
        // If the thread is in initialized state, just change state to terminated.
        if (this->GetState() == ThreadState::Initialized) {
            m_thread_state = ThreadState::Terminated;
            return ThreadState::Terminated;
        }

        // Register the terminating dpc.
        this->RegisterDpc(DpcFlag::Terminating);

        // If the thread is pinned, unpin it.
        if (this->GetStackParameters().is_pinned) {
            this->GetOwnerProcess()->UnpinThread(this);
        }

        // If the thread is suspended, continue it.
        if (this->IsSuspended()) {
            m_suspend_allowed_flags = 0;
            this->UpdateState();
        }

        // Change the thread's priority to be higher than any system thread's.
        this->IncreaseBasePriority(TerminatingThreadPriority);

        // If the thread is runnable, send a termination interrupt to cores it may be running on.
        if (this->GetState() == ThreadState::Runnable) {
            // NOTE: We do not mask the "current core", because this code may not actually be
            //       executing from the thread representing the "current core".
            if (const u64 core_mask = m_physical_affinity_mask.GetAffinityMask(); core_mask != 0) {
                Kernel::KInterruptManager::SendInterProcessorInterrupt(m_kernel, core_mask);
            }
        }

        // Wake up the thread.
        if (this->GetState() == ThreadState::Waiting) {
            m_wait_queue->CancelWait(this, ResultTerminationRequested, true);
        }
    }

    return this->GetState();
}

Result KThread::Sleep(s64 timeout) {
    ASSERT(!KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));
    ASSERT(this == GetCurrentThreadPointer(m_kernel));
    ASSERT(timeout > 0);

    ThreadQueueImplForKThreadSleep wait_queue(m_kernel);
    KHardwareTimer* timer{};
    {
        // Setup the scheduling lock and sleep.
        KScopedSchedulerLockAndSleep slp(m_kernel, std::addressof(timer), this, timeout);

        // Check if the thread should terminate.
        if (this->IsTerminationRequested()) {
            slp.CancelSleep();
            R_THROW(ResultTerminationRequested);
        }

        // Wait for the sleep to end.
        wait_queue.SetHardwareTimer(timer);
        this->BeginWait(std::addressof(wait_queue));
        this->SetWaitReasonForDebugging(ThreadWaitReasonForDebugging::Sleep);
    }

    R_SUCCEED();
}

void KThread::RequestDummyThreadWait() {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));
    ASSERT(this->IsDummyThread());

    // We will block when the scheduler lock is released.
    std::scoped_lock lock{m_dummy_thread_mutex};
    m_dummy_thread_runnable = false;
}

void KThread::DummyThreadBeginWait() {
    if (!this->IsDummyThread() || m_kernel.IsPhantomModeForSingleCore()) {
        // Occurs in single core mode.
        return;
    }

    // Block until runnable is no longer false.
    std::unique_lock lock{m_dummy_thread_mutex};
    m_dummy_thread_cv.wait(lock, [this] { return m_dummy_thread_runnable; });
}

void KThread::DummyThreadEndWait() {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));
    ASSERT(this->IsDummyThread());

    // Wake up the waiting thread.
    {
        std::scoped_lock lock{m_dummy_thread_mutex};
        m_dummy_thread_runnable = true;
    }
    m_dummy_thread_cv.notify_one();
}

void KThread::BeginWait(KThreadQueue* queue) {
    // Set our state as waiting.
    this->SetState(ThreadState::Waiting);

    // Set our wait queue.
    m_wait_queue = queue;
}

void KThread::NotifyAvailable(KSynchronizationObject* signaled_object, Result wait_result) {
    // Lock the scheduler.
    KScopedSchedulerLock sl(m_kernel);

    // If we're waiting, notify our queue that we're available.
    if (this->GetState() == ThreadState::Waiting) {
        m_wait_queue->NotifyAvailable(this, signaled_object, wait_result);
    }
}

void KThread::EndWait(Result wait_result) {
    // Lock the scheduler.
    KScopedSchedulerLock sl(m_kernel);

    // If we're waiting, notify our queue that we're available.
    if (this->GetState() == ThreadState::Waiting) {
        if (m_wait_queue == nullptr) {
            // This should never happen, but avoid a hard crash below to get this logged.
            ASSERT_MSG(false, "wait_queue is nullptr!");
            return;
        }

        m_wait_queue->EndWait(this, wait_result);
    }
}

void KThread::CancelWait(Result wait_result, bool cancel_timer_task) {
    // Lock the scheduler.
    KScopedSchedulerLock sl(m_kernel);

    // If we're waiting, notify our queue that we're available.
    if (this->GetState() == ThreadState::Waiting) {
        m_wait_queue->CancelWait(this, wait_result, cancel_timer_task);
    }
}

void KThread::SetState(ThreadState state) {
    KScopedSchedulerLock sl{m_kernel};

    // Clear debugging state
    this->SetWaitReasonForDebugging({});

    const ThreadState old_state = m_thread_state.load(std::memory_order_relaxed);
    m_thread_state.store(
        static_cast<ThreadState>((old_state & ~ThreadState::Mask) | (state & ThreadState::Mask)),
        std::memory_order_relaxed);
    if (m_thread_state.load(std::memory_order_relaxed) != old_state) {
        KScheduler::OnThreadStateChanged(m_kernel, this, old_state);
    }
}

std::shared_ptr<Common::Fiber>& KThread::GetHostContext() {
    return m_host_context;
}

void SetCurrentThread(KernelCore& kernel, KThread* thread) {
    kernel.SetCurrentEmuThread(thread);
}

KThread* GetCurrentThreadPointer(KernelCore& kernel) {
    return kernel.GetCurrentEmuThread();
}

KThread& GetCurrentThread(KernelCore& kernel) {
    return *GetCurrentThreadPointer(kernel);
}

KProcess* GetCurrentProcessPointer(KernelCore& kernel) {
    return GetCurrentThread(kernel).GetOwnerProcess();
}

KProcess& GetCurrentProcess(KernelCore& kernel) {
    return *GetCurrentProcessPointer(kernel);
}

s32 GetCurrentCoreId(KernelCore& kernel) {
    return GetCurrentThread(kernel).GetCurrentCore();
}

Core::Memory::Memory& GetCurrentMemory(KernelCore& kernel) {
    return GetCurrentProcess(kernel).GetMemory();
}

KScopedDisableDispatch::~KScopedDisableDispatch() {
    // If we are shutting down the kernel, none of this is relevant anymore.
    if (m_kernel.IsShuttingDown()) {
        return;
    }

    if (GetCurrentThread(m_kernel).GetDisableDispatchCount() <= 1) {
        auto* scheduler = m_kernel.CurrentScheduler();

        if (scheduler && !m_kernel.IsPhantomModeForSingleCore()) {
            scheduler->RescheduleCurrentCore();
        } else {
            KScheduler::RescheduleCurrentHLEThread(m_kernel);
        }
    } else {
        GetCurrentThread(m_kernel).EnableDispatch();
    }
}

} // namespace Kernel
