// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

#include "common/logging/log.h"
#include "common/polyfill_ranges.h"
#include "common/settings.h"
#include "core/core.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"
#include "video_core/vulkan_common/vk_enum_string_helper.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"
#include "vulkan/vulkan_core.h"

namespace Vulkan {

namespace {

VkSurfaceFormatKHR ChooseSwapSurfaceFormat(vk::Span<VkSurfaceFormatKHR> formats) {
    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        VkSurfaceFormatKHR format;
        format.format = VK_FORMAT_B8G8R8A8_UNORM;
        format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        return format;
    }
    const auto& found = std::find_if(formats.begin(), formats.end(), [](const auto& format) {
        return format.format == VK_FORMAT_B8G8R8A8_UNORM &&
               format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    });
    return found != formats.end() ? *found : formats[0];
}

static VkPresentModeKHR ChooseSwapPresentMode(bool has_imm, bool has_mailbox,
                                              bool has_fifo_relaxed) {
    // Mailbox doesn't lock the application like FIFO (vsync)
    // FIFO present mode locks the framerate to the monitor's refresh rate
    Settings::VSyncMode setting = [has_imm, has_mailbox]() {
        // Choose Mailbox or Immediate if unlocked and those modes are supported
        const auto mode = Settings::values.vsync_mode.GetValue();
        if (Settings::values.use_speed_limit.GetValue()) {
            return mode;
        }
        switch (mode) {
        case Settings::VSyncMode::Fifo:
        case Settings::VSyncMode::FifoRelaxed:
            if (has_mailbox) {
                return Settings::VSyncMode::Mailbox;
            } else if (has_imm) {
                return Settings::VSyncMode::Immediate;
            }
            [[fallthrough]];
        default:
            return mode;
        }
    }();
    if ((setting == Settings::VSyncMode::Mailbox && !has_mailbox) ||
        (setting == Settings::VSyncMode::Immediate && !has_imm) ||
        (setting == Settings::VSyncMode::FifoRelaxed && !has_fifo_relaxed)) {
        setting = Settings::VSyncMode::Fifo;
    }

    switch (setting) {
    case Settings::VSyncMode::Immediate:
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    case Settings::VSyncMode::Mailbox:
        return VK_PRESENT_MODE_MAILBOX_KHR;
    case Settings::VSyncMode::Fifo:
        return VK_PRESENT_MODE_FIFO_KHR;
    case Settings::VSyncMode::FifoRelaxed:
        return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    default:
        return VK_PRESENT_MODE_FIFO_KHR;
    }
}

VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, u32 width, u32 height) {
    constexpr auto undefined_size{std::numeric_limits<u32>::max()};
    if (capabilities.currentExtent.width != undefined_size) {
        return capabilities.currentExtent;
    }
    VkExtent2D extent;
    extent.width = std::max(capabilities.minImageExtent.width,
                            std::min(capabilities.maxImageExtent.width, width));
    extent.height = std::max(capabilities.minImageExtent.height,
                             std::min(capabilities.maxImageExtent.height, height));
    return extent;
}

VkCompositeAlphaFlagBitsKHR ChooseAlphaFlags(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
        return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    } else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
        return VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    } else {
        LOG_ERROR(Render_Vulkan, "Unknown composite alpha flags value {:#x}",
                  capabilities.supportedCompositeAlpha);
        return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }
}

} // Anonymous namespace

Swapchain::Swapchain(VkSurfaceKHR surface_, const Device& device_, Scheduler& scheduler_,
                     u32 width_, u32 height_)
    : surface{surface_}, device{device_}, scheduler{scheduler_} {
    Create(surface_, width_, height_);
}

Swapchain::~Swapchain() = default;

void Swapchain::Create(VkSurfaceKHR surface_, u32 width_, u32 height_) {
    is_outdated = false;
    is_suboptimal = false;
    width = width_;
    height = height_;
    surface = surface_;

    const auto physical_device = device.GetPhysical();
    const auto capabilities{physical_device.GetSurfaceCapabilitiesKHR(surface)};
    if (capabilities.maxImageExtent.width == 0 || capabilities.maxImageExtent.height == 0) {
        return;
    }

    Destroy();

    CreateSwapchain(capabilities);
    CreateSemaphores();

    resource_ticks.clear();
    resource_ticks.resize(image_count);
}

bool Swapchain::AcquireNextImage() {
    const VkResult result = device.GetLogical().AcquireNextImageKHR(
        *swapchain, std::numeric_limits<u64>::max(), *present_semaphores[frame_index],
        VK_NULL_HANDLE, &image_index);
    switch (result) {
    case VK_SUCCESS:
        break;
    case VK_SUBOPTIMAL_KHR:
        is_suboptimal = true;
        break;
    case VK_ERROR_OUT_OF_DATE_KHR:
        is_outdated = true;
        break;
    case VK_ERROR_SURFACE_LOST_KHR:
        vk::Check(result);
        break;
    default:
        LOG_ERROR(Render_Vulkan, "vkAcquireNextImageKHR returned {}", string_VkResult(result));
        break;
    }

    scheduler.Wait(resource_ticks[image_index]);
    resource_ticks[image_index] = scheduler.CurrentTick();

    return is_suboptimal || is_outdated;
}

