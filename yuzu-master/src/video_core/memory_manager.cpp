// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/kernel/k_page_table.h"
#include "core/hle/kernel/k_process.h"
#include "video_core/guest_memory.h"
#include "video_core/host1x/host1x.h"
#include "video_core/invalidation_accumulator.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_base.h"

namespace Tegra {
using Tegra::Memory::GuestMemoryFlags;

std::atomic<size_t> MemoryManager::unique_identifier_generator{};

MemoryManager::MemoryManager(Core::System& system_, MaxwellDeviceMemoryManager& memory_,
                             u64 address_space_bits_, GPUVAddr split_address_, u64 big_page_bits_,
                             u64 page_bits_)
    : system{system_}, memory{memory_}, address_space_bits{address_space_bits_},
      split_address{split_address_}, page_bits{page_bits_}, big_page_bits{big_page_bits_},
      entries{}, big_entries{}, page_table{address_space_bits, address_space_bits + page_bits - 38,
                                           page_bits != big_page_bits ? page_bits : 0},
      kind_map{PTEKind::INVALID}, unique_identifier{unique_identifier_generator.fetch_add(
                                      1, std::memory_order_acq_rel)},
      accumulator{std::make_unique<VideoCommon::InvalidationAccumulator>()} {
    address_space_size = 1ULL << address_space_bits;
    page_size = 1ULL << page_bits;
    page_mask = page_size - 1ULL;
    big_page_size = 1ULL << big_page_bits;
    big_page_mask = big_page_size - 1ULL;
    const u64 page_table_bits = address_space_bits - page_bits;
    const u64 big_page_table_bits = address_space_bits - big_page_bits;
    const u64 page_table_size = 1ULL << page_table_bits;
    const u64 big_page_table_size = 1ULL << big_page_table_bits;
    page_table_mask = page_table_size - 1;
    big_page_table_mask = big_page_table_size - 1;

    big_entries.resize(big_page_table_size / 32, 0);
    big_page_table_dev.resize(big_page_table_size);
    big_page_continuous.resize(big_page_table_size / continuous_bits, 0);
    entries.resize(page_table_size / 32, 0);
}

MemoryManager::MemoryManager(Core::System& system_, u64 address_space_bits_,
                             GPUVAddr split_address_, u64 big_page_bits_, u64 page_bits_)
    : MemoryManager(system_, system_.Host1x().MemoryManager(), address_space_bits_, split_address_,
                    big_page_bits_, page_bits_) {}

MemoryManager::~MemoryManager() = default;

template <bool is_big_page>
MemoryManager::EntryType MemoryManager::GetEntry(size_t position) const {
    if constexpr (is_big_page) {
        position = position >> big_page_bits;
        const u64 entry_mask = big_entries[position / 32];
        const size_t sub_index = position % 32;
        return static_cast<EntryType>((entry_mask >> (2 * sub_index)) & 0x03ULL);
    } else {
        position = position >> page_bits;
        const u64 entry_mask = entries[position / 32];
        const size_t sub_index = position % 32;
        return static_cast<EntryType>((entry_mask >> (2 * sub_index)) & 0x03ULL);
    }
}

template <bool is_big_page>
void MemoryManager::SetEntry(size_t position, MemoryManager::EntryType entry) {
    if constexpr (is_big_page) {
        position = position >> big_page_bits;
        const u64 entry_mask = big_entries[position / 32];
        const size_t sub_index = position % 32;
        big_entries[position / 32] =
            (~(3ULL << sub_index * 2) & entry_mask) | (static_cast<u64>(entry) << sub_index * 2);
    } else {
        position = position >> page_bits;
        const u64 entry_mask = entries[position / 32];
        const size_t sub_index = position % 32;
        entries[position / 32] =
            (~(3ULL << sub_index * 2) & entry_mask) | (static_cast<u64>(entry) << sub_index * 2);
    }
}

PTEKind MemoryManager::GetPageKind(GPUVAddr gpu_addr) const {
    std::unique_lock<std::mutex> lock(guard);
    return kind_map.GetValueAt(gpu_addr);
}

inline bool MemoryManager::IsBigPageContinuous(size_t big_page_index) const {
    const u64 entry_mask = big_page_continuous[big_page_index / continuous_bits];
    const size_t sub_index = big_page_index % continuous_bits;
    return ((entry_mask >> sub_index) & 0x1ULL) != 0;
}

inline void MemoryManager::SetBigPageContinuous(size_t big_page_index, bool value) {
    const u64 continuous_mask = big_page_continuous[big_page_index / continuous_bits];
    const size_t sub_index = big_page_index % continuous_bits;
    big_page_continuous[big_page_index / continuous_bits] =
        (~(1ULL << sub_index) & continuous_mask) | (value ? 1ULL << sub_index : 0);
}

template <MemoryManager::EntryType entry_type>
GPUVAddr MemoryManager::PageTableOp(GPUVAddr gpu_addr, [[maybe_unused]] DAddr dev_addr, size_t size,
                                    PTEKind kind) {
    [[maybe_unused]] u64 remaining_size{size};
    if constexpr (entry_type == EntryType::Mapped) {
        page_table.ReserveRange(gpu_addr, size);
    }
    for (u64 offset{}; offset < size; offset += page_size) {
        const GPUVAddr current_gpu_addr = gpu_addr + offset;
        [[maybe_unused]] const auto current_entry_type = GetEntry<false>(current_gpu_addr);
        SetEntry<false>(current_gpu_addr, entry_type);
        if (current_entry_type != entry_type) {
            rasterizer->ModifyGPUMemory(unique_identifier, current_gpu_addr, page_size);
        }
        if constexpr (entry_type == EntryType::Mapped) {
            const DAddr current_dev_addr = dev_addr + offset;
            const auto index = PageEntryIndex<false>(current_gpu_addr);
            const u32 sub_value = static_cast<u32>(current_dev_addr >> cpu_page_bits);
            page_table[index] = sub_value;
        }
        remaining_size -= page_size;
    }
    kind_map.Map(gpu_addr, gpu_addr + size, kind);
    return gpu_addr;
}

template <MemoryManager::EntryType entry_type>
GPUVAddr MemoryManager::BigPageTableOp(GPUVAddr gpu_addr, [[maybe_unused]] DAddr dev_addr,
                                       size_t size, PTEKind kind) {
    [[maybe_unused]] u64 remaining_size{size};
    for (u64 offset{}; offset < size; offset += big_page_size) {
        const GPUVAddr current_gpu_addr = gpu_addr + offset;
        [[maybe_unused]] const auto current_entry_type = GetEntry<true>(current_gpu_addr);
        SetEntry<true>(current_gpu_addr, entry_type);
        if (current_entry_type != entry_type) {
            rasterizer->ModifyGPUMemory(unique_identifier, current_gpu_addr, big_page_size);
        }
        if constexpr (entry_type == EntryType::Mapped) {
            const DAddr current_dev_addr = dev_addr + offset;
            const auto index = PageEntryIndex<true>(current_gpu_addr);
            const u32 sub_value = static_cast<u32>(current_dev_addr >> cpu_page_bits);
            big_page_table_dev[index] = sub_value;
            const bool is_continuous = ([&] {
                uintptr_t base_ptr{
                    reinterpret_cast<uintptr_t>(memory.GetPointer<u8>(current_dev_addr))};
                if (base_ptr == 0) {
                    return false;
                }
                for (DAddr start_cpu = current_dev_addr + page_size;
                     start_cpu < current_dev_addr + big_page_size; start_cpu += page_size) {
                    base_ptr += page_size;
                    auto next_ptr = reinterpret_cast<uintptr_t>(memory.GetPointer<u8>(start_cpu));
                    if (next_ptr == 0 || base_ptr != next_ptr) {
                        return false;
                    }
                }
                return true;
            })();
            SetBigPageContinuous(index, is_continuous);
        }
        remaining_size -= big_page_size;
    }
    {
        std::unique_lock<std::mutex> lock(guard);
        kind_map.Map(gpu_addr, gpu_addr + size, kind);
    }
    return gpu_addr;
}

void MemoryManager::BindRasterizer(VideoCore::RasterizerInterface* rasterizer_) {
    rasterizer = rasterizer_;
}

GPUVAddr MemoryManager::Map(GPUVAddr gpu_addr, DAddr dev_addr, std::size_t size, PTEKind kind,
                            bool is_big_pages) {
    if (is_big_pages) [[likely]] {
        return BigPageTableOp<EntryType::Mapped>(gpu_addr, dev_addr, size, kind);
    }
    return PageTableOp<EntryType::Mapped>(gpu_addr, dev_addr, size, kind);
}

GPUVAddr MemoryManager::MapSparse(GPUVAddr gpu_addr, std::size_t size, bool is_big_pages) {
    if (is_big_pages) [[likely]] {
        return BigPageTableOp<EntryType::Reserved>(gpu_addr, 0, size, PTEKind::INVALID);
    }
    return PageTableOp<EntryType::Reserved>(gpu_addr, 0, size, PTEKind::INVALID);
}

void MemoryManager::Unmap(GPUVAddr gpu_addr, std::size_t size) {
    if (size == 0) {
        return;
    }
    GetSubmappedRangeImpl<false>(gpu_addr, size, page_stash);

    for (const auto& [map_addr, map_size] : page_stash) {
        rasterizer->UnmapMemory(map_addr, map_size);
    }
    page_stash.clear();

    BigPageTableOp<EntryType::Free>(gpu_addr, 0, size, PTEKind::INVALID);
    PageTableOp<EntryType::Free>(gpu_addr, 0, size, PTEKind::INVALID);
}

std::optional<DAddr> MemoryManager::GpuToCpuAddress(GPUVAddr gpu_addr) const {
    if (!IsWithinGPUAddressRange(gpu_addr)) [[unlikely]] {
        return std::nullopt;
    }
    if (GetEntry<true>(gpu_addr) != EntryType::Mapped) [[unlikely]] {
        if (GetEntry<false>(gpu_addr) != EntryType::Mapped) {
            return std::nullopt;
        }

        const DAddr dev_addr_base = static_cast<DAddr>(page_table[PageEntryIndex<false>(gpu_addr)])
                                    << cpu_page_bits;
        return dev_addr_base + (gpu_addr & page_mask);
    }

    const DAddr dev_addr_base =
        static_cast<DAddr>(big_page_table_dev[PageEntryIndex<true>(gpu_addr)]) << cpu_page_bits;
    return dev_addr_base + (gpu_addr & big_page_mask);
}

std::optional<DAddr> MemoryManager::GpuToCpuAddress(GPUVAddr addr, std::size_t size) const {
    size_t page_index{addr >> page_bits};
    const size_t page_last{(addr + size + page_size - 1) >> page_bits};
    while (page_index < page_last) {
        const auto page_addr{GpuToCpuAddress(page_index << page_bits)};
        if (page_addr) {
            return page_addr;
        }
        ++page_index;
    }
    return std::nullopt;
}

template <typename T>
T MemoryManager::Read(GPUVAddr addr) const {
    if (auto page_pointer{GetPointer(addr)}; page_pointer) {
        // NOTE: Avoid adding any extra logic to this fast-path block
        T value;
        std::memcpy(&value, page_pointer, sizeof(T));
        return value;
    }

    ASSERT(false);

    return {};
}

template <typename T>
void MemoryManager::Write(GPUVAddr addr, T data) {
    if (auto page_pointer{GetPointer(addr)}; page_pointer) {
        // NOTE: Avoid adding any extra logic to this fast-path block
        std::memcpy(page_pointer, &data, sizeof(T));
        return;
    }

    ASSERT(false);
}

template u8 MemoryManager::Read<u8>(GPUVAddr addr) const;
template u16 MemoryManager::Read<u16>(GPUVAddr addr) const;
template u32 MemoryManager::Read<u32>(GPUVAddr addr) const;
template u64 MemoryManager::Read<u64>(GPUVAddr addr) const;
template void MemoryManager::Write<u8>(GPUVAddr addr, u8 data);
template void MemoryManager::Write<u16>(GPUVAddr addr, u16 data);
template void MemoryManager::Write<u32>(GPUVAddr addr, u32 data);
template void MemoryManager::Write<u64>(GPUVAddr addr, u64 data);

u8* MemoryManager::GetPointer(GPUVAddr gpu_addr) {
    const auto address{GpuToCpuAddress(gpu_addr)};
    if (!address) {
        return {};
    }

    return memory.GetPointer<u8>(*address);
}

const u8* MemoryManager::GetPointer(GPUVAddr gpu_addr) const {
    const auto address{GpuToCpuAddress(gpu_addr)};
    if (!address) {
        return {};
    }

    return memory.GetPointer<u8>(*address);
}

#ifdef _MSC_VER // no need for gcc / clang but msvc's compiler is more conservative with inlining.
#pragma inline_recursion(on)
#endif

template <bool is_big_pages, typename FuncMapped, typename FuncReserved, typename FuncUnmapped>
inline void MemoryManager::MemoryOperation(GPUVAddr gpu_src_addr, std::size_t size,
                                           FuncMapped&& func_mapped, FuncReserved&& func_reserved,
                                           FuncUnmapped&& func_unmapped) const {
    using FuncMappedReturn =
        typename std::invoke_result<FuncMapped, std::size_t, std::size_t, std::size_t>::type;
    using FuncReservedReturn =
        typename std::invoke_result<FuncReserved, std::size_t, std::size_t, std::size_t>::type;
    using FuncUnmappedReturn =
        typename std::invoke_result<FuncUnmapped, std::size_t, std::size_t, std::size_t>::type;
    static constexpr bool BOOL_BREAK_MAPPED = std::is_same_v<FuncMappedReturn, bool>;
    static constexpr bool BOOL_BREAK_RESERVED = std::is_same_v<FuncReservedReturn, bool>;
    static constexpr bool BOOL_BREAK_UNMAPPED = std::is_same_v<FuncUnmappedReturn, bool>;
    u64 used_page_size;
    u64 used_page_mask;
    u64 used_page_bits;
    if constexpr (is_big_pages) {
        used_page_size = big_page_size;
        used_page_mask = big_page_mask;
        used_page_bits = big_page_bits;
    } else {
        used_page_size = page_size;
        used_page_mask = page_mask;
        used_page_bits = page_bits;
    }
    std::size_t remaining_size{size};
    std::size_t page_index{gpu_src_addr >> used_page_bits};
    std::size_t page_offset{gpu_src_addr & used_page_mask};
    GPUVAddr current_address = gpu_src_addr;

    while (remaining_size > 0) {
        const std::size_t copy_amount{
            std::min(static_cast<std::size_t>(used_page_size) - page_offset, remaining_size)};
        auto entry = GetEntry<is_big_pages>(current_address);
        if (entry == EntryType::Mapped) [[likely]] {
            if constexpr (BOOL_BREAK_MAPPED) {
                if (func_mapped(page_index, page_offset, copy_amount)) {
                    return;
                }
            } else {
                func_mapped(page_index, page_offset, copy_amount);
            }

        } else if (entry == EntryType::Reserved) {
            if constexpr (BOOL_BREAK_RESERVED) {
                if (func_reserved(page_index, page_offset, copy_amount)) {
                    return;
                }
            } else {
                func_reserved(page_index, page_offset, copy_amount);
            }

        } else [[unlikely]] {
            if constexpr (BOOL_BREAK_UNMAPPED) {
                if (func_unmapped(page_index, page_offset, copy_amount)) {
                    return;
                }
            } else {
                func_unmapped(page_index, page_offset, copy_amount);
            }
        }
        page_index++;
        page_offset = 0;
        remaining_size -= copy_amount;
        current_address += copy_amount;
    }
}

template <bool is_safe>
void MemoryManager::ReadBlockImpl(GPUVAddr gpu_src_addr, void* dest_buffer, std::size_t size,
                                  [[maybe_unused]] VideoCommon::CacheType which) const {
    auto set_to_zero = [&]([[maybe_unused]] std::size_t page_index,
                           [[maybe_unused]] std::size_t offset, std::size_t copy_amount) {
        std::memset(dest_buffer, 0, copy_amount);
        dest_buffer = static_cast<u8*>(dest_buffer) + copy_amount;
    };
    auto mapped_normal = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const DAddr dev_addr_base =
            (static_cast<DAddr>(page_table[page_index]) << cpu_page_bits) + offset;
        if constexpr (is_safe) {
            rasterizer->FlushRegion(dev_addr_base, copy_amount, which);
        }
        u8* physical = memory.GetPointer<u8>(dev_addr_base);
        std::memcpy(dest_buffer, physical, copy_amount);
        dest_buffer = static_cast<u8*>(dest_buffer) + copy_amount;
    };
    auto mapped_big = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const DAddr dev_addr_base =
            (static_cast<DAddr>(big_page_table_dev[page_index]) << cpu_page_bits) + offset;
        if constexpr (is_safe) {
            rasterizer->FlushRegion(dev_addr_base, copy_amount, which);
        }
        if (!IsBigPageContinuous(page_index)) [[unlikely]] {
            memory.ReadBlockUnsafe(dev_addr_base, dest_buffer, copy_amount);
        } else {
            u8* physical = memory.GetPointer<u8>(dev_addr_base);
            std::memcpy(dest_buffer, physical, copy_amount);
        }
        dest_buffer = static_cast<u8*>(dest_buffer) + copy_amount;
    };
    auto read_short_pages = [&](std::size_t page_index, std::size_t offset,
                                std::size_t copy_amount) {
        GPUVAddr base = (page_index << big_page_bits) + offset;
        MemoryOperation<false>(base, copy_amount, mapped_normal, set_to_zero, set_to_zero);
    };
    MemoryOperation<true>(gpu_src_addr, size, mapped_big, set_to_zero, read_short_pages);
}

