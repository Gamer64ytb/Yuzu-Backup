// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "common/common_types.h"
#include "common/logging/log.h"
#include "video_core/vulkan_common/vk_enum_string_helper.h"
#include "video_core/vulkan_common/vma.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan::vk {

namespace {

template <typename Func>
void SortPhysicalDevices(std::vector<VkPhysicalDevice>& devices, const InstanceDispatch& dld,
                         Func&& func) {
    // Calling GetProperties calls Vulkan more than needed. But they are supposed to be cheap
    // functions.
    std::stable_sort(devices.begin(), devices.end(),
                     [&dld, &func](VkPhysicalDevice lhs, VkPhysicalDevice rhs) {
                         return func(vk::PhysicalDevice(lhs, dld).GetProperties(),
                                     vk::PhysicalDevice(rhs, dld).GetProperties());
                     });
}

void SortPhysicalDevicesPerVendor(std::vector<VkPhysicalDevice>& devices,
                                  const InstanceDispatch& dld,
                                  std::initializer_list<u32> vendor_ids) {
    for (auto it = vendor_ids.end(); it != vendor_ids.begin();) {
        --it;
        SortPhysicalDevices(devices, dld, [id = *it](const auto& lhs, const auto& rhs) {
            return lhs.vendorID == id && rhs.vendorID != id;
        });
    }
}

bool IsMicrosoftDozen(const char* device_name) {
    return std::strstr(device_name, "Microsoft") != nullptr;
}

void SortPhysicalDevices(std::vector<VkPhysicalDevice>& devices, const InstanceDispatch& dld) {
    // Sort by name, this will set a base and make GPUs with higher numbers appear first
    // (e.g. GTX 1650 will intentionally be listed before a GTX 1080).
    SortPhysicalDevices(devices, dld, [](const auto& lhs, const auto& rhs) {
        return std::string_view{lhs.deviceName} > std::string_view{rhs.deviceName};
    });
    // Prefer discrete over non-discrete
    SortPhysicalDevices(devices, dld, [](const auto& lhs, const auto& rhs) {
        return lhs.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
               rhs.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    });
    // Prefer Nvidia over AMD, AMD over Intel, Intel over the rest.
    SortPhysicalDevicesPerVendor(devices, dld, {0x10DE, 0x1002, 0x8086});
    // Demote Microsoft's Dozen devices to the bottom.
    SortPhysicalDevices(
        devices, dld,
        [](const VkPhysicalDeviceProperties& lhs, const VkPhysicalDeviceProperties& rhs) {
            return IsMicrosoftDozen(rhs.deviceName) && !IsMicrosoftDozen(lhs.deviceName);
        });
}

template <typename T>
bool Proc(T& result, const InstanceDispatch& dld, const char* proc_name,
          VkInstance instance = nullptr) noexcept {
    result = reinterpret_cast<T>(dld.vkGetInstanceProcAddr(instance, proc_name));
    return result != nullptr;
}

template <typename T>
void Proc(T& result, const DeviceDispatch& dld, const char* proc_name, VkDevice device) noexcept {
    result = reinterpret_cast<T>(dld.vkGetDeviceProcAddr(device, proc_name));
}

void Load(VkDevice device, DeviceDispatch& dld) noexcept {
#define X(name) Proc(dld.name, dld, #name, device)
    X(vkAcquireNextImageKHR);
    X(vkAllocateCommandBuffers);
    X(vkAllocateDescriptorSets);
    X(vkAllocateMemory);
    X(vkBeginCommandBuffer);
    X(vkBindBufferMemory);
    X(vkBindImageMemory);
    X(vkCmdBeginConditionalRenderingEXT);
    X(vkCmdBeginQuery);
    X(vkCmdBeginRenderPass);
    X(vkCmdBeginTransformFeedbackEXT);
    X(vkCmdBeginDebugUtilsLabelEXT);
    X(vkCmdBindDescriptorSets);
    X(vkCmdBindIndexBuffer);
    X(vkCmdBindPipeline);
    X(vkCmdBindTransformFeedbackBuffersEXT);
    X(vkCmdBindVertexBuffers);
    X(vkCmdBlitImage);
    X(vkCmdClearAttachments);
    X(vkCmdClearColorImage);
    X(vkCmdCopyBuffer);
    X(vkCmdCopyBufferToImage);
    X(vkCmdCopyImage);
    X(vkCmdCopyImageToBuffer);
    X(vkCmdCopyQueryPoolResults);
    X(vkCmdDispatch);
    X(vkCmdDispatchIndirect);
    X(vkCmdDraw);
    X(vkCmdDrawIndexed);
    X(vkCmdDrawIndirect);
    X(vkCmdDrawIndexedIndirect);
    X(vkCmdDrawIndirectCount);
    X(vkCmdDrawIndexedIndirectCount);
    X(vkCmdDrawIndirectByteCountEXT);
    X(vkCmdEndConditionalRenderingEXT);
    X(vkCmdEndQuery);
    X(vkCmdEndRenderPass);
    X(vkCmdEndTransformFeedbackEXT);
    X(vkCmdEndDebugUtilsLabelEXT);
    X(vkCmdFillBuffer);
    X(vkCmdPipelineBarrier);
    X(vkCmdPushConstants);
    X(vkCmdPushDescriptorSetWithTemplateKHR);
    X(vkCmdSetBlendConstants);
    X(vkCmdSetDepthBias);
    X(vkCmdSetDepthBias2EXT);
    X(vkCmdSetDepthBounds);
    X(vkCmdSetEvent);
    X(vkCmdSetScissor);
    X(vkCmdSetStencilCompareMask);
    X(vkCmdSetStencilReference);
    X(vkCmdSetStencilWriteMask);
    X(vkCmdSetViewport);
    X(vkCmdWaitEvents);
    X(vkCmdBindVertexBuffers2EXT);
    X(vkCmdSetCullModeEXT);
    X(vkCmdSetDepthBoundsTestEnableEXT);
    X(vkCmdSetDepthCompareOpEXT);
    X(vkCmdSetDepthTestEnableEXT);
    X(vkCmdSetDepthWriteEnableEXT);
    X(vkCmdSetPrimitiveRestartEnableEXT);
    X(vkCmdSetRasterizerDiscardEnableEXT);
    X(vkCmdSetDepthBiasEnableEXT);
    X(vkCmdSetLogicOpEnableEXT);
    X(vkCmdSetDepthClampEnableEXT);
    X(vkCmdSetFrontFaceEXT);
    X(vkCmdSetLogicOpEXT);
    X(vkCmdSetPatchControlPointsEXT);
    X(vkCmdSetLineWidth);
    X(vkCmdSetPrimitiveTopologyEXT);
    X(vkCmdSetStencilOpEXT);
    X(vkCmdSetStencilTestEnableEXT);
    X(vkCmdSetVertexInputEXT);
    X(vkCmdSetColorWriteMaskEXT);
    X(vkCmdSetColorBlendEnableEXT);
    X(vkCmdSetColorBlendEquationEXT);
    X(vkCmdResolveImage);
    X(vkCreateBuffer);
    X(vkCreateBufferView);
    X(vkCreateCommandPool);
    X(vkCreateComputePipelines);
    X(vkCreateDescriptorPool);
    X(vkCreateDescriptorSetLayout);
    X(vkCreateDescriptorUpdateTemplate);
    X(vkCreateEvent);
    X(vkCreateFence);
    X(vkCreateFramebuffer);
    X(vkCreateGraphicsPipelines);
    X(vkCreateImage);
    X(vkCreateImageView);
    X(vkCreatePipelineCache);
    X(vkCreatePipelineLayout);
    X(vkCreateQueryPool);
    X(vkCreateRenderPass);
    X(vkCreateSampler);
    X(vkCreateSemaphore);
    X(vkCreateShaderModule);
    X(vkCreateSwapchainKHR);
    X(vkDestroyBuffer);
    X(vkDestroyBufferView);
    X(vkDestroyCommandPool);
    X(vkDestroyDescriptorPool);
    X(vkDestroyDescriptorSetLayout);
    X(vkDestroyDescriptorUpdateTemplate);
    X(vkDestroyEvent);
    X(vkDestroyFence);
    X(vkDestroyFramebuffer);
    X(vkDestroyImage);
    X(vkDestroyImageView);
    X(vkDestroyPipeline);
    X(vkDestroyPipelineCache);
    X(vkDestroyPipelineLayout);
    X(vkDestroyQueryPool);
    X(vkDestroyRenderPass);
    X(vkDestroySampler);
    X(vkDestroySemaphore);
    X(vkDestroyShaderModule);
    X(vkDestroySwapchainKHR);
    X(vkDeviceWaitIdle);
    X(vkEndCommandBuffer);
    X(vkFreeCommandBuffers);
    X(vkFreeDescriptorSets);
    X(vkFreeMemory);
    X(vkGetBufferMemoryRequirements2);
    X(vkGetDeviceQueue);
    X(vkGetEventStatus);
    X(vkGetFenceStatus);
    X(vkGetImageMemoryRequirements);
    X(vkGetPipelineCacheData);
    X(vkGetMemoryFdKHR);
#ifdef _WIN32
    X(vkGetMemoryWin32HandleKHR);
#endif
    X(vkGetQueryPoolResults);
    X(vkGetPipelineExecutablePropertiesKHR);
    X(vkGetPipelineExecutableStatisticsKHR);
    X(vkGetSemaphoreCounterValue);
    X(vkMapMemory);
    X(vkQueueSubmit);
    X(vkResetFences);
    X(vkResetQueryPool);
    X(vkSetDebugUtilsObjectNameEXT);
    X(vkSetDebugUtilsObjectTagEXT);
    X(vkUnmapMemory);
    X(vkUpdateDescriptorSetWithTemplate);
    X(vkUpdateDescriptorSets);
    X(vkWaitForFences);
    X(vkWaitSemaphores);

    // Support for timeline semaphores is mandatory in Vulkan 1.2
    if (!dld.vkGetSemaphoreCounterValue) {
        Proc(dld.vkGetSemaphoreCounterValue, dld, "vkGetSemaphoreCounterValueKHR", device);
        Proc(dld.vkWaitSemaphores, dld, "vkWaitSemaphoresKHR", device);
    }

    // Support for host query reset is mandatory in Vulkan 1.2
    if (!dld.vkResetQueryPool) {
        Proc(dld.vkResetQueryPool, dld, "vkResetQueryPoolEXT", device);
    }

    // Support for draw indirect with count is optional in Vulkan 1.2
    if (!dld.vkCmdDrawIndirectCount) {
        Proc(dld.vkCmdDrawIndirectCount, dld, "vkCmdDrawIndirectCountKHR", device);
        Proc(dld.vkCmdDrawIndexedIndirectCount, dld, "vkCmdDrawIndexedIndirectCountKHR", device);
    }
#undef X
}

template <typename T>
void SetObjectName(const DeviceDispatch* dld, VkDevice device, T handle, VkObjectType type,
                   const char* name) {
    const VkDebugUtilsObjectNameInfoEXT name_info{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = nullptr,
        .objectType = type,
        .objectHandle = reinterpret_cast<u64>(handle),
        .pObjectName = name,
    };
    if (dld->vkSetDebugUtilsObjectNameEXT) {
        Check(dld->vkSetDebugUtilsObjectNameEXT(device, &name_info));
    }
}

} // Anonymous namespace

