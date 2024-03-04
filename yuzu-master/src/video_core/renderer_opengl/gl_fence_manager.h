// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "common/common_types.h"
#include "video_core/fence_manager.h"
#include "video_core/renderer_opengl/gl_buffer_cache.h"
#include "video_core/renderer_opengl/gl_query_cache.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_texture_cache.h"

namespace OpenGL {

class GLInnerFence : public VideoCommon::FenceBase {
public:
    explicit GLInnerFence(bool is_stubbed_);
    ~GLInnerFence();

    void Queue();

    bool IsSignaled() const;

    void Wait();

private:
    OGLSync sync_object;
};

using Fence = std::shared_ptr<GLInnerFence>;

struct FenceManagerParams {
    using FenceType = Fence;
    using BufferCacheType = BufferCache;
    using TextureCacheType = TextureCache;
    using QueryCacheType = QueryCache;

    static constexpr bool HAS_ASYNC_CHECK = false;
};

using GenericFenceManager = VideoCommon::FenceManager<FenceManagerParams>;

class FenceManagerOpenGL final : public GenericFenceManager {
public:
    explicit FenceManagerOpenGL(VideoCore::RasterizerInterface& rasterizer, Tegra::GPU& gpu,
                                TextureCache& texture_cache, BufferCache& buffer_cache,
                                QueryCache& query_cache);

protected:
    Fence CreateFence(bool is_stubbed) override;
    void QueueFence(Fence& fence) override;
    bool IsFenceSignaled(Fence& fence) const override;
    void WaitFence(Fence& fence) override;
};

} // namespace OpenGL