void MemoryManager::ReadBlock(GPUVAddr gpu_src_addr, void* dest_buffer, std::size_t size,
                              VideoCommon::CacheType which) const {
    ReadBlockImpl<true>(gpu_src_addr, dest_buffer, size, which);
}

void MemoryManager::ReadBlockUnsafe(GPUVAddr gpu_src_addr, void* dest_buffer,
                                    const std::size_t size) const {
    ReadBlockImpl<false>(gpu_src_addr, dest_buffer, size, VideoCommon::CacheType::None);
}

template <bool is_safe>
void MemoryManager::WriteBlockImpl(GPUVAddr gpu_dest_addr, const void* src_buffer, std::size_t size,
                                   [[maybe_unused]] VideoCommon::CacheType which) {
    auto just_advance = [&]([[maybe_unused]] std::size_t page_index,
                            [[maybe_unused]] std::size_t offset, std::size_t copy_amount) {
        src_buffer = static_cast<const u8*>(src_buffer) + copy_amount;
    };
    auto mapped_normal = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const DAddr dev_addr_base =
            (static_cast<DAddr>(page_table[page_index]) << cpu_page_bits) + offset;
        if constexpr (is_safe) {
            rasterizer->InvalidateRegion(dev_addr_base, copy_amount, which);
        }
        u8* physical = memory.GetPointer<u8>(dev_addr_base);
        std::memcpy(physical, src_buffer, copy_amount);
        src_buffer = static_cast<const u8*>(src_buffer) + copy_amount;
    };
    auto mapped_big = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const DAddr dev_addr_base =
            (static_cast<DAddr>(big_page_table_dev[page_index]) << cpu_page_bits) + offset;
        if constexpr (is_safe) {
            rasterizer->InvalidateRegion(dev_addr_base, copy_amount, which);
        }
        if (!IsBigPageContinuous(page_index)) [[unlikely]] {
            memory.WriteBlockUnsafe(dev_addr_base, src_buffer, copy_amount);
        } else {
            u8* physical = memory.GetPointer<u8>(dev_addr_base);
            std::memcpy(physical, src_buffer, copy_amount);
        }
        src_buffer = static_cast<const u8*>(src_buffer) + copy_amount;
    };
    auto write_short_pages = [&](std::size_t page_index, std::size_t offset,
                                 std::size_t copy_amount) {
        GPUVAddr base = (page_index << big_page_bits) + offset;
        MemoryOperation<false>(base, copy_amount, mapped_normal, just_advance, just_advance);
    };
    MemoryOperation<true>(gpu_dest_addr, size, mapped_big, just_advance, write_short_pages);
}

