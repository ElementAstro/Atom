/*!
 * \file concept.hpp
 * \brief C++ Concepts
 * \author Max Qian <lightapt.com>
 * \date 2024-03-01
 * \copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef ATOM_META_CONCEPT_HPP
#define ATOM_META_CONCEPT_HPP

#include <complex>
#include <concepts>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#if __cplusplus < 202002L
#error "C++20 is required for this library"
#endif

namespace atom::meta {

//==============================================================================
// Function Concepts
//==============================================================================

/*!
 * \brief Concept for types that can be invoked with given arguments
 * \tparam F Function type
 * \tparam Args Argument types
 */
template <typename F, typename... Args>
concept Invocable = requires(F func, Args&&... args) {
    { std::invoke(func, std::forward<Args>(args)...) };
};

/*!
 * \brief Concept for types that can be invoked with given arguments and return
 * type
 * \tparam F Function type
 * \tparam R Return type
 * \tparam Args Argument types
 */
template <typename F, typename R, typename... Args>
concept InvocableR = requires(F func, Args&&... args) {
    {
        std::invoke(func, std::forward<Args>(args)...)
    } -> std::convertible_to<R>;
};

/*!
 * \brief Concept for types that can be invoked with no exceptions
 * \tparam F Function type
 * \tparam Args Argument types
 */
template <typename F, typename... Args>
concept NothrowInvocable = requires(F func, Args&&... args) {
    { std::invoke(func, std::forward<Args>(args)...) } noexcept;
};

/*!
 * \brief Concept for types that can be invoked with no exceptions and return
 * type
 * \tparam F Function type
 * \tparam R Return type
 * \tparam Args Argument types
 */
template <typename F, typename R, typename... Args>
concept NothrowInvocableR = requires(F func, Args&&... args) {
    {
        std::invoke(func, std::forward<Args>(args)...)
    } noexcept -> std::convertible_to<R>;
};

/*!
 * \brief Concept for function pointer types
 * \tparam T Type to check
 */
template <typename T>
concept FunctionPointer = std::is_function_v<std::remove_pointer_t<T>>;

/*!
 * \brief Concept for member function pointer types
 * \tparam T Type to check
 */
template <typename T>
concept MemberFunctionPointer = std::is_member_function_pointer_v<T>;

/*!
 * \brief Concept for callable types
 * \tparam T Type to check
 */
template <typename T>
concept Callable = requires(T obj) {
    { std::function{std::declval<T>()} };
};

/*!
 * \brief Concept for callable types with specific return type
 * \tparam T Type to check
 * \tparam Ret Return type
 * \tparam Args Argument types
 */
template <typename T, typename Ret, typename... Args>
concept CallableReturns = std::is_invocable_r_v<Ret, T, Args...>;

/*!
 * \brief Concept for callable types that are noexcept
 * \tparam T Type to check
 * \tparam Args Argument types
 */
template <typename T, typename... Args>
concept CallableNoexcept = requires(T obj, Args&&... args) {
    { obj(std::forward<Args>(args)...) } noexcept;
};

//==============================================================================
// Object Concepts
//==============================================================================

/*!
 * \brief Concept for relocatable types
 * \tparam T Type to check
 */
template <typename T>
concept Relocatable = std::is_nothrow_move_constructible_v<T> &&
                      std::is_nothrow_move_assignable_v<T>;

/*!
 * \brief Concept for default constructible types
 * \tparam T Type to check
 */
template <typename T>
concept DefaultConstructible = std::is_default_constructible_v<T>;

/*!
 * \brief Concept for copy constructible types
 * \tparam T Type to check
 */
template <typename T>
concept CopyConstructible = std::is_copy_constructible_v<T>;

/*!
 * \brief Concept for copy assignable types
 * \tparam T Type to check
 */
template <typename T>
concept CopyAssignable = std::is_copy_assignable_v<T>;

/*!
 * \brief Concept for move assignable types
 * \tparam T Type to check
 */
template <typename T>
concept MoveAssignable = std::is_move_assignable_v<T>;

/*!
 * \brief Concept for equality comparable types
 * \tparam T Type to check
 */
template <typename T>
concept EqualityComparable = requires(const T& a, const T& b) {
    { a == b } -> std::convertible_to<bool>;
    { a != b } -> std::convertible_to<bool>;
};

/*!
 * \brief Concept for less than comparable types
 * \tparam T Type to check
 */
template <typename T>
concept LessThanComparable = requires(const T& a, const T& b) {
    { a < b } -> std::convertible_to<bool>;
};

/*!
 * \brief Concept for hashable types
 * \tparam T Type to check
 */
template <typename T>
concept Hashable = requires(const T& obj) {
    { std::hash<T>{}(obj) } -> std::convertible_to<std::size_t>;
};

/*!
 * \brief Concept for swappable types
 * \tparam T Type to check
 */
