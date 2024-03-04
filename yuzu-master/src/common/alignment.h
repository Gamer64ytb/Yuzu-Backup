// SPDX-FileCopyrightText: 2014 Jannik Vogel <email@jannikvogel.de>
// SPDX-License-Identifier: CC0-1.0

#pragma once

#include <bit>
#include <cstddef>
#include <new>
#include <type_traits>

namespace Common {

template <typename T>
    requires std::is_integral_v<T>
[[nodiscard]] constexpr T AlignUp(T value_, size_t size) {
    using U = typename std::make_unsigned_t<T>;
    auto value{static_cast<U>(value_)};
    auto mod{static_cast<T>(value % size)};
    value -= mod;
    return static_cast<T>(mod == T{0} ? value : value + size);
}

template <typename T>
    requires std::is_unsigned_v<T>
[[nodiscard]] constexpr T AlignUpLog2(T value, size_t align_log2) {
    return static_cast<T>((value + ((1ULL << align_log2) - 1)) >> align_log2 << align_log2);
}

template <typename T>
    requires std::is_integral_v<T>
[[nodiscard]] constexpr T AlignDown(T value_, size_t size) {
    using U = typename std::make_unsigned_t<T>;
    const auto value{static_cast<U>(value_)};
    return static_cast<T>(value - value % size);
}

template <typename T>
    requires std::is_unsigned_v<T>
[[nodiscard]] constexpr bool Is4KBAligned(T value) {
    return (value & 0xFFF) == 0;
}

template <typename T>
    requires std::is_unsigned_v<T>
[[nodiscard]] constexpr bool IsWordAligned(T value) {
    return (value & 0b11) == 0;
}

template <typename T>
    requires std::is_integral_v<T>
[[nodiscard]] constexpr bool IsAligned(T value, size_t alignment) {
    using U = typename std::make_unsigned_t<T>;
    const U mask = static_cast<U>(alignment - 1);
    return (value & mask) == 0;
}

template <typename T, typename U>
    requires std::is_integral_v<T>
[[nodiscard]] constexpr T DivideUp(T x, U y) {
    return (x + (y - 1)) / y;
}

template <typename T>
    requires std::is_integral_v<T>
[[nodiscard]] constexpr T LeastSignificantOneBit(T x) {
    return x & ~(x - 1);
}

template <typename T>
    requires std::is_integral_v<T>
[[nodiscard]] constexpr T ResetLeastSignificantOneBit(T x) {
    return x & (x - 1);
}

template <typename T>
    requires std::is_integral_v<T>
[[nodiscard]] constexpr bool IsPowerOfTwo(T x) {
    return x > 0 && ResetLeastSignificantOneBit(x) == 0;
}

template <typename T>
    requires std::is_integral_v<T>
[[nodiscard]] constexpr T FloorPowerOfTwo(T x) {
    return T{1} << (sizeof(T) * 8 - std::countl_zero(x) - 1);
}

template <typename T, size_t Align = 16>
class AlignmentAllocator {
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;

    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::false_type;

    constexpr AlignmentAllocator() noexcept = default;

    template <typename T2>
    constexpr AlignmentAllocator(const AlignmentAllocator<T2, Align>&) noexcept {}

    [[nodiscard]] T* allocate(size_type n) {
        return static_cast<T*>(::operator new(n * sizeof(T), std::align_val_t{Align}));
    }

    void deallocate(T* p, size_type n) {
        ::operator delete(p, n * sizeof(T), std::align_val_t{Align});
    }

    template <typename T2>
    struct rebind {
        using other = AlignmentAllocator<T2, Align>;
    };

    template <typename T2, size_t Align2>
    constexpr bool operator==(const AlignmentAllocator<T2, Align2>&) const noexcept {
        return std::is_same_v<T, T2> && Align == Align2;
    }
};

} // namespace Common
