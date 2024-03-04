// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <variant>

#include "common/bounded_threadsafe_queue.h"
#include "common/polyfill_thread.h"
#include "video_core/framebuffer_config.h"

namespace Tegra {
struct FramebufferConfig;
namespace Control {
class Scheduler;
}
} // namespace Tegra

namespace Core {
namespace Frontend {
class GraphicsContext;
}
class System;
} // namespace Core

namespace VideoCore {
class RasterizerInterface;
class RendererBase;
} // namespace VideoCore

namespace VideoCommon::GPUThread {

/// Command to signal to the GPU thread that a command list is ready for processing
struct SubmitListCommand final {
    explicit SubmitListCommand(s32 channel_, Tegra::CommandList&& entries_)
        : channel{channel_}, entries{std::move(entries_)} {}

    s32 channel;
    Tegra::CommandList entries;
};

/// Command to signal to the GPU thread to flush a region
struct FlushRegionCommand final {
    explicit constexpr FlushRegionCommand(DAddr addr_, u64 size_) : addr{addr_}, size{size_} {}

    DAddr addr;
    u64 size;
};

/// Command to signal to the GPU thread to invalidate a region
struct InvalidateRegionCommand final {
    explicit constexpr InvalidateRegionCommand(DAddr addr_, u64 size_) : addr{addr_}, size{size_} {}

    DAddr addr;
    u64 size;
};

/// Command to signal to the GPU thread to flush and invalidate a region
struct FlushAndInvalidateRegionCommand final {
    explicit constexpr FlushAndInvalidateRegionCommand(DAddr addr_, u64 size_)
        : addr{addr_}, size{size_} {}

    DAddr addr;
    u64 size;
};

/// Command to make the gpu look into pending requests
struct GPUTickCommand final {};

using CommandData =
    std::variant<std::monostate, SubmitListCommand, FlushRegionCommand, InvalidateRegionCommand,
                 FlushAndInvalidateRegionCommand, GPUTickCommand>;

struct CommandDataContainer {
    CommandDataContainer() = default;

    explicit CommandDataContainer(CommandData&& data_, u64 next_fence_, bool block_)
        : data{std::move(data_)}, fence{next_fence_}, block(block_) {}

    CommandData data;
    u64 fence{};
    bool block{};
};

/// Struct used to synchronize the GPU thread
struct SynchState final {
    using CommandQueue = Common::MPSCQueue<CommandDataContainer>;
    std::mutex write_lock;
    CommandQueue queue;
    u64 last_fence{};
    std::atomic<u64> signaled_fence{};
    std::condition_variable_any cv;
};

/// Class used to manage the GPU thread
class ThreadManager final {
public:
    explicit ThreadManager(Core::System& system_, bool is_async_);
    ~ThreadManager();

    /// Creates and starts the GPU thread.
    void StartThread(VideoCore::RendererBase& renderer, Core::Frontend::GraphicsContext& context,
                     Tegra::Control::Scheduler& scheduler);

    /// Push GPU command entries to be processed
    void SubmitList(s32 channel, Tegra::CommandList&& entries);

    /// Notify rasterizer that any caches of the specified region should be flushed to Switch memory
    void FlushRegion(DAddr addr, u64 size);

    /// Notify rasterizer that any caches of the specified region should be invalidated
    void InvalidateRegion(DAddr addr, u64 size);

    /// Notify rasterizer that any caches of the specified region should be flushed and invalidated
    void FlushAndInvalidateRegion(DAddr addr, u64 size);

    void TickGPU();

private:
    /// Pushes a command to be executed by the GPU thread
    u64 PushCommand(CommandData&& command_data, bool block = false);

    Core::System& system;
    const bool is_async;
    VideoCore::RasterizerInterface* rasterizer = nullptr;

    SynchState state;
    std::jthread thread;
};

} // namespace VideoCommon::GPUThread
