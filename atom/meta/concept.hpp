/*!
 * \file concept.hpp
 * \brief C++ Concepts
 * \author Max Qian <lightapt.com>
 * \date 2024-03-01
 * \copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef ATOM_META_CONCEPT_HPP
#define ATOM_META_CONCEPT_HPP

#if __cplusplus < 202002L
#error "C++20 or later is required for this header"
#endif

#include <atomic>
#include <chrono>
#include <concepts>
#include <coroutine>
#include <deque>
#include <format>
#include <functional>
#include <future>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <version>

// C++23 feature detection
#if __cpp_lib_expected >= 202202L
#include <expected>
#define ATOM_HAS_STD_EXPECTED 1
#else
#define ATOM_HAS_STD_EXPECTED 0
#endif

#if __cpp_lib_move_only_function >= 202110L
#define ATOM_HAS_MOVE_ONLY_FUNCTION 1
#else
#define ATOM_HAS_MOVE_ONLY_FUNCTION 0
#endif

#if __cpp_lib_flat_map >= 202207L
#include <flat_map>
#define ATOM_HAS_FLAT_MAP 1
#else
#define ATOM_HAS_FLAT_MAP 0
#endif

#if __cpp_lib_flat_set >= 202207L
#include <flat_set>
#define ATOM_HAS_FLAT_SET 1
#else
#define ATOM_HAS_FLAT_SET 0
#endif

#include "atom/containers/high_performance.hpp"

#if defined(_MSVC_LANG)
#if _MSVC_LANG < 202002L
#error "C++20 is required for this library"
#endif
#elif defined(__cplusplus)
#if __cplusplus < 202002L
#error "C++20 is required for this library"
#endif
#endif

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
 * \brief Concept for move constructible types
 * \tparam T Type to check
 */
template <typename T>
concept MoveConstructible = std::is_move_constructible_v<T>;

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

/*!
 * \brief Concept for string types
 * \tparam T Type to check
 */
template <typename T>
concept StringType = [] {
    using Decayed = std::remove_cvref_t<T>;
    using Elem = std::remove_all_extents_t<Decayed>;
    if constexpr (std::is_same_v<Decayed, std::string> ||
                  std::is_same_v<Decayed, std::string_view> ||
                  std::is_same_v<Decayed, std::wstring> ||
                  std::is_same_v<Decayed, std::u8string> ||
                  std::is_same_v<Decayed, std::u16string> ||
                  std::is_same_v<Decayed, std::u32string> ||
                  std::is_same_v<Decayed, atom::containers::String>) {
        return true;
    } else if constexpr (std::is_array_v<Decayed>) {
        return std::is_same_v<Elem, char> || std::is_same_v<Elem, const char> ||
               std::is_same_v<Elem, wchar_t> ||
               std::is_same_v<Elem, const wchar_t>;
    } else {
        return false;
    }
}();

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

//==============================================================================
// C++23 Enhanced Concepts
//==============================================================================

namespace detail {
/**
 * @brief Helper to check if a type has a specific member function
 */
template <typename T, typename = void>
struct has_to_string_impl : std::false_type {};

template <typename T>
struct has_to_string_impl<
    T, std::void_t<decltype(std::declval<const T&>().toString())>>
    : std::true_type {};
}  // namespace detail

/**
 * @brief Check if std::expected is available
 */
inline constexpr bool has_std_expected = ATOM_HAS_STD_EXPECTED;

/**
 * @brief Check if std::move_only_function is available
 */
inline constexpr bool has_move_only_function = ATOM_HAS_MOVE_ONLY_FUNCTION;

/**
 * @brief Check if std::flat_map is available
 */
inline constexpr bool has_flat_map = ATOM_HAS_FLAT_MAP;

/**
 * @brief Check if std::flat_set is available
 */
inline constexpr bool has_flat_set = ATOM_HAS_FLAT_SET;

//==============================================================================
// Formatting and Serialization Concepts
//==============================================================================

