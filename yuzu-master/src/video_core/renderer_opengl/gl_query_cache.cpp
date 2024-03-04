// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include <glad/glad.h>

#include "core/core.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_opengl/gl_query_cache.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"

namespace OpenGL {

namespace {

constexpr GLenum GetTarget(VideoCore::QueryType type) {
    switch (type) {
    case VideoCore::QueryType::SamplesPassed:
        return GL_SAMPLES_PASSED;
    case VideoCore::QueryType::PrimitivesGenerated:
        return GL_PRIMITIVES_GENERATED;
    case VideoCore::QueryType::TfbPrimitivesWritten:
        return GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN;
    default:
        break;
    }
    UNIMPLEMENTED_MSG("Query type {}", type);
    return 0;
}

} // Anonymous namespace

QueryCache::QueryCache(RasterizerOpenGL& rasterizer_,
                       Tegra::MaxwellDeviceMemoryManager& device_memory_)
    : QueryCacheLegacy(rasterizer_, device_memory_), gl_rasterizer{rasterizer_} {
    EnableCounters();
}

QueryCache::~QueryCache() = default;

OGLQuery QueryCache::AllocateQuery(VideoCore::QueryType type) {
    auto& reserve = query_pools[static_cast<std::size_t>(type)];
    OGLQuery query;
    if (reserve.empty()) {
        query.Create(GetTarget(type));
        return query;
    }

    query = std::move(reserve.back());
    reserve.pop_back();
    return query;
}

void QueryCache::Reserve(VideoCore::QueryType type, OGLQuery&& query) {
    query_pools[static_cast<std::size_t>(type)].push_back(std::move(query));
}

bool QueryCache::AnyCommandQueued() const noexcept {
    return gl_rasterizer.AnyCommandQueued();
}

HostCounter::HostCounter(QueryCache& cache_, std::shared_ptr<HostCounter> dependency_,
                         VideoCore::QueryType type_)
    : HostCounterBase{std::move(dependency_)}, cache{cache_}, type{type_}, query{
                                                                               cache.AllocateQuery(
                                                                                   type)} {
    glBeginQuery(GetTarget(type), query.handle);
}

HostCounter::~HostCounter() {
    cache.Reserve(type, std::move(query));
}

void HostCounter::EndQuery() {
    if (!cache.AnyCommandQueued()) {
        // There are chances a query waited on without commands (glDraw, glClear, glDispatch). Not
        // having any of these causes a lock. glFlush is considered a command, so we can safely wait
        // for this. Insert to the OpenGL command stream a flush.
        glFlush();
    }
    glEndQuery(GetTarget(type));
}

u64 HostCounter::BlockingQuery([[maybe_unused]] bool async) const {
    GLint64 value;
    glGetQueryObjecti64v(query.handle, GL_QUERY_RESULT, &value);
    return static_cast<u64>(value);
}

CachedQuery::CachedQuery(QueryCache& cache_, VideoCore::QueryType type_, VAddr cpu_addr_,
                         u8* host_ptr_)
    : CachedQueryBase{cpu_addr_, host_ptr_}, cache{&cache_}, type{type_} {}

CachedQuery::~CachedQuery() = default;

CachedQuery::CachedQuery(CachedQuery&& rhs) noexcept
    : CachedQueryBase(std::move(rhs)), cache{rhs.cache}, type{rhs.type} {}

CachedQuery& CachedQuery::operator=(CachedQuery&& rhs) noexcept {
    cache = rhs.cache;
    type = rhs.type;
    CachedQueryBase<HostCounter>::operator=(std::move(rhs));
    return *this;
}

u64 CachedQuery::Flush([[maybe_unused]] bool async) {
    // Waiting for a query while another query of the same target is enabled locks Nvidia's driver.
    // To avoid this disable and re-enable keeping the dependency stream.
    // But we only have to do this if we have pending waits to be done.
    auto& stream = cache->Stream(type);
    const bool slice_counter = WaitPending() && stream.IsEnabled();
    if (slice_counter) {
        stream.Disable();
    }

    auto result = VideoCommon::CachedQueryBase<HostCounter>::Flush();

    if (slice_counter) {
        stream.Enable();
    }

    return result;
}

} // namespace OpenGL
