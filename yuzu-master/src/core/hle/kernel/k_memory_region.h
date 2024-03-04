// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/intrusive_red_black_tree.h"
#include "core/hle/kernel/k_memory_region_type.h"
#include "core/hle/kernel/k_typed_address.h"

namespace Kernel {

class KMemoryRegionAllocator;

class KMemoryRegion final : public Common::IntrusiveRedBlackTreeBaseNode<KMemoryRegion> {
    friend class KMemoryRegionTree;

public:
    YUZU_NON_COPYABLE(KMemoryRegion);
    YUZU_NON_MOVEABLE(KMemoryRegion);

    constexpr KMemoryRegion() = default;
    constexpr KMemoryRegion(u64 address, u64 last_address)
        : m_address{address}, m_last_address{last_address} {}
    constexpr KMemoryRegion(u64 address, u64 last_address, u64 pair_address, u32 attributes,
                            u32 type_id)
        : m_address(address), m_last_address(last_address), m_pair_address(pair_address),
          m_attributes(attributes), m_type_id(type_id) {}
    constexpr KMemoryRegion(u64 address, u64 last_address, u32 attributes, u32 type_id)
        : KMemoryRegion(address, last_address, std::numeric_limits<u64>::max(), attributes,
                        type_id) {}

    ~KMemoryRegion() = default;

    static constexpr int Compare(const KMemoryRegion& lhs, const KMemoryRegion& rhs) {
        if (lhs.GetAddress() < rhs.GetAddress()) {
            return -1;
        } else if (lhs.GetAddress() <= rhs.GetLastAddress()) {
            return 0;
        } else {
            return 1;
        }
    }

    constexpr u64 GetAddress() const {
        return m_address;
    }

    constexpr u64 GetPairAddress() const {
        return m_pair_address;
    }

    constexpr u64 GetLastAddress() const {
        return m_last_address;
    }

    constexpr u64 GetEndAddress() const {
        return this->GetLastAddress() + 1;
    }

    constexpr size_t GetSize() const {
        return this->GetEndAddress() - this->GetAddress();
    }

    constexpr u32 GetAttributes() const {
        return m_attributes;
    }

    constexpr u32 GetType() const {
        return m_type_id;
    }

    constexpr void SetType(u32 type) {
        ASSERT(this->CanDerive(type));
        m_type_id = type;
    }

    constexpr bool Contains(u64 addr) const {
        ASSERT(this->GetEndAddress() != 0);
        return this->GetAddress() <= addr && addr <= this->GetLastAddress();
    }

    constexpr bool IsDerivedFrom(u32 type) const {
        return (this->GetType() | type) == this->GetType();
    }

    constexpr bool HasTypeAttribute(u32 attr) const {
        return (this->GetType() | attr) == this->GetType();
    }

    constexpr bool CanDerive(u32 type) const {
        return (this->GetType() | type) == type;
    }

    constexpr void SetPairAddress(u64 a) {
        m_pair_address = a;
    }

    constexpr void SetTypeAttribute(u32 attr) {
        m_type_id |= attr;
    }

private:
    constexpr void Reset(u64 a, u64 la, u64 p, u32 r, u32 t) {
        m_address = a;
        m_pair_address = p;
        m_last_address = la;
        m_attributes = r;
        m_type_id = t;
    }

    u64 m_address{};
    u64 m_last_address{};
    u64 m_pair_address{};
    u32 m_attributes{};
    u32 m_type_id{};
};

class KMemoryRegionTree final {
private:
    using TreeType =
        Common::IntrusiveRedBlackTreeBaseTraits<KMemoryRegion>::TreeType<KMemoryRegion>;

public:
    YUZU_NON_COPYABLE(KMemoryRegionTree);
    YUZU_NON_MOVEABLE(KMemoryRegionTree);

    using value_type = TreeType::value_type;
    using size_type = TreeType::size_type;
    using difference_type = TreeType::difference_type;
    using pointer = TreeType::pointer;
    using const_pointer = TreeType::const_pointer;
    using reference = TreeType::reference;
    using const_reference = TreeType::const_reference;
    using iterator = TreeType::iterator;
    using const_iterator = TreeType::const_iterator;

    struct DerivedRegionExtents {
        const KMemoryRegion* first_region{};
        const KMemoryRegion* last_region{};

        constexpr DerivedRegionExtents() = default;

        constexpr u64 GetAddress() const {
            return this->first_region->GetAddress();
        }

        constexpr u64 GetLastAddress() const {
            return this->last_region->GetLastAddress();
        }

        constexpr u64 GetEndAddress() const {
            return this->GetLastAddress() + 1;
        }

        constexpr size_t GetSize() const {
            return this->GetEndAddress() - this->GetAddress();
        }
    };

    explicit KMemoryRegionTree(KMemoryRegionAllocator& memory_region_allocator_);
    ~KMemoryRegionTree() = default;

    KMemoryRegion* FindModifiable(u64 address) {
        if (auto it = this->find(KMemoryRegion(address, address, 0, 0)); it != this->end()) {
            return std::addressof(*it);
        } else {
            return nullptr;
        }
    }

