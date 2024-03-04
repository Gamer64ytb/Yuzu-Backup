// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-FileCopyrightText: Copyright 2012 The Android Open Source Project
// SPDX-License-Identifier: GPL-3.0-or-later
// Parts of this implementation were based on:
// https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/BufferItemConsumer.h

#pragma once

#include <chrono>
#include <memory>

#include "common/common_types.h"
#include "core/hle/service/nvnflinger/consumer_base.h"
#include "core/hle/service/nvnflinger/status.h"

namespace Service::android {

class BufferItem;

class BufferItemConsumer final : public ConsumerBase {
public:
    explicit BufferItemConsumer(std::shared_ptr<BufferQueueConsumer> consumer);
    Status AcquireBuffer(BufferItem* item, std::chrono::nanoseconds present_when,
                         bool wait_for_fence = true);
    Status ReleaseBuffer(const BufferItem& item, const Fence& release_fence);
};

} // namespace Service::android
