// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <utility>

#include "common/common_types.h"
#include "core/frontend/framebuffer_layout.h"

namespace Core::Frontend {

class GraphicsContext;

/// Information for the Graphics Backends signifying what type of screen pointer is in
/// WindowInformation
enum class WindowSystemType {
    Headless,
    Windows,
    X11,
    Wayland,
    Cocoa,
    Android,
};

/**
 * Abstraction class used to provide an interface between emulation code and the frontend
 * (e.g. SDL, QGLWidget, GLFW, etc...).
 *
 * Design notes on the interaction between EmuWindow and the emulation core:
 * - Generally, decisions on anything visible to the user should be left up to the GUI.
 *   For example, the emulation core should not try to dictate some window title or size.
 *   This stuff is not the core's business and only causes problems with regards to thread-safety
 *   anyway.
 * - Under certain circumstances, it may be desirable for the core to politely request the GUI
 *   to set e.g. a minimum window size. However, the GUI should always be free to ignore any
 *   such hints.
 * - EmuWindow may expose some of its state as read-only to the emulation core, however care
 *   should be taken to make sure the provided information is self-consistent. This requires
 *   some sort of synchronization (most of this is still a TODO).
 * - DO NOT TREAT THIS CLASS AS A GUI TOOLKIT ABSTRACTION LAYER. That's not what it is. Please
 *   re-read the upper points again and think about it if you don't see this.
 */
class EmuWindow {
public:
    /// Data structure to store emuwindow configuration
    struct WindowConfig {
        bool fullscreen = false;
        int res_width = 0;
        int res_height = 0;
        std::pair<u32, u32> min_client_area_size;
    };

    /// Data describing host window system information
    struct WindowSystemInfo {
        // Window system type. Determines which GL context or Vulkan WSI is used.
        WindowSystemType type = WindowSystemType::Headless;

        // Connection to a display server. This is used on X11 and Wayland platforms.
        void* display_connection = nullptr;

        // Render surface. This is a pointer to the native window handle, which depends
        // on the platform. e.g. HWND for Windows, Window for X11. If the surface is
        // set to nullptr, the video backend will run in headless mode.
        void* render_surface = nullptr;

        // Scale of the render surface. For hidpi systems, this will be >1.
        float render_surface_scale = 1.0f;
    };

    /// Called from GPU thread when a frame is displayed.
    virtual void OnFrameDisplayed() {}

    /**
     * Returns a GraphicsContext that the frontend provides to be used for rendering.
     */
    virtual std::unique_ptr<GraphicsContext> CreateSharedContext() const = 0;

    /// Returns if window is shown (not minimized)
    virtual bool IsShown() const = 0;

    /**
     * Returns currently active configuration.
     * @note Accesses to the returned object need not be consistent because it may be modified in
     * another thread
     */
    const WindowConfig& GetActiveConfig() const {
        return active_config;
    }

    bool StrictContextRequired() const {
        return strict_context_required;
    }

    /**
     * Requests the internal configuration to be replaced by the specified argument at some point in
     * the future.
     * @note This method is thread-safe, because it delays configuration changes to the GUI event
     * loop. Hence there is no guarantee on when the requested configuration will be active.
     */
    void SetConfig(const WindowConfig& val) {
        config = val;
    }

    /**
     * Returns system information about the drawing area.
     */
    const WindowSystemInfo& GetWindowInfo() const {
        return window_info;
    }

    /**
     * Gets the framebuffer layout (width, height, and screen regions)
     * @note This method is thread-safe
     */
    const Layout::FramebufferLayout& GetFramebufferLayout() const {
        return framebuffer_layout;
    }

    /**
     * Convenience method to update the current frame layout
     * Read from the current settings to determine which layout to use.
     */
    void UpdateCurrentFramebufferLayout(u32 width, u32 height);

protected:
    explicit EmuWindow();
    virtual ~EmuWindow();

    /**
     * Processes any pending configuration changes from the last SetConfig call.
     * This method invokes OnMinimalClientAreaChangeRequest if the corresponding configuration
     * field changed.
     * @note Implementations will usually want to call this from the GUI thread.
     * @todo Actually call this in existing implementations.
     */
    void ProcessConfigurationChanges() {
        // TODO: For proper thread safety, we should eventually implement a proper
        // multiple-writer/single-reader queue...

        if (config.min_client_area_size != active_config.min_client_area_size) {
            OnMinimalClientAreaChangeRequest(config.min_client_area_size);
            config.min_client_area_size = active_config.min_client_area_size;
        }
    }

    /**
     * Update framebuffer layout with the given parameter.
     * @note EmuWindow implementations will usually use this in window resize event handlers.
     */
    void NotifyFramebufferLayoutChanged(const Layout::FramebufferLayout& layout) {
        framebuffer_layout = layout;
    }

    /**
     * Update internal client area size with the given parameter.
     * @note EmuWindow implementations will usually use this in window resize event handlers.
     */
    void NotifyClientAreaSizeChanged(std::pair<u32, u32> size) {
        client_area_width = size.first;
        client_area_height = size.second;
    }

    /**
     * Converts a screen position into the equivalent touchscreen position.
     */
    std::pair<f32, f32> MapToTouchScreen(u32 framebuffer_x, u32 framebuffer_y) const;

    /**
     * Clip the provided coordinates to be inside the touchscreen area.
     */
    std::pair<u32, u32> ClipToTouchScreen(u32 new_x, u32 new_y) const;

    WindowSystemInfo window_info;

    bool strict_context_required = false;

private:
    /**
     * Handler called when the minimal client area was requested to be changed via SetConfig.
     * For the request to be honored, EmuWindow implementations will usually reimplement this
     * function.
     */
    virtual void OnMinimalClientAreaChangeRequest(std::pair<u32, u32>) {
        // By default, ignore this request and do nothing.
    }

    Layout::FramebufferLayout framebuffer_layout; ///< Current framebuffer layout

    u32 client_area_width;  ///< Current client width, should be set by window impl.
    u32 client_area_height; ///< Current client height, should be set by window impl.

    WindowConfig config;        ///< Internal configuration (changes pending for being applied in
                                /// ProcessConfigurationChanges)
    WindowConfig active_config; ///< Internal active configuration
};

} // namespace Core::Frontend
