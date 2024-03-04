// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/host_memory.h"
#include "common/typed_address.h"

namespace Core {

namespace DramMemoryMap {
enum : u64 {
    Base = 0x80000000ULL,
    KernelReserveBase = Base + 0x60000,
    SlabHeapBase = KernelReserveBase + 0x85000,
};
}; // namespace DramMemoryMap

class DeviceMemory {
public:
    explicit DeviceMemory();
    ~DeviceMemory();

    DeviceMemory& operator=(const DeviceMemory&) = delete;
    DeviceMemory(const DeviceMemory&) = delete;

    template <typename T>
    Common::PhysicalAddress GetPhysicalAddr(const T* ptr) const {
        return (reinterpret_cast<uintptr_t>(ptr) -
                reinterpret_cast<uintptr_t>(buffer.BackingBasePointer())) +
               DramMemoryMap::Base;
    }

    template <typename T>
    PAddr GetRawPhysicalAddr(const T* ptr) const {
        return static_cast<PAddr>(reinterpret_cast<uintptr_t>(ptr) -
                                  reinterpret_cast<uintptr_t>(buffer.BackingBasePointer()));
    }

    template <typename T>
    T* GetPointer(Common::PhysicalAddress addr) {
        return reinterpret_cast<T*>(buffer.BackingBasePointer() +
                                    (GetInteger(addr) - DramMemoryMap::Base));
    }

    template <typename T>
    const T* GetPointer(Common::PhysicalAddress addr) const {
        return reinterpret_cast<T*>(buffer.BackingBasePointer() +
                                    (GetInteger(addr) - DramMemoryMap::Base));
    }

    template <typename T>
    T* GetPointerFromRaw(PAddr addr) {
        return reinterpret_cast<T*>(buffer.BackingBasePointer() + addr);
    }

    template <typename T>
    const T* GetPointerFromRaw(PAddr addr) const {
        return reinterpret_cast<T*>(buffer.BackingBasePointer() + addr);
    }

    Common::HostMemory buffer;
};

} // namespace Core