void Swapchain::Present(VkSemaphore render_semaphore) {
    const auto present_queue{device.GetPresentQueue()};
    const VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = render_semaphore ? 1U : 0U,
        .pWaitSemaphores = &render_semaphore,
        .swapchainCount = 1,
        .pSwapchains = swapchain.address(),
        .pImageIndices = &image_index,
        .pResults = nullptr,
    };
    std::scoped_lock lock{scheduler.submit_mutex};
    switch (const VkResult result = present_queue.Present(present_info)) {
    case VK_SUCCESS:
        break;
    case VK_SUBOPTIMAL_KHR:
        LOG_DEBUG(Render_Vulkan, "Suboptimal swapchain");
        break;
    case VK_ERROR_OUT_OF_DATE_KHR:
        is_outdated = true;
        break;
    case VK_ERROR_SURFACE_LOST_KHR:
        vk::Check(result);
        break;
    default:
        LOG_CRITICAL(Render_Vulkan, "Failed to present with error {}", string_VkResult(result));
        break;
    }
    ++frame_index;
    if (frame_index >= image_count) {
        frame_index = 0;
    }
}

void Swapchain::CreateSwapchain(const VkSurfaceCapabilitiesKHR& capabilities) {
    const auto physical_device{device.GetPhysical()};
    const auto formats{physical_device.GetSurfaceFormatsKHR(surface)};
    const auto present_modes = physical_device.GetSurfacePresentModesKHR(surface);
    has_mailbox = std::find(present_modes.begin(), present_modes.end(),
                            VK_PRESENT_MODE_MAILBOX_KHR) != present_modes.end();
    has_imm = std::find(present_modes.begin(), present_modes.end(),
                        VK_PRESENT_MODE_IMMEDIATE_KHR) != present_modes.end();
    has_fifo_relaxed = std::find(present_modes.begin(), present_modes.end(),
                                 VK_PRESENT_MODE_FIFO_RELAXED_KHR) != present_modes.end();

    const VkCompositeAlphaFlagBitsKHR alpha_flags{ChooseAlphaFlags(capabilities)};
    surface_format = ChooseSwapSurfaceFormat(formats);
    present_mode = ChooseSwapPresentMode(has_imm, has_mailbox, has_fifo_relaxed);

    u32 requested_image_count{capabilities.minImageCount + 1};
    // Ensure Triple buffering if possible.
    if (capabilities.maxImageCount > 0) {
        if (requested_image_count > capabilities.maxImageCount) {
            requested_image_count = capabilities.maxImageCount;
        } else {
            requested_image_count =
                std::max(requested_image_count, std::min(3U, capabilities.maxImageCount));
        }
    } else {
        requested_image_count = std::max(requested_image_count, 3U);
    }
    VkSwapchainCreateInfoKHR swapchain_ci{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = surface,
        .minImageCount = requested_image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = {},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
#ifdef ANDROID
        // On Android, do not allow surface rotation to deviate from the frontend.
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
#else
        .preTransform = capabilities.currentTransform,
#endif
        .compositeAlpha = alpha_flags,
        .presentMode = present_mode,
        .clipped = VK_FALSE,
        .oldSwapchain = nullptr,
    };
    const u32 graphics_family{device.GetGraphicsFamily()};
    const u32 present_family{device.GetPresentFamily()};
    const std::array<u32, 2> queue_indices{graphics_family, present_family};
    if (graphics_family != present_family) {
        swapchain_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_ci.queueFamilyIndexCount = static_cast<u32>(queue_indices.size());
        swapchain_ci.pQueueFamilyIndices = queue_indices.data();
    }
    static constexpr std::array view_formats{VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB};
    VkImageFormatListCreateInfo format_list{
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR,
        .pNext = nullptr,
        .viewFormatCount = static_cast<u32>(view_formats.size()),
        .pViewFormats = view_formats.data(),
    };
    if (device.IsKhrSwapchainMutableFormatEnabled()) {
        format_list.pNext = std::exchange(swapchain_ci.pNext, &format_list);
        swapchain_ci.flags |= VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR;
    }
    // Request the size again to reduce the possibility of a TOCTOU race condition.
    const auto updated_capabilities = physical_device.GetSurfaceCapabilitiesKHR(surface);
    swapchain_ci.imageExtent = ChooseSwapExtent(updated_capabilities, width, height);
    // Don't add code within this and the swapchain creation.
    swapchain = device.GetLogical().CreateSwapchainKHR(swapchain_ci);

    extent = swapchain_ci.imageExtent;

    images = swapchain.GetImages();
    image_count = static_cast<u32>(images.size());
#ifdef ANDROID
    // Android is already ordered the same as Switch.
    image_view_format = VK_FORMAT_R8G8B8A8_UNORM;
#else
    image_view_format = VK_FORMAT_B8G8R8A8_UNORM;
#endif
}

void Swapchain::CreateSemaphores() {
    present_semaphores.resize(image_count);
    std::ranges::generate(present_semaphores,
                          [this] { return device.GetLogical().CreateSemaphore(); });
    render_semaphores.resize(image_count);
    std::ranges::generate(render_semaphores,
                          [this] { return device.GetLogical().CreateSemaphore(); });
}

void Swapchain::Destroy() {
    frame_index = 0;
    present_semaphores.clear();
    swapchain.reset();
}

bool Swapchain::NeedsPresentModeUpdate() const {
    const auto requested_mode = ChooseSwapPresentMode(has_imm, has_mailbox, has_fifo_relaxed);
    return present_mode != requested_mode;
}

} // namespace Vulkan