bool Load(InstanceDispatch& dld) noexcept {
#define X(name) Proc(dld.name, dld, #name)
    return X(vkCreateInstance) && X(vkEnumerateInstanceExtensionProperties) &&
           X(vkEnumerateInstanceLayerProperties);
#undef X
}

bool Load(VkInstance instance, InstanceDispatch& dld) noexcept {
#define X(name) Proc(dld.name, dld, #name, instance)
    // These functions may fail to load depending on the enabled extensions.
    // Don't return a failure on these.
    X(vkCreateDebugUtilsMessengerEXT);
    X(vkCreateDebugReportCallbackEXT);
    X(vkDestroyDebugUtilsMessengerEXT);
    X(vkDestroyDebugReportCallbackEXT);
    X(vkDestroySurfaceKHR);
    X(vkGetPhysicalDeviceFeatures2);
    X(vkGetPhysicalDeviceProperties2);
    X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    X(vkGetPhysicalDeviceSurfaceFormatsKHR);
    X(vkGetPhysicalDeviceSurfacePresentModesKHR);
    X(vkGetPhysicalDeviceSurfaceSupportKHR);
    X(vkGetPhysicalDeviceToolProperties);
    X(vkGetSwapchainImagesKHR);
    X(vkQueuePresentKHR);

    return X(vkCreateDevice) && X(vkDestroyDevice) && X(vkDestroyDevice) &&
           X(vkEnumerateDeviceExtensionProperties) && X(vkEnumeratePhysicalDevices) &&
           X(vkGetDeviceProcAddr) && X(vkGetPhysicalDeviceFormatProperties) &&
           X(vkGetPhysicalDeviceMemoryProperties) && X(vkGetPhysicalDeviceMemoryProperties2) &&
           X(vkGetPhysicalDeviceProperties) && X(vkGetPhysicalDeviceQueueFamilyProperties);
#undef X
}