void MemoryManager::WriteBlock(GPUVAddr gpu_dest_addr, const void* src_buffer, std::size_t size,
                               VideoCommon::CacheType which) {
    WriteBlockImpl<true>(gpu_dest_addr, src_buffer, size, which);
}

void MemoryManager::WriteBlockUnsafe(GPUVAddr gpu_dest_addr, const void* src_buffer,
                                     std::size_t size) {
    WriteBlockImpl<false>(gpu_dest_addr, src_buffer, size, VideoCommon::CacheType::None);
}

void MemoryManager::WriteBlockCached(GPUVAddr gpu_dest_addr, const void* src_buffer,
                                     std::size_t size) {
    WriteBlockImpl<false>(gpu_dest_addr, src_buffer, size, VideoCommon::CacheType::None);
    accumulator->Add(gpu_dest_addr, size);
}

void MemoryManager::FlushRegion(GPUVAddr gpu_addr, size_t size,
                                VideoCommon::CacheType which) const {
    auto do_nothing = [&]([[maybe_unused]] std::size_t page_index,
                          [[maybe_unused]] std::size_t offset,
                          [[maybe_unused]] std::size_t copy_amount) {};

    auto mapped_normal = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const DAddr dev_addr_base =
            (static_cast<DAddr>(page_table[page_index]) << cpu_page_bits) + offset;
        rasterizer->FlushRegion(dev_addr_base, copy_amount, which);
    };
    auto mapped_big = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const DAddr dev_addr_base =
            (static_cast<DAddr>(big_page_table_dev[page_index]) << cpu_page_bits) + offset;
        rasterizer->FlushRegion(dev_addr_base, copy_amount, which);
    };
    auto flush_short_pages = [&](std::size_t page_index, std::size_t offset,
                                 std::size_t copy_amount) {
        GPUVAddr base = (page_index << big_page_bits) + offset;
        MemoryOperation<false>(base, copy_amount, mapped_normal, do_nothing, do_nothing);
    };
    MemoryOperation<true>(gpu_addr, size, mapped_big, do_nothing, flush_short_pages);
}

