// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <bitset>
#include <cstddef>

#include "shader_recompiler/frontend/ir/attribute.h"

namespace Shader {

struct VaryingState {
    std::bitset<512> mask{};

    void Set(IR::Attribute attribute, bool state = true) {
        mask[static_cast<size_t>(attribute)] = state;
    }

    [[nodiscard]] bool operator[](IR::Attribute attribute) const noexcept {
        return mask[static_cast<size_t>(attribute)];
    }

    [[nodiscard]] bool AnyComponent(IR::Attribute base) const noexcept {
        return mask[static_cast<size_t>(base) + 0] || mask[static_cast<size_t>(base) + 1] ||
               mask[static_cast<size_t>(base) + 2] || mask[static_cast<size_t>(base) + 3];
    }

    [[nodiscard]] bool AllComponents(IR::Attribute base) const noexcept {
        return mask[static_cast<size_t>(base) + 0] && mask[static_cast<size_t>(base) + 1] &&
               mask[static_cast<size_t>(base) + 2] && mask[static_cast<size_t>(base) + 3];
    }

    [[nodiscard]] bool IsUniform(IR::Attribute base) const noexcept {
        return AnyComponent(base) == AllComponents(base);
    }

    [[nodiscard]] bool Generic(size_t index, size_t component) const noexcept {
        return mask[static_cast<size_t>(IR::Attribute::Generic0X) + index * 4 + component];
    }

    [[nodiscard]] bool Generic(size_t index) const noexcept {
        return Generic(index, 0) || Generic(index, 1) || Generic(index, 2) || Generic(index, 3);
    }

    [[nodiscard]] bool ClipDistances() const noexcept {
        return AnyComponent(IR::Attribute::ClipDistance0) ||
               AnyComponent(IR::Attribute::ClipDistance4);
    }

    [[nodiscard]] bool Legacy() const noexcept {
        return AnyComponent(IR::Attribute::ColorFrontDiffuseR) ||
               AnyComponent(IR::Attribute::ColorFrontSpecularR) ||
               AnyComponent(IR::Attribute::ColorBackDiffuseR) ||
               AnyComponent(IR::Attribute::ColorBackSpecularR) || FixedFunctionTexture() ||
               mask[static_cast<size_t>(IR::Attribute::FogCoordinate)];
    }

    [[nodiscard]] bool FixedFunctionTexture() const noexcept {
        for (size_t index = 0; index < 10; ++index) {
            if (AnyComponent(IR::Attribute::FixedFncTexture0S + index * 4)) {
                return true;
            }
        }
        return false;
    }
};

} // namespace Shader