const char* Exception::what() const noexcept {
    return string_VkResult(result);
}

void Destroy(VkInstance instance, const InstanceDispatch& dld) noexcept {
    dld.vkDestroyInstance(instance, nullptr);
}

void Destroy(VkDevice device, const InstanceDispatch& dld) noexcept {
    dld.vkDestroyDevice(device, nullptr);
}

void Destroy(VkDevice device, VkBuffer handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyBuffer(device, handle, nullptr);
}

void Destroy(VkDevice device, VkBufferView handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyBufferView(device, handle, nullptr);
}

void Destroy(VkDevice device, VkCommandPool handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyCommandPool(device, handle, nullptr);
}

void Destroy(VkDevice device, VkDescriptorPool handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyDescriptorPool(device, handle, nullptr);
}

void Destroy(VkDevice device, VkDescriptorSetLayout handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyDescriptorSetLayout(device, handle, nullptr);
}

void Destroy(VkDevice device, VkDescriptorUpdateTemplate handle,
             const DeviceDispatch& dld) noexcept {
    dld.vkDestroyDescriptorUpdateTemplate(device, handle, nullptr);
}

void Destroy(VkDevice device, VkDeviceMemory handle, const DeviceDispatch& dld) noexcept {
    dld.vkFreeMemory(device, handle, nullptr);
}

void Destroy(VkDevice device, VkEvent handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyEvent(device, handle, nullptr);
}

void Destroy(VkDevice device, VkFence handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyFence(device, handle, nullptr);
}