bool MemoryManager::IsMemoryDirty(GPUVAddr gpu_addr, size_t size,
                                  VideoCommon::CacheType which) const {
    bool result = false;
    auto do_nothing = [&]([[maybe_unused]] std::size_t page_index,
                          [[maybe_unused]] std::size_t offset,
                          [[maybe_unused]] std::size_t copy_amount) { return false; };

    auto mapped_normal = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const DAddr dev_addr_base =
            (static_cast<DAddr>(page_table[page_index]) << cpu_page_bits) + offset;
        result |= rasterizer->MustFlushRegion(dev_addr_base, copy_amount, which);
        return result;
    };
    auto mapped_big = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const DAddr dev_addr_base =
            (static_cast<DAddr>(big_page_table_dev[page_index]) << cpu_page_bits) + offset;
        result |= rasterizer->MustFlushRegion(dev_addr_base, copy_amount, which);
        return result;
    };
    auto check_short_pages = [&](std::size_t page_index, std::size_t offset,
                                 std::size_t copy_amount) {
        GPUVAddr base = (page_index << big_page_bits) + offset;
        MemoryOperation<false>(base, copy_amount, mapped_normal, do_nothing, do_nothing);
        return result;
    };
    MemoryOperation<true>(gpu_addr, size, mapped_big, do_nothing, check_short_pages);
    return result;
}