/**
 * @brief Concept for types that can be formatted with std::format
 */
template <typename T>
concept Formattable = requires(const T& t) {
    { std::format("{}", t) } -> std::convertible_to<std::string>;
};

/**
 * @brief Concept for types that have a toString method
 */
template <typename T>
concept HasToString = detail::has_to_string_impl<T>::value;

/**
 * @brief Concept for types that can be converted to string_view
 */
template <typename T>
concept StringViewConvertible = requires(const T& t) {
    { std::string_view(t) } -> std::same_as<std::string_view>;
} || std::is_convertible_v<T, std::string_view>;

/**
 * @brief Concept for types that have JSON serialization
 */
template <typename T>
concept JsonSerializable = requires(const T& t) {
    { t.toJson() };
};

//==============================================================================
// Range and Container Concepts (Enhanced)
//==============================================================================

/**
 * @brief Concept for types that work with std::span
 */
template <typename T>
concept SpanCompatible = requires(T& t) {
    { std::span(t) };
};

/**
 * @brief Concept for contiguous ranges
 */
template <typename T>
concept ContiguousRange = std::ranges::contiguous_range<T>;

/**
 * @brief Concept for sized ranges
 */
template <typename T>
concept SizedRange = std::ranges::sized_range<T>;

/**
 * @brief Concept for borrowed ranges
 */
template <typename T>
concept BorrowedRange = std::ranges::borrowed_range<T>;

/**
 * @brief Concept for viewable ranges
 */
template <typename T>
concept ViewableRange = std::ranges::viewable_range<T>;

//==============================================================================
// Coroutine Concepts (Enhanced)
//==============================================================================

/**
 * @brief Concept for coroutine promise types
 */
template <typename T>
concept CoroutinePromise = requires { typename T::promise_type; };

/**
 * @brief Concept for awaitable types
 */
template <typename T>
concept AwaitableType = requires(T t) {
    { t.await_ready() } -> std::convertible_to<bool>;
    { t.await_resume() };
};

//==============================================================================
// Memory and Lifetime Concepts
//==============================================================================

/**
 * @brief Concept for trivially relocatable types
 */
template <typename T>
concept TriviallyRelocatable =
    std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>;

/**
 * @brief Concept for aggregate types
 */
template <typename T>
concept Aggregate = std::is_aggregate_v<T>;

/**
 * @brief Concept for standard layout types
 */
template <typename T>
concept StandardLayout = std::is_standard_layout_v<T>;

/**
 * @brief Concept for POD types
 */
template <typename T>
concept PodType = std::is_trivial_v<T> && StandardLayout<T>;

//==============================================================================
// Callable Concepts (Enhanced)
//==============================================================================

/**
 * @brief Concept for move-only callable types
 */
template <typename F, typename... Args>
concept MoveOnlyInvocable =
    std::invocable<F, Args...> && std::move_constructible<F> &&
    !std::copy_constructible<F>;

/**
 * @brief Concept for const-callable types
 */
template <typename F, typename... Args>
concept ConstInvocable = requires(const F& f, Args&&... args) {
    { f(std::forward<Args>(args)...) };
};

/**
 * @brief Concept for noexcept callable types
 */
template <typename F, typename... Args>
concept NoexceptInvocable = std::is_nothrow_invocable_v<F, Args...>;

/**
 * @brief Concept for predicate types
 */
template <typename F, typename... Args>
concept PredicateType = std::predicate<F, Args...>;

/**
 * @brief Concept for comparison function objects
 */
template <typename F, typename T>
concept Comparator = std::strict_weak_order<F, T, T>;

//==============================================================================
// Type Relationship Concepts
//==============================================================================

/**
 * @brief Concept for types with virtual destructor
 */
template <typename T>
concept HasVirtualDestructor = std::has_virtual_destructor_v<T>;

/**
 * @brief Concept for polymorphic types
 */
template <typename T>
concept PolymorphicType = std::is_polymorphic_v<T>;

/**
 * @brief Concept for final classes
 */