void Destroy(VkDevice device, VkFramebuffer handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyFramebuffer(device, handle, nullptr);
}

void Destroy(VkDevice device, VkImage handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyImage(device, handle, nullptr);
}

void Destroy(VkDevice device, VkImageView handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyImageView(device, handle, nullptr);
}

void Destroy(VkDevice device, VkPipeline handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyPipeline(device, handle, nullptr);
}

void Destroy(VkDevice device, VkPipelineCache handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyPipelineCache(device, handle, nullptr);
}

void Destroy(VkDevice device, VkPipelineLayout handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyPipelineLayout(device, handle, nullptr);
}

void Destroy(VkDevice device, VkQueryPool handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyQueryPool(device, handle, nullptr);
}

void Destroy(VkDevice device, VkRenderPass handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyRenderPass(device, handle, nullptr);
}

void Destroy(VkDevice device, VkSampler handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroySampler(device, handle, nullptr);
}

void Destroy(VkDevice device, VkSwapchainKHR handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroySwapchainKHR(device, handle, nullptr);
}

void Destroy(VkDevice device, VkSemaphore handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroySemaphore(device, handle, nullptr);
}

void Destroy(VkDevice device, VkShaderModule handle, const DeviceDispatch& dld) noexcept {
    dld.vkDestroyShaderModule(device, handle, nullptr);
}

void Destroy(VkInstance instance, VkDebugUtilsMessengerEXT handle,
             const InstanceDispatch& dld) noexcept {
    dld.vkDestroyDebugUtilsMessengerEXT(instance, handle, nullptr);
}

void Destroy(VkInstance instance, VkDebugReportCallbackEXT handle,
             const InstanceDispatch& dld) noexcept {
    dld.vkDestroyDebugReportCallbackEXT(instance, handle, nullptr);
}

void Destroy(VkInstance instance, VkSurfaceKHR handle, const InstanceDispatch& dld) noexcept {
    dld.vkDestroySurfaceKHR(instance, handle, nullptr);
}

VkResult Free(VkDevice device, VkDescriptorPool handle, Span<VkDescriptorSet> sets,
              const DeviceDispatch& dld) noexcept {
    return dld.vkFreeDescriptorSets(device, handle, sets.size(), sets.data());
}

VkResult Free(VkDevice device, VkCommandPool handle, Span<VkCommandBuffer> buffers,
              const DeviceDispatch& dld) noexcept {
    dld.vkFreeCommandBuffers(device, handle, buffers.size(), buffers.data());
    return VK_SUCCESS;
}

Instance Instance::Create(u32 version, Span<const char*> layers, Span<const char*> extensions,
                          InstanceDispatch& dispatch) {
#ifdef __APPLE__
    constexpr VkFlags ci_flags{VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR};
#else
    constexpr VkFlags ci_flags{};
#endif

    const VkApplicationInfo application_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "yuzu Emulator",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "yuzu Emulator",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };
    const VkInstanceCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = ci_flags,
        .pApplicationInfo = &application_info,
        .enabledLayerCount = layers.size(),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = extensions.size(),
        .ppEnabledExtensionNames = extensions.data(),
    };
    VkInstance instance;
    Check(dispatch.vkCreateInstance(&ci, nullptr, &instance));
    if (!Proc(dispatch.vkDestroyInstance, dispatch, "vkDestroyInstance", instance)) {
        // We successfully created an instance but the destroy function couldn't be loaded.
        // This is a good moment to panic.
        throw vk::Exception(VK_ERROR_INITIALIZATION_FAILED);
    }
    return Instance(instance, dispatch);
}

std::vector<VkPhysicalDevice> Instance::EnumeratePhysicalDevices() const {
    u32 num;
    Check(dld->vkEnumeratePhysicalDevices(handle, &num, nullptr));
    std::vector<VkPhysicalDevice> physical_devices(num);
    Check(dld->vkEnumeratePhysicalDevices(handle, &num, physical_devices.data()));
    SortPhysicalDevices(physical_devices, *dld);
    return physical_devices;
}

DebugUtilsMessenger Instance::CreateDebugUtilsMessenger(
    const VkDebugUtilsMessengerCreateInfoEXT& create_info) const {
    VkDebugUtilsMessengerEXT object;
    Check(dld->vkCreateDebugUtilsMessengerEXT(handle, &create_info, nullptr, &object));
    return DebugUtilsMessenger(object, handle, *dld);
}

DebugReportCallback Instance::CreateDebugReportCallback(
    const VkDebugReportCallbackCreateInfoEXT& create_info) const {
    VkDebugReportCallbackEXT object;
    Check(dld->vkCreateDebugReportCallbackEXT(handle, &create_info, nullptr, &object));
    return DebugReportCallback(object, handle, *dld);
}

void Image::SetObjectNameEXT(const char* name) const {
    SetObjectName(dld, owner, handle, VK_OBJECT_TYPE_IMAGE, name);
}

