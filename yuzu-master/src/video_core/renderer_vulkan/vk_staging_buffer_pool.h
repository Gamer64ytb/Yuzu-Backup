// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <climits>
#include <vector>

#include "common/common_types.h"

#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
class Scheduler;

struct StagingBufferRef {
    VkBuffer buffer;
    VkDeviceSize offset;
    std::span<u8> mapped_span;
    MemoryUsage usage;
    u32 log2_level;
    u64 index;
};

class StagingBufferPool {
public:
    static constexpr size_t NUM_SYNCS = 16;

    explicit StagingBufferPool(const Device& device, MemoryAllocator& memory_allocator,
                               Scheduler& scheduler);
    ~StagingBufferPool();

    StagingBufferRef Request(size_t size, MemoryUsage usage, bool deferred = false);
    void FreeDeferred(StagingBufferRef& ref);

    [[nodiscard]] VkBuffer StreamBuf() const noexcept {
        return *stream_buffer;
    }

    void TickFrame();

private:
    struct StreamBufferCommit {
        size_t upper_bound;
        u64 tick;
    };

    struct StagingBuffer {
        vk::Buffer buffer;
        std::span<u8> mapped_span;
        MemoryUsage usage;
        u32 log2_level;
        u64 index;
        u64 tick = 0;
        bool deferred{};

        StagingBufferRef Ref() const noexcept {
            return {
                .buffer = *buffer,
                .offset = 0,
                .mapped_span = mapped_span,
                .usage = usage,
                .log2_level = log2_level,
                .index = index,
            };
        }
    };

    struct StagingBuffers {
        std::vector<StagingBuffer> entries;
        size_t delete_index = 0;
        size_t iterate_index = 0;
    };

    static constexpr size_t NUM_LEVELS = sizeof(size_t) * CHAR_BIT;
    using StagingBuffersCache = std::array<StagingBuffers, NUM_LEVELS>;

    StagingBufferRef GetStreamBuffer(size_t size);

    bool AreRegionsActive(size_t region_begin, size_t region_end) const;

    StagingBufferRef GetStagingBuffer(size_t size, MemoryUsage usage, bool deferred = false);

    std::optional<StagingBufferRef> TryGetReservedBuffer(size_t size, MemoryUsage usage,
                                                         bool deferred);

    StagingBufferRef CreateStagingBuffer(size_t size, MemoryUsage usage, bool deferred);

    StagingBuffersCache& GetCache(MemoryUsage usage);

    void ReleaseCache(MemoryUsage usage);

    void ReleaseLevel(StagingBuffersCache& cache, size_t log2);
    size_t Region(size_t iter) const noexcept {
        return iter / region_size;
    }

    const Device& device;
    MemoryAllocator& memory_allocator;
    Scheduler& scheduler;

    vk::Buffer stream_buffer;
    std::span<u8> stream_pointer;
    VkDeviceSize stream_buffer_size;
    VkDeviceSize region_size;

    size_t iterator = 0;
    size_t used_iterator = 0;
    size_t free_iterator = 0;
    std::array<u64, NUM_SYNCS> sync_ticks{};

    StagingBuffersCache device_local_cache;
    StagingBuffersCache upload_cache;
    StagingBuffersCache download_cache;

    size_t current_delete_level = 0;
    u64 buffer_index = 0;
    u64 unique_ids{};
};

} // namespace Vulkan