size_t MemoryManager::MaxContinuousRange(GPUVAddr gpu_addr, size_t size) const {
    std::optional<DAddr> old_page_addr{};
    size_t range_so_far = 0;
    bool result{false};
    auto fail = [&]([[maybe_unused]] std::size_t page_index, [[maybe_unused]] std::size_t offset,
                    std::size_t copy_amount) {
        result = true;
        return true;
    };
    auto short_check = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const DAddr dev_addr_base =
            (static_cast<DAddr>(page_table[page_index]) << cpu_page_bits) + offset;
        if (old_page_addr && *old_page_addr != dev_addr_base) {
            result = true;
            return true;
        }
        range_so_far += copy_amount;
        old_page_addr = {dev_addr_base + copy_amount};
        return false;
    };
    auto big_check = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const DAddr dev_addr_base =
            (static_cast<DAddr>(big_page_table_dev[page_index]) << cpu_page_bits) + offset;
        if (old_page_addr && *old_page_addr != dev_addr_base) {
            return true;
        }
        range_so_far += copy_amount;
        old_page_addr = {dev_addr_base + copy_amount};
        return false;
    };
    auto check_short_pages = [&](std::size_t page_index, std::size_t offset,
                                 std::size_t copy_amount) {
        GPUVAddr base = (page_index << big_page_bits) + offset;
        MemoryOperation<false>(base, copy_amount, short_check, fail, fail);
        return result;
    };
    MemoryOperation<true>(gpu_addr, size, big_check, fail, check_short_pages);
    return range_so_far;
}

