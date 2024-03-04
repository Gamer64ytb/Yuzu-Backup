// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-FileCopyrightText: Copyright 2012 The Android Open Source Project
// SPDX-License-Identifier: GPL-3.0-or-later
// Parts of this implementation were based on:
// https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/ui/Fence.h

#pragma once

#include <array>

#include "common/common_types.h"
#include "core/hle/service/nvdrv/nvdata.h"

namespace Service::android {

class Fence {
public:
    constexpr Fence() = default;

    static constexpr Fence NoFence() {
        Fence fence;
        fence.fences[0].id = -1;
        fence.fences[1].id = -1;
        fence.fences[2].id = -1;
        fence.fences[3].id = -1;
        return fence;
    }

public:
    u32 num_fences{};
    std::array<Service::Nvidia::NvFence, 4> fences{};
};
static_assert(sizeof(Fence) == 36, "Fence has wrong size");

} // namespace Service::android
