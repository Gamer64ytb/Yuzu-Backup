// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <utility>
#include <vector>

namespace VideoCommon {

/// Container to push objects to be destroyed a few ticks in the future
template <typename T, size_t TICKS_TO_DESTROY>
class DelayedDestructionRing {
public:
    void Tick() {
        index = (index + 1) % TICKS_TO_DESTROY;
        elements[index].clear();
    }

    void Push(T&& object) {
        elements[index].push_back(std::move(object));
    }

private:
    size_t index = 0;
    std::array<std::vector<T>, TICKS_TO_DESTROY> elements;
};

} // namespace VideoCommon
