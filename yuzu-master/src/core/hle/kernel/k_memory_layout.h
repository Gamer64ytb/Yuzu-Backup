// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <utility>

#include "common/alignment.h"
#include "common/literals.h"
#include "core/device_memory.h"
#include "core/hle/kernel/k_memory_region.h"
#include "core/hle/kernel/k_memory_region_type.h"
#include "core/hle/kernel/k_typed_address.h"
#include "core/hle/kernel/memory_types.h"

namespace Kernel {

using namespace Common::Literals;

constexpr std::size_t L1BlockSize = 1_GiB;
constexpr std::size_t L2BlockSize = 2_MiB;

constexpr std::size_t GetMaximumOverheadSize(std::size_t size) {
    return (Common::DivideUp(size, L1BlockSize) + Common::DivideUp(size, L2BlockSize)) * PageSize;
}

constexpr std::size_t MainMemorySize = 4_GiB;
constexpr std::size_t MainMemorySizeMax = 8_GiB;

constexpr std::size_t ReservedEarlyDramSize = 384_KiB;
constexpr std::size_t DramPhysicalAddress = 0x80000000;

constexpr std::size_t KernelAslrAlignment = 2_MiB;
constexpr std::size_t KernelVirtualAddressSpaceWidth = 1ULL << 39;
constexpr std::size_t KernelPhysicalAddressSpaceWidth = 1ULL << 48;

constexpr std::size_t KernelVirtualAddressSpaceBase = 0ULL - KernelVirtualAddressSpaceWidth;
constexpr std::size_t KernelVirtualAddressSpaceEnd =
    KernelVirtualAddressSpaceBase + (KernelVirtualAddressSpaceWidth - KernelAslrAlignment);
constexpr std::size_t KernelVirtualAddressSpaceLast = KernelVirtualAddressSpaceEnd - 1ULL;
constexpr std::size_t KernelVirtualAddressSpaceSize =
    KernelVirtualAddressSpaceEnd - KernelVirtualAddressSpaceBase;
constexpr std::size_t KernelVirtualAddressCodeBase = KernelVirtualAddressSpaceBase;
constexpr std::size_t KernelVirtualAddressCodeSize = 392_KiB;
constexpr std::size_t KernelVirtualAddressCodeEnd =
    KernelVirtualAddressCodeBase + KernelVirtualAddressCodeSize;

constexpr std::size_t KernelPhysicalAddressSpaceBase = 0ULL;
constexpr std::size_t KernelPhysicalAddressSpaceEnd =
    KernelPhysicalAddressSpaceBase + KernelPhysicalAddressSpaceWidth;
constexpr std::size_t KernelPhysicalAddressSpaceLast = KernelPhysicalAddressSpaceEnd - 1ULL;
constexpr std::size_t KernelPhysicalAddressSpaceSize =
    KernelPhysicalAddressSpaceEnd - KernelPhysicalAddressSpaceBase;
constexpr std::size_t KernelPhysicalAddressCodeBase = DramPhysicalAddress + ReservedEarlyDramSize;

constexpr std::size_t KernelPageTableHeapSize = GetMaximumOverheadSize(MainMemorySizeMax);
constexpr std::size_t KernelInitialPageHeapSize = 128_KiB;

constexpr std::size_t KernelSlabHeapDataSize = 5_MiB;
constexpr std::size_t KernelSlabHeapGapsSizeMax = 2_MiB - 64_KiB;
constexpr std::size_t KernelSlabHeapSize = KernelSlabHeapDataSize + KernelSlabHeapGapsSizeMax;

// NOTE: This is calculated from KThread slab counts, assuming KThread size <= 0x860.
constexpr size_t KernelPageBufferHeapSize = 0x3E0000;
constexpr size_t KernelSlabHeapAdditionalSize = 0x148000;
constexpr size_t KernelPageBufferAdditionalSize = 0x33C000;

constexpr std::size_t KernelResourceSize = KernelPageTableHeapSize + KernelInitialPageHeapSize +
                                           KernelSlabHeapSize + KernelPageBufferHeapSize;

//! NB: Use KThread::GetAddressKeyIsKernel().
//! See explanation for deviation of GetAddressKey.
bool IsKernelAddressKey(KProcessAddress key) = delete;

constexpr bool IsKernelAddress(KProcessAddress address) {
    return KernelVirtualAddressSpaceBase <= GetInteger(address) &&
           address < KernelVirtualAddressSpaceEnd;
}

class KMemoryLayout final {
public:
    KMemoryLayout();