template <typename T>
concept Swappable = std::is_swappable_v<T>;

/*!
 * \brief Concept for copyable types
 * \tparam T Type to check
 */
template <typename T>
concept Copyable = CopyConstructible<T> && CopyAssignable<T>;

/*!
 * \brief Concept for destructible types
 * \tparam T Type to check
 */
template <typename T>
concept Destructible = std::is_destructible_v<T>;

//==============================================================================
// Type Concepts
//==============================================================================

/*!
 * \brief Concept for arithmetic types
 * \tparam T Type to check
 */
template <typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

/*!
 * \brief Concept for integral types
 * \tparam T Type to check
 */
template <typename T>
concept Integral = std::is_integral_v<T>;

/*!
 * \brief Concept for floating point types
 * \tparam T Type to check
 */
template <typename T>
concept FloatingPoint = std::is_floating_point_v<T>;

/*!
 * \brief Concept for signed integer types
 * \tparam T Type to check
 */
template <typename T>
concept SignedInteger = std::is_integral_v<T> && std::is_signed_v<T>;

/*!
 * \brief Concept for unsigned integer types
 * \tparam T Type to check
 */
template <typename T>
concept UnsignedInteger = std::is_integral_v<T> && std::is_unsigned_v<T>;

/*!
 * \brief Concept for numeric types
 * \tparam T Type to check
 */
template <typename T>
concept Number = Arithmetic<T>;

/*!
 * \brief Concept for complex number types
 * \tparam T Type to check
 */
template <typename T>
concept ComplexNumber = requires {
    typename T::value_type;
    requires std::is_same_v<T, std::complex<typename T::value_type>>;
};

/*!
 * \brief Concept for char type
 * \tparam T Type to check
 */
template <typename T>
concept Char = std::is_same_v<T, char>;

/*!
 * \brief Concept for wchar_t type
 * \tparam T Type to check
 */
template <typename T>
concept WChar = std::is_same_v<T, wchar_t>;

/*!
 * \brief Concept for char16_t type
 * \tparam T Type to check
 */
template <typename T>
concept Char16 = std::is_same_v<T, char16_t>;

/*!
 * \brief Concept for char32_t type
 * \tparam T Type to check
 */
template <typename T>
concept Char32 = std::is_same_v<T, char32_t>;

/*!
 * \brief Concept for any character type
 * \tparam T Type to check
 */
template <typename T>
concept AnyChar = Char<T> || WChar<T> || Char16<T> || Char32<T>;

// Forward declaration for custom String type
namespace atom::containers {
class String;
}

/*!
 * \brief Concept for string types
 * \tparam T Type to check
 */
template <typename T>
concept StringType =
    std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> ||
    std::is_same_v<T, std::wstring> || std::is_same_v<T, std::u8string> ||
    std::is_same_v<T, std::u16string> || std::is_same_v<T, std::u32string> ||
    std::is_same_v<T, atom::containers::String>;

/*!
 * \brief Concept for built-in types
 * \tparam T Type to check
 */
template <typename T>
concept IsBuiltIn = std::is_fundamental_v<T> || StringType<T>;

/*!
 * \brief Concept for enumeration types
 * \tparam T Type to check
 */
template <typename T>
concept Enum = std::is_enum_v<T>;

/*!
 * \brief Concept for pointer types
 * \tparam T Type to check
 */
template <typename T>
concept Pointer = std::is_pointer_v<T>;

/*!
 * \brief Concept for unique_ptr types
 * \tparam T Type to check
 */
template <typename T>
concept UniquePointer = requires {
    typename T::element_type;
    requires std::is_same_v<T, std::unique_ptr<typename T::element_type>> ||
                 std::is_same_v<T, std::unique_ptr<typename T::element_type,
                                                   typename T::deleter_type>>;
};

/*!
 * \brief Concept for shared_ptr types
 * \tparam T Type to check
 */
template <typename T>
concept SharedPointer = requires {
    typename T::element_type;
    requires std::is_same_v<T, std::shared_ptr<typename T::element_type>>;
};

/*!
 * \brief Concept for weak_ptr types
 * \tparam T Type to check
 */
template <typename T>
concept WeakPointer = requires {
    typename T::element_type;
    requires std::is_same_v<T, std::weak_ptr<typename T::element_type>>;
};

/*!
 * \brief Concept for smart pointer types
 * \tparam T Type to check
 */
template <typename T>
concept SmartPointer = UniquePointer<T> || SharedPointer<T> || WeakPointer<T>;

/*!
 * \brief Concept for reference types
 * \tparam T Type to check
 */
template <typename T>
concept Reference = std::is_reference_v<T>;

/*!
 * \brief Concept for lvalue reference types
 * \tparam T Type to check
 */
template <typename T>
concept LvalueReference = std::is_lvalue_reference_v<T>;

/*!
 * \brief Concept for rvalue reference types
 * \tparam T Type to check
 */