void Image::Release() const noexcept {
    if (handle) {
        vmaDestroyImage(allocator, handle, allocation);
    }
}

void Buffer::Flush() const {
    if (!is_coherent) {
        vmaFlushAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);
    }
}

void Buffer::Invalidate() const {
    if (!is_coherent) {
        vmaInvalidateAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);
    }
}

void Buffer::SetObjectNameEXT(const char* name) const {
    SetObjectName(dld, owner, handle, VK_OBJECT_TYPE_BUFFER, name);
}

void Buffer::Release() const noexcept {
    if (handle) {
        vmaDestroyBuffer(allocator, handle, allocation);
    }
}

void BufferView::SetObjectNameEXT(const char* name) const {
    SetObjectName(dld, owner, handle, VK_OBJECT_TYPE_BUFFER_VIEW, name);
}

void ImageView::SetObjectNameEXT(const char* name) const {
    SetObjectName(dld, owner, handle, VK_OBJECT_TYPE_IMAGE_VIEW, name);
}

int DeviceMemory::GetMemoryFdKHR() const {
    const VkMemoryGetFdInfoKHR get_fd_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        .pNext = nullptr,
        .memory = handle,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR,
    };
    int fd;
    Check(dld->vkGetMemoryFdKHR(owner, &get_fd_info, &fd));
    return fd;
}

#ifdef _WIN32
HANDLE DeviceMemory::GetMemoryWin32HandleKHR() const {
    const VkMemoryGetWin32HandleInfoKHR get_win32_handle_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
        .pNext = nullptr,
        .memory = handle,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR,
    };
    HANDLE win32_handle;
    Check(dld->vkGetMemoryWin32HandleKHR(owner, &get_win32_handle_info, &win32_handle));
    return win32_handle;
}
#endif

void DeviceMemory::SetObjectNameEXT(const char* name) const {
    SetObjectName(dld, owner, handle, VK_OBJECT_TYPE_DEVICE_MEMORY, name);
}

void Fence::SetObjectNameEXT(const char* name) const {
    SetObjectName(dld, owner, handle, VK_OBJECT_TYPE_FENCE, name);
}

void Framebuffer::SetObjectNameEXT(const char* name) const {
    SetObjectName(dld, owner, handle, VK_OBJECT_TYPE_FRAMEBUFFER, name);
}

DescriptorSets DescriptorPool::Allocate(const VkDescriptorSetAllocateInfo& ai) const {
    const std::size_t num = ai.descriptorSetCount;
    std::unique_ptr sets = std::make_unique<VkDescriptorSet[]>(num);
    switch (const VkResult result = dld->vkAllocateDescriptorSets(owner, &ai, sets.get())) {
    case VK_SUCCESS:
        return DescriptorSets(std::move(sets), num, owner, handle, *dld);
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return {};
    default:
        throw Exception(result);
    }
}

void DescriptorPool::SetObjectNameEXT(const char* name) const {
    SetObjectName(dld, owner, handle, VK_OBJECT_TYPE_DESCRIPTOR_POOL, name);
}

CommandBuffers CommandPool::Allocate(std::size_t num_buffers, VkCommandBufferLevel level) const {
    const VkCommandBufferAllocateInfo ai{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = handle,
        .level = level,
        .commandBufferCount = static_cast<u32>(num_buffers),
    };

    std::unique_ptr buffers = std::make_unique<VkCommandBuffer[]>(num_buffers);
    switch (const VkResult result = dld->vkAllocateCommandBuffers(owner, &ai, buffers.get())) {
    case VK_SUCCESS:
        return CommandBuffers(std::move(buffers), num_buffers, owner, handle, *dld);
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return {};
    default:
        throw Exception(result);
    }
}

void CommandPool::SetObjectNameEXT(const char* name) const {
    SetObjectName(dld, owner, handle, VK_OBJECT_TYPE_COMMAND_POOL, name);
}

std::vector<VkImage> SwapchainKHR::GetImages() const {
    u32 num;
    Check(dld->vkGetSwapchainImagesKHR(owner, handle, &num, nullptr));
    std::vector<VkImage> images(num);
    Check(dld->vkGetSwapchainImagesKHR(owner, handle, &num, images.data()));
    return images;
}

void Event::SetObjectNameEXT(const char* name) const {
    SetObjectName(dld, owner, handle, VK_OBJECT_TYPE_EVENT, name);
}

void ShaderModule::SetObjectNameEXT(const char* name) const {
    SetObjectName(dld, owner, handle, VK_OBJECT_TYPE_SHADER_MODULE, name);
}

void PipelineCache::SetObjectNameEXT(const char* name) const {
    SetObjectName(dld, owner, handle, VK_OBJECT_TYPE_PIPELINE_CACHE, name);
}

void Semaphore::SetObjectNameEXT(const char* name) const {
    SetObjectName(dld, owner, handle, VK_OBJECT_TYPE_SEMAPHORE, name);
}

