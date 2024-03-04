// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/service/nvdrv/devices/nvdevice.h"
#include "core/hle/service/nvdrv/nvdata.h"
#include "video_core/dma_pusher.h"

namespace Tegra {
namespace Control {
struct ChannelState;
}
} // namespace Tegra

namespace Service::Nvidia {

namespace NvCore {
class Container;
class NvMap;
class SyncpointManager;
} // namespace NvCore

class EventInterface;
} // namespace Service::Nvidia

namespace Service::Nvidia::Devices {

class nvhost_as_gpu;
class nvmap;
class nvhost_gpu final : public nvdevice {
public:
    explicit nvhost_gpu(Core::System& system_, EventInterface& events_interface_,
                        NvCore::Container& core);
    ~nvhost_gpu() override;

    NvResult Ioctl1(DeviceFD fd, Ioctl command, std::span<const u8> input,
                    std::span<u8> output) override;
    NvResult Ioctl2(DeviceFD fd, Ioctl command, std::span<const u8> input,
                    std::span<const u8> inline_input, std::span<u8> output) override;
    NvResult Ioctl3(DeviceFD fd, Ioctl command, std::span<const u8> input, std::span<u8> output,
                    std::span<u8> inline_output) override;

    void OnOpen(NvCore::SessionId session_id, DeviceFD fd) override;
    void OnClose(DeviceFD fd) override;

    Kernel::KEvent* QueryEvent(u32 event_id) override;

private:
    friend class nvhost_as_gpu;
    enum class CtxObjects : u32_le {
        Ctx2D = 0x902D,
        Ctx3D = 0xB197,
        CtxCompute = 0xB1C0,
        CtxKepler = 0xA140,
        CtxDMA = 0xB0B5,
        CtxChannelGPFIFO = 0xB06F,
    };

    struct IoctlSetNvmapFD {
        s32_le nvmap_fd{};
    };
    static_assert(sizeof(IoctlSetNvmapFD) == 4, "IoctlSetNvmapFD is incorrect size");

    struct IoctlChannelSetTimeout {
        u32_le timeout{};
    };
    static_assert(sizeof(IoctlChannelSetTimeout) == 4, "IoctlChannelSetTimeout is incorrect size");

    struct IoctlAllocGPFIFO {
        u32_le num_entries{};
        u32_le flags{};
    };
    static_assert(sizeof(IoctlAllocGPFIFO) == 8, "IoctlAllocGPFIFO is incorrect size");

    struct IoctlClientData {
        u64_le data{};
    };
    static_assert(sizeof(IoctlClientData) == 8, "IoctlClientData is incorrect size");

    struct IoctlZCullBind {
        u64_le gpu_va{};
        u32_le mode{}; // 0=global, 1=no_ctxsw, 2=separate_buffer, 3=part_of_regular_buf
        INSERT_PADDING_WORDS(1);
    };
    static_assert(sizeof(IoctlZCullBind) == 16, "IoctlZCullBind is incorrect size");

    struct IoctlSetErrorNotifier {
        u64_le offset{};
        u64_le size{};
        u32_le mem{}; // nvmap object handle
        INSERT_PADDING_WORDS(1);
    };
    static_assert(sizeof(IoctlSetErrorNotifier) == 24, "IoctlSetErrorNotifier is incorrect size");

    struct IoctlChannelSetPriority {
        u32_le priority{};
    };
    static_assert(sizeof(IoctlChannelSetPriority) == 4,
                  "IoctlChannelSetPriority is incorrect size");

    struct IoctlSetTimeslice {
        u32_le timeslice{};
    };
    static_assert(sizeof(IoctlSetTimeslice) == 4, "IoctlSetTimeslice is incorrect size");

    struct IoctlEventIdControl {
        u32_le cmd{}; // 0=disable, 1=enable, 2=clear
        u32_le id{};
    };
    static_assert(sizeof(IoctlEventIdControl) == 8, "IoctlEventIdControl is incorrect size");

    struct IoctlGetErrorNotification {
        u64_le timestamp{};
        u32_le info32{};
        u16_le info16{};
        u16_le status{}; // always 0xFFFF
    };
    static_assert(sizeof(IoctlGetErrorNotification) == 16,
                  "IoctlGetErrorNotification is incorrect size");

    static_assert(sizeof(NvFence) == 8, "Fence is incorrect size");