size_t MemoryManager::GetMemoryLayoutSize(GPUVAddr gpu_addr, size_t max_size) const {
    std::unique_lock<std::mutex> lock(guard);
    return kind_map.GetContinuousSizeFrom(gpu_addr);
}

void MemoryManager::InvalidateRegion(GPUVAddr gpu_addr, size_t size,
                                     VideoCommon::CacheType which) const {
    auto do_nothing = [&]([[maybe_unused]] std::size_t page_index,
                          [[maybe_unused]] std::size_t offset,
                          [[maybe_unused]] std::size_t copy_amount) {};

    auto mapped_normal = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const DAddr dev_addr_base =
            (static_cast<DAddr>(page_table[page_index]) << cpu_page_bits) + offset;
        rasterizer->InvalidateRegion(dev_addr_base, copy_amount, which);
    };
    auto mapped_big = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const DAddr dev_addr_base =
            (static_cast<DAddr>(big_page_table_dev[page_index]) << cpu_page_bits) + offset;
        rasterizer->InvalidateRegion(dev_addr_base, copy_amount, which);
    };
    auto invalidate_short_pages = [&](std::size_t page_index, std::size_t offset,
                                      std::size_t copy_amount) {
        GPUVAddr base = (page_index << big_page_bits) + offset;
        MemoryOperation<false>(base, copy_amount, mapped_normal, do_nothing, do_nothing);
    };
    MemoryOperation<true>(gpu_addr, size, mapped_big, do_nothing, invalidate_short_pages);
}

