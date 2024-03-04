// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <bit>
#include <cstring>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/kernel/k_page_table.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/service/nvdrv/core/container.h"
#include "core/hle/service/nvdrv/core/nvmap.h"
#include "core/hle/service/nvdrv/devices/ioctl_serialization.h"
#include "core/hle/service/nvdrv/devices/nvmap.h"
#include "core/memory.h"

using Core::Memory::YUZU_PAGESIZE;

namespace Service::Nvidia::Devices {

nvmap::nvmap(Core::System& system_, NvCore::Container& container_)
    : nvdevice{system_}, container{container_}, file{container.GetNvMapFile()} {}

nvmap::~nvmap() = default;

NvResult nvmap::Ioctl1(DeviceFD fd, Ioctl command, std::span<const u8> input,
                       std::span<u8> output) {
    switch (command.group) {
    case 0x1:
        switch (command.cmd) {
        case 0x1:
            return WrapFixed(this, &nvmap::IocCreate, input, output);
        case 0x3:
            return WrapFixed(this, &nvmap::IocFromId, input, output);
        case 0x4:
            return WrapFixed(this, &nvmap::IocAlloc, input, output, fd);
        case 0x5:
            return WrapFixed(this, &nvmap::IocFree, input, output, fd);
        case 0x9:
            return WrapFixed(this, &nvmap::IocParam, input, output);
        case 0xe:
            return WrapFixed(this, &nvmap::IocGetId, input, output);
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

NvResult nvmap::Ioctl2(DeviceFD fd, Ioctl command, std::span<const u8> input,
                       std::span<const u8> inline_input, std::span<u8> output) {
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

NvResult nvmap::Ioctl3(DeviceFD fd, Ioctl command, std::span<const u8> input, std::span<u8> output,
                       std::span<u8> inline_output) {
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

void nvmap::OnOpen(NvCore::SessionId session_id, DeviceFD fd) {
    sessions[fd] = session_id;
}
void nvmap::OnClose(DeviceFD fd) {
    auto it = sessions.find(fd);
    if (it != sessions.end()) {
        sessions.erase(it);
    }
}

NvResult nvmap::IocCreate(IocCreateParams& params) {
    LOG_DEBUG(Service_NVDRV, "called, size=0x{:08X}", params.size);

    std::shared_ptr<NvCore::NvMap::Handle> handle_description{};
    auto result =
        file.CreateHandle(Common::AlignUp(params.size, YUZU_PAGESIZE), handle_description);
    if (result != NvResult::Success) {
        LOG_CRITICAL(Service_NVDRV, "Failed to create Object");
        return result;
    }
    handle_description->orig_size = params.size; // Orig size is the unaligned size
    params.handle = handle_description->id;
    LOG_DEBUG(Service_NVDRV, "handle: {}, size: 0x{:X}", handle_description->id, params.size);

    return NvResult::Success;
}

NvResult nvmap::IocAlloc(IocAllocParams& params, DeviceFD fd) {
    LOG_DEBUG(Service_NVDRV, "called, addr={:X}", params.address);

    if (!params.handle) {
        LOG_CRITICAL(Service_NVDRV, "Handle is 0");
        return NvResult::BadValue;
    }

    if ((params.align - 1) & params.align) {
        LOG_CRITICAL(Service_NVDRV, "Incorrect alignment used, alignment={:08X}", params.align);
        return NvResult::BadValue;
    }

    // Force page size alignment at a minimum
    if (params.align < YUZU_PAGESIZE) {
        params.align = YUZU_PAGESIZE;
    }

    auto handle_description{file.GetHandle(params.handle)};
    if (!handle_description) {
        LOG_CRITICAL(Service_NVDRV, "Object does not exist, handle={:08X}", params.handle);
        return NvResult::BadValue;
    }

    if (handle_description->allocated) {
        LOG_CRITICAL(Service_NVDRV, "Object is already allocated, handle={:08X}", params.handle);
        return NvResult::InsufficientMemory;
    }

    const auto result = handle_description->Alloc(params.flags, params.align, params.kind,
                                                  params.address, sessions[fd]);
    if (result != NvResult::Success) {
        LOG_CRITICAL(Service_NVDRV, "Object failed to allocate, handle={:08X}", params.handle);
        return result;
    }
    bool is_out_io{};
    auto process = container.GetSession(sessions[fd])->process;
    ASSERT(process->GetPageTable()
               .LockForMapDeviceAddressSpace(&is_out_io, handle_description->address,
                                             handle_description->size,
                                             Kernel::KMemoryPermission::None, true, false)
               .IsSuccess());
    return result;
}

NvResult nvmap::IocGetId(IocGetIdParams& params) {
    LOG_DEBUG(Service_NVDRV, "called");

    // See the comment in FromId for extra info on this function
    if (!params.handle) {
        LOG_CRITICAL(Service_NVDRV, "Error!");
        return NvResult::BadValue;
    }

    auto handle_description{file.GetHandle(params.handle)};
    if (!handle_description) {
        LOG_CRITICAL(Service_NVDRV, "Error!");
        return NvResult::AccessDenied; // This will always return EPERM irrespective of if the
                                       // handle exists or not
    }

    params.id = handle_description->id;
    return NvResult::Success;
}

NvResult nvmap::IocFromId(IocFromIdParams& params) {
    LOG_DEBUG(Service_NVDRV, "called, id:{}", params.id);

    // Handles and IDs are always the same value in nvmap however IDs can be used globally given the
    // right permissions.
    // Since we don't plan on ever supporting multiprocess we can skip implementing handle refs and
    // so this function just does simple validation and passes through the handle id.
    if (!params.id) {
        LOG_CRITICAL(Service_NVDRV, "Zero Id is invalid!");
        return NvResult::BadValue;
    }

    auto handle_description{file.GetHandle(params.id)};
    if (!handle_description) {
        LOG_CRITICAL(Service_NVDRV, "Unregistered handle!");
        return NvResult::BadValue;
    }

    auto result = handle_description->Duplicate(false);
    if (result != NvResult::Success) {
        LOG_CRITICAL(Service_NVDRV, "Could not duplicate handle!");
        return result;
    }
    params.handle = handle_description->id;
    return NvResult::Success;
}

NvResult nvmap::IocParam(IocParamParams& params) {
    enum class ParamTypes { Size = 1, Alignment = 2, Base = 3, Heap = 4, Kind = 5, Compr = 6 };

    LOG_DEBUG(Service_NVDRV, "called type={}", params.param);

    if (!params.handle) {
        LOG_CRITICAL(Service_NVDRV, "Invalid handle!");
        return NvResult::BadValue;
    }

    auto handle_description{file.GetHandle(params.handle)};
    if (!handle_description) {
        LOG_CRITICAL(Service_NVDRV, "Not registered handle!");
        return NvResult::BadValue;
    }

    switch (params.param) {
    case HandleParameterType::Size:
        params.result = static_cast<u32_le>(handle_description->orig_size);
        break;
    case HandleParameterType::Alignment:
        params.result = static_cast<u32_le>(handle_description->align);
        break;
    case HandleParameterType::Base:
        params.result = static_cast<u32_le>(-22); // posix EINVAL
        break;
    case HandleParameterType::Heap:
        if (handle_description->allocated)
            params.result = 0x40000000;
        else
            params.result = 0;
        break;
    case HandleParameterType::Kind:
        params.result = handle_description->kind;
        break;
    case HandleParameterType::IsSharedMemMapped:
        params.result = handle_description->is_shared_mem_mapped;
        break;
    default:
        return NvResult::BadValue;
    }

    return NvResult::Success;
}

NvResult nvmap::IocFree(IocFreeParams& params, DeviceFD fd) {
    LOG_DEBUG(Service_NVDRV, "called");

    if (!params.handle) {
        LOG_CRITICAL(Service_NVDRV, "Handle null freed?");
        return NvResult::Success;
    }

    if (auto freeInfo{file.FreeHandle(params.handle, false)}) {
        auto process = container.GetSession(sessions[fd])->process;
        if (freeInfo->can_unlock) {
            ASSERT(process->GetPageTable()
                       .UnlockForDeviceAddressSpace(freeInfo->address, freeInfo->size)
                       .IsSuccess());
        }
        params.address = freeInfo->address;
        params.size = static_cast<u32>(freeInfo->size);
        params.flags.raw = 0;
        params.flags.map_uncached.Assign(freeInfo->was_uncached);
    } else {
        // This is possible when there's internal dups or other duplicates.
    }

    return NvResult::Success;
}

} // namespace Service::Nvidia::Devices
