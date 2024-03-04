// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "common/common_types.h"

namespace Service::HID {

template <typename State>
struct AtomicStorage {
    s64 sampling_number;
    State state;
};

template <typename State, std::size_t max_buffer_size>
struct Lifo {
    s64 timestamp{};
    s64 total_buffer_count = static_cast<s64>(max_buffer_size);
    s64 buffer_tail{};
    s64 buffer_count{};
    std::array<AtomicStorage<State>, max_buffer_size> entries{};

    const AtomicStorage<State>& ReadCurrentEntry() const {
        return entries[buffer_tail];
    }

    const AtomicStorage<State>& ReadPreviousEntry() const {
        return entries[GetPreviousEntryIndex()];
    }

    std::size_t GetPreviousEntryIndex() const {
        return static_cast<size_t>((buffer_tail + max_buffer_size - 1) % max_buffer_size);
    }

    std::size_t GetNextEntryIndex() const {
        return static_cast<size_t>((buffer_tail + 1) % max_buffer_size);
    }

    void WriteNextEntry(const State& new_state) {
        if (buffer_count < static_cast<s64>(max_buffer_size) - 1) {
            buffer_count++;
        }
        buffer_tail = GetNextEntryIndex();
        const auto& previous_entry = ReadPreviousEntry();
        entries[buffer_tail].sampling_number = previous_entry.sampling_number + 1;
        entries[buffer_tail].state = new_state;
    }
};

} // namespace Service::HID