template <typename T>
concept RvalueReference = std::is_rvalue_reference_v<T>;

/*!
 * \brief Concept for const types
 * \tparam T Type to check
 */
template <typename T>
concept Const = std::is_const_v<std::remove_reference_t<T>>;

/*!
 * \brief Concept for trivial types
 * \tparam T Type to check
 */
template <typename T>
concept Trivial = std::is_trivial_v<T>;

/*!
 * \brief Concept for trivially constructible types
 * \tparam T Type to check
 */
template <typename T>
concept TriviallyConstructible = std::is_trivially_constructible_v<T>;

/*!
 * \brief Concept for trivially copyable types
 * \tparam T Type to check
 */
template <typename T>
concept TriviallyCopyable =
    std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

//==============================================================================
// Container Concepts
//==============================================================================

/*!
 * \brief Concept for iterable types
 * \tparam T Type to check
 */
template <typename T>
concept Iterable = requires(T& obj) {
    { obj.begin() } -> std::input_or_output_iterator;
    { obj.end() } -> std::input_or_output_iterator;
};

/*!
 * \brief Concept for container types
 * \tparam T Type to check
 */
template <typename T>
concept Container = requires(const T& obj) {
    { obj.size() } -> std::convertible_to<std::size_t>;
    requires Iterable<T>;
};

/*!
 * \brief Concept for string container types
 * \tparam T Type to check
 */
template <typename T>
concept StringContainer = requires(T& obj) {
    typename T::value_type;
    requires AnyChar<typename T::value_type>;
    { obj.push_back(std::declval<typename T::value_type>()) };
};

/*!
 * \brief Concept for numeric container types
 * \tparam T Type to check
 */
template <typename T>
concept NumberContainer = requires(T& obj) {
    typename T::value_type;
    requires Number<typename T::value_type>;
    { obj.push_back(std::declval<typename T::value_type>()) };
};

/*!
 * \brief Concept for associative container types
 * \tparam T Type to check
 */
template <typename T>
concept AssociativeContainer = requires {
    typename T::key_type;
    typename T::mapped_type;
    requires Container<T>;
};

/*!
 * \brief Concept for iterator types
 * \tparam T Type to check
 */
template <typename T>
concept Iterator = std::input_or_output_iterator<T>;

/*!
 * \brief Concept for sequence container types
 * \tparam T Type to check
 */
template <typename T>
concept SequenceContainer = requires {
    typename T::value_type;
    requires std::is_same_v<T, std::vector<typename T::value_type>> ||
                 std::is_same_v<T, std::list<typename T::value_type>> ||
                 std::is_same_v<T, std::deque<typename T::value_type>>;
};

/*!
 * \brief Concept for string-like types
 * \tparam T Type to check
 */
template <typename T>
concept StringLike = requires(const T& obj) {
    { obj.size() } -> std::convertible_to<std::size_t>;
    { obj.empty() } -> std::convertible_to<bool>;
    requires Iterable<T>;
    requires !SequenceContainer<T>;
};

//==============================================================================
// Multi-threading Concepts
//==============================================================================

/*!
 * \brief Concept for lockable types
 * \tparam T Type to check
 */
template <typename T>
concept Lockable = requires(T& obj) {
    { obj.lock() } -> std::same_as<void>;
    { obj.unlock() } -> std::same_as<void>;
};

/*!
 * \brief Concept for shared lockable types
 * \tparam T Type to check
 */
template <typename T>
concept SharedLockable = requires(T& obj) {
    { obj.lock_shared() } -> std::same_as<void>;
    { obj.unlock_shared() } -> std::same_as<void>;
};

/*!
 * \brief Concept for mutex types
 * \tparam T Type to check
 */
template <typename T>
concept Mutex = Lockable<T> && requires(T& obj) {
    { obj.try_lock() } -> std::same_as<bool>;
};

/*!
 * \brief Concept for shared mutex types
 * \tparam T Type to check
 */
template <typename T>
concept SharedMutex = SharedLockable<T> && requires(T& obj) {
    { obj.try_lock_shared() } -> std::same_as<bool>;
};

//==============================================================================
// Asynchronous Concepts
//==============================================================================

/*!
 * \brief Concept for future types
 * \tparam T Type to check
 */
template <typename T>
concept Future = requires(T& obj) {
    { obj.get() };
    { obj.wait() } -> std::same_as<void>;
};

/*!
 * \brief Concept for promise types
 * \tparam T Type to check
 */
template <typename T>
concept Promise = requires(T& obj) {
    {
        obj.set_exception(std::declval<std::exception_ptr>())
    } -> std::same_as<void>;
};

/*!
 * \brief Concept for async result types
 * \tparam T Type to check
 */
template <typename T>
concept AsyncResult = Future<T> || Promise<T>;

}  // namespace atom::meta

#endif  // ATOM_META_CONCEPT_HPP
