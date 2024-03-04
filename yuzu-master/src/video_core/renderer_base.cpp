// SPDX-FileCopyrightText: 2015 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <thread>

#include "common/logging/log.h"
#include "core/frontend/emu_window.h"
#include "core/frontend/graphics_context.h"
#include "video_core/renderer_base.h"

namespace VideoCore {

RendererBase::RendererBase(Core::Frontend::EmuWindow& window_,
                           std::unique_ptr<Core::Frontend::GraphicsContext> context_)
    : render_window{window_}, context{std::move(context_)} {
    RefreshBaseSettings();
}

RendererBase::~RendererBase() = default;

void RendererBase::RefreshBaseSettings() {
    UpdateCurrentFramebufferLayout();
}

void RendererBase::UpdateCurrentFramebufferLayout() {
    const Layout::FramebufferLayout& layout = render_window.GetFramebufferLayout();

    render_window.UpdateCurrentFramebufferLayout(layout.width, layout.height);
}

bool RendererBase::IsScreenshotPending() const {
    return renderer_settings.screenshot_requested;
}

void RendererBase::RequestScreenshot(void* data, std::function<void(bool)> callback,
                                     const Layout::FramebufferLayout& layout) {
    if (renderer_settings.screenshot_requested) {
        LOG_ERROR(Render, "A screenshot is already requested or in progress, ignoring the request");
        return;
    }
    auto async_callback{[callback_ = std::move(callback)](bool invert_y) {
        std::thread t{callback_, invert_y};
        t.detach();
    }};
    renderer_settings.screenshot_bits = data;
    renderer_settings.screenshot_complete_callback = async_callback;
    renderer_settings.screenshot_framebuffer_layout = layout;
    renderer_settings.screenshot_requested = true;
}

} // namespace VideoCore
