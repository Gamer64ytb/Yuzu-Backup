// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/service/nvdrv/devices/ioctl_serialization.h"
#include "core/hle/service/nvdrv/devices/nvhost_ctrl_gpu.h"
#include "core/hle/service/nvdrv/nvdrv.h"

namespace Service::Nvidia::Devices {

nvhost_ctrl_gpu::nvhost_ctrl_gpu(Core::System& system_, EventInterface& events_interface_)
    : nvdevice{system_}, events_interface{events_interface_} {
    error_notifier_event = events_interface.CreateEvent("CtrlGpuErrorNotifier");
    unknown_event = events_interface.CreateEvent("CtrlGpuUnknownEvent");
}
nvhost_ctrl_gpu::~nvhost_ctrl_gpu() {
    events_interface.FreeEvent(error_notifier_event);
    events_interface.FreeEvent(unknown_event);
}

NvResult nvhost_ctrl_gpu::Ioctl1(DeviceFD fd, Ioctl command, std::span<const u8> input,
                                 std::span<u8> output) {
    switch (command.group) {
    case 'G':
        switch (command.cmd) {
        case 0x1:
            return WrapFixed(this, &nvhost_ctrl_gpu::ZCullGetCtxSize, input, output);
        case 0x2:
            return WrapFixed(this, &nvhost_ctrl_gpu::ZCullGetInfo, input, output);
        case 0x3:
            return WrapFixed(this, &nvhost_ctrl_gpu::ZBCSetTable, input, output);
        case 0x4:
            return WrapFixed(this, &nvhost_ctrl_gpu::ZBCQueryTable, input, output);
        case 0x5:
            return WrapFixed(this, &nvhost_ctrl_gpu::GetCharacteristics1, input, output);
        case 0x6:
            return WrapFixed(this, &nvhost_ctrl_gpu::GetTPCMasks1, input, output);
        case 0x7:
            return WrapFixed(this, &nvhost_ctrl_gpu::FlushL2, input, output);
        case 0x14:
            return WrapFixed(this, &nvhost_ctrl_gpu::GetActiveSlotMask, input, output);
        case 0x1c:
            return WrapFixed(this, &nvhost_ctrl_gpu::GetGpuTime, input, output);
        default:
            break;
        }
        break;
    }
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

NvResult nvhost_ctrl_gpu::Ioctl2(DeviceFD fd, Ioctl command, std::span<const u8> input,
                                 std::span<const u8> inline_input, std::span<u8> output) {
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

NvResult nvhost_ctrl_gpu::Ioctl3(DeviceFD fd, Ioctl command, std::span<const u8> input,
                                 std::span<u8> output, std::span<u8> inline_output) {
    switch (command.group) {
    case 'G':
        switch (command.cmd) {
        case 0x5:
            return WrapFixedInlOut(this, &nvhost_ctrl_gpu::GetCharacteristics3, input, output,
                                   inline_output);
        case 0x6:
            return WrapFixedInlOut(this, &nvhost_ctrl_gpu::GetTPCMasks3, input, output,
                                   inline_output);
        default:
            break;
        }
        break;
    default:
        break;
    }
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

void nvhost_ctrl_gpu::OnOpen(NvCore::SessionId session_id, DeviceFD fd) {}
void nvhost_ctrl_gpu::OnClose(DeviceFD fd) {}

NvResult nvhost_ctrl_gpu::GetCharacteristics1(IoctlCharacteristics& params) {
    LOG_DEBUG(Service_NVDRV, "called");
    params.gc.arch = 0x120;
    params.gc.impl = 0xb;
    params.gc.rev = 0xa1;
    params.gc.num_gpc = 0x1;
    params.gc.l2_cache_size = 0x40000;
    params.gc.on_board_video_memory_size = 0x0;
    params.gc.num_tpc_per_gpc = 0x2;
    params.gc.bus_type = 0x20;
    params.gc.big_page_size = 0x20000;
    params.gc.compression_page_size = 0x20000;
    params.gc.pde_coverage_bit_count = 0x1B;
    params.gc.available_big_page_sizes = 0x30000;
    params.gc.gpc_mask = 0x1;
    params.gc.sm_arch_sm_version = 0x503;
    params.gc.sm_arch_spa_version = 0x503;
    params.gc.sm_arch_warp_count = 0x80;
    params.gc.gpu_va_bit_count = 0x28;
    params.gc.reserved = 0x0;
    params.gc.flags = 0x55;
    params.gc.twod_class = 0x902D;
    params.gc.threed_class = 0xB197;
    params.gc.compute_class = 0xB1C0;
    params.gc.gpfifo_class = 0xB06F;
    params.gc.inline_to_memory_class = 0xA140;
    params.gc.dma_copy_class = 0xB0B5;
    params.gc.max_fbps_count = 0x1;
    params.gc.fbp_en_mask = 0x0;
    params.gc.max_ltc_per_fbp = 0x2;
    params.gc.max_lts_per_ltc = 0x1;
    params.gc.max_tex_per_tpc = 0x0;
    params.gc.max_gpc_count = 0x1;
    params.gc.rop_l2_en_mask_0 = 0x21D70;
    params.gc.rop_l2_en_mask_1 = 0x0;
    params.gc.chipname = 0x6230326D67;
    params.gc.gr_compbit_store_base_hw = 0x0;
    params.gpu_characteristics_buf_size = 0xA0;
    params.gpu_characteristics_buf_addr = 0xdeadbeef; // Cannot be 0 (UNUSED)
    return NvResult::Success;
}

NvResult nvhost_ctrl_gpu::GetCharacteristics3(
    IoctlCharacteristics& params, std::span<IoctlGpuCharacteristics> gpu_characteristics) {
    LOG_DEBUG(Service_NVDRV, "called");

    params.gc.arch = 0x120;
    params.gc.impl = 0xb;
    params.gc.rev = 0xa1;
    params.gc.num_gpc = 0x1;
    params.gc.l2_cache_size = 0x40000;
    params.gc.on_board_video_memory_size = 0x0;
    params.gc.num_tpc_per_gpc = 0x2;
    params.gc.bus_type = 0x20;
    params.gc.big_page_size = 0x20000;
    params.gc.compression_page_size = 0x20000;
    params.gc.pde_coverage_bit_count = 0x1B;
    params.gc.available_big_page_sizes = 0x30000;
    params.gc.gpc_mask = 0x1;
    params.gc.sm_arch_sm_version = 0x503;
    params.gc.sm_arch_spa_version = 0x503;
    params.gc.sm_arch_warp_count = 0x80;
    params.gc.gpu_va_bit_count = 0x28;
    params.gc.reserved = 0x0;
    params.gc.flags = 0x55;
    params.gc.twod_class = 0x902D;
    params.gc.threed_class = 0xB197;
    params.gc.compute_class = 0xB1C0;
    params.gc.gpfifo_class = 0xB06F;
    params.gc.inline_to_memory_class = 0xA140;
    params.gc.dma_copy_class = 0xB0B5;
    params.gc.max_fbps_count = 0x1;
    params.gc.fbp_en_mask = 0x0;
    params.gc.max_ltc_per_fbp = 0x2;
    params.gc.max_lts_per_ltc = 0x1;
    params.gc.max_tex_per_tpc = 0x0;
    params.gc.max_gpc_count = 0x1;
    params.gc.rop_l2_en_mask_0 = 0x21D70;
    params.gc.rop_l2_en_mask_1 = 0x0;
    params.gc.chipname = 0x6230326D67;
    params.gc.gr_compbit_store_base_hw = 0x0;
    params.gpu_characteristics_buf_size = 0xA0;
    params.gpu_characteristics_buf_addr = 0xdeadbeef; // Cannot be 0 (UNUSED)
    if (!gpu_characteristics.empty()) {
        gpu_characteristics.front() = params.gc;
    }
    return NvResult::Success;
}

NvResult nvhost_ctrl_gpu::GetTPCMasks1(IoctlGpuGetTpcMasksArgs& params) {
    LOG_DEBUG(Service_NVDRV, "called, mask_buffer_size=0x{:X}", params.mask_buffer_size);
    if (params.mask_buffer_size != 0) {
        params.tcp_mask = 3;
    }
    return NvResult::Success;
}

NvResult nvhost_ctrl_gpu::GetTPCMasks3(IoctlGpuGetTpcMasksArgs& params, std::span<u32> tpc_mask) {
    LOG_DEBUG(Service_NVDRV, "called, mask_buffer_size=0x{:X}", params.mask_buffer_size);
    if (params.mask_buffer_size != 0) {
        params.tcp_mask = 3;
    }
    if (!tpc_mask.empty()) {
        tpc_mask.front() = params.tcp_mask;
    }
    return NvResult::Success;
}

NvResult nvhost_ctrl_gpu::GetActiveSlotMask(IoctlActiveSlotMask& params) {
    LOG_DEBUG(Service_NVDRV, "called");

    params.slot = 0x07;
    params.mask = 0x01;
    return NvResult::Success;
}

NvResult nvhost_ctrl_gpu::ZCullGetCtxSize(IoctlZcullGetCtxSize& params) {
    LOG_DEBUG(Service_NVDRV, "called");
    params.size = 0x1;
    return NvResult::Success;
}

NvResult nvhost_ctrl_gpu::ZCullGetInfo(IoctlNvgpuGpuZcullGetInfoArgs& params) {
    LOG_DEBUG(Service_NVDRV, "called");
    params.width_align_pixels = 0x20;
    params.height_align_pixels = 0x20;
    params.pixel_squares_by_aliquots = 0x400;
    params.aliquot_total = 0x800;
    params.region_byte_multiplier = 0x20;
    params.region_header_size = 0x20;
    params.subregion_header_size = 0xc0;
    params.subregion_width_align_pixels = 0x20;
    params.subregion_height_align_pixels = 0x40;
    params.subregion_count = 0x10;
    return NvResult::Success;
}

NvResult nvhost_ctrl_gpu::ZBCSetTable(IoctlZbcSetTable& params) {
    LOG_WARNING(Service_NVDRV, "(STUBBED) called");
    // TODO(ogniK): What does this even actually do?
    return NvResult::Success;
}

NvResult nvhost_ctrl_gpu::ZBCQueryTable(IoctlZbcQueryTable& params) {
    LOG_WARNING(Service_NVDRV, "(STUBBED) called");
    return NvResult::Success;
}

NvResult nvhost_ctrl_gpu::FlushL2(IoctlFlushL2& params) {
    LOG_WARNING(Service_NVDRV, "(STUBBED) called");
    return NvResult::Success;
}

NvResult nvhost_ctrl_gpu::GetGpuTime(IoctlGetGpuTime& params) {
    LOG_DEBUG(Service_NVDRV, "called");
    params.gpu_time = static_cast<u64_le>(system.CoreTiming().GetGlobalTimeNs().count());
    return NvResult::Success;
}

Kernel::KEvent* nvhost_ctrl_gpu::QueryEvent(u32 event_id) {
    switch (event_id) {
    case 1:
        return error_notifier_event;
    case 2:
        return unknown_event;
    default:
        LOG_CRITICAL(Service_NVDRV, "Unknown Ctrl GPU Event {}", event_id);
        return nullptr;
    }
}

} // namespace Service::Nvidia::Devices
