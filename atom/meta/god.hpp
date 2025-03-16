/*!
 * \file god.hpp
 * \brief Advanced utility functions, inspired by Coost
 * \author Max Qian <lightapt.com>
 * \date 2023-06-17
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * \version 2.0
 */

#ifndef ATOM_META_GOD_HPP
#define ATOM_META_GOD_HPP

#include <atomic>
#include <bit>       // C++20: For bit manipulation functions
#include <concepts>  // C++20: For concept definitions
#include <cstddef>
#include <cstring>
#include <functional>
#include <type_traits>
#include <utility>

#include "atom/macro.hpp"

namespace atom::meta {

//==============================================================================
// Concepts (C++20)
//==============================================================================

/*!
 * \brief Concept for integral types with bitwise operations support
 */
template <typename T>
concept BitwiseOperatable =
    std::integral<T> || std::is_pointer_v<T> || std::is_enum_v<T>;

/*!
 * \brief Concept for types that can be aligned
 */
template <typename T>
concept Alignable = std::is_integral_v<T> || std::is_pointer_v<T>;

/*!
 * \brief Concept for types that are safely copyable with memcpy
 */
template <typename T>
concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

//==============================================================================
// Basic Utilities
//==============================================================================

/*!
 * \brief No-op function for blessing with no bugs.
 */
ATOM_INLINE void blessNoBugs() {}

/*!
 * \brief Casts a value from one type to another.
 * \tparam To The type to cast to.
 * \tparam From The type to cast from.
 * \param fromValue The value to cast.
 * \return The casted value.
 */
template <typename To, typename From>
[[nodiscard]] constexpr auto cast(From&& fromValue) noexcept -> To {
    return static_cast<To>(std::forward<From>(fromValue));
}

/*!
 * \brief Safe enumeration cast with compile-time verification.
 * \tparam ToEnum Target enum type
 * \tparam FromEnum Source enum type
 * \param value Value to cast
 * \return The safely casted enum value
 */
template <typename ToEnum, typename FromEnum>
    requires std::is_enum_v<ToEnum> && std::is_enum_v<FromEnum>
[[nodiscard]] constexpr auto enumCast(FromEnum value) noexcept -> ToEnum {
    using ToType = std::underlying_type_t<ToEnum>;
    using FromType = std::underlying_type_t<FromEnum>;
    return static_cast<ToEnum>(
        static_cast<ToType>(static_cast<FromType>(value)));
}

//==============================================================================
// Alignment Functions
//==============================================================================

/*!
 * \brief Checks if a value is aligned to the specified power-of-2 boundary.
 * \tparam Alignment The alignment value, must be a power of 2.
 * \tparam ValueType The type of the value to check.
 * \param value The value to check.
 * \return True if aligned, false otherwise.
 */
template <std::size_t Alignment, Alignable ValueType>
[[nodiscard]] constexpr auto isAligned(ValueType value) noexcept -> bool {
    static_assert((Alignment & (Alignment - 1)) == 0,
                  "Alignment must be power of 2");
    if constexpr (std::is_pointer_v<ValueType>) {
        return (reinterpret_cast<std::size_t>(value) & (Alignment - 1)) == 0;
    } else {
        return (static_cast<std::size_t>(value) & (Alignment - 1)) == 0;
    }
}

/*!
 * \brief Aligns a value up to the nearest multiple of A.
 * \tparam Alignment The alignment value, must be a power of 2.
 * \tparam ValueType The type of the value to align.
 * \param value The value to align.
 * \return The aligned value.
 */
template <std::size_t Alignment, Alignable ValueType>
[[nodiscard]] constexpr auto alignUp(ValueType value) noexcept -> ValueType {
    static_assert((Alignment & (Alignment - 1)) == 0,
                  "Alignment must be power of 2");
    return (value + static_cast<ValueType>(Alignment - 1)) &
           ~static_cast<ValueType>(Alignment - 1);
}

/*!
 * \brief Aligns a pointer up to the nearest multiple of A.
 * \tparam Alignment The alignment value, must be a power of 2.
 * \tparam PointerType The type of the pointer to align.
 * \param pointer The pointer to align.
 * \return The aligned pointer.
 */
template <std::size_t Alignment, typename PointerType>
[[nodiscard]] constexpr auto alignUp(PointerType* pointer) noexcept
    -> PointerType* {
    return reinterpret_cast<PointerType*>(
        alignUp<Alignment>(reinterpret_cast<std::size_t>(pointer)));
}

/*!
 * \brief Aligns a value up to the nearest multiple of alignment.
 * \tparam ValueType The type of the value to align.
 * \tparam AlignmentType The type of the alignment value, must be integral.
 * \param value The value to align.
 * \param alignment The alignment value.
 * \return The aligned value.
 */
template <Alignable ValueType, std::integral AlignmentType>
[[nodiscard]] constexpr auto alignUp(
    ValueType value, AlignmentType alignment) noexcept -> ValueType {
    // Assert alignment is a power of 2 at runtime
    assert((alignment & (alignment - 1)) == 0 &&
           "Alignment must be power of 2");
    return (value + static_cast<ValueType>(alignment - 1)) &
           ~static_cast<ValueType>(alignment - 1);
}

/*!
 * \brief Aligns a pointer up to the nearest multiple of alignment.
 * \tparam PointerType The type of the pointer to align.
 * \tparam AlignmentType The type of the alignment value, must be integral.
 * \param pointer The pointer to align.
 * \param alignment The alignment value.
 * \return The aligned pointer.
 */
template <typename PointerType, std::integral AlignmentType>
[[nodiscard]] constexpr auto alignUp(
    PointerType* pointer, AlignmentType alignment) noexcept -> PointerType* {
    return reinterpret_cast<PointerType*>(
        alignUp(reinterpret_cast<std::size_t>(pointer), alignment));
}

/*!
 * \brief Aligns a value down to the nearest multiple of A.
 * \tparam Alignment The alignment value, must be a power of 2.
 * \tparam ValueType The type of the value to align.
 * \param value The value to align.
 * \return The aligned value.
 */
template <std::size_t Alignment, Alignable ValueType>
[[nodiscard]] constexpr auto alignDown(ValueType value) noexcept -> ValueType {
    static_assert((Alignment & (Alignment - 1)) == 0,
                  "Alignment must be power of 2");
    return value & ~static_cast<ValueType>(Alignment - 1);
}

/*!
 * \brief Aligns a pointer down to the nearest multiple of A.
 * \tparam Alignment The alignment value, must be a power of 2.
 * \tparam PointerType The type of the pointer to align.
 * \param pointer The pointer to align.
 * \return The aligned pointer.
 */
template <std::size_t Alignment, typename PointerType>
[[nodiscard]] constexpr auto alignDown(PointerType* pointer) noexcept
    -> PointerType* {
    return reinterpret_cast<PointerType*>(
        alignDown<Alignment>(reinterpret_cast<std::size_t>(pointer)));
}

/*!
 * \brief Aligns a value down to the nearest multiple of alignment.
 * \tparam ValueType The type of the value to align.
 * \tparam AlignmentType The type of the alignment value, must be integral.
 * \param value The value to align.
 * \param alignment The alignment value.
 * \return The aligned value.
 */
template <Alignable ValueType, std::integral AlignmentType>
[[nodiscard]] constexpr auto alignDown(
    ValueType value, AlignmentType alignment) noexcept -> ValueType {
    // Assert alignment is a power of 2 at runtime
    assert((alignment & (alignment - 1)) == 0 &&
           "Alignment must be power of 2");
    return value & ~static_cast<ValueType>(alignment - 1);
}

/*!
 * \brief Aligns a pointer down to the nearest multiple of alignment.
 * \tparam PointerType The type of the pointer to align.
 * \tparam AlignmentType The type of the alignment value, must be integral.
 * \param pointer The pointer to align.
 * \param alignment The alignment value.
 * \return The aligned pointer.
 */
template <typename PointerType, std::integral AlignmentType>
[[nodiscard]] constexpr auto alignDown(
    PointerType* pointer, AlignmentType alignment) noexcept -> PointerType* {
    return reinterpret_cast<PointerType*>(
        alignDown(reinterpret_cast<std::size_t>(pointer), alignment));
}

//==============================================================================
// Math Functions
//==============================================================================

/*!
 * \brief Computes the base-2 logarithm of an integral value.
 * \tparam IntegralType The type of the value, must be integral.
 * \param value The value to compute the logarithm of.
 * \return The base-2 logarithm of the value.
 */
template <std::integral IntegralType>
[[nodiscard]] constexpr auto log2(IntegralType value) noexcept -> IntegralType {
    // Use C++20's std::bit_width for better performance when available
    if constexpr (std::is_unsigned_v<IntegralType> &&
                  sizeof(IntegralType) <= sizeof(unsigned long long)) {
        return value == 0
                   ? 0
                   : static_cast<IntegralType>(std::bit_width(value) - 1);
    } else {
        return value <= 1 ? 0 : 1 + log2(value >> 1);
    }
}

/*!
 * \brief Computes the number of blocks of size BlockSize needed to cover a
 * value. \tparam BlockSize The block size, must be a power of 2. \tparam
 * ValueType The type of the value. \param value The value to compute the number
 * of blocks for. \return The number of blocks needed to cover the value.
 */
template <std::size_t BlockSize, std::integral ValueType>
[[nodiscard]] constexpr auto nb(ValueType value) noexcept -> ValueType {
    static_assert((BlockSize & (BlockSize - 1)) == 0,
                  "BlockSize must be power of 2");
    return (value >> log2(static_cast<ValueType>(BlockSize))) +
           !!(value & static_cast<ValueType>(BlockSize - 1));
}

/*!
 * \brief Compute the ceiling of value/divisor for integer division.
 * \tparam T Numeric type
 * \param value The dividend
 * \param divisor The divisor
 * \return Ceiling of the division
 */
template <std::integral T>
[[nodiscard]] constexpr T divCeil(T value, T divisor) noexcept {
    return (value + divisor - 1) / divisor;
}

/*!
 * \brief Determine if a number is a power of two
 * \tparam T Integer type
 * \param value Value to test
 * \return True if value is a power of two
 */
template <std::integral T>
[[nodiscard]] constexpr bool isPowerOf2(T value) noexcept {
    return value > 0 && (value & (value - 1)) == 0;
}

//==============================================================================
// Memory Functions
//==============================================================================

/*!
 * \brief Compares two values for equality.
 * \tparam ValueType The type of the values.
 * \param first Pointer to the first value.
 * \param second Pointer to the second value.
 * \return True if the values are equal, false otherwise.
 */
template <typename ValueType>
[[nodiscard]] ATOM_INLINE auto eq(const void* first,
                                  const void* second) noexcept -> bool {
    return *reinterpret_cast<const ValueType*>(first) ==
           *reinterpret_cast<const ValueType*>(second);
}

/*!
 * \brief Copies N bytes from src to dst with compile-time optimization.
 * \tparam NumBytes The number of bytes to copy.
 * \param destination Pointer to the destination.
 * \param source Pointer to the source.
 * \return Pointer to the destination after copy
 */
template <std::size_t NumBytes>
ATOM_INLINE void* copy(void* destination, const void* source) noexcept {
    if constexpr (NumBytes == 0) {
        return destination;
    } else if constexpr (NumBytes == 1) {
        *static_cast<std::byte*>(destination) =
            *static_cast<const std::byte*>(source);
        return destination;
    } else if constexpr (NumBytes == 2) {
        *static_cast<uint16_t*>(destination) =
            *static_cast<const uint16_t*>(source);
        return destination;
    } else if constexpr (NumBytes == 4) {
        *static_cast<uint32_t*>(destination) =
            *static_cast<const uint32_t*>(source);
        return destination;
    } else if constexpr (NumBytes == 8) {
        *static_cast<uint64_t*>(destination) =
            *static_cast<const uint64_t*>(source);
        return destination;
    } else {
        return std::memcpy(destination, source, NumBytes);
    }
}

/*!
 * \brief Safely copies data from source to destination with bounds checking.
 * \param destination Destination memory buffer
 * \param destSize Size of destination buffer
 * \param source Source memory buffer
 * \param sourceSize Size of source buffer
 * \return Number of bytes copied
 */
[[nodiscard]] inline std::size_t safeCopy(void* destination,
                                          std::size_t destSize,
                                          const void* source,
                                          std::size_t sourceSize) noexcept {
    const std::size_t copySize = std::min(destSize, sourceSize);
    std::memcpy(destination, source, copySize);
    return copySize;
}

/*!
 * \brief Zero-initializes a memory region.
 * \param ptr Pointer to the memory region
 * \param size Size in bytes
 */
ATOM_INLINE void zeroMemory(void* ptr, std::size_t size) noexcept {
    std::memset(ptr, 0, size);
}

/*!
 * \brief Checks if memory regions contain equal data
 * \param a First memory region
 * \param b Second memory region
 * \param size Size to compare
 * \return True if regions contain identical data
 */
[[nodiscard]] ATOM_INLINE bool memoryEquals(const void* a, const void* b,
                                            std::size_t size) noexcept {
    return std::memcmp(a, b, size) == 0;
}

//==============================================================================
// Atomic Operations
//==============================================================================

/*!
 * \brief Swaps the value pointed to by pointer with value atomically.
 * \tparam T The type of the value.
 * \param pointer Pointer to the value to swap.
 * \param value The value to swap with.
 * \param order Memory order for the operation.
 * \return The original value pointed to by pointer.
 */
template <typename T>
[[nodiscard]] ATOM_INLINE T
atomicSwap(std::atomic<T>* pointer, T value,
           std::memory_order order = std::memory_order_seq_cst) noexcept {
    return pointer->exchange(value, order);
}

/*!
 * \brief Non-atomic swap for regular (non-atomic) values.
 * \tparam PointerType The type of the value pointed to by pointer.
 * \tparam ValueType The type of the value.
 * \param pointer Pointer to the value to swap.
 * \param value The value to swap with.
 * \return The original value pointed to by pointer.
 */
template <typename PointerType, typename ValueType>
[[nodiscard]] ATOM_INLINE auto swap(PointerType* pointer,
                                    ValueType value) noexcept -> PointerType {
    PointerType originalValue = *pointer;
    *pointer = static_cast<PointerType>(value);
    return originalValue;
}

/*!
 * \brief Adds value to the value pointed to by pointer and returns the original
 * value. \tparam PointerType The type of the value pointed to by pointer.
 * \tparam ValueType The type of the value.
 * \param pointer Pointer to the value to add to.
 * \param value The value to add.
 * \return The original value pointed to by pointer.
 */
template <typename PointerType, typename ValueType>
[[nodiscard]] ATOM_INLINE auto fetchAdd(
    PointerType* pointer, ValueType value) noexcept -> PointerType {
    PointerType originalValue = *pointer;
    *pointer += value;
    return originalValue;
}

/*!
 * \brief Atomic version of fetchAdd
 * \tparam T Type supporting atomic operations
 * \param pointer Pointer to atomic value
 * \param value Value to add
 * \param order Memory ordering constraint
 * \return Original value before addition
 */
template <typename T>
[[nodiscard]] ATOM_INLINE T
atomicFetchAdd(std::atomic<T>* pointer, T value,
               std::memory_order order = std::memory_order_seq_cst) noexcept {
    return pointer->fetch_add(value, order);
}

/*!
 * \brief Subtracts value from the value pointed to by pointer and returns the
 * original value. \tparam PointerType The type of the value pointed to by
 * pointer. \tparam ValueType The type of the value. \param pointer Pointer to
 * the value to subtract from. \param value The value to subtract. \return The
 * original value pointed to by pointer.
 */
template <typename PointerType, typename ValueType>
[[nodiscard]] ATOM_INLINE auto fetchSub(
    PointerType* pointer, ValueType value) noexcept -> PointerType {
    PointerType originalValue = *pointer;
    *pointer -= value;
    return originalValue;
}

/*!
 * \brief Atomic version of fetchSub
 * \tparam T Type supporting atomic operations
 * \param pointer Pointer to atomic value
 * \param value Value to subtract
 * \param order Memory ordering constraint
 * \return Original value before subtraction
 */
template <typename T>
[[nodiscard]] ATOM_INLINE T
atomicFetchSub(std::atomic<T>* pointer, T value,
               std::memory_order order = std::memory_order_seq_cst) noexcept {
    return pointer->fetch_sub(value, order);
}

/*!
 * \brief Performs a bitwise AND between the value pointed to by pointer and
 * value, and returns the original value. \tparam PointerType The type of the
 * value pointed to by pointer. \tparam ValueType The type of the value. \param
 * pointer Pointer to the value to AND. \param value The value to AND with.
 * \return The original value pointed to by pointer.
 */
template <typename PointerType, typename ValueType>
    requires BitwiseOperatable<PointerType>
[[nodiscard]] ATOM_INLINE auto fetchAnd(
    PointerType* pointer, ValueType value) noexcept -> PointerType {
    PointerType originalValue = *pointer;
    *pointer &= static_cast<PointerType>(value);
    return originalValue;
}

/*!
 * \brief Atomic version of fetchAnd
 * \tparam T Type supporting atomic operations
 * \param pointer Pointer to atomic value
 * \param value Value to AND with
 * \param order Memory ordering constraint
 * \return Original value before AND operation
 */
template <typename T>
    requires BitwiseOperatable<T>
[[nodiscard]] ATOM_INLINE T
atomicFetchAnd(std::atomic<T>* pointer, T value,
               std::memory_order order = std::memory_order_seq_cst) noexcept {
    return pointer->fetch_and(value, order);
}

/*!
 * \brief Performs a bitwise OR between the value pointed to by pointer and
 * value, and returns the original value. \tparam PointerType The type of the
 * value pointed to by pointer. \tparam ValueType The type of the value. \param
 * pointer Pointer to the value to OR. \param value The value to OR with.
 * \return The original value pointed to by pointer.
 */
template <typename PointerType, typename ValueType>
    requires BitwiseOperatable<PointerType>
[[nodiscard]] ATOM_INLINE auto fetchOr(
    PointerType* pointer, ValueType value) noexcept -> PointerType {
    PointerType originalValue = *pointer;
    *pointer |= static_cast<PointerType>(value);
    return originalValue;
}

/*!
 * \brief Atomic version of fetchOr
 * \tparam T Type supporting atomic operations
 * \param pointer Pointer to atomic value
 * \param value Value to OR with
 * \param order Memory ordering constraint
 * \return Original value before OR operation
 */
template <typename T>
    requires BitwiseOperatable<T>
[[nodiscard]] ATOM_INLINE T
atomicFetchOr(std::atomic<T>* pointer, T value,
              std::memory_order order = std::memory_order_seq_cst) noexcept {
    return pointer->fetch_or(value, order);
}

/*!
 * \brief Performs a bitwise XOR between the value pointed to by pointer and
 * value, and returns the original value. \tparam PointerType The type of the
 * value pointed to by pointer. \tparam ValueType The type of the value. \param
 * pointer Pointer to the value to XOR. \param value The value to XOR with.
 * \return The original value pointed to by pointer.
 */
template <typename PointerType, typename ValueType>
    requires BitwiseOperatable<PointerType>
[[nodiscard]] ATOM_INLINE auto fetchXor(
    PointerType* pointer, ValueType value) noexcept -> PointerType {
    PointerType originalValue = *pointer;
    *pointer ^= static_cast<PointerType>(value);
    return originalValue;
}

/*!
 * \brief Atomic version of fetchXor
 * \tparam T Type supporting atomic operations
 * \param pointer Pointer to atomic value
 * \param value Value to XOR with
 * \param order Memory ordering constraint
 * \return Original value before XOR operation
 */
template <typename T>
    requires BitwiseOperatable<T>
[[nodiscard]] ATOM_INLINE T
atomicFetchXor(std::atomic<T>* pointer, T value,
               std::memory_order order = std::memory_order_seq_cst) noexcept {
    return pointer->fetch_xor(value, order);
}

//==============================================================================
// Type Traits
//==============================================================================

/*!
 * \brief Alias for std::enable_if_t.
 * \tparam Condition The condition.
 * \tparam Type The type to enable if the condition is true.
 */
template <bool _Cond, typename _IfTrue, typename _IfFalse = void>
using if_t = typename std::conditional<_Cond, _IfTrue, _IfFalse>::type;

/*!
 * \brief Alias for std::remove_reference_t.
 * \tparam Type The type to remove reference from.
 */
template <typename Type>
using rmRefT = std::remove_reference_t<Type>;

/*!
 * \brief Alias for std::remove_cv_t.
 * \tparam Type The type to remove const and volatile qualifiers from.
 */
template <typename Type>
using rmCvT = std::remove_cv_t<Type>;

/*!
 * \brief Alias for removing both const, volatile qualifiers and reference.
 * \tparam Type The type to remove const, volatile qualifiers and reference
 * from.
 */
template <typename Type>
using rmCvRefT = rmCvT<rmRefT<Type>>;

/*!
 * \brief Alias for std::remove_extent_t.
 * \tparam Type The type to remove extent from.
 */
template <typename Type>
using rmArrT = std::remove_extent_t<Type>;

/*!
 * \brief Alias for std::add_const_t.
 * \tparam Type The type to add const qualifier to.
 */
template <typename Type>
using constT = std::add_const_t<Type>;

/*!
 * \brief Alias for adding const qualifier and lvalue reference.
 * \tparam Type The type to add const qualifier and lvalue reference to.
 */
template <typename Type>
using constRefT = std::add_lvalue_reference_t<constT<rmRefT<Type>>>;

/*!
 * \brief Alias for removing pointer
 * \tparam Type The type to remove pointer from
 */
template <typename Type>
using rmPtrT = std::remove_pointer_t<Type>;

/*!
 * \brief Checks if Type is nothrow relocatable
 * \tparam Type Type to check
 */
template <typename Type>
inline constexpr bool isNothrowRelocatable =
    std::is_nothrow_move_constructible_v<Type> &&
    std::is_nothrow_destructible_v<Type>;

namespace detail {

/*!
 * \brief Helper struct to check if all types are the same.
 * \tparam Types The types to check.
 */
template <typename... Types>
struct IsSame {
    static constexpr bool K_VALUE = false;
};

/*!
 * \brief Specialization of IsSame for two or more types.
 * \tparam FirstType The first type.
 * \tparam SecondType The second type.
 * \tparam RemainingTypes The remaining types.
 */
template <typename FirstType, typename SecondType, typename... RemainingTypes>
struct IsSame<FirstType, SecondType, RemainingTypes...> {
    static constexpr bool K_VALUE =
        std::is_same_v<FirstType, SecondType> ||
        IsSame<FirstType, RemainingTypes...>::K_VALUE;
};

}  // namespace detail

/*!
 * \brief Checks if all types are the same.
 * \tparam FirstType The first type.
 * \tparam SecondType The second type.
 * \tparam RemainingTypes The remaining types.
 * \return True if all types are the same, false otherwise.
 */
template <typename FirstType, typename SecondType, typename... RemainingTypes>
[[nodiscard]] constexpr auto isSame() noexcept -> bool {
    return detail::IsSame<FirstType, SecondType, RemainingTypes...>::K_VALUE;
}

/*!
 * \brief Checks if a type is a reference.
 * \tparam Type The type to check.
 * \return True if the type is a reference, false otherwise.
 */
template <typename Type>
[[nodiscard]] constexpr auto isRef() noexcept -> bool {
    return std::is_reference_v<Type>;
}

/*!
 * \brief Checks if a type is an array.
 * \tparam Type The type to check.
 * \return True if the type is an array, false otherwise.
 */
template <typename Type>
[[nodiscard]] constexpr auto isArray() noexcept -> bool {
    return std::is_array_v<Type>;
}

/*!
 * \brief Checks if a type is a class.
 * \tparam Type The type to check.
 * \return True if the type is a class, false otherwise.
 */
template <typename Type>
[[nodiscard]] constexpr auto isClass() noexcept -> bool {
    return std::is_class_v<Type>;
}

/*!
 * \brief Checks if a type is a scalar.
 * \tparam Type The type to check.
 * \return True if the type is a scalar, false otherwise.
 */
template <typename Type>
[[nodiscard]] constexpr auto isScalar() noexcept -> bool {
    return std::is_scalar_v<Type>;
}

/*!
 * \brief Checks if a type is trivially copyable.
 * \tparam Type The type to check.
 * \return True if the type is trivially copyable, false otherwise.
 */
template <typename Type>
[[nodiscard]] constexpr auto isTriviallyCopyable() noexcept -> bool {
    return std::is_trivially_copyable_v<Type>;
}

/*!
 * \brief Checks if a type is trivially destructible.
 * \tparam Type The type to check.
 * \return True if the type is trivially destructible, false otherwise.
 */
template <typename Type>
[[nodiscard]] constexpr auto isTriviallyDestructible() noexcept -> bool {
    return std::is_trivially_destructible_v<Type>;
}

/*!
 * \brief Checks if a type is a base of another type.
 * \tparam BaseType The base type.
 * \tparam DerivedType The derived type.
 * \return True if BaseType is a base of DerivedType, false otherwise.
 */
template <typename BaseType, typename DerivedType>
[[nodiscard]] constexpr auto isBaseOf() noexcept -> bool {
    return std::is_base_of_v<BaseType, DerivedType>;
}

/*!
 * \brief Checks if a type has a virtual destructor.
 * \tparam Type The type to check.
 * \return True if the type has a virtual destructor, false otherwise.
 */
template <typename Type>
[[nodiscard]] constexpr auto hasVirtualDestructor() noexcept -> bool {
    return std::has_virtual_destructor_v<Type>;
}

//==============================================================================
// Resource Management
//==============================================================================

/*!
 * \brief RAII scope guard to execute a function when going out of scope
 */
class ScopeGuard {
public:
    template <typename Callback>
    explicit ScopeGuard(Callback&& callback)
        : m_callback(std::forward<Callback>(callback)), m_active(true) {}

