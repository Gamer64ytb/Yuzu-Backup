// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include "common/common_types.h"
#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ir/reg.h"
#include "shader_recompiler/frontend/maxwell/location.h"

namespace Shader::Maxwell {

struct IndirectBranchTableInfo {
    u32 cbuf_index{};
    u32 cbuf_offset{};
    u32 num_entries{};
    s32 branch_offset{};
    IR::Reg branch_reg{};
};

std::optional<IndirectBranchTableInfo> TrackIndirectBranchTable(Environment& env, Location brx_pos,
                                                                Location block_begin);

} // namespace Shader::Maxwell