void MemoryManager::CopyBlock(GPUVAddr gpu_dest_addr, GPUVAddr gpu_src_addr, std::size_t size,
                              VideoCommon::CacheType which) {
    Tegra::Memory::GpuGuestMemoryScoped<u8, GuestMemoryFlags::SafeReadWrite> data(
        *this, gpu_src_addr, size);
    data.SetAddressAndSize(gpu_dest_addr, size);
    FlushRegion(gpu_dest_addr, size, which);
}

bool MemoryManager::IsGranularRange(GPUVAddr gpu_addr, std::size_t size) const {
    if (GetEntry<true>(gpu_addr) == EntryType::Mapped) [[likely]] {
        size_t page_index = gpu_addr >> big_page_bits;
        if (IsBigPageContinuous(page_index)) [[likely]] {
            const std::size_t page{(page_index & big_page_mask) + size};
            return page <= big_page_size;
        }
        const std::size_t page{(gpu_addr & Core::DEVICE_PAGEMASK) + size};
        return page <= Core::DEVICE_PAGESIZE;
    }
    if (GetEntry<false>(gpu_addr) != EntryType::Mapped) {
        return false;
    }
    const std::size_t page{(gpu_addr & Core::DEVICE_PAGEMASK) + size};
    return page <= Core::DEVICE_PAGESIZE;
}

bool MemoryManager::IsContinuousRange(GPUVAddr gpu_addr, std::size_t size) const {
    std::optional<DAddr> old_page_addr{};
    bool result{true};
    auto fail = [&]([[maybe_unused]] std::size_t page_index, [[maybe_unused]] std::size_t offset,
                    std::size_t copy_amount) {
        result = false;
        return true;
    };
    auto short_check = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const DAddr dev_addr_base =
            (static_cast<DAddr>(page_table[page_index]) << cpu_page_bits) + offset;
        if (old_page_addr && *old_page_addr != dev_addr_base) {
            result = false;
            return true;
        }
        old_page_addr = {dev_addr_base + copy_amount};
        return false;
    };
    auto big_check = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const DAddr dev_addr_base =
            (static_cast<DAddr>(big_page_table_dev[page_index]) << cpu_page_bits) + offset;
        if (old_page_addr && *old_page_addr != dev_addr_base) {
            result = false;
            return true;
        }
        old_page_addr = {dev_addr_base + copy_amount};
        return false;
    };
    auto check_short_pages = [&](std::size_t page_index, std::size_t offset,
                                 std::size_t copy_amount) {
        GPUVAddr base = (page_index << big_page_bits) + offset;
        MemoryOperation<false>(base, copy_amount, short_check, fail, fail);
        return !result;
    };
    MemoryOperation<true>(gpu_addr, size, big_check, fail, check_short_pages);
    return result;
}

bool MemoryManager::IsFullyMappedRange(GPUVAddr gpu_addr, std::size_t size) const {
    bool result{true};
    auto fail = [&]([[maybe_unused]] std::size_t page_index, [[maybe_unused]] std::size_t offset,
                    [[maybe_unused]] std::size_t copy_amount) {
        result = false;
        return true;
    };
    auto pass = [&]([[maybe_unused]] std::size_t page_index, [[maybe_unused]] std::size_t offset,
                    [[maybe_unused]] std::size_t copy_amount) { return false; };
    auto check_short_pages = [&](std::size_t page_index, std::size_t offset,
                                 std::size_t copy_amount) {
        GPUVAddr base = (page_index << big_page_bits) + offset;
        MemoryOperation<false>(base, copy_amount, pass, pass, fail);
        return !result;
    };
    MemoryOperation<true>(gpu_addr, size, pass, fail, check_short_pages);
    return result;
}

