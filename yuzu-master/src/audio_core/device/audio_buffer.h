// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_types.h"

namespace AudioCore {

struct AudioBuffer {
    /// Timestamp this buffer started playing.
    u64 start_timestamp;
    /// Timestamp this buffer should finish playing.
    u64 end_timestamp;
    /// Timestamp this buffer completed playing.
    s64 played_timestamp;
    /// Game memory address for these samples.
    VAddr samples;
    /// Unique identifier for this buffer.
    u64 tag;
    /// Size of the samples buffer.
    u64 size;
};

} // namespace AudioCore