template <typename T>
concept FinalClass = std::is_final_v<T>;

/**
 * @brief Concept for abstract classes
 */
template <typename T>
concept AbstractClass = std::is_abstract_v<T>;

/**
 * @brief Concept for enum types
 */
template <typename T>
concept EnumType = std::is_enum_v<T>;

/**
 * @brief Concept for scoped enum types
 */
template <typename T>
concept ScopedEnumType =
    EnumType<T> && !std::is_convertible_v<T, std::underlying_type_t<T>>;

/**
 * @brief Concept for unscoped enum types
 */
template <typename T>
concept UnscopedEnumType =
    EnumType<T> && std::is_convertible_v<T, std::underlying_type_t<T>>;

//==============================================================================
// Numeric Concepts (Enhanced)
//==============================================================================

/**
 * @brief Concept for signed integral types
 */
template <typename T>
concept SignedIntegralType = std::signed_integral<T>;

/**
 * @brief Concept for unsigned integral types
 */
template <typename T>
concept UnsignedIntegralType = std::unsigned_integral<T>;

/**
 * @brief Concept for floating-point types with specific precision
 */
template <typename T>
concept FloatingPointPrecise =
    std::floating_point<T> &&
    (std::same_as<T, float> || std::same_as<T, double> ||
     std::same_as<T, long double>);

/**
 * @brief Concept for numeric types that support basic arithmetic
 */
template <typename T>
concept NumericArithmetic = requires(T a, T b) {
    { a + b } -> std::convertible_to<T>;
    { a - b } -> std::convertible_to<T>;
    { a* b } -> std::convertible_to<T>;
    { a / b } -> std::convertible_to<T>;
};

/**
 * @brief Concept for types supporting bitwise operations
 */
template <typename T>
concept BitwiseOperable = requires(T a, T b) {
    { a& b } -> std::convertible_to<T>;
    { a | b } -> std::convertible_to<T>;
    { a ^ b } -> std::convertible_to<T>;
    { ~a } -> std::convertible_to<T>;
    { a << 1 } -> std::convertible_to<T>;
    { a >> 1 } -> std::convertible_to<T>;
};

//==============================================================================
// Expected/Optional Concepts
//==============================================================================

#if ATOM_HAS_STD_EXPECTED
/**
 * @brief Concept for expected types
 */
template <typename T>
concept ExpectedType = requires(T t) {
    typename T::value_type;
    typename T::error_type;
    { t.has_value() } -> std::same_as<bool>;
    { t.value() } -> std::same_as<typename T::value_type&>;
    { t.error() } -> std::same_as<typename T::error_type&>;
};
#endif

/**
 * @brief Concept for optional-like types
 */
template <typename T>
concept OptionalLike = requires(T t) {
    { t.has_value() } -> std::same_as<bool>;
    { *t };
    { static_cast<bool>(t) };
};

//==============================================================================
// Tuple and Variant Concepts
//==============================================================================

/**
 * @brief Concept for tuple-like types
 */
template <typename T>
concept TupleLikeType = requires {
    {
        std::tuple_size<std::remove_cvref_t<T>>::value
    } -> std::convertible_to<std::size_t>;
};

/**
 * @brief Concept for variant-like types
 */
template <typename T>
concept VariantLike = requires(T t) {
    { t.index() } -> std::convertible_to<std::size_t>;
    {
        std::visit([](auto&&) {}, t)
    };
};

//==============================================================================
// Meta Module Interoperability Concepts
//==============================================================================

/**
 * @brief Concept for types that support demangling via abi.hpp
 */
template <typename T>
concept Demanglable = requires {
    { typeid(T).name() } -> std::convertible_to<const char*>;
};

/**
 * @brief Concept for types with TypeInfo support
 */
template <typename T>
concept TypeInfoSupported =
    Demanglable<T> && (std::is_class_v<T> || std::is_enum_v<T> ||
                       std::is_fundamental_v<T> || std::is_pointer_v<T>);

