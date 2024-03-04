// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/backend/glasm/glasm_emit_context.h"

namespace Shader::Backend::GLASM {

void EmitLogicalOr(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("OR.S {},{},{};", inst, a, b);
}

void EmitLogicalAnd(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("AND.S {},{},{};", inst, a, b);
}

void EmitLogicalXor(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("XOR.S {},{},{};", inst, a, b);
}

void EmitLogicalNot(EmitContext& ctx, IR::Inst& inst, ScalarS32 value) {
    ctx.Add("SEQ.S {},{},0;", inst, value);
}

} // namespace Shader::Backend::GLASM