    KMemoryRegionTree& GetVirtualMemoryRegionTree() {
        return m_virtual_tree;
    }
    const KMemoryRegionTree& GetVirtualMemoryRegionTree() const {
        return m_virtual_tree;
    }
    KMemoryRegionTree& GetPhysicalMemoryRegionTree() {
        return m_physical_tree;
    }
    const KMemoryRegionTree& GetPhysicalMemoryRegionTree() const {
        return m_physical_tree;
    }
    KMemoryRegionTree& GetVirtualLinearMemoryRegionTree() {
        return m_virtual_linear_tree;
    }
    const KMemoryRegionTree& GetVirtualLinearMemoryRegionTree() const {
        return m_virtual_linear_tree;
    }
    KMemoryRegionTree& GetPhysicalLinearMemoryRegionTree() {
        return m_physical_linear_tree;
    }
    const KMemoryRegionTree& GetPhysicalLinearMemoryRegionTree() const {
        return m_physical_linear_tree;
    }

    KVirtualAddress GetLinearVirtualAddress(KPhysicalAddress address) const {
        return GetInteger(address) + m_linear_phys_to_virt_diff;
    }
    KPhysicalAddress GetLinearPhysicalAddress(KVirtualAddress address) const {
        return GetInteger(address) + m_linear_virt_to_phys_diff;
    }

    const KMemoryRegion* FindVirtual(KVirtualAddress address) const {
        return Find(address, GetVirtualMemoryRegionTree());
    }
    const KMemoryRegion* FindPhysical(KPhysicalAddress address) const {
        return Find(address, GetPhysicalMemoryRegionTree());
    }

    const KMemoryRegion* FindVirtualLinear(KVirtualAddress address) const {
        return Find(address, GetVirtualLinearMemoryRegionTree());
    }
    const KMemoryRegion* FindPhysicalLinear(KPhysicalAddress address) const {
        return Find(address, GetPhysicalLinearMemoryRegionTree());
    }

    KVirtualAddress GetMainStackTopAddress(s32 core_id) const {
        return GetStackTopAddress(core_id, KMemoryRegionType_KernelMiscMainStack);
    }
    KVirtualAddress GetIdleStackTopAddress(s32 core_id) const {
        return GetStackTopAddress(core_id, KMemoryRegionType_KernelMiscIdleStack);
    }
    KVirtualAddress GetExceptionStackTopAddress(s32 core_id) const {
        return GetStackTopAddress(core_id, KMemoryRegionType_KernelMiscExceptionStack);
    }

    const KMemoryRegion& GetSlabRegion() const {
        return Dereference(GetVirtualMemoryRegionTree().FindByType(KMemoryRegionType_KernelSlab));
    }
    const KMemoryRegion& GetDeviceRegion(KMemoryRegionType type) const {
        return Dereference(GetPhysicalMemoryRegionTree().FindFirstDerived(type));
    }
    KPhysicalAddress GetDevicePhysicalAddress(KMemoryRegionType type) const {
        return GetDeviceRegion(type).GetAddress();
    }
    KVirtualAddress GetDeviceVirtualAddress(KMemoryRegionType type) const {
        return GetDeviceRegion(type).GetPairAddress();
    }

    const KMemoryRegion& GetPoolManagementRegion() const {
        return Dereference(
            GetVirtualMemoryRegionTree().FindByType(KMemoryRegionType_VirtualDramPoolManagement));
    }
    const KMemoryRegion& GetPageTableHeapRegion() const {
        return Dereference(
            GetVirtualMemoryRegionTree().FindByType(KMemoryRegionType_VirtualDramKernelPtHeap));
    }
    const KMemoryRegion& GetKernelStackRegion() const {
        return Dereference(GetVirtualMemoryRegionTree().FindByType(KMemoryRegionType_KernelStack));
    }
    const KMemoryRegion& GetTempRegion() const {
        return Dereference(GetVirtualMemoryRegionTree().FindByType(KMemoryRegionType_KernelTemp));
    }