Device Device::Create(VkPhysicalDevice physical_device, Span<VkDeviceQueueCreateInfo> queues_ci,
                      Span<const char*> enabled_extensions, const void* next,
                      DeviceDispatch& dispatch) {
    const VkDeviceCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = next,
        .flags = 0,
        .queueCreateInfoCount = queues_ci.size(),
        .pQueueCreateInfos = queues_ci.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = enabled_extensions.size(),
        .ppEnabledExtensionNames = enabled_extensions.data(),
        .pEnabledFeatures = nullptr,
    };
    VkDevice device;
    Check(dispatch.vkCreateDevice(physical_device, &ci, nullptr, &device));
    Load(device, dispatch);
    return Device(device, dispatch);
}

Queue Device::GetQueue(u32 family_index) const noexcept {
    VkQueue queue;
    dld->vkGetDeviceQueue(handle, family_index, 0, &queue);
    return Queue(queue, *dld);
}

BufferView Device::CreateBufferView(const VkBufferViewCreateInfo& ci) const {
    VkBufferView object;
    Check(dld->vkCreateBufferView(handle, &ci, nullptr, &object));
    return BufferView(object, handle, *dld);
}

ImageView Device::CreateImageView(const VkImageViewCreateInfo& ci) const {
    VkImageView object;
    Check(dld->vkCreateImageView(handle, &ci, nullptr, &object));
    return ImageView(object, handle, *dld);
}

Semaphore Device::CreateSemaphore() const {
    static constexpr VkSemaphoreCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    return CreateSemaphore(ci);
}

Semaphore Device::CreateSemaphore(const VkSemaphoreCreateInfo& ci) const {
    VkSemaphore object;
    Check(dld->vkCreateSemaphore(handle, &ci, nullptr, &object));
    return Semaphore(object, handle, *dld);
}

Fence Device::CreateFence(const VkFenceCreateInfo& ci) const {
    VkFence object;
    Check(dld->vkCreateFence(handle, &ci, nullptr, &object));
    return Fence(object, handle, *dld);
}

DescriptorPool Device::CreateDescriptorPool(const VkDescriptorPoolCreateInfo& ci) const {
    VkDescriptorPool object;
    Check(dld->vkCreateDescriptorPool(handle, &ci, nullptr, &object));
    return DescriptorPool(object, handle, *dld);
}

RenderPass Device::CreateRenderPass(const VkRenderPassCreateInfo& ci) const {
    VkRenderPass object;
    Check(dld->vkCreateRenderPass(handle, &ci, nullptr, &object));
    return RenderPass(object, handle, *dld);
}

DescriptorSetLayout Device::CreateDescriptorSetLayout(
    const VkDescriptorSetLayoutCreateInfo& ci) const {
    VkDescriptorSetLayout object;
    Check(dld->vkCreateDescriptorSetLayout(handle, &ci, nullptr, &object));
    return DescriptorSetLayout(object, handle, *dld);
}

PipelineCache Device::CreatePipelineCache(const VkPipelineCacheCreateInfo& ci) const {
    VkPipelineCache cache;
    Check(dld->vkCreatePipelineCache(handle, &ci, nullptr, &cache));
    return PipelineCache(cache, handle, *dld);
}

PipelineLayout Device::CreatePipelineLayout(const VkPipelineLayoutCreateInfo& ci) const {
    VkPipelineLayout object;
    Check(dld->vkCreatePipelineLayout(handle, &ci, nullptr, &object));
    return PipelineLayout(object, handle, *dld);
}

Pipeline Device::CreateGraphicsPipeline(const VkGraphicsPipelineCreateInfo& ci,
                                        VkPipelineCache cache) const {
    VkPipeline object;
    Check(dld->vkCreateGraphicsPipelines(handle, cache, 1, &ci, nullptr, &object));
    return Pipeline(object, handle, *dld);
}

Pipeline Device::CreateComputePipeline(const VkComputePipelineCreateInfo& ci,
                                       VkPipelineCache cache) const {
    VkPipeline object;
    Check(dld->vkCreateComputePipelines(handle, cache, 1, &ci, nullptr, &object));
    return Pipeline(object, handle, *dld);
}

Sampler Device::CreateSampler(const VkSamplerCreateInfo& ci) const {
    VkSampler object;
    Check(dld->vkCreateSampler(handle, &ci, nullptr, &object));
    return Sampler(object, handle, *dld);
}

Framebuffer Device::CreateFramebuffer(const VkFramebufferCreateInfo& ci) const {
    VkFramebuffer object;
    Check(dld->vkCreateFramebuffer(handle, &ci, nullptr, &object));
    return Framebuffer(object, handle, *dld);
}

CommandPool Device::CreateCommandPool(const VkCommandPoolCreateInfo& ci) const {
    VkCommandPool object;
    Check(dld->vkCreateCommandPool(handle, &ci, nullptr, &object));
    return CommandPool(object, handle, *dld);
}