boost::container::small_vector<std::pair<GPUVAddr, std::size_t>, 32>
MemoryManager::GetSubmappedRange(GPUVAddr gpu_addr, std::size_t size) const {
    boost::container::small_vector<std::pair<GPUVAddr, std::size_t>, 32> result{};
    GetSubmappedRangeImpl<true>(gpu_addr, size, result);
    return result;
}

template <bool is_gpu_address>
void MemoryManager::GetSubmappedRangeImpl(
    GPUVAddr gpu_addr, std::size_t size,
    boost::container::small_vector<
        std::pair<std::conditional_t<is_gpu_address, GPUVAddr, DAddr>, std::size_t>, 32>& result)
    const {
    std::optional<std::pair<std::conditional_t<is_gpu_address, GPUVAddr, DAddr>, std::size_t>>
        last_segment{};
    std::optional<DAddr> old_page_addr{};
    const auto split = [&last_segment, &result]([[maybe_unused]] std::size_t page_index,
                                                [[maybe_unused]] std::size_t offset,
                                                [[maybe_unused]] std::size_t copy_amount) {
        if (last_segment) {
            result.push_back(*last_segment);
            last_segment = std::nullopt;
        }
    };
    const auto extend_size_big = [this, &split, &old_page_addr,
                                  &last_segment](std::size_t page_index, std::size_t offset,
                                                 std::size_t copy_amount) {
        const DAddr dev_addr_base =
            (static_cast<DAddr>(big_page_table_dev[page_index]) << cpu_page_bits) + offset;
        if (old_page_addr) {
            if (*old_page_addr != dev_addr_base) {
                split(0, 0, 0);
            }
        }
        old_page_addr = {dev_addr_base + copy_amount};
        if (!last_segment) {
            if constexpr (is_gpu_address) {
                const GPUVAddr new_base_addr = (page_index << big_page_bits) + offset;
                last_segment = {new_base_addr, copy_amount};
            } else {
                last_segment = {dev_addr_base, copy_amount};
            }
        } else {
            last_segment->second += copy_amount;
        }
    };
    const auto extend_size_short = [this, &split, &old_page_addr,
                                    &last_segment](std::size_t page_index, std::size_t offset,
                                                   std::size_t copy_amount) {
        const DAddr dev_addr_base =
            (static_cast<DAddr>(page_table[page_index]) << cpu_page_bits) + offset;
        if (old_page_addr) {
            if (*old_page_addr != dev_addr_base) {
                split(0, 0, 0);
            }
        }
        old_page_addr = {dev_addr_base + copy_amount};
        if (!last_segment) {
            if constexpr (is_gpu_address) {
                const GPUVAddr new_base_addr = (page_index << page_bits) + offset;
                last_segment = {new_base_addr, copy_amount};
            } else {
                last_segment = {dev_addr_base, copy_amount};
            }
        } else {
            last_segment->second += copy_amount;
        }
    };
    auto do_short_pages = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        GPUVAddr base = (page_index << big_page_bits) + offset;
        MemoryOperation<false>(base, copy_amount, extend_size_short, split, split);
    };
    MemoryOperation<true>(gpu_addr, size, extend_size_big, split, do_short_pages);
    split(0, 0, 0);
}

void MemoryManager::FlushCaching() {
    if (!accumulator->AnyAccumulated()) {
        return;
    }
    accumulator->Callback([this](GPUVAddr addr, size_t size) {
        GetSubmappedRangeImpl<false>(addr, size, page_stash2);
    });
    rasterizer->InnerInvalidation(page_stash2);
    page_stash2.clear();
    accumulator->Clear();
}

const u8* MemoryManager::GetSpan(const GPUVAddr src_addr, const std::size_t size) const {
    if (!IsContinuousRange(src_addr, size)) {
        return nullptr;
    }
    auto dev_addr = GpuToCpuAddress(src_addr);
    if (dev_addr) {
        return memory.GetSpan(*dev_addr, size);
    }
    return nullptr;
}

u8* MemoryManager::GetSpan(const GPUVAddr src_addr, const std::size_t size) {
    if (!IsContinuousRange(src_addr, size)) {
        return nullptr;
    }
    auto dev_addr = GpuToCpuAddress(src_addr);
    if (dev_addr) {
        return memory.GetSpan(*dev_addr, size);
    }
    return nullptr;
}

} // namespace Tegra
