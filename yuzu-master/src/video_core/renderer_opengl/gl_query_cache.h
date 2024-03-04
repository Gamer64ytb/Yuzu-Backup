// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <vector>

#include "common/common_types.h"
#include "video_core/host1x/gpu_device_memory_manager.h"
#include "video_core/query_cache.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace Core {
class System;
}

namespace OpenGL {

class CachedQuery;
class HostCounter;
class QueryCache;
class RasterizerOpenGL;

using CounterStream = VideoCommon::CounterStreamBase<QueryCache, HostCounter>;

class QueryCache final
    : public VideoCommon::QueryCacheLegacy<QueryCache, CachedQuery, CounterStream, HostCounter> {
public:
    explicit QueryCache(RasterizerOpenGL& rasterizer_,
                        Tegra::MaxwellDeviceMemoryManager& device_memory_);
    ~QueryCache();

    OGLQuery AllocateQuery(VideoCore::QueryType type);

    void Reserve(VideoCore::QueryType type, OGLQuery&& query);

    bool AnyCommandQueued() const noexcept;

private:
    RasterizerOpenGL& gl_rasterizer;
    std::array<std::vector<OGLQuery>, VideoCore::NumQueryTypes> query_pools;
};

class HostCounter final : public VideoCommon::HostCounterBase<QueryCache, HostCounter> {
public:
    explicit HostCounter(QueryCache& cache_, std::shared_ptr<HostCounter> dependency_,
                         VideoCore::QueryType type_);
    ~HostCounter();

    void EndQuery();

private:
    u64 BlockingQuery(bool async = false) const override;

    QueryCache& cache;
    const VideoCore::QueryType type;
    OGLQuery query;
};

class CachedQuery final : public VideoCommon::CachedQueryBase<HostCounter> {
public:
    explicit CachedQuery(QueryCache& cache_, VideoCore::QueryType type_, VAddr cpu_addr_,
                         u8* host_ptr_);
    ~CachedQuery() override;

    CachedQuery(CachedQuery&& rhs) noexcept;
    CachedQuery& operator=(CachedQuery&& rhs) noexcept;

    CachedQuery(const CachedQuery&) = delete;
    CachedQuery& operator=(const CachedQuery&) = delete;

    u64 Flush(bool async = false) override;

private:
    QueryCache* cache;
    VideoCore::QueryType type;
};

} // namespace OpenGL