/**
 * @brief Concept for boxable types (for any.hpp)
 */
template <typename T>
concept BoxCompatible = (std::copy_constructible<std::decay_t<T>> ||
                         std::move_constructible<std::decay_t<T>>) &&
                        std::destructible<std::decay_t<T>>;

/**
 * @brief Concept for types that can be wrapped in a proxy
 */
template <typename F>
concept ProxyCompatible =
    std::is_invocable_v<F> || std::is_member_function_pointer_v<F> ||
    std::is_function_v<std::remove_pointer_t<F>>;

/**
 * @brief Concept for decorator-compatible functions
 */
template <typename F>
concept DecoratorCompatible =
    std::is_invocable_v<F> && std::is_object_v<std::decay_t<F>>;

/**
 * @brief Concept for reflection-compatible types
 */
template <typename T>
concept ReflectionCompatible = std::is_class_v<T> && std::is_aggregate_v<T>;

/**
 * @brief Concept for enum types with traits
 */
template <typename T>
concept EnumWithTraits = std::is_enum_v<T>;

/**
 * @brief Concept for invokable with result capture
 */
template <typename F, typename... Args>
concept InvokableWithResult =
    std::invocable<F, Args...> &&
    (!std::is_void_v<std::invoke_result_t<F, Args...>>);

/**
 * @brief Concept for void-returning invokables
 */
template <typename F, typename... Args>
concept VoidInvokable = std::invocable<F, Args...> &&
                        std::is_void_v<std::invoke_result_t<F, Args...>>;

/**
 * @brief Concept for nothrow invokables
 */
template <typename F, typename... Args>
concept NothrowInvokable =
    std::invocable<F, Args...> && std::is_nothrow_invocable_v<F, Args...>;

/**
 * @brief Concept for types that support serialization
 */
template <typename T>
concept MetaSerializable = requires(const T& t) {
    { t.toString() } -> std::convertible_to<std::string>;
} || requires(const T& t, std::ostream& os) {
    { os << t } -> std::same_as<std::ostream&>;
};

/**
 * @brief Concept for comparable types
 */
template <typename T>
concept MetaComparable =
    std::equality_comparable<T> || requires(const T& a, const T& b) {
        { a == b } -> std::convertible_to<bool>;
    };

/**
 * @brief Concept for hashable types
 */
template <typename T>
concept MetaHashable = requires(const T& t) {
    { std::hash<T>{}(t) } -> std::convertible_to<std::size_t>;
};

/**
 * @brief Combined concept for registry-compatible types
 */
template <typename T>
concept RegistryCompatible =
    TypeInfoSupported<T> && std::default_initializable<T>;

/**
 * @brief Concept for factory-creatable types
 */
template <typename T>
concept FactoryCreatable =
    std::default_initializable<T> || std::is_constructible_v<T>;

/**
 * @brief Concept for cloneable types
 */
template <typename T>
concept Cloneable = requires(const T& t) {
    { t.clone() } -> std::convertible_to<std::unique_ptr<T>>;
} || std::copy_constructible<T>;

// Note: Advanced Type Manipulation Concepts (Aggregate, StandardLayout, POD,
// HasVirtualDestructor) are defined earlier in this file - removed duplicates

//==============================================================================
// Container Operation Concepts
//==============================================================================

/**
 * @brief Concept for types subscriptable with an index type
 */
template <typename T, typename Index = std::size_t>
concept Subscriptable = requires(T t, Index i) { t[i]; };

/**
 * @brief Concept for containers supporting capacity reservation
 */
template <typename T>
concept Reservable = requires(T t, std::size_t n) { t.reserve(n); };

/**
 * @brief Concept for containers with key-based lookup
 */
template <typename T>
concept AssociativeLookup = requires(T t, typename T::key_type key) {
    { t.find(key) } -> std::same_as<typename T::iterator>;
};

/**
 * @brief Concept for ordered associative containers
 */
template <typename T>
concept OrderedContainer =
    AssociativeLookup<T> && requires { typename T::key_compare; };

