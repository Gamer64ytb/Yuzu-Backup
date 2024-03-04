// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>

#include "common/fiber.h"
#include "common/polyfill_thread.h"
#include "common/thread.h"
#include "core/hardware_properties.h"

namespace Common {
class Event;
class Fiber;
} // namespace Common

namespace Core {

class System;

class CpuManager {
public:
    explicit CpuManager(System& system_);
    CpuManager(const CpuManager&) = delete;
    CpuManager(CpuManager&&) = delete;

    ~CpuManager();

    CpuManager& operator=(const CpuManager&) = delete;
    CpuManager& operator=(CpuManager&&) = delete;

    /// Sets if emulation is multicore or single core, must be set before Initialize
    void SetMulticore(bool is_multi) {
        is_multicore = is_multi;
    }

    /// Sets if emulation is using an asynchronous GPU.
    void SetAsyncGpu(bool is_async) {
        is_async_gpu = is_async;
    }

    void OnGpuReady() {
        gpu_barrier->Sync();
    }

    void Initialize();
    void Shutdown();

    std::function<void()> GetGuestActivateFunc() {
        return [this] { GuestActivate(); };
    }
    std::function<void()> GetGuestThreadFunc() {
        return [this] { GuestThreadFunction(); };
    }
    std::function<void()> GetIdleThreadStartFunc() {
        return [this] { IdleThreadFunction(); };
    }
    std::function<void()> GetShutdownThreadStartFunc() {
        return [this] { ShutdownThreadFunction(); };
    }

    void PreemptSingleCore(bool from_running_environment = true);

    std::size_t CurrentCore() const {
        return current_core.load();
    }

private:
    void GuestThreadFunction();
    void IdleThreadFunction();
    void ShutdownThreadFunction();

    void MultiCoreRunGuestThread();
    void MultiCoreRunIdleThread();

    void SingleCoreRunGuestThread();
    void SingleCoreRunIdleThread();

    void GuestActivate();
    void HandleInterrupt();
    void ShutdownThread();
    void RunThread(std::stop_token stop_token, std::size_t core);

    struct CoreData {
        std::shared_ptr<Common::Fiber> host_context;
        std::jthread host_thread;
    };

    std::unique_ptr<Common::Barrier> gpu_barrier{};
    std::array<CoreData, Core::Hardware::NUM_CPU_CORES> core_data{};

    bool is_async_gpu{};
    bool is_multicore{};
    std::atomic<std::size_t> current_core{};
    std::size_t idle_count{};
    std::size_t num_cores{};
    static constexpr std::size_t max_cycle_runs = 5;

    System& system;
};

} // namespace Core
