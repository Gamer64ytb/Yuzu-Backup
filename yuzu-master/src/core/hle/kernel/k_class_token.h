// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/bit_util.h"
#include "common/common_types.h"

namespace Kernel {

class KAutoObject;

class KSystemResource;

class KClassTokenGenerator {
public:
    using TokenBaseType = u16;

public:
    static constexpr size_t BaseClassBits = 8;
    static constexpr size_t FinalClassBits = (sizeof(TokenBaseType) * CHAR_BIT) - BaseClassBits;
    // One bit per base class.
    static constexpr size_t NumBaseClasses = BaseClassBits;
    // Final classes are permutations of three bits.
    static constexpr size_t NumFinalClasses = [] {
        TokenBaseType index = 0;
        for (size_t i = 0; i < FinalClassBits; i++) {
            for (size_t j = i + 1; j < FinalClassBits; j++) {
                for (size_t k = j + 1; k < FinalClassBits; k++) {
                    index++;
                }
            }
        }
        return index;
    }();

private:
    template <TokenBaseType Index>
    static constexpr inline TokenBaseType BaseClassToken = 1U << Index;

    template <TokenBaseType Index>
    static constexpr inline TokenBaseType FinalClassToken = [] {
        TokenBaseType index = 0;
        for (size_t i = 0; i < FinalClassBits; i++) {
            for (size_t j = i + 1; j < FinalClassBits; j++) {
                for (size_t k = j + 1; k < FinalClassBits; k++) {
                    if ((index++) == Index) {
                        return static_cast<TokenBaseType>(((1ULL << i) | (1ULL << j) | (1ULL << k))
                                                          << BaseClassBits);
                    }
                }
            }
        }
        UNREACHABLE();
    }();

    template <typename T>
    static constexpr inline TokenBaseType GetClassToken() {
        static_assert(std::is_base_of<KAutoObject, T>::value);
        if constexpr (std::is_same<T, KAutoObject>::value) {
            static_assert(T::ObjectType == ObjectType::KAutoObject);
            return 0;
        } else if constexpr (!std::is_final<T>::value && !std::same_as<T, KSystemResource>) {
            static_assert(ObjectType::BaseClassesStart <= T::ObjectType &&
                          T::ObjectType < ObjectType::BaseClassesEnd);
            constexpr auto ClassIndex = static_cast<TokenBaseType>(T::ObjectType) -
                                        static_cast<TokenBaseType>(ObjectType::BaseClassesStart);
            return BaseClassToken<ClassIndex> | GetClassToken<typename T::BaseClass>();
        } else if constexpr (ObjectType::FinalClassesStart <= T::ObjectType &&
                             T::ObjectType < ObjectType::FinalClassesEnd) {
            constexpr auto ClassIndex = static_cast<TokenBaseType>(T::ObjectType) -
                                        static_cast<TokenBaseType>(ObjectType::FinalClassesStart);
            return FinalClassToken<ClassIndex> | GetClassToken<typename T::BaseClass>();
        } else {
            static_assert(!std::is_same<T, T>::value, "GetClassToken: Invalid Type");
        }
    };

public:
    enum class ObjectType {
        KAutoObject,

        BaseClassesStart,

        KSynchronizationObject = BaseClassesStart,
        KReadableEvent,

        BaseClassesEnd,

        FinalClassesStart = BaseClassesEnd,

        KInterruptEvent = FinalClassesStart,
        KDebug,
        KThread,
        KServerPort,
        KServerSession,
        KClientPort,
        KClientSession,
        KProcess,
        KResourceLimit,
        KLightSession,
        KPort,
        KSession,
        KSharedMemory,
        KEvent,
        KLightClientSession,
        KLightServerSession,
        KTransferMemory,
        KDeviceAddressSpace,
        KSessionRequest,
        KCodeMemory,

        KSystemResource,

        // NOTE: True order for these has not been determined yet.
        KAlpha,
        KBeta,

        FinalClassesEnd = FinalClassesStart + NumFinalClasses,
    };

    template <typename T>
    static constexpr inline TokenBaseType ClassToken = GetClassToken<T>();
};

using ClassTokenType = KClassTokenGenerator::TokenBaseType;

template <typename T>
static constexpr inline ClassTokenType ClassToken = KClassTokenGenerator::ClassToken<T>;

} // namespace Kernel
