// SPDX-FileCopyrightText: 2021 yuzu Emulator Project
// SPDX-FileCopyrightText: 2021 Skyline Team and Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstring>
#include <utility>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/service/nvdrv/core/container.h"
#include "core/hle/service/nvdrv/core/nvmap.h"
#include "core/hle/service/nvdrv/devices/ioctl_serialization.h"
#include "core/hle/service/nvdrv/devices/nvhost_as_gpu.h"
#include "core/hle/service/nvdrv/devices/nvhost_gpu.h"
#include "core/hle/service/nvdrv/nvdrv.h"
#include "video_core/control/channel_state.h"
#include "video_core/gpu.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"

namespace Service::Nvidia::Devices {

nvhost_as_gpu::nvhost_as_gpu(Core::System& system_, Module& module_, NvCore::Container& core)
    : nvdevice{system_}, module{module_}, container{core}, nvmap{core.GetNvMapFile()}, vm{},
      gmmu{} {}

nvhost_as_gpu::~nvhost_as_gpu() = default;

NvResult nvhost_as_gpu::Ioctl1(DeviceFD fd, Ioctl command, std::span<const u8> input,
                               std::span<u8> output) {
    switch (command.group) {
    case 'A':
        switch (command.cmd) {
        case 0x1:
            return WrapFixed(this, &nvhost_as_gpu::BindChannel, input, output);
        case 0x2:
            return WrapFixed(this, &nvhost_as_gpu::AllocateSpace, input, output);
        case 0x3:
            return WrapFixed(this, &nvhost_as_gpu::FreeSpace, input, output);
        case 0x5:
            return WrapFixed(this, &nvhost_as_gpu::UnmapBuffer, input, output);
        case 0x6:
            return WrapFixed(this, &nvhost_as_gpu::MapBufferEx, input, output);
        case 0x8:
            return WrapFixed(this, &nvhost_as_gpu::GetVARegions1, input, output);
        case 0x9:
            return WrapFixed(this, &nvhost_as_gpu::AllocAsEx, input, output);
        case 0x14:
            return WrapVariable(this, &nvhost_as_gpu::Remap, input, output);
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

NvResult nvhost_as_gpu::Ioctl2(DeviceFD fd, Ioctl command, std::span<const u8> input,
                               std::span<const u8> inline_input, std::span<u8> output) {
    UNIMPLEMENTED_MSG("Unimplemented ioctl={:08X}", command.raw);
    return NvResult::NotImplemented;
}

NvResult nvhost_as_gpu::Ioctl3(DeviceFD fd, Ioctl command, std::span<const u8> input,
                               std::span<u8> output, std::span<u8> inline_output) {
    switch (command.group) {
    case 'A':
        switch (command.cmd) {
        case 0x8:
            return WrapFixedInlOut(this, &nvhost_as_gpu::GetVARegions3, input, output,
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

void nvhost_as_gpu::OnOpen(NvCore::SessionId session_id, DeviceFD fd) {}
void nvhost_as_gpu::OnClose(DeviceFD fd) {}

NvResult nvhost_as_gpu::AllocAsEx(IoctlAllocAsEx& params) {
    LOG_DEBUG(Service_NVDRV, "called, big_page_size=0x{:X}", params.big_page_size);

    std::scoped_lock lock(mutex);

    if (vm.initialised) {
        ASSERT_MSG(false, "Cannot initialise an address space twice!");
        return NvResult::InvalidState;
    }

    if (params.big_page_size) {
        if (!std::has_single_bit(params.big_page_size)) {
            LOG_ERROR(Service_NVDRV, "Non power-of-2 big page size: 0x{:X}!", params.big_page_size);
            return NvResult::BadValue;
        }

        if ((params.big_page_size & VM::SUPPORTED_BIG_PAGE_SIZES) == 0) {
            LOG_ERROR(Service_NVDRV, "Unsupported big page size: 0x{:X}!", params.big_page_size);
            return NvResult::BadValue;
        }

        vm.big_page_size = params.big_page_size;
        vm.big_page_size_bits = static_cast<u32>(std::countr_zero(params.big_page_size));

        vm.va_range_start = params.big_page_size << VM::VA_START_SHIFT;
    }

    // If this is unspecified then default values should be used
    if (params.va_range_start) {
        vm.va_range_start = params.va_range_start;
        vm.va_range_split = params.va_range_split;
        vm.va_range_end = params.va_range_end;
    }

    const u64 max_big_page_bits = Common::Log2Ceil64(vm.va_range_end);

    const auto start_pages{static_cast<u32>(vm.va_range_start >> VM::PAGE_SIZE_BITS)};
    const auto end_pages{static_cast<u32>(vm.va_range_split >> VM::PAGE_SIZE_BITS)};
    vm.small_page_allocator = std::make_shared<VM::Allocator>(start_pages, end_pages);

    const auto start_big_pages{static_cast<u32>(vm.va_range_split >> vm.big_page_size_bits)};
    const auto end_big_pages{
        static_cast<u32>((vm.va_range_end - vm.va_range_split) >> vm.big_page_size_bits)};
    vm.big_page_allocator = std::make_unique<VM::Allocator>(start_big_pages, end_big_pages);

    gmmu = std::make_shared<Tegra::MemoryManager>(system, max_big_page_bits, vm.va_range_split,
                                                  vm.big_page_size_bits, VM::PAGE_SIZE_BITS);
    system.GPU().InitAddressSpace(*gmmu);
    vm.initialised = true;

    return NvResult::Success;
}

NvResult nvhost_as_gpu::AllocateSpace(IoctlAllocSpace& params) {
    LOG_DEBUG(Service_NVDRV, "called, pages={:X}, page_size={:X}, flags={:X}", params.pages,
              params.page_size, params.flags);

    std::scoped_lock lock(mutex);

    if (!vm.initialised) {
        return NvResult::BadValue;
    }

    if (params.page_size != VM::YUZU_PAGESIZE && params.page_size != vm.big_page_size) {
        return NvResult::BadValue;
    }

    if (params.page_size != vm.big_page_size &&
        ((params.flags & MappingFlags::Sparse) != MappingFlags::None)) {
        UNIMPLEMENTED_MSG("Sparse small pages are not implemented!");
        return NvResult::NotImplemented;
    }

    const u32 page_size_bits{params.page_size == VM::YUZU_PAGESIZE ? VM::PAGE_SIZE_BITS
                                                                   : vm.big_page_size_bits};

    auto& allocator{params.page_size == VM::YUZU_PAGESIZE ? *vm.small_page_allocator
                                                          : *vm.big_page_allocator};

    if ((params.flags & MappingFlags::Fixed) != MappingFlags::None) {
        allocator.AllocateFixed(static_cast<u32>(params.offset >> page_size_bits), params.pages);
    } else {
        params.offset = static_cast<u64>(allocator.Allocate(params.pages)) << page_size_bits;
        if (!params.offset) {
            ASSERT_MSG(false, "Failed to allocate free space in the GPU AS!");
            return NvResult::InsufficientMemory;
        }
    }

    u64 size{static_cast<u64>(params.pages) * params.page_size};

    if ((params.flags & MappingFlags::Sparse) != MappingFlags::None) {
        gmmu->MapSparse(params.offset, size);
    }

    allocation_map[params.offset] = {
        .size = size,
        .mappings{},
        .page_size = params.page_size,
        .sparse = (params.flags & MappingFlags::Sparse) != MappingFlags::None,
        .big_pages = params.page_size != VM::YUZU_PAGESIZE,
    };

    return NvResult::Success;
}

void nvhost_as_gpu::FreeMappingLocked(u64 offset) {
    auto mapping{mapping_map.at(offset)};

    if (!mapping->fixed) {
        auto& allocator{mapping->big_page ? *vm.big_page_allocator : *vm.small_page_allocator};
        u32 page_size_bits{mapping->big_page ? vm.big_page_size_bits : VM::PAGE_SIZE_BITS};
        u32 page_size{mapping->big_page ? vm.big_page_size : VM::YUZU_PAGESIZE};
        u64 aligned_size{Common::AlignUp(mapping->size, page_size)};

        allocator.Free(static_cast<u32>(mapping->offset >> page_size_bits),
                       static_cast<u32>(aligned_size >> page_size_bits));
    }

    nvmap.UnpinHandle(mapping->handle);

    // Sparse mappings shouldn't be fully unmapped, just returned to their sparse state
    // Only FreeSpace can unmap them fully
    if (mapping->sparse_alloc) {
        gmmu->MapSparse(offset, mapping->size, mapping->big_page);
    } else {
        gmmu->Unmap(offset, mapping->size);
    }

    mapping_map.erase(offset);
}

NvResult nvhost_as_gpu::FreeSpace(IoctlFreeSpace& params) {
    LOG_DEBUG(Service_NVDRV, "called, offset={:X}, pages={:X}, page_size={:X}", params.offset,
              params.pages, params.page_size);

    std::scoped_lock lock(mutex);

    if (!vm.initialised) {
        return NvResult::BadValue;
    }

    try {
        auto allocation{allocation_map[params.offset]};

        if (allocation.page_size != params.page_size ||
            allocation.size != (static_cast<u64>(params.pages) * params.page_size)) {
            return NvResult::BadValue;
        }

        for (const auto& mapping : allocation.mappings) {
            FreeMappingLocked(mapping->offset);
        }

        // Unset sparse flag if required
        if (allocation.sparse) {
            gmmu->Unmap(params.offset, allocation.size);
        }

        auto& allocator{params.page_size == VM::YUZU_PAGESIZE ? *vm.small_page_allocator
                                                              : *vm.big_page_allocator};
        u32 page_size_bits{params.page_size == VM::YUZU_PAGESIZE ? VM::PAGE_SIZE_BITS
                                                                 : vm.big_page_size_bits};

        allocator.Free(static_cast<u32>(params.offset >> page_size_bits),
                       static_cast<u32>(allocation.size >> page_size_bits));
        allocation_map.erase(params.offset);
    } catch (const std::out_of_range&) {
        return NvResult::BadValue;
    }

    return NvResult::Success;
}

NvResult nvhost_as_gpu::Remap(std::span<IoctlRemapEntry> entries) {
    LOG_DEBUG(Service_NVDRV, "called, num_entries=0x{:X}", entries.size());

    if (!vm.initialised) {
        return NvResult::BadValue;
    }

    for (const auto& entry : entries) {
        GPUVAddr virtual_address{static_cast<u64>(entry.as_offset_big_pages)
                                 << vm.big_page_size_bits};
        u64 size{static_cast<u64>(entry.big_pages) << vm.big_page_size_bits};

        auto alloc{allocation_map.upper_bound(virtual_address)};

        if (alloc-- == allocation_map.begin() ||
            (virtual_address - alloc->first) + size > alloc->second.size) {
            LOG_WARNING(Service_NVDRV, "Cannot remap into an unallocated region!");
            return NvResult::BadValue;
        }

        if (!alloc->second.sparse) {
            LOG_WARNING(Service_NVDRV, "Cannot remap a non-sparse mapping!");
            return NvResult::BadValue;
        }

        const bool use_big_pages = alloc->second.big_pages;
        if (!entry.handle) {
            gmmu->MapSparse(virtual_address, size, use_big_pages);
        } else {
            auto handle{nvmap.GetHandle(entry.handle)};
            if (!handle) {
                return NvResult::BadValue;
            }

            DAddr base = nvmap.PinHandle(entry.handle, false);
            DAddr device_address{static_cast<DAddr>(
                base + (static_cast<u64>(entry.handle_offset_big_pages) << vm.big_page_size_bits))};

            gmmu->Map(virtual_address, device_address, size,
                      static_cast<Tegra::PTEKind>(entry.kind), use_big_pages);
        }
    }

    return NvResult::Success;
}

NvResult nvhost_as_gpu::MapBufferEx(IoctlMapBufferEx& params) {
    LOG_DEBUG(Service_NVDRV,
              "called, flags={:X}, nvmap_handle={:X}, buffer_offset={}, mapping_size={}"
              ", offset={}",
              params.flags, params.handle, params.buffer_offset, params.mapping_size,
              params.offset);

    std::scoped_lock lock(mutex);

    if (!vm.initialised) {
        return NvResult::BadValue;
    }

    // Remaps a subregion of an existing mapping to a different PA
    if ((params.flags & MappingFlags::Remap) != MappingFlags::None) {
        try {
            auto mapping{mapping_map.at(params.offset)};

            if (mapping->size < params.mapping_size) {
                LOG_WARNING(Service_NVDRV,
                            "Cannot remap a partially mapped GPU address space region: 0x{:X}",
                            params.offset);
                return NvResult::BadValue;
            }

            u64 gpu_address{static_cast<u64>(params.offset + params.buffer_offset)};
            VAddr device_address{mapping->ptr + params.buffer_offset};

            gmmu->Map(gpu_address, device_address, params.mapping_size,
                      static_cast<Tegra::PTEKind>(params.kind), mapping->big_page);

            return NvResult::Success;
        } catch (const std::out_of_range&) {
            LOG_WARNING(Service_NVDRV, "Cannot remap an unmapped GPU address space region: 0x{:X}",
                        params.offset);
            return NvResult::BadValue;
        }
    }

    auto handle{nvmap.GetHandle(params.handle)};
    if (!handle) {
        return NvResult::BadValue;
    }

    DAddr device_address{
        static_cast<DAddr>(nvmap.PinHandle(params.handle, false) + params.buffer_offset)};
    u64 size{params.mapping_size ? params.mapping_size : handle->orig_size};

    bool big_page{[&]() {
        if (Common::IsAligned(handle->align, vm.big_page_size)) {
            return true;
        } else if (Common::IsAligned(handle->align, VM::YUZU_PAGESIZE)) {
            return false;
        } else {
            ASSERT(false);
            return false;
        }
    }()};

    if ((params.flags & MappingFlags::Fixed) != MappingFlags::None) {
        auto alloc{allocation_map.upper_bound(params.offset)};

        if (alloc-- == allocation_map.begin() ||
            (params.offset - alloc->first) + size > alloc->second.size) {
            ASSERT_MSG(false, "Cannot perform a fixed mapping into an unallocated region!");
            return NvResult::BadValue;
        }

        const bool use_big_pages = alloc->second.big_pages && big_page;
        gmmu->Map(params.offset, device_address, size, static_cast<Tegra::PTEKind>(params.kind),
                  use_big_pages);

        auto mapping{std::make_shared<Mapping>(params.handle, device_address, params.offset, size,
                                               true, use_big_pages, alloc->second.sparse)};
        alloc->second.mappings.push_back(mapping);
        mapping_map[params.offset] = mapping;
    } else {
        auto& allocator{big_page ? *vm.big_page_allocator : *vm.small_page_allocator};
        u32 page_size{big_page ? vm.big_page_size : VM::YUZU_PAGESIZE};
        u32 page_size_bits{big_page ? vm.big_page_size_bits : VM::PAGE_SIZE_BITS};

        params.offset = static_cast<u64>(allocator.Allocate(
                            static_cast<u32>(Common::AlignUp(size, page_size) >> page_size_bits)))
                        << page_size_bits;
        if (!params.offset) {
            ASSERT_MSG(false, "Failed to allocate free space in the GPU AS!");
            return NvResult::InsufficientMemory;
        }

        gmmu->Map(params.offset, device_address, Common::AlignUp(size, page_size),
                  static_cast<Tegra::PTEKind>(params.kind), big_page);

        auto mapping{std::make_shared<Mapping>(params.handle, device_address, params.offset, size,
                                               false, big_page, false)};
        mapping_map[params.offset] = mapping;
    }

    return NvResult::Success;
}

NvResult nvhost_as_gpu::UnmapBuffer(IoctlUnmapBuffer& params) {
    LOG_DEBUG(Service_NVDRV, "called, offset=0x{:X}", params.offset);

    std::scoped_lock lock(mutex);

    if (!vm.initialised) {
        return NvResult::BadValue;
    }

    try {
        auto mapping{mapping_map.at(params.offset)};

        if (!mapping->fixed) {
            auto& allocator{mapping->big_page ? *vm.big_page_allocator : *vm.small_page_allocator};
            u32 page_size_bits{mapping->big_page ? vm.big_page_size_bits : VM::PAGE_SIZE_BITS};

            allocator.Free(static_cast<u32>(mapping->offset >> page_size_bits),
                           static_cast<u32>(mapping->size >> page_size_bits));
        }

        // Sparse mappings shouldn't be fully unmapped, just returned to their sparse state
        // Only FreeSpace can unmap them fully
        if (mapping->sparse_alloc) {
            gmmu->MapSparse(params.offset, mapping->size, mapping->big_page);
        } else {
            gmmu->Unmap(params.offset, mapping->size);
        }

        nvmap.UnpinHandle(mapping->handle);

        mapping_map.erase(params.offset);
    } catch (const std::out_of_range&) {
        LOG_WARNING(Service_NVDRV, "Couldn't find region to unmap at 0x{:X}", params.offset);
    }

    return NvResult::Success;
}

NvResult nvhost_as_gpu::BindChannel(IoctlBindChannel& params) {
    LOG_DEBUG(Service_NVDRV, "called, fd={:X}", params.fd);

    auto gpu_channel_device = module.GetDevice<nvhost_gpu>(params.fd);
    gpu_channel_device->channel_state->memory_manager = gmmu;
    return NvResult::Success;
}

void nvhost_as_gpu::GetVARegionsImpl(IoctlGetVaRegions& params) {
    params.buf_size = 2 * sizeof(VaRegion);

    params.regions = std::array<VaRegion, 2>{
        VaRegion{
            .offset = vm.small_page_allocator->GetVAStart() << VM::PAGE_SIZE_BITS,
            .page_size = VM::YUZU_PAGESIZE,
            ._pad0_{},
            .pages = vm.small_page_allocator->GetVALimit() - vm.small_page_allocator->GetVAStart(),
        },
        VaRegion{
            .offset = vm.big_page_allocator->GetVAStart() << vm.big_page_size_bits,
            .page_size = vm.big_page_size,
            ._pad0_{},
            .pages = vm.big_page_allocator->GetVALimit() - vm.big_page_allocator->GetVAStart(),
        },
    };
}

NvResult nvhost_as_gpu::GetVARegions1(IoctlGetVaRegions& params) {
    LOG_DEBUG(Service_NVDRV, "called, buf_addr={:X}, buf_size={:X}", params.buf_addr,
              params.buf_size);

    std::scoped_lock lock(mutex);

    if (!vm.initialised) {
        return NvResult::BadValue;
    }

    GetVARegionsImpl(params);

    return NvResult::Success;
}

NvResult nvhost_as_gpu::GetVARegions3(IoctlGetVaRegions& params, std::span<VaRegion> regions) {
    LOG_DEBUG(Service_NVDRV, "called, buf_addr={:X}, buf_size={:X}", params.buf_addr,
              params.buf_size);

    std::scoped_lock lock(mutex);

    if (!vm.initialised) {
        return NvResult::BadValue;
    }

    GetVARegionsImpl(params);

    const size_t num_regions = std::min(params.regions.size(), regions.size());
    for (size_t i = 0; i < num_regions; i++) {
        regions[i] = params.regions[i];
    }

    return NvResult::Success;
}

Kernel::KEvent* nvhost_as_gpu::QueryEvent(u32 event_id) {
    LOG_CRITICAL(Service_NVDRV, "Unknown AS GPU Event {}", event_id);
    return nullptr;
}

} // namespace Service::Nvidia::Devices
