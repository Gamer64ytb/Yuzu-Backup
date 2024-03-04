// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <vector>

#include "common/common_types.h"
#include "shader_recompiler/runtime_info.h"
#include "video_core/engines/maxwell_3d.h"

namespace VideoCommon {

struct TransformFeedbackState {
    struct Layout {
        u32 stream;
        u32 varying_count;
        u32 stride;
    };
    std::array<Layout, Tegra::Engines::Maxwell3D::Regs::NumTransformFeedbackBuffers> layouts;
    std::array<std::array<Tegra::Engines::Maxwell3D::Regs::StreamOutLayout, 32>,
               Tegra::Engines::Maxwell3D::Regs::NumTransformFeedbackBuffers>
        varyings;
};

std::pair<std::array<Shader::TransformFeedbackVarying, 256>, u32> MakeTransformFeedbackVaryings(
    const TransformFeedbackState& state);

} // namespace VideoCommon
