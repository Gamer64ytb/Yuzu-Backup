// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
void MOV(TranslatorVisitor& v, u64 insn, const IR::U32& src, bool is_mov32i = false) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<39, 4, u64> mask;
        BitField<12, 4, u64> mov32i_mask;
    } const mov{insn};

    u64 mask = is_mov32i ? mov.mov32i_mask : mov.mask;
    if (mask != 0xf && mask != 0x1) {
        LOG_WARNING(Shader, "(STUBBED) Masked Mov");
        return;
    }
    v.X(mov.dest_reg, src);
}
} // Anonymous namespace

void TranslatorVisitor::MOV_reg(u64 insn) {
    MOV(*this, insn, GetReg20(insn));
}

void TranslatorVisitor::MOV_cbuf(u64 insn) {
    MOV(*this, insn, GetCbuf(insn));
}

void TranslatorVisitor::MOV_imm(u64 insn) {
    MOV(*this, insn, GetImm20(insn));
}

void TranslatorVisitor::MOV32I(u64 insn) {
    MOV(*this, insn, GetImm32(insn), true);
}

} // namespace Shader::Maxwell