    struct IoctlAllocGpfifoEx {
        u32_le num_entries{};
        u32_le flags{};
        u32_le unk0{};
        u32_le unk1{};
        u32_le unk2{};
        u32_le unk3{};
        u32_le unk4{};
        u32_le unk5{};
    };
    static_assert(sizeof(IoctlAllocGpfifoEx) == 32, "IoctlAllocGpfifoEx is incorrect size");

    struct IoctlAllocGpfifoEx2 {
        u32_le num_entries{}; // in
        u32_le flags{};       // in
        u32_le unk0{};        // in (1 works)
        NvFence fence_out{};  // out
        u32_le unk1{};        // in
        u32_le unk2{};        // in
        u32_le unk3{};        // in
    };
    static_assert(sizeof(IoctlAllocGpfifoEx2) == 32, "IoctlAllocGpfifoEx2 is incorrect size");

    struct IoctlAllocObjCtx {
        u32_le class_num{}; // 0x902D=2d, 0xB197=3d, 0xB1C0=compute, 0xA140=kepler, 0xB0B5=DMA,
                            // 0xB06F=channel_gpfifo
        u32_le flags{};
        u64_le obj_id{}; // (ignored) used for FREE_OBJ_CTX ioctl, which is not supported
    };
    static_assert(sizeof(IoctlAllocObjCtx) == 16, "IoctlAllocObjCtx is incorrect size");

    struct IoctlSubmitGpfifo {
        u64_le address{};     // pointer to gpfifo entry structs
        u32_le num_entries{}; // number of fence objects being submitted
        union {
            u32_le raw;
            BitField<0, 1, u32_le> fence_wait;      // append a wait sync_point to the list
            BitField<1, 1, u32_le> fence_increment; // append an increment to the list
            BitField<2, 1, u32_le> new_hw_format;   // mostly ignored
            BitField<4, 1, u32_le> suppress_wfi;    // suppress wait for interrupt
            BitField<8, 1, u32_le> increment_value; // increment the returned fence
        } flags;
        NvFence fence{}; // returned new fence object for others to wait on
    };
    static_assert(sizeof(IoctlSubmitGpfifo) == 16 + sizeof(NvFence),
                  "IoctlSubmitGpfifo is incorrect size");

    struct IoctlGetWaitbase {
        u32 unknown{}; // seems to be ignored? Nintendo added this
        u32 value{};
    };
    static_assert(sizeof(IoctlGetWaitbase) == 8, "IoctlGetWaitbase is incorrect size");

    s32_le nvmap_fd{};
    u64_le user_data{};
    IoctlZCullBind zcull_params{};
    u32_le channel_priority{};
    u32_le channel_timeslice{};

    NvResult SetNVMAPfd(IoctlSetNvmapFD& params);
    NvResult SetClientData(IoctlClientData& params);
    NvResult GetClientData(IoctlClientData& params);
    NvResult ZCullBind(IoctlZCullBind& params);
    NvResult SetErrorNotifier(IoctlSetErrorNotifier& params);
    NvResult SetChannelPriority(IoctlChannelSetPriority& params);
    NvResult AllocGPFIFOEx2(IoctlAllocGpfifoEx2& params, DeviceFD fd);
    NvResult AllocateObjectContext(IoctlAllocObjCtx& params);

    NvResult SubmitGPFIFOImpl(IoctlSubmitGpfifo& params, Tegra::CommandList&& entries);
    NvResult SubmitGPFIFOBase1(IoctlSubmitGpfifo& params,
                               std::span<Tegra::CommandListHeader> commands, bool kickoff = false);
    NvResult SubmitGPFIFOBase2(IoctlSubmitGpfifo& params,
                               std::span<const Tegra::CommandListHeader> commands);

    NvResult GetWaitbase(IoctlGetWaitbase& params);
    NvResult ChannelSetTimeout(IoctlChannelSetTimeout& params);
    NvResult ChannelSetTimeslice(IoctlSetTimeslice& params);

    EventInterface& events_interface;
    NvCore::Container& core;
    NvCore::SyncpointManager& syncpoint_manager;
    NvCore::NvMap& nvmap;
    std::shared_ptr<Tegra::Control::ChannelState> channel_state;
    std::unordered_map<DeviceFD, NvCore::SessionId> sessions;
    u32 channel_syncpoint;
    std::mutex channel_mutex;

    // Events
    Kernel::KEvent* sm_exception_breakpoint_int_report_event;
    Kernel::KEvent* sm_exception_breakpoint_pause_report_event;
    Kernel::KEvent* error_notifier_event;
};

} // namespace Service::Nvidia::Devices