    const KMemoryRegion& GetKernelTraceBufferRegion() const {
        return Dereference(GetVirtualLinearMemoryRegionTree().FindByType(
            KMemoryRegionType_VirtualDramKernelTraceBuffer));
    }

    const KMemoryRegion& GetSecureAppletMemoryRegion() {
        return Dereference(GetVirtualMemoryRegionTree().FindByType(
            KMemoryRegionType_VirtualDramKernelSecureAppletMemory));
    }

    const KMemoryRegion& GetVirtualLinearRegion(KVirtualAddress address) const {
        return Dereference(FindVirtualLinear(address));
    }

    const KMemoryRegion& GetPhysicalLinearRegion(KPhysicalAddress address) const {
        return Dereference(FindPhysicalLinear(address));
    }

    const KMemoryRegion* GetPhysicalKernelTraceBufferRegion() const {
        return GetPhysicalMemoryRegionTree().FindFirstDerived(KMemoryRegionType_KernelTraceBuffer);
    }
    const KMemoryRegion* GetPhysicalOnMemoryBootImageRegion() const {
        return GetPhysicalMemoryRegionTree().FindFirstDerived(KMemoryRegionType_OnMemoryBootImage);
    }
    const KMemoryRegion* GetPhysicalDTBRegion() const {
        return GetPhysicalMemoryRegionTree().FindFirstDerived(KMemoryRegionType_DTB);
    }

    bool IsHeapPhysicalAddress(const KMemoryRegion*& region, KPhysicalAddress address) const {
        return IsTypedAddress(region, address, GetPhysicalLinearMemoryRegionTree(),
                              KMemoryRegionType_DramUserPool);
    }
    bool IsHeapVirtualAddress(const KMemoryRegion*& region, KVirtualAddress address) const {
        return IsTypedAddress(region, address, GetVirtualLinearMemoryRegionTree(),
                              KMemoryRegionType_VirtualDramUserPool);
    }

    bool IsHeapPhysicalAddress(const KMemoryRegion*& region, KPhysicalAddress address,
                               size_t size) const {
        return IsTypedAddress(region, address, size, GetPhysicalLinearMemoryRegionTree(),
                              KMemoryRegionType_DramUserPool);
    }
    bool IsHeapVirtualAddress(const KMemoryRegion*& region, KVirtualAddress address,
                              size_t size) const {
        return IsTypedAddress(region, address, size, GetVirtualLinearMemoryRegionTree(),
                              KMemoryRegionType_VirtualDramUserPool);
    }

    bool IsLinearMappedPhysicalAddress(const KMemoryRegion*& region,
                                       KPhysicalAddress address) const {
        return IsTypedAddress(region, address, GetPhysicalLinearMemoryRegionTree(),
                              static_cast<KMemoryRegionType>(KMemoryRegionAttr_LinearMapped));
    }
    bool IsLinearMappedPhysicalAddress(const KMemoryRegion*& region, KPhysicalAddress address,
                                       size_t size) const {
        return IsTypedAddress(region, address, size, GetPhysicalLinearMemoryRegionTree(),
                              static_cast<KMemoryRegionType>(KMemoryRegionAttr_LinearMapped));
    }

    std::pair<size_t, size_t> GetTotalAndKernelMemorySizes() const {
        size_t total_size = 0, kernel_size = 0;
        for (const auto& region : GetPhysicalMemoryRegionTree()) {
            if (region.IsDerivedFrom(KMemoryRegionType_Dram)) {
                total_size += region.GetSize();
                if (!region.IsDerivedFrom(KMemoryRegionType_DramUserPool)) {
                    kernel_size += region.GetSize();
                }
            }
        }
        return std::make_pair(total_size, kernel_size);
    }

