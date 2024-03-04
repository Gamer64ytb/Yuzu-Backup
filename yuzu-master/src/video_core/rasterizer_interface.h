// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <optional>
#include <span>
#include <utility>
#include "common/common_types.h"
#include "common/polyfill_thread.h"
#include "video_core/cache_types.h"
#include "video_core/engines/fermi_2d.h"
#include "video_core/gpu.h"
#include "video_core/query_cache/types.h"
#include "video_core/rasterizer_download_area.h"

namespace Tegra {
class MemoryManager;
namespace Engines {
class AccelerateDMAInterface;
}
namespace Control {
struct ChannelState;
}
} // namespace Tegra

namespace VideoCore {

enum class LoadCallbackStage {
    Prepare,
    Build,
    Complete,
};
using DiskResourceLoadCallback = std::function<void(LoadCallbackStage, std::size_t, std::size_t)>;

class RasterizerInterface {
public:
    virtual ~RasterizerInterface() = default;

    /// Dispatches a draw invocation
    virtual void Draw(bool is_indexed, u32 instance_count) = 0;

    /// Dispatches an indirect draw invocation
    virtual void DrawIndirect() {}

    /// Dispatches an draw texture invocation
    virtual void DrawTexture() = 0;

    /// Clear the current framebuffer
    virtual void Clear(u32 layer_count) = 0;

    /// Dispatches a compute shader invocation
    virtual void DispatchCompute() = 0;

    /// Resets the counter of a query
    virtual void ResetCounter(VideoCommon::QueryType type) = 0;

    /// Records a GPU query and caches it
    virtual void Query(GPUVAddr gpu_addr, VideoCommon::QueryType type,
                       VideoCommon::QueryPropertiesFlags flags, u32 payload, u32 subreport) = 0;

    /// Signal an uniform buffer binding
    virtual void BindGraphicsUniformBuffer(size_t stage, u32 index, GPUVAddr gpu_addr,
                                           u32 size) = 0;

    /// Signal disabling of a uniform buffer
    virtual void DisableGraphicsUniformBuffer(size_t stage, u32 index) = 0;

    /// Signal a GPU based semaphore as a fence
    virtual void SignalFence(std::function<void()>&& func) = 0;

    /// Send an operation to be done after a certain amount of flushes.
    virtual void SyncOperation(std::function<void()>&& func) = 0;

    /// Signal a GPU based syncpoint as a fence
    virtual void SignalSyncPoint(u32 value) = 0;

    /// Signal a GPU based reference as point
    virtual void SignalReference() = 0;

    /// Release all pending fences.
    virtual void ReleaseFences(bool force = true) = 0;

    /// Notify rasterizer that all caches should be flushed to Switch memory
    virtual void FlushAll() = 0;

    /// Notify rasterizer that any caches of the specified region should be flushed to Switch memory
    virtual void FlushRegion(DAddr addr, u64 size,
                             VideoCommon::CacheType which = VideoCommon::CacheType::All) = 0;

    /// Check if the the specified memory area requires flushing to CPU Memory.
    virtual bool MustFlushRegion(DAddr addr, u64 size,
                                 VideoCommon::CacheType which = VideoCommon::CacheType::All) = 0;

    virtual RasterizerDownloadArea GetFlushArea(DAddr addr, u64 size) = 0;

    /// Notify rasterizer that any caches of the specified region should be invalidated
    virtual void InvalidateRegion(DAddr addr, u64 size,
                                  VideoCommon::CacheType which = VideoCommon::CacheType::All) = 0;

    virtual void InnerInvalidation(std::span<const std::pair<DAddr, std::size_t>> sequences) {
        for (const auto& [cpu_addr, size] : sequences) {
            InvalidateRegion(cpu_addr, size);
        }
    }

    /// Notify rasterizer that any caches of the specified region are desync with guest
    virtual void OnCacheInvalidation(PAddr addr, u64 size) = 0;

    virtual bool OnCPUWrite(PAddr addr, u64 size) = 0;

    /// Sync memory between guest and host.
    virtual void InvalidateGPUCache() = 0;

    /// Unmap memory range
    virtual void UnmapMemory(DAddr addr, u64 size) = 0;

    /// Remap GPU memory range. This means underneath backing memory changed
    virtual void ModifyGPUMemory(size_t as_id, GPUVAddr addr, u64 size) = 0;

    /// Notify rasterizer that any caches of the specified region should be flushed to Switch memory
    /// and invalidated
    virtual void FlushAndInvalidateRegion(
        DAddr addr, u64 size, VideoCommon::CacheType which = VideoCommon::CacheType::All) = 0;

    /// Notify the host renderer to wait for previous primitive and compute operations.
    virtual void WaitForIdle() = 0;

    /// Notify the host renderer to wait for reads and writes to render targets and flush caches.
    virtual void FragmentBarrier() = 0;

    /// Notify the host renderer to make available previous render target writes.
    virtual void TiledCacheBarrier() = 0;

    /// Notify the rasterizer to send all written commands to the host GPU.
    virtual void FlushCommands() = 0;

    /// Notify rasterizer that a frame is about to finish
    virtual void TickFrame() = 0;

    virtual bool AccelerateConditionalRendering() {
        return false;
    }

    /// Attempt to use a faster method to perform a surface copy
    [[nodiscard]] virtual bool AccelerateSurfaceCopy(
        const Tegra::Engines::Fermi2D::Surface& src, const Tegra::Engines::Fermi2D::Surface& dst,
        const Tegra::Engines::Fermi2D::Config& copy_config) {
        return false;
    }

    [[nodiscard]] virtual Tegra::Engines::AccelerateDMAInterface& AccessAccelerateDMA() = 0;

    virtual void AccelerateInlineToMemory(GPUVAddr address, size_t copy_size,
                                          std::span<const u8> memory) = 0;

    /// Initialize disk cached resources for the game being emulated
    virtual void LoadDiskResources(u64 title_id, std::stop_token stop_loading,
                                   const DiskResourceLoadCallback& callback) {}

    virtual void InitializeChannel(Tegra::Control::ChannelState& channel) {}

    virtual void BindChannel(Tegra::Control::ChannelState& channel) {}

    virtual void ReleaseChannel(s32 channel_id) {}

    /// Register the address as a Transform Feedback Object
    virtual void RegisterTransformFeedback(GPUVAddr tfb_object_addr) {}

    /// Returns true when the rasterizer has Draw Transform Feedback capabilities
    virtual bool HasDrawTransformFeedback() {
        return false;
    }
};
} // namespace VideoCore
