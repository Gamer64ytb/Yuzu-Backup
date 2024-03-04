// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>

#include "common/logging/types.h"

namespace Common::Log {

/**
 * A log entry. Log entries are store in a structured format to permit more varied output
 * formatting on different frontends, as well as facilitating filtering and aggregation.
 */
struct Entry {
    std::chrono::microseconds timestamp;
    Class log_class{};
    Level log_level{};
    const char* filename = nullptr;
    unsigned int line_num = 0;
    std::string function;
    std::string message;
};

} // namespace Common::Log