    ~ScopeGuard() noexcept {
        if (m_active) {
            m_callback();
        }
    }

    // Disable copy
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    // Enable move
    ScopeGuard(ScopeGuard&& other) noexcept
        : m_callback(std::move(other.m_callback)), m_active(other.m_active) {
        other.m_active = false;
    }

    ScopeGuard& operator=(ScopeGuard&& other) noexcept {
        if (this != &other) {
            m_callback = std::move(other.m_callback);
            m_active = other.m_active;
            other.m_active = false;
        }
        return *this;
    }

    // Cancel the guard
    void dismiss() noexcept { m_active = false; }

private:
    std::function<void()> m_callback;
    bool m_active;
};

/*!
 * \brief Create a scope guard with the provided callback
 * \tparam Callback Function type to execute on scope exit
 * \param callback Function to call when going out of scope
 * \return ScopeGuard object
 */
template <typename Callback>
[[nodiscard]] auto makeGuard(Callback&& callback) {
    return ScopeGuard(std::forward<Callback>(callback));
}

/*!
 * \brief Thread safe singleton access template
 * \tparam T The singleton class type
 * \return Reference to the singleton instance
 */
template <typename T>
T& singleton() {
    static T instance;
    return instance;
}

}  // namespace atom::meta

#endif  // ATOM_META_GOD_HPP