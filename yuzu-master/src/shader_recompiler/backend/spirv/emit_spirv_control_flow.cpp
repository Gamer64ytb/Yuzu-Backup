// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"

namespace Shader::Backend::SPIRV {

void EmitJoin(EmitContext&) {
    throw NotImplementedException("Join shouldn't be emitted");
}

void EmitDemoteToHelperInvocation(EmitContext& ctx) {
    if (ctx.profile.support_demote_to_helper_invocation) {
        ctx.OpDemoteToHelperInvocation();
    } else {
        const Id kill_label{ctx.OpLabel()};
        const Id impossible_label{ctx.OpLabel()};
        ctx.OpSelectionMerge(impossible_label, spv::SelectionControlMask::MaskNone);
        ctx.OpBranchConditional(ctx.true_value, kill_label, impossible_label);
        ctx.AddLabel(kill_label);
        ctx.OpKill();
        ctx.AddLabel(impossible_label);
    }
}

} // namespace Shader::Backend::SPIRV
