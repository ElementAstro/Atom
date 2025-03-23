/*!
 * \file template_traits.hpp
 * \brief Advanced Template Traits Library (C++20/23)
 * \author Max Qian <lightapt.com> (Enhanced by [Your Name])
 * \date 2024-05-25
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_TEMPLATE_TRAITS_HPP
#define ATOM_META_TEMPLATE_TRAITS_HPP

#include <concepts>
#include <functional>
#include <limits>
#include <source_location>  // For better error reporting
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>  // For variant-related traits

#include "abi.hpp"

#ifdef _WIN32
#undef max
#endif

namespace atom::meta {

// Forward declarations for interdependent components
template <typename T>
struct type_identity {
    using type = T;
};

// 在identity结构体中修复字符串字面量的问题 - 使用std::string_view代替
template <typename T, auto... Values>
struct identity {
    using type = T;
    static constexpr bool has_value = sizeof...(Values) > 0;

    template <std::size_t N = 0>
    static constexpr auto value_at() noexcept {
        static_assert(N < sizeof...(Values),
                      "Index out of range in identity<T, Values...>");
        return std::get<N>(std::tuple{Values...});
    }

    static constexpr auto value = has_value ? value_at<0>() : T{};

    // Allow structured binding support
    template <std::size_t I>
    static constexpr auto get() noexcept {
        if constexpr (I == 0)
            return type_identity<T>{};
        else if constexpr (I < sizeof...(Values) + 1)
            return value_at<I - 1>();
        else
            static_assert(
                I < sizeof...(Values) + 1,
                "Index out of range in identity<T, Values...>::get()");
    }
};

// Structured binding support declarations
}  // namespace atom::meta

// Specializations for std::tuple_size and std::tuple_element
namespace std {
template <typename T, auto... Values>
struct tuple_size<atom::meta::identity<T, Values...>>
    : std::integral_constant<std::size_t, sizeof...(Values) + 1> {};

template <std::size_t I, typename T, auto... Values>
struct tuple_element<I, atom::meta::identity<T, Values...>> {
    using type =
        decltype(atom::meta::identity<T, Values...>::template get<I>());
};
}  // namespace std

namespace atom::meta {

//------------------------------------------------------------------------------
// Type list operations
//------------------------------------------------------------------------------

// Type list implementation with operations
template <typename... Ts>
struct type_list {
    static constexpr std::size_t size = sizeof...(Ts);

    template <typename... Us>
    using append = type_list<Ts..., Us...>;

    template <typename... Us>
    using prepend = type_list<Us..., Ts...>;

    template <template <typename> typename F>
    using transform = type_list<typename F<Ts>::type...>;

    // Get type at index
    template <std::size_t I>
    using at = std::tuple_element_t<I, std::tuple<Ts...>>;

    template <template <typename> typename F, typename Result, typename... Rest>
    struct filter_impl;

    template <template <typename> typename F, typename... Filtered>
    struct filter_impl<F, type_list<Filtered...>> {
        using type = type_list<Filtered...>;
    };

    template <template <typename> typename F, typename... Filtered, typename T,
              typename... Rest>
    struct filter_impl<F, type_list<Filtered...>, T, Rest...> {
        using type = std::conditional_t<
            F<T>::value,
            typename filter_impl<F, type_list<Filtered..., T>, Rest...>::type,
            typename filter_impl<F, type_list<Filtered...>, Rest...>::type>;
    };

    // Filter types based on predicate F
    template <template <typename> typename F>
    using filter = typename filter_impl<F, type_list<>, Ts...>::type;
};

//------------------------------------------------------------------------------
// Template detection and traits
//------------------------------------------------------------------------------

// Check if a type is a template instantiation with improved diagnostics
template <typename T>
struct is_template : std::false_type {
    static constexpr std::string_view reason =
        "Type is not a template instantiation";
};

template <template <typename...> typename Template, typename... Args>
struct is_template<Template<Args...>> : std::true_type {
    static constexpr std::string_view reason =
        "Type is a template instantiation";
};

template <typename T>
inline constexpr bool is_template_v = is_template<T>::value;

// Concept for template instantiations
template <typename T>
concept TemplateInstantiation = is_template_v<T>;

// Extract template parameters and full name with additional metadata
template <typename T>
struct template_traits {
    // Default implementation for non-templates with better error handling
    static_assert(is_template_v<T>,
                  "template_traits: Type must be a template instantiation");
};

template <template <typename...> typename Template, typename... Args>
struct template_traits<Template<Args...>> {
    using args_type = std::tuple<Args...>;
    using type_list_args = type_list<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);
    static const inline std::string full_name =
        DemangleHelper::demangle(typeid(Template<Args...>).name());
    static const inline std::string template_name = [] {
        std::string name = full_name;
        auto pos = name.find('<');
        return pos != std::string::npos ? name.substr(0, pos) : name;
    }();

    // Extract template parameters as array of demangled names
    static const inline std::array<std::string, sizeof...(Args)> arg_names = {
        DemangleHelper::demangle(typeid(Args).name())...};

    // Check if the template has a specific argument type
    template <typename T>
    static constexpr bool has_arg = (std::is_same_v<T, Args> || ...);
};

// Helper alias templates with better naming
template <typename T>
using args_type_of = typename template_traits<T>::args_type;

template <typename T>
using type_list_of = typename template_traits<T>::type_list_args;

template <typename T>
inline constexpr std::size_t template_arity_v = template_traits<T>::arity;

// Check if a type is a specialization of a given template with diagnostics
template <template <typename...> typename Template, typename T>
struct is_specialization_of : std::false_type {
    static constexpr std::string_view reason =
        "Type is not a specialization of the specified template";
};

template <template <typename...> typename Template, typename... Args>
struct is_specialization_of<Template, Template<Args...>> : std::true_type {
    static constexpr std::string_view reason =
        "Type is a specialization of the specified template";
};

template <template <typename...> typename Template, typename T>
inline constexpr bool is_specialization_of_v =
    is_specialization_of<Template, T>::value;

// Concept for template specializations
template <template <typename...> typename Template, typename T>
concept SpecializationOf = is_specialization_of_v<Template, T>;

// Extract the N-th template parameter type with bounds checking
template <std::size_t N, typename T, typename = void>
struct nth_template_arg {
    static_assert(is_template_v<T>,
                  "nth_template_arg: Type must be a template instantiation");
    static_assert(N < template_arity_v<T>,
                  "nth_template_arg: Index out of bounds");
    using type = std::tuple_element_t<N, args_type_of<T>>;
};

template <std::size_t N, typename T>
using template_arg_t = typename nth_template_arg<N, T>::type;

//------------------------------------------------------------------------------
// Inheritance and derived type traits
//------------------------------------------------------------------------------

// Check if a type is derived from multiple base classes with detailed feedback
template <typename Derived, typename... Bases>
struct is_derived_from_all
    : std::conjunction<std::is_base_of<Bases, Derived>...> {
    // For diagnostic purposes, check individual inheritance relationships
    template <typename Base>
    static constexpr bool inherits_from = std::is_base_of_v<Base, Derived>;

    static constexpr std::array<bool, sizeof...(Bases)> inheritance_map = {
        inherits_from<Bases>...};

    // Identify which inheritance relationships are missing
    template <std::size_t... Is>
    static constexpr auto missing_bases_impl(std::index_sequence<Is...>) {
        return std::array{
            (!inheritance_map[Is]
                 ? DemangleHelper::demangle(
                       typeid(std::tuple_element_t<Is, std::tuple<Bases...>>)
                           .name())
                 : std::string{})...};
    }

    static const inline std::vector<std::string> missing_bases = [] {
        auto arr =
            missing_bases_impl(std::make_index_sequence<sizeof...(Bases)>{});
        std::vector<std::string> result;
        for (const auto& name : arr) {
            if (!name.empty())
                result.push_back(name);
        }
        return result;
    }();
};

template <typename Derived, typename... Bases>
inline constexpr bool is_derived_from_all_v =
    is_derived_from_all<Derived, Bases...>::value;

// Concept for multiple inheritance checking
template <typename Derived, typename... Bases>
concept DerivedFromAll = is_derived_from_all_v<Derived, Bases...>;

// Check if a type is derived from any of the specified base classes
template <typename Derived, typename... Bases>
struct is_derived_from_any
    : std::disjunction<std::is_base_of<Bases, Derived>...> {};

template <typename Derived, typename... Bases>
inline constexpr bool is_derived_from_any_v =
    is_derived_from_any<Derived, Bases...>::value;

// Concept for inheritance from any base
template <typename Derived, typename... Bases>
concept DerivedFromAny = is_derived_from_any_v<Derived, Bases...>;

//------------------------------------------------------------------------------
// Enhanced template-of-templates detection
//------------------------------------------------------------------------------

// Check if a type is a partial specialization of a given template
template <typename T, template <typename, typename...> typename Template>
struct is_partial_specialization_of : std::false_type {};

template <template <typename, typename...> typename Template, typename Arg,
          typename... Args>
struct is_partial_specialization_of<Template<Arg, Args...>, Template>
    : std::true_type {};

template <typename T, template <typename, typename...> typename Template>
inline constexpr bool is_partial_specialization_of_v =
    is_partial_specialization_of<T, Template>::value;

// Alias template detection
template <typename T>
struct is_alias_template : std::false_type {};

template <template <typename...> typename Template, typename... Args>
struct is_alias_template<Template<Args...>> : std::true_type {
    // Attempt to detect if this is likely an alias template (not foolproof)
    static constexpr bool likely_alias = !std::is_class_v<Template<Args...>> ||
                                         !std::is_union_v<Template<Args...>>;
};

template <typename T>
inline constexpr bool is_alias_template_v = is_alias_template<T>::value;

// Improved template-related concepts
template <typename T>
concept ClassTemplate = is_template_v<T> && std::is_class_v<T>;

template <typename T>
concept FunctionTemplate = requires(T t) {
    &T::operator();
    is_template_v<T>;
};

// Extract template arguments as different containers
template <typename T>
using template_args_as_tuple_t = typename template_traits<T>::args_type;

template <typename T>
using template_args_as_type_list_t =
    typename template_traits<T>::type_list_args;

//------------------------------------------------------------------------------
// Type sequence and parameter pack utilities
//------------------------------------------------------------------------------

// Count the number of occurrences of a type in a parameter pack with improved
// implementation
template <typename T, typename... Args>
constexpr std::size_t count_occurrences_v =
    (0 + ... + (std::is_same_v<T, Args> ? 1 : 0));

// 修复find_first_index_v实现
template <typename T, typename... Args>
constexpr std::size_t find_first_index_v = []() consteval {
    constexpr std::size_t value = []<std::size_t... I>(
                                      std::index_sequence<I...>) consteval {
        std::size_t index = std::numeric_limits<std::size_t>::max();
        (((std::is_same_v<T, std::tuple_element_t<I, std::tuple<Args...>>> &&
           index == std::numeric_limits<std::size_t>::max())
              ? (index = I, 0)
              : 0),
         ...);
        return index;
    }(std::index_sequence_for<Args...>{});

    // Provide useful static_assert while still returning a valid value for
    // SFINAE
    if constexpr (value == std::numeric_limits<std::size_t>::max()) {
        if constexpr (std::is_same_v<T, void>) {
            // Special case to avoid static_assert when used in SFINAE contexts
            return value;
        } else {
            static_assert(value != std::numeric_limits<std::size_t>::max(),
                          "Type not found in parameter pack");
            return value;
        }
    } else {
        return value;
    }
}();

// 修复find_all_indices实现
template <typename T, typename... Args>
struct find_all_indices {
private:
    template <std::size_t... I>
    static constexpr auto indices_impl(std::index_sequence<I...>) {
        constexpr std::size_t count = count_occurrences_v<T, Args...>;
        std::array<std::size_t, count> result{};

        std::size_t idx = 0;
        ((std::is_same_v<T, std::tuple_element_t<I, std::tuple<Args...>>>
              ? (result[idx++] = I, 0)
              : 0),
         ...);

        return result;
    }

public:
    static constexpr auto value =
        indices_impl(std::index_sequence_for<Args...>{});
    static constexpr std::size_t count = value.size();
};

//------------------------------------------------------------------------------
// Type extraction and manipulation utilities
//------------------------------------------------------------------------------

// Extract reference wrapper or pointer types with improved safety
template <typename T>
struct extract_reference_wrapper {
    using type = std::conditional_t<
        is_specialization_of_v<std::reference_wrapper, std::remove_cvref_t<T>>,
        typename std::remove_cvref_t<T>::type, std::remove_reference_t<T>>;
};

// Extract the type from a std::reference_wrapper or reference
template <typename T>
struct extract_reference_wrapper_type {
    using type = T;
};

// Specialization for std::reference_wrapper
template <typename T>
struct extract_reference_wrapper_type<std::reference_wrapper<T>> {
    using type = T;
};

// Specialization for reference types
template <typename T>
struct extract_reference_wrapper_type<T&> {
    using type = T;
};

// Specialization for const reference types
template <typename T>
struct extract_reference_wrapper_type<const T&> {
    using type = const T;
};

// Helper alias template
template <typename T>
using extract_reference_wrapper_type_t =
    typename extract_reference_wrapper_type<T>::type;

template <typename T>
struct extract_pointer {
    using type = std::remove_pointer_t<T>;
    static constexpr bool is_pointer = std::is_pointer_v<T>;
    static constexpr bool is_smart_pointer =
        is_specialization_of_v<std::shared_ptr, std::remove_cvref_t<T>> ||
        is_specialization_of_v<std::unique_ptr, std::remove_cvref_t<T>> ||
        is_specialization_of_v<std::weak_ptr, std::remove_cvref_t<T>>;

    using element_type =
        std::conditional_t<is_smart_pointer,
                           typename std::remove_cvref_t<T>::element_type,
                           std::remove_pointer_t<T>>;
};

// Specialization for raw pointers
template <typename T>
struct extract_pointer<T*> {
    using type = T;
    using element_type = T;
    static constexpr bool is_pointer = true;
    static constexpr bool is_smart_pointer = false;
};

template <typename T>
using extract_pointer_type_t = typename extract_pointer<T>::element_type;

// Extract function return type and parameter types with support for member
// functions
template <typename T>
struct extract_function_traits;

// Regular functions
template <typename R, typename... Args>
struct extract_function_traits<R(Args...)> {
    using return_type = R;
    using parameter_types = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);

    template <std::size_t I>
    using arg_t = std::tuple_element_t<I, parameter_types>;
};

// Function pointers
template <typename R, typename... Args>
struct extract_function_traits<R (*)(Args...)>
    : extract_function_traits<R(Args...)> {};

// Member function pointers
template <typename C, typename R, typename... Args>
struct extract_function_traits<R (C::*)(Args...)> {
    using class_type = C;
    using return_type = R;
    using parameter_types = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);

    template <std::size_t I>
    using arg_t = std::tuple_element_t<I, parameter_types>;
};

// Const member function pointers
template <typename C, typename R, typename... Args>
struct extract_function_traits<R (C::*)(Args...) const>
    : extract_function_traits<R (C::*)(Args...)> {};

// Noexcept function variants
template <typename R, typename... Args>
struct extract_function_traits<R(Args...) noexcept>
    : extract_function_traits<R(Args...)> {
    static constexpr bool is_noexcept = true;
};

// Functors and lambdas
template <typename F>
struct extract_function_traits {
private:
    using call_type = extract_function_traits<
        decltype(&std::remove_reference_t<F>::operator())>;

public:
    using return_type = typename call_type::return_type;
    using parameter_types = typename call_type::parameter_types;
    static constexpr std::size_t arity = call_type::arity;

    template <std::size_t I>
    using arg_t = typename call_type::template arg_t<I>;
};

template <typename T>
using extract_function_return_type_t =
    typename extract_function_traits<T>::return_type;

template <typename T>
using extract_function_parameters_t =
    typename extract_function_traits<T>::parameter_types;

//------------------------------------------------------------------------------
// Tuple and structured binding support detection
//------------------------------------------------------------------------------

// Enhanced tuple-like type detection
template <class T, std::size_t I>
concept has_tuple_element = requires {
    typename std::tuple_element_t<I, std::remove_cvref_t<T>>;
    { std::get<I>(std::declval<T>()) };
};

template <class Expr>
consteval bool is_consteval(Expr) {
    return requires { typename std::bool_constant<(Expr{}(), false)>; };
}

template <class T>
consteval bool is_tuple_like_well_formed() {
    if constexpr (requires {
                      {
                          std::tuple_size<std::remove_cvref_t<T>>::value
                      } -> std::same_as<const std::size_t&>;
                  }) {
        if constexpr (is_consteval([] {
                          return std::tuple_size<std::remove_cvref_t<T>>::value;
                      })) {
            return []<std::size_t... I>(std::index_sequence<I...>) {
                return (has_tuple_element<T, I> && ...);
            }(std::make_index_sequence<
                       std::tuple_size_v<std::remove_cvref_t<T>>>{});
        }
    }
    return false;
}

// Concept for tuple-like types
template <typename T>
concept TupleLike = is_tuple_like_well_formed<T>();

//------------------------------------------------------------------------------
// Advanced type constraint detection
//------------------------------------------------------------------------------

// Enhanced constraint level enum with stronger typing
enum class constraint_level {
    none,        // No constraints
    nontrivial,  // Basic operation support
    nothrow,     // Nothrow guarantee
    trivial      // Trivial implementation
};

// 修复拼写错误 - 替换copyability和relocatability
// Check copy operations with detailed constraints
template <typename T>
consteval bool has_copy_operations(constraint_level level) {
    switch (level) {
        case constraint_level::none:
            return true;
        case constraint_level::nontrivial:
            return std::is_copy_constructible_v<T> &&
                   std::is_copy_assignable_v<T>;
        case constraint_level::nothrow:
            return std::is_nothrow_copy_constructible_v<T> &&
                   std::is_nothrow_copy_assignable_v<T>;
        case constraint_level::trivial:
            return std::is_trivially_copy_constructible_v<T> &&
                   std::is_trivially_copy_assignable_v<T> &&
                   std::is_trivially_destructible_v<T>;
        default:
            return false;
    }
}

// Check move operations with improved consistency
template <typename T>
consteval bool has_move_operations(constraint_level level) {
    switch (level) {
        case constraint_level::none:
            return true;
        case constraint_level::nontrivial:
            return std::is_move_constructible_v<T> &&
                   std::is_move_assignable_v<T> && std::is_destructible_v<T>;
        case constraint_level::nothrow:
            return std::is_nothrow_move_constructible_v<T> &&
                   std::is_nothrow_move_assignable_v<T> &&
                   std::is_nothrow_destructible_v<T>;
        case constraint_level::trivial:
            return std::is_trivially_move_constructible_v<T> &&
                   std::is_trivially_move_assignable_v<T> &&
                   std::is_trivially_destructible_v<T>;
        default:
            return false;
    }
}

// Check destructibility with enhanced diagnostics
template <typename T>
consteval bool has_destructibility(constraint_level level) {
    switch (level) {
        case constraint_level::none:
            return true;
        case constraint_level::nontrivial:
            return std::is_destructible_v<T>;
        case constraint_level::nothrow:
            return std::is_nothrow_destructible_v<T>;
        case constraint_level::trivial:
            return std::is_trivially_destructible_v<T>;
        default:
            return false;
    }
}

// 修复概念定义，使用新的函数名
template <typename T>
concept Copyable = has_copy_operations<T>(constraint_level::nontrivial);

template <typename T>
concept NothrowCopyable = has_copy_operations<T>(constraint_level::nothrow);

template <typename T>
concept TriviallyCopyable = has_copy_operations<T>(constraint_level::trivial);

template <typename T>
concept Relocatable = has_move_operations<T>(constraint_level::nontrivial);

template <typename T>
concept NothrowRelocatable = has_move_operations<T>(constraint_level::nothrow);

template <typename T>
concept TriviallyRelocatable =
    has_move_operations<T>(constraint_level::trivial);

//------------------------------------------------------------------------------
// Template base class detection (improved)
//------------------------------------------------------------------------------

template <template <typename...> class Base, typename Derived>
struct is_base_of_template_impl {
private:
    // Use SFINAE to detect if Derived can be converted to Base<U...>*
    template <typename... U>
    static constexpr std::true_type test(const Base<U...>*);
    static constexpr std::false_type test(...);

    // For better diagnostics
    template <typename T>
    static auto derived_name() {
        return DemangleHelper::demangle(typeid(T).name());
    }

    template <typename... U>
    static auto base_name() {
        return DemangleHelper::demangle(typeid(Base<U...>).name());
    }

public:
    static constexpr bool value =
        decltype(test(std::declval<Derived*>()))::value;

    static const inline std::string diagnostic_message = [] {
        if constexpr (value) {
            return derived_name<Derived>() + " inherits from template " +
                   base_name<typename Derived::some_param_type>();
        } else {
            return derived_name<Derived>() +
                   " does not inherit from the specified template";
        }
    }();
};

// Support multiple inheritance checking with fold expressions
template <typename Derived, template <typename...> class... Bases>
struct is_base_of_any_template {
    static constexpr bool value =
        (is_base_of_template_impl<Bases, Derived>::value || ...);

    // Track which templates are bases
    template <template <typename...> class Base>
    static constexpr bool is_base =
        is_base_of_template_impl<Base, Derived>::value;

    static constexpr std::array<bool, sizeof...(Bases)> inheritance_map = {
        is_base<Bases>...};
};

// Variable templates for simpler interface
template <template <typename...> class Base, typename Derived>
inline constexpr bool is_base_of_template_v =
    is_base_of_template_impl<Base, Derived>::value;

template <typename Derived, template <typename...> class... Bases>
inline constexpr bool is_base_of_any_template_v =
    is_base_of_any_template<Derived, Bases...>::value;

// Template inheritance concepts
template <template <typename...> class Base, typename Derived>
concept DerivedFromTemplate = is_base_of_template_v<Base, Derived>;

template <typename Derived, template <typename...> class... Bases>
concept DerivedFromAnyTemplate = is_base_of_any_template_v<Derived, Bases...>;

//------------------------------------------------------------------------------
// New additions for thread safety, variants, and containers
//------------------------------------------------------------------------------

// Thread safety detection traits
template <typename T>
concept ThreadSafe = requires {
    typename T::is_thread_safe;
    { T::is_thread_safe::value } -> std::convertible_to<bool>;
    requires T::is_thread_safe::value;
};

// Variant traits for working with std::variant
template <typename T>
struct variant_traits {
    static constexpr bool is_variant =
        is_specialization_of_v<std::variant, std::remove_cvref_t<T>>;

    // Only valid if T is a variant
    template <typename U>
    static constexpr bool contains = []() {
        if constexpr (!is_variant) {
            return false;
        } else {
            return []<std::size_t... Is>(std::index_sequence<Is...>) {
                return (std::is_same_v<U, std::variant_alternative_t<
                                              Is, std::remove_cvref_t<T>>> ||
                        ...);
            }(std::make_index_sequence<
                       template_arity_v<std::remove_cvref_t<T>>>{});
        }
    }();

    static constexpr std::size_t size =
        is_variant ? template_arity_v<std::remove_cvref_t<T>> : 0;

    template <std::size_t I>
    using alternative_t = std::variant_alternative_t<I, std::remove_cvref_t<T>>;
};

// Container traits
template <typename T>
struct container_traits {
    // Standard container type detection
    static constexpr bool is_container = requires(T t) {
        typename T::value_type;
        typename T::reference;
        typename T::const_reference;
        typename T::iterator;
        typename T::const_iterator;
        typename T::difference_type;
        typename T::size_type;
        { t.begin() } -> std::same_as<typename T::iterator>;
        { t.end() } -> std::same_as<typename T::iterator>;
        { t.size() } -> std::same_as<typename T::size_type>;
    };

    // Additional container properties
    static constexpr bool is_sequence_container =
        is_container && requires(T t) {
            { t.front() } -> std::same_as<typename T::reference>;
            { t.back() } -> std::same_as<typename T::reference>;
        };

    static constexpr bool is_associative_container =
        is_container && requires(T t) {
            typename T::key_type;
            {
                t.find(std::declval<typename T::key_type>())
            } -> std::same_as<typename T::iterator>;
        };

    // Fixed size check
    static constexpr bool is_fixed_size = requires(T) {
        { T::static_size } -> std::convertible_to<std::size_t>;
    };
};

//------------------------------------------------------------------------------
// Error reporting and static diagnostics
//------------------------------------------------------------------------------

// Improved static diagnostics
template <bool Condition, const char* Message = nullptr>
struct static_check {
    static_assert(Condition, "Condition failed");
    static constexpr bool value = Condition;
};

// Static error with source location
template <const char* Message, auto Location = std::source_location::current()>
struct static_error {
    static_assert(false,
                  "Static error triggered");  // Use a string literal here
    static constexpr const char* message = Message;
    static constexpr auto location = Location;
};

// 修复type_name实现，避免返回悬空指针
template <typename T>
inline constexpr auto type_name = [] {
    std::string name = DemangleHelper::demangle(typeid(T).name());
    static std::string stored_name = name;
    return stored_name;
}();

}  // namespace atom::meta

// Additional specializations for standard library types to ensure compatibility
namespace std {
// Allow decomposition of atom::meta::identity
template <size_t I, typename T, auto... Values>
auto get(const atom::meta::identity<T, Values...>&) {
    return atom::meta::identity<T, Values...>::template get<I>();
}
}  // namespace std

#endif  // ATOM_META_TEMPLATE_TRAITS_HPP