DescriptorUpdateTemplate Device::CreateDescriptorUpdateTemplate(
    const VkDescriptorUpdateTemplateCreateInfo& ci) const {
    VkDescriptorUpdateTemplate object;
    Check(dld->vkCreateDescriptorUpdateTemplate(handle, &ci, nullptr, &object));
    return DescriptorUpdateTemplate(object, handle, *dld);
}

QueryPool Device::CreateQueryPool(const VkQueryPoolCreateInfo& ci) const {
    VkQueryPool object;
    Check(dld->vkCreateQueryPool(handle, &ci, nullptr, &object));
    return QueryPool(object, handle, *dld);
}

ShaderModule Device::CreateShaderModule(const VkShaderModuleCreateInfo& ci) const {
    VkShaderModule object;
    Check(dld->vkCreateShaderModule(handle, &ci, nullptr, &object));
    return ShaderModule(object, handle, *dld);
}

Event Device::CreateEvent() const {
    static constexpr VkEventCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };

    VkEvent object;
    Check(dld->vkCreateEvent(handle, &ci, nullptr, &object));
    return Event(object, handle, *dld);
}

SwapchainKHR Device::CreateSwapchainKHR(const VkSwapchainCreateInfoKHR& ci) const {
    VkSwapchainKHR object;
    Check(dld->vkCreateSwapchainKHR(handle, &ci, nullptr, &object));
    return SwapchainKHR(object, handle, *dld);
}

DeviceMemory Device::TryAllocateMemory(const VkMemoryAllocateInfo& ai) const noexcept {
    VkDeviceMemory memory;
    if (dld->vkAllocateMemory(handle, &ai, nullptr, &memory) != VK_SUCCESS) {
        return {};
    }
    return DeviceMemory(memory, handle, *dld);
}

DeviceMemory Device::AllocateMemory(const VkMemoryAllocateInfo& ai) const {
    VkDeviceMemory memory;
    Check(dld->vkAllocateMemory(handle, &ai, nullptr, &memory));
    return DeviceMemory(memory, handle, *dld);
}

VkMemoryRequirements Device::GetBufferMemoryRequirements(VkBuffer buffer,
                                                         void* pnext) const noexcept {
    const VkBufferMemoryRequirementsInfo2 info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
        .pNext = nullptr,
        .buffer = buffer,
    };
    VkMemoryRequirements2 requirements{
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
        .pNext = pnext,
        .memoryRequirements{},
    };
    dld->vkGetBufferMemoryRequirements2(handle, &info, &requirements);
    return requirements.memoryRequirements;
}

VkMemoryRequirements Device::GetImageMemoryRequirements(VkImage image) const noexcept {
    VkMemoryRequirements requirements;
    dld->vkGetImageMemoryRequirements(handle, image, &requirements);
    return requirements;
}

std::vector<VkPipelineExecutablePropertiesKHR> Device::GetPipelineExecutablePropertiesKHR(
    VkPipeline pipeline) const {
    const VkPipelineInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR,
        .pNext = nullptr,
        .pipeline = pipeline,
    };
    u32 num{};
    dld->vkGetPipelineExecutablePropertiesKHR(handle, &info, &num, nullptr);
    std::vector<VkPipelineExecutablePropertiesKHR> properties(num);
    for (auto& property : properties) {
        property.sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR;
    }
    Check(dld->vkGetPipelineExecutablePropertiesKHR(handle, &info, &num, properties.data()));
    return properties;
}

std::vector<VkPipelineExecutableStatisticKHR> Device::GetPipelineExecutableStatisticsKHR(
    VkPipeline pipeline, u32 executable_index) const {
    const VkPipelineExecutableInfoKHR executable_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR,
        .pNext = nullptr,
        .pipeline = pipeline,
        .executableIndex = executable_index,
    };
    u32 num{};
    dld->vkGetPipelineExecutableStatisticsKHR(handle, &executable_info, &num, nullptr);
    std::vector<VkPipelineExecutableStatisticKHR> statistics(num);
    for (auto& statistic : statistics) {
        statistic.sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR;
    }
    Check(dld->vkGetPipelineExecutableStatisticsKHR(handle, &executable_info, &num,
                                                    statistics.data()));
    return statistics;
}

void Device::UpdateDescriptorSets(Span<VkWriteDescriptorSet> writes,
                                  Span<VkCopyDescriptorSet> copies) const noexcept {
    dld->vkUpdateDescriptorSets(handle, writes.size(), writes.data(), copies.size(), copies.data());
}

VkPhysicalDeviceProperties PhysicalDevice::GetProperties() const noexcept {
    VkPhysicalDeviceProperties properties;
    dld->vkGetPhysicalDeviceProperties(physical_device, &properties);
    return properties;
}

void PhysicalDevice::GetProperties2(VkPhysicalDeviceProperties2& properties) const noexcept {
    dld->vkGetPhysicalDeviceProperties2(physical_device, &properties);
}