    void InitializeLinearMemoryRegionTrees(KPhysicalAddress aligned_linear_phys_start,
                                           KVirtualAddress linear_virtual_start);
    static size_t GetResourceRegionSizeForInit(bool use_extra_resource);

    auto GetKernelRegionExtents() const {
        return GetVirtualMemoryRegionTree().GetDerivedRegionExtents(KMemoryRegionType_Kernel);
    }
    auto GetKernelCodeRegionExtents() const {
        return GetVirtualMemoryRegionTree().GetDerivedRegionExtents(KMemoryRegionType_KernelCode);
    }
    auto GetKernelStackRegionExtents() const {
        return GetVirtualMemoryRegionTree().GetDerivedRegionExtents(KMemoryRegionType_KernelStack);
    }
    auto GetKernelMiscRegionExtents() const {
        return GetVirtualMemoryRegionTree().GetDerivedRegionExtents(KMemoryRegionType_KernelMisc);
    }
    auto GetKernelSlabRegionExtents() const {
        return GetVirtualMemoryRegionTree().GetDerivedRegionExtents(KMemoryRegionType_KernelSlab);
    }

    auto GetLinearRegionPhysicalExtents() const {
        return GetPhysicalMemoryRegionTree().GetDerivedRegionExtents(
            KMemoryRegionAttr_LinearMapped);
    }

    auto GetLinearRegionVirtualExtents() const {
        const auto physical = GetLinearRegionPhysicalExtents();
        return KMemoryRegion(GetInteger(GetLinearVirtualAddress(physical.GetAddress())),
                             GetInteger(GetLinearVirtualAddress(physical.GetLastAddress())), 0,
                             KMemoryRegionType_None);
    }

    auto GetMainMemoryPhysicalExtents() const {
        return GetPhysicalMemoryRegionTree().GetDerivedRegionExtents(KMemoryRegionType_Dram);
    }
    auto GetCarveoutRegionExtents() const {
        return GetPhysicalMemoryRegionTree().GetDerivedRegionExtents(
            KMemoryRegionAttr_CarveoutProtected);
    }

    auto GetKernelRegionPhysicalExtents() const {
        return GetPhysicalMemoryRegionTree().GetDerivedRegionExtents(
            KMemoryRegionType_DramKernelBase);
    }
    auto GetKernelCodeRegionPhysicalExtents() const {
        return GetPhysicalMemoryRegionTree().GetDerivedRegionExtents(
            KMemoryRegionType_DramKernelCode);
    }
    auto GetKernelSlabRegionPhysicalExtents() const {
        return GetPhysicalMemoryRegionTree().GetDerivedRegionExtents(
            KMemoryRegionType_DramKernelSlab);
    }
    auto GetKernelSecureAppletMemoryRegionPhysicalExtents() {
        return GetPhysicalMemoryRegionTree().GetDerivedRegionExtents(
            KMemoryRegionType_DramKernelSecureAppletMemory);
    }
    auto GetKernelPageTableHeapRegionPhysicalExtents() const {
        return GetPhysicalMemoryRegionTree().GetDerivedRegionExtents(
            KMemoryRegionType_DramKernelPtHeap);
    }
    auto GetKernelInitPageTableRegionPhysicalExtents() const {
        return GetPhysicalMemoryRegionTree().GetDerivedRegionExtents(
            KMemoryRegionType_DramKernelInitPt);
    }

    auto GetKernelPoolManagementRegionPhysicalExtents() const {
        return GetPhysicalMemoryRegionTree().GetDerivedRegionExtents(
            KMemoryRegionType_DramPoolManagement);
    }
    auto GetKernelPoolPartitionRegionPhysicalExtents() const {
        return GetPhysicalMemoryRegionTree().GetDerivedRegionExtents(
            KMemoryRegionType_DramPoolPartition);
    }
    auto GetKernelSystemPoolRegionPhysicalExtents() const {
        return GetPhysicalMemoryRegionTree().GetDerivedRegionExtents(
            KMemoryRegionType_DramSystemPool);
    }
    auto GetKernelSystemNonSecurePoolRegionPhysicalExtents() const {
        return GetPhysicalMemoryRegionTree().GetDerivedRegionExtents(
            KMemoryRegionType_DramSystemNonSecurePool);
    }
    auto GetKernelAppletPoolRegionPhysicalExtents() const {
        return GetPhysicalMemoryRegionTree().GetDerivedRegionExtents(
            KMemoryRegionType_DramAppletPool);
    }
    auto GetKernelApplicationPoolRegionPhysicalExtents() const {
        return GetPhysicalMemoryRegionTree().GetDerivedRegionExtents(
            KMemoryRegionType_DramApplicationPool);
    }