    const KMemoryRegion* Find(u64 address) const {
        if (auto it = this->find(KMemoryRegion(address, address, 0, 0)); it != this->cend()) {
            return std::addressof(*it);
        } else {
            return nullptr;
        }
    }

    const KMemoryRegion* FindByType(KMemoryRegionType type_id) const {
        for (auto it = this->cbegin(); it != this->cend(); ++it) {
            if (it->GetType() == static_cast<u32>(type_id)) {
                return std::addressof(*it);
            }
        }
        return nullptr;
    }

    const KMemoryRegion* FindByTypeAndAttribute(u32 type_id, u32 attr) const {
        for (auto it = this->cbegin(); it != this->cend(); ++it) {
            if (it->GetType() == type_id && it->GetAttributes() == attr) {
                return std::addressof(*it);
            }
        }
        return nullptr;
    }

    const KMemoryRegion* FindFirstDerived(KMemoryRegionType type_id) const {
        for (auto it = this->cbegin(); it != this->cend(); it++) {
            if (it->IsDerivedFrom(type_id)) {
                return std::addressof(*it);
            }
        }
        return nullptr;
    }

    const KMemoryRegion* FindLastDerived(KMemoryRegionType type_id) const {
        const KMemoryRegion* region = nullptr;
        for (auto it = this->begin(); it != this->end(); it++) {
            if (it->IsDerivedFrom(type_id)) {
                region = std::addressof(*it);
            }
        }
        return region;
    }

    DerivedRegionExtents GetDerivedRegionExtents(KMemoryRegionType type_id) const {
        DerivedRegionExtents extents;

        ASSERT(extents.first_region == nullptr);
        ASSERT(extents.last_region == nullptr);

        for (auto it = this->cbegin(); it != this->cend(); it++) {
            if (it->IsDerivedFrom(type_id)) {
                if (extents.first_region == nullptr) {
                    extents.first_region = std::addressof(*it);
                }
                extents.last_region = std::addressof(*it);
            }
        }

        ASSERT(extents.first_region != nullptr);
        ASSERT(extents.last_region != nullptr);

        return extents;
    }

    DerivedRegionExtents GetDerivedRegionExtents(u32 type_id) const {
        return GetDerivedRegionExtents(static_cast<KMemoryRegionType>(type_id));
    }

    void InsertDirectly(u64 address, u64 last_address, u32 attr = 0, u32 type_id = 0);
    bool Insert(u64 address, size_t size, u32 type_id, u32 new_attr = 0, u32 old_attr = 0);

    KVirtualAddress GetRandomAlignedRegion(size_t size, size_t alignment, u32 type_id);

    KVirtualAddress GetRandomAlignedRegionWithGuard(size_t size, size_t alignment, u32 type_id,
                                                    size_t guard_size) {
        return this->GetRandomAlignedRegion(size + 2 * guard_size, alignment, type_id) + guard_size;
    }

    // Iterator accessors.
    iterator begin() {
        return m_tree.begin();
    }

    const_iterator begin() const {
        return m_tree.begin();
    }

    iterator end() {
        return m_tree.end();
    }

    const_iterator end() const {
        return m_tree.end();
    }

    const_iterator cbegin() const {
        return this->begin();
    }

    const_iterator cend() const {
        return this->end();
    }

    iterator iterator_to(reference ref) {
        return m_tree.iterator_to(ref);
    }

    const_iterator iterator_to(const_reference ref) const {
        return m_tree.iterator_to(ref);
    }

    // Content management.
    bool empty() const {
        return m_tree.empty();
    }

    reference back() {
        return m_tree.back();
    }

    const_reference back() const {
        return m_tree.back();
    }

    reference front() {
        return m_tree.front();
    }

    const_reference front() const {
        return m_tree.front();
    }

    iterator insert(reference ref) {
        return m_tree.insert(ref);
    }

    iterator erase(iterator it) {
        return m_tree.erase(it);
    }

    iterator find(const_reference ref) const {
        return m_tree.find(ref);
    }

    iterator nfind(const_reference ref) const {
        return m_tree.nfind(ref);
    }

private:
    TreeType m_tree{};
    KMemoryRegionAllocator& m_memory_region_allocator;
};

class KMemoryRegionAllocator final {
public:
    YUZU_NON_COPYABLE(KMemoryRegionAllocator);
    YUZU_NON_MOVEABLE(KMemoryRegionAllocator);

    static constexpr size_t MaxMemoryRegions = 200;

    constexpr KMemoryRegionAllocator() = default;
    constexpr ~KMemoryRegionAllocator() = default;

    template <typename... Args>
    KMemoryRegion* Allocate(Args&&... args) {
        // Ensure we stay within the bounds of our heap.
        ASSERT(m_num_regions < MaxMemoryRegions);

        // Create the new region.
        KMemoryRegion* region = std::addressof(m_region_heap[m_num_regions++]);
        std::construct_at(region, std::forward<Args>(args)...);

        return region;
    }

private:
    std::array<KMemoryRegion, MaxMemoryRegions> m_region_heap{};
    size_t m_num_regions{};
};

} // namespace Kernel