/**
 * @brief Concept for types reporting a size
 */
template <typename T>
concept HasSize = requires(const T& t) {
    { t.size() } -> std::convertible_to<std::size_t>;
};

/**
 * @brief Concept for types with an emptiness check
 */
template <typename T>
concept EmptyCheckable = requires(const T& t) {
    { t.empty() } -> std::convertible_to<bool>;
};

/**
 * @brief Concept for containers that can be cleared
 */
template <typename T>
concept Clearable = requires(T t) { t.clear(); };

/**
 * @brief Concept for containers supporting push_back
 */
template <typename T>
concept BackInsertable =
    requires(T t, typename T::value_type v) { t.push_back(v); };

/**
 * @brief Concept for containers supporting emplace_back
 */
template <typename T>
concept BackEmplaceable =
    requires(T t, typename T::value_type v) { t.emplace_back(std::move(v)); };

/**
 * @brief Concept for containers with front/back element access
 */
template <typename T>
concept FrontBackAccessible = requires(T t) {
    { t.front() } -> std::same_as<typename T::reference>;
    { t.back() } -> std::same_as<typename T::reference>;
};

//==============================================================================
// Chrono Concepts
//==============================================================================

namespace detail {
template <typename T>
inline constexpr bool is_duration_v = false;
template <typename Rep, typename Period>
inline constexpr bool is_duration_v<std::chrono::duration<Rep, Period>> = true;

template <typename T>
inline constexpr bool is_time_point_v = false;
template <typename Clock, typename Dur>
inline constexpr bool is_time_point_v<std::chrono::time_point<Clock, Dur>> =
    true;
}  // namespace detail

/**
 * @brief Concept for std::chrono::duration specializations
 */
template <typename T>
concept Duration = detail::is_duration_v<std::remove_cvref_t<T>>;

/**
 * @brief Concept for std::chrono::time_point specializations
 */
template <typename T>
concept TimePoint = detail::is_time_point_v<std::remove_cvref_t<T>>;

//==============================================================================
// Value Semantics Concepts
//==============================================================================

/**
 * @brief Concept for nullable handle types (bool-testable and resettable)
 */
template <typename T>
concept Nullable = requires(T t) {
    { static_cast<bool>(t) };
    t.reset();
};

/**
 * @brief Concept for atomic-like types
 */
template <typename T>
concept AtomicLike = requires(T t, typename T::value_type v) {
    { t.load() } -> std::convertible_to<typename T::value_type>;
    t.store(v);
    { t.exchange(v) } -> std::convertible_to<typename T::value_type>;
};

/**
 * @brief Concept for move-only types
 */
template <typename T>
concept MoveOnly = std::movable<T> && !std::copy_constructible<T>;

/**
 * @brief Concept for regular types (see std::regular)
 */
template <typename T>
concept Regular = std::regular<T>;

/**
 * @brief Concept for semiregular types (see std::semiregular)
 */
template <typename T>
concept Semiregular = std::semiregular<T>;

/**
 * @brief Concept for three-way comparable types
 */
template <typename T>
concept ThreeWayComparable = std::three_way_comparable<T>;

//==============================================================================
// Type Pack Utilities
//==============================================================================

/**
 * @brief True when T is the same as one of Ts
 */
template <typename T, typename... Ts>
inline constexpr bool is_one_of_v = (std::same_as<T, Ts> || ...);

/**
 * @brief First type of a parameter pack
 */
template <typename First, typename... Rest>
using first_type_t = First;

namespace detail {
template <typename... Ts>
struct last_type_impl {
    using type = std::tuple_element_t<sizeof...(Ts) - 1, std::tuple<Ts...>>;
};
}  // namespace detail

/**
 * @brief Last type of a parameter pack
 */
template <typename... Ts>
    requires(sizeof...(Ts) > 0)
using last_type_t = typename detail::last_type_impl<Ts...>::type;

#endif  // ATOM_META_CONCEPT_HPP
