// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-FileCopyrightText: Copyright 2007 The Android Open Source Project
// SPDX-License-Identifier: GPL-3.0-or-later
// Parts of this implementation were based on:
// https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/ui/GraphicBuffer.h

#pragma once

#include <memory>

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "core/hle/service/nvnflinger/pixel_format.h"

namespace Service::Nvidia::NvCore {
class NvMap;
} // namespace Service::Nvidia::NvCore

namespace Service::android {

struct NvGraphicBuffer {
    constexpr NvGraphicBuffer() = default;

    constexpr NvGraphicBuffer(u32 width_, u32 height_, PixelFormat format_, u32 usage_)
        : width{static_cast<s32>(width_)}, height{static_cast<s32>(height_)}, format{format_},
          usage{static_cast<s32>(usage_)} {}

    constexpr u32 Width() const {
        return static_cast<u32>(width);
    }

    constexpr u32 Height() const {
        return static_cast<u32>(height);
    }

    constexpr u32 Stride() const {
        return static_cast<u32>(stride);
    }

    constexpr u32 Usage() const {
        return static_cast<u32>(usage);
    }

    constexpr PixelFormat Format() const {
        return format;
    }

    constexpr u32 BufferId() const {
        return buffer_id;
    }

    constexpr PixelFormat ExternalFormat() const {
        return external_format;
    }

    constexpr u32 Handle() const {
        return handle;
    }

    constexpr u32 Offset() const {
        return offset;
    }

    constexpr bool NeedsReallocation(u32 width_, u32 height_, PixelFormat format_,
                                     u32 usage_) const {
        if (static_cast<s32>(width_) != width) {
            return true;
        }

        if (static_cast<s32>(height_) != height) {
            return true;
        }

        if (format_ != format) {
            return true;
        }

        if ((static_cast<u32>(usage) & usage_) != usage_) {
            return true;
        }

        return false;
    }

    u32 magic{};
    s32 width{};
    s32 height{};
    s32 stride{};
    PixelFormat format{};
    s32 usage{};
    INSERT_PADDING_WORDS(1);
    u32 index{};
    INSERT_PADDING_WORDS(3);
    u32 buffer_id{};
    INSERT_PADDING_WORDS(6);
    PixelFormat external_format{};
    INSERT_PADDING_WORDS(10);
    u32 handle{};
    u32 offset{};
    INSERT_PADDING_WORDS(60);
};
static_assert(sizeof(NvGraphicBuffer) == 0x16C, "NvGraphicBuffer has wrong size");

class GraphicBuffer final : public NvGraphicBuffer {
public:
    explicit GraphicBuffer(u32 width, u32 height, PixelFormat format, u32 usage);
    explicit GraphicBuffer(Service::Nvidia::NvCore::NvMap& nvmap,
                           std::shared_ptr<NvGraphicBuffer> buffer);
    ~GraphicBuffer();

private:
    Service::Nvidia::NvCore::NvMap* m_nvmap{};
};

} // namespace Service::android
