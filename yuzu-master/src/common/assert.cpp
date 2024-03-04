// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/logging/backend.h"

#include "common/settings.h"

void assert_fail_impl() {
    if (Settings::values.use_debug_asserts) {
        Common::Log::Stop();
        Crash();
    }
}

[[noreturn]] void unreachable_impl() {
    Common::Log::Stop();
    Crash();
    throw std::runtime_error("Unreachable code");
}