    auto GetKernelTraceBufferRegionPhysicalExtents() const {
        return GetPhysicalMemoryRegionTree().GetDerivedRegionExtents(
            KMemoryRegionType_KernelTraceBuffer);
    }

private:
    template <typename AddressType>
    static bool IsTypedAddress(const KMemoryRegion*& region, AddressType address,
                               const KMemoryRegionTree& tree, KMemoryRegionType type) {
        // Check if the cached region already contains the address.
        if (region != nullptr && region->Contains(GetInteger(address))) {
            return true;
        }

        // Find the containing region, and update the cache.
        if (const KMemoryRegion* found = tree.Find(GetInteger(address));
            found != nullptr && found->IsDerivedFrom(type)) {
            region = found;
            return true;
        } else {
            return false;
        }
    }

    template <typename AddressType>
    static bool IsTypedAddress(const KMemoryRegion*& region, AddressType address, size_t size,
                               const KMemoryRegionTree& tree, KMemoryRegionType type) {
        // Get the end of the checked region.
        const u64 last_address = GetInteger(address) + size - 1;

        // Walk the tree to verify the region is correct.
        const KMemoryRegion* cur = (region != nullptr && region->Contains(GetInteger(address)))
                                       ? region
                                       : tree.Find(GetInteger(address));
        while (cur != nullptr && cur->IsDerivedFrom(type)) {
            if (last_address <= cur->GetLastAddress()) {
                region = cur;
                return true;
            }

            cur = cur->GetNext();
        }
        return false;
    }

    template <typename AddressType>
    static const KMemoryRegion* Find(AddressType address, const KMemoryRegionTree& tree) {
        return tree.Find(GetInteger(address));
    }

    static KMemoryRegion& Dereference(KMemoryRegion* region) {
        ASSERT(region != nullptr);
        return *region;
    }

    static const KMemoryRegion& Dereference(const KMemoryRegion* region) {
        ASSERT(region != nullptr);
        return *region;
    }

    KVirtualAddress GetStackTopAddress(s32 core_id, KMemoryRegionType type) const {
        const auto& region = Dereference(
            GetVirtualMemoryRegionTree().FindByTypeAndAttribute(type, static_cast<u32>(core_id)));
        ASSERT(region.GetEndAddress() != 0);
        return region.GetEndAddress();
    }

public:
    static const KMemoryRegion* Find(const KMemoryLayout& layout, KVirtualAddress address) {
        return Find(address, layout.GetVirtualMemoryRegionTree());
    }
    static const KMemoryRegion* Find(const KMemoryLayout& layout, KPhysicalAddress address) {
        return Find(address, layout.GetPhysicalMemoryRegionTree());
    }

private:
    u64 m_linear_phys_to_virt_diff{};
    u64 m_linear_virt_to_phys_diff{};
    KMemoryRegionAllocator m_memory_region_allocator;
    KMemoryRegionTree m_virtual_tree;
    KMemoryRegionTree m_physical_tree;
    KMemoryRegionTree m_virtual_linear_tree;
    KMemoryRegionTree m_physical_linear_tree;
};

namespace Init {

// These should be generic, regardless of board.
void SetupPoolPartitionMemoryRegions(KMemoryLayout& memory_layout);

// These may be implemented in a board-specific manner.
void SetupDevicePhysicalMemoryRegions(KMemoryLayout& memory_layout);
void SetupDramPhysicalMemoryRegions(KMemoryLayout& memory_layout);

} // namespace Init

} // namespace Kernel
