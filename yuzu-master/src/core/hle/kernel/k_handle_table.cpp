// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/k_handle_table.h"
#include "core/hle/kernel/k_process.h"

namespace Kernel {

Result KHandleTable::Finalize() {
    // Get the table and clear our record of it.
    u16 saved_table_size = 0;
    {
        KScopedDisableDispatch dd{m_kernel};
        KScopedSpinLock lk(m_lock);

        std::swap(m_table_size, saved_table_size);
    }

    // Close and free all entries.
    for (size_t i = 0; i < saved_table_size; i++) {
        if (KAutoObject* obj = m_objects[i]; obj != nullptr) {
            obj->Close();
        }
    }

    R_SUCCEED();
}

bool KHandleTable::Remove(Handle handle) {
    // Don't allow removal of a pseudo-handle.
    if (Svc::IsPseudoHandle(handle)) [[unlikely]] {
        return false;
    }

    // Handles must not have reserved bits set.
    const auto handle_pack = HandlePack(handle);
    if (handle_pack.reserved != 0) [[unlikely]] {
        return false;
    }

    // Find the object and free the entry.
    KAutoObject* obj = nullptr;
    {
        KScopedDisableDispatch dd{m_kernel};
        KScopedSpinLock lk(m_lock);

        if (this->IsValidHandle(handle)) [[likely]] {
            const auto index = handle_pack.index;

            obj = m_objects[index];
            this->FreeEntry(index);
        } else {
            return false;
        }
    }

    // Close the object.
    m_kernel.UnregisterInUseObject(obj);
    obj->Close();
    return true;
}

Result KHandleTable::Add(Handle* out_handle, KAutoObject* obj) {
    KScopedDisableDispatch dd{m_kernel};
    KScopedSpinLock lk(m_lock);

    // Never exceed our capacity.
    R_UNLESS(m_count < m_table_size, ResultOutOfHandles);

    // Allocate entry, set output handle.
    {
        const auto linear_id = this->AllocateLinearId();
        const auto index = this->AllocateEntry();

        m_entry_infos[index].linear_id = linear_id;
        m_objects[index] = obj;

        obj->Open();

        *out_handle = EncodeHandle(static_cast<u16>(index), linear_id);
    }

    R_SUCCEED();
}

KScopedAutoObject<KAutoObject> KHandleTable::GetObjectForIpc(Handle handle,
                                                             KThread* cur_thread) const {
    // Handle pseudo-handles.
    ASSERT(cur_thread != nullptr);
    if (handle == Svc::PseudoHandle::CurrentProcess) {
        auto* const cur_process = cur_thread->GetOwnerProcess();
        ASSERT(cur_process != nullptr);
        return cur_process;
    }
    if (handle == Svc::PseudoHandle::CurrentThread) {
        return cur_thread;
    }

    return GetObjectForIpcWithoutPseudoHandle(handle);
}

Result KHandleTable::Reserve(Handle* out_handle) {
    KScopedDisableDispatch dd{m_kernel};
    KScopedSpinLock lk(m_lock);

    // Never exceed our capacity.
    R_UNLESS(m_count < m_table_size, ResultOutOfHandles);

    *out_handle = EncodeHandle(static_cast<u16>(this->AllocateEntry()), this->AllocateLinearId());
    R_SUCCEED();
}

void KHandleTable::Unreserve(Handle handle) {
    KScopedDisableDispatch dd{m_kernel};
    KScopedSpinLock lk(m_lock);

    // Unpack the handle.
    const auto handle_pack = HandlePack(handle);
    const auto index = handle_pack.index;
    const auto linear_id = handle_pack.linear_id;
    const auto reserved = handle_pack.reserved;
    ASSERT(reserved == 0);
    ASSERT(linear_id != 0);

    if (index < m_table_size) [[likely]] {
        // NOTE: This code does not check the linear id.
        ASSERT(m_objects[index] == nullptr);
        this->FreeEntry(index);
    }
}

void KHandleTable::Register(Handle handle, KAutoObject* obj) {
    KScopedDisableDispatch dd{m_kernel};
    KScopedSpinLock lk(m_lock);

    // Unpack the handle.
    const auto handle_pack = HandlePack(handle);
    const auto index = handle_pack.index;
    const auto linear_id = handle_pack.linear_id;
    const auto reserved = handle_pack.reserved;
    ASSERT(reserved == 0);
    ASSERT(linear_id != 0);

    if (index < m_table_size) [[likely]] {
        // Set the entry.
        ASSERT(m_objects[index] == nullptr);

        m_entry_infos[index].linear_id = static_cast<u16>(linear_id);
        m_objects[index] = obj;

        obj->Open();
    }
}

} // namespace Kernel