VkPhysicalDeviceFeatures PhysicalDevice::GetFeatures() const noexcept {
    VkPhysicalDeviceFeatures2 features2;
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = nullptr;
    dld->vkGetPhysicalDeviceFeatures2(physical_device, &features2);
    return features2.features;
}

void PhysicalDevice::GetFeatures2(VkPhysicalDeviceFeatures2& features) const noexcept {
    dld->vkGetPhysicalDeviceFeatures2(physical_device, &features);
}

VkFormatProperties PhysicalDevice::GetFormatProperties(VkFormat format) const noexcept {
    VkFormatProperties properties;
    dld->vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);
    return properties;
}

std::vector<VkExtensionProperties> PhysicalDevice::EnumerateDeviceExtensionProperties() const {
    u32 num;
    dld->vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &num, nullptr);
    std::vector<VkExtensionProperties> properties(num);
    dld->vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &num, properties.data());
    return properties;
}

std::vector<VkQueueFamilyProperties> PhysicalDevice::GetQueueFamilyProperties() const {
    u32 num;
    dld->vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num, nullptr);
    std::vector<VkQueueFamilyProperties> properties(num);
    dld->vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num, properties.data());
    return properties;
}

std::vector<VkPhysicalDeviceToolProperties> PhysicalDevice::GetPhysicalDeviceToolProperties()
    const {
    u32 num = 0;
    if (!dld->vkGetPhysicalDeviceToolProperties) {
        return {};
    }
    dld->vkGetPhysicalDeviceToolProperties(physical_device, &num, nullptr);
    std::vector<VkPhysicalDeviceToolProperties> properties(num);
    dld->vkGetPhysicalDeviceToolProperties(physical_device, &num, properties.data());
    return properties;
}

bool PhysicalDevice::GetSurfaceSupportKHR(u32 queue_family_index, VkSurfaceKHR surface) const {
    VkBool32 supported;
    Check(dld->vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, queue_family_index, surface,
                                                    &supported));
    return supported == VK_TRUE;
}

VkSurfaceCapabilitiesKHR PhysicalDevice::GetSurfaceCapabilitiesKHR(VkSurfaceKHR surface) const {
    VkSurfaceCapabilitiesKHR capabilities;
    Check(dld->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities));
    return capabilities;
}

std::vector<VkSurfaceFormatKHR> PhysicalDevice::GetSurfaceFormatsKHR(VkSurfaceKHR surface) const {
    u32 num;
    Check(dld->vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &num, nullptr));
    std::vector<VkSurfaceFormatKHR> formats(num);
    Check(
        dld->vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &num, formats.data()));
    return formats;
}

std::vector<VkPresentModeKHR> PhysicalDevice::GetSurfacePresentModesKHR(
    VkSurfaceKHR surface) const {
    u32 num;
    Check(dld->vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &num, nullptr));
    std::vector<VkPresentModeKHR> modes(num);
    Check(dld->vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &num,
                                                         modes.data()));
    return modes;
}

VkPhysicalDeviceMemoryProperties2 PhysicalDevice::GetMemoryProperties(
    void* next_structures) const noexcept {
    VkPhysicalDeviceMemoryProperties2 properties{};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    properties.pNext = next_structures;
    dld->vkGetPhysicalDeviceMemoryProperties2(physical_device, &properties);
    return properties;
}

u32 AvailableVersion(const InstanceDispatch& dld) noexcept {
    PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion;
    if (!Proc(vkEnumerateInstanceVersion, dld, "vkEnumerateInstanceVersion")) {
        // If the procedure is not found, Vulkan 1.0 is assumed
        return VK_API_VERSION_1_0;
    }
    u32 version;
    if (const VkResult result = vkEnumerateInstanceVersion(&version); result != VK_SUCCESS) {
        LOG_ERROR(Render_Vulkan, "vkEnumerateInstanceVersion returned {}, assuming Vulkan 1.1",
                  string_VkResult(result));
        return VK_API_VERSION_1_1;
    }
    return version;
}

std::optional<std::vector<VkExtensionProperties>> EnumerateInstanceExtensionProperties(
    const InstanceDispatch& dld) {
    u32 num;
    if (dld.vkEnumerateInstanceExtensionProperties(nullptr, &num, nullptr) != VK_SUCCESS) {
        return std::nullopt;
    }
    std::vector<VkExtensionProperties> properties(num);
    if (dld.vkEnumerateInstanceExtensionProperties(nullptr, &num, properties.data()) !=
        VK_SUCCESS) {
        return std::nullopt;
    }
    return properties;
}

std::optional<std::vector<VkLayerProperties>> EnumerateInstanceLayerProperties(
    const InstanceDispatch& dld) {
    u32 num;
    if (dld.vkEnumerateInstanceLayerProperties(&num, nullptr) != VK_SUCCESS) {
        return std::nullopt;
    }
    std::vector<VkLayerProperties> properties(num);
    if (dld.vkEnumerateInstanceLayerProperties(&num, properties.data()) != VK_SUCCESS) {
        return std::nullopt;
    }
    return properties;
}

} // namespace Vulkan::vk
