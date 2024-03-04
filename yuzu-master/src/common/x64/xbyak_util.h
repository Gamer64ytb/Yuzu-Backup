// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <type_traits>
#include <xbyak/xbyak.h>
#include "common/x64/xbyak_abi.h"

namespace Common::X64 {

// Constants for use with cmpps/cmpss
enum {
    CMP_EQ = 0,
    CMP_LT = 1,
    CMP_LE = 2,
    CMP_UNORD = 3,
    CMP_NEQ = 4,
    CMP_NLT = 5,
    CMP_NLE = 6,
    CMP_ORD = 7,
};

constexpr bool IsWithin2G(uintptr_t ref, uintptr_t target) {
    const u64 distance = target - (ref + 5);
    return !(distance >= 0x8000'0000ULL && distance <= ~0x8000'0000ULL);
}

inline bool IsWithin2G(const Xbyak::CodeGenerator& code, uintptr_t target) {
    return IsWithin2G(reinterpret_cast<uintptr_t>(code.getCurr()), target);
}

template <typename T>
inline void CallFarFunction(Xbyak::CodeGenerator& code, const T f) {
    static_assert(std::is_pointer_v<T>, "Argument must be a (function) pointer.");
    size_t addr = reinterpret_cast<size_t>(f);
    if (IsWithin2G(code, addr)) {
        code.call(f);
    } else {
        // ABI_RETURN is a safe temp register to use before a call
        code.mov(ABI_RETURN, addr);
        code.call(ABI_RETURN);
    }
}

} // namespace Common::X64
