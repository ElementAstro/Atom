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
#include <source_location>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "abi.hpp"

#ifdef _WIN32
#undef max
#endif

namespace atom::meta {

template <typename T>
struct type_identity {
    using type = T;
};

/**
 * @brief Identity type wrapper with value support
 * @tparam T Type to wrap
 * @tparam Values Optional compile-time values
 */
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

}  // namespace atom::meta

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

/**
 * @brief Type list implementation with operations
 * @tparam Ts Types in the list
 */
template <typename... Ts>
struct type_list {
    static constexpr std::size_t size = sizeof...(Ts);

    template <typename... Us>
    using append = type_list<Ts..., Us...>;

    template <typename... Us>
    using prepend = type_list<Us..., Ts...>;

    template <template <typename> typename F>
    using transform = type_list<typename F<Ts>::type...>;

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

    template <template <typename> typename F>
    using filter = typename filter_impl<F, type_list<>, Ts...>::type;
};

/**
 * @brief Check if a type is a template instantiation
 * @tparam T Type to check
 */
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

template <typename T>
concept TemplateInstantiation = is_template_v<T>;

/**
 * @brief Extract template parameters and metadata
 * @tparam T Template type to analyze
 */
template <typename T>
struct template_traits {
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

    static const inline std::array<std::string, sizeof...(Args)> arg_names = {
        DemangleHelper::demangle(typeid(Args).name())...};

    template <typename T>
    static constexpr bool has_arg = (std::is_same_v<T, Args> || ...);
};

template <typename T>
using args_type_of = typename template_traits<T>::args_type;

template <typename T>
using type_list_of = typename template_traits<T>::type_list_args;

template <typename T>
inline constexpr std::size_t template_arity_v = template_traits<T>::arity;

/**
 * @brief Check if a type is a specialization of a given template
 * @tparam Template Template to check against
 * @tparam T Type to check
 */
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

template <template <typename...> typename Template, typename T>
concept SpecializationOf = is_specialization_of_v<Template, T>;

/**
 * @brief Extract the N-th template parameter type
 * @tparam N Index of parameter
 * @tparam T Template type
 */
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

/**
 * @brief Check if a type is derived from multiple base classes
 * @tparam Derived Derived type
 * @tparam Bases Base types
 */
template <typename Derived, typename... Bases>
struct is_derived_from_all
    : std::conjunction<std::is_base_of<Bases, Derived>...> {
    template <typename Base>
    static constexpr bool inherits_from = std::is_base_of_v<Base, Derived>;

    static constexpr std::array<bool, sizeof...(Bases)> inheritance_map = {
        inherits_from<Bases>...};

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

template <typename Derived, typename... Bases>
concept DerivedFromAll = is_derived_from_all_v<Derived, Bases...>;

template <typename Derived, typename... Bases>
struct is_derived_from_any
    : std::disjunction<std::is_base_of<Bases, Derived>...> {};

template <typename Derived, typename... Bases>
inline constexpr bool is_derived_from_any_v =
    is_derived_from_any<Derived, Bases...>::value;

template <typename Derived, typename... Bases>
concept DerivedFromAny = is_derived_from_any_v<Derived, Bases...>;

/**
 * @brief Check if a type is a partial specialization of a given template
 * @tparam T Type to check
 * @tparam Template Template to check against
 */
template <typename T, template <typename, typename...> typename Template>
struct is_partial_specialization_of : std::false_type {};

template <template <typename, typename...> typename Template, typename Arg,
          typename... Args>
struct is_partial_specialization_of<Template<Arg, Args...>, Template>
    : std::true_type {};

template <typename T, template <typename, typename...> typename Template>
inline constexpr bool is_partial_specialization_of_v =
    is_partial_specialization_of<T, Template>::value;

template <typename T>
struct is_alias_template : std::false_type {};

template <template <typename...> typename Template, typename... Args>
struct is_alias_template<Template<Args...>> : std::true_type {
    static constexpr bool likely_alias = !std::is_class_v<Template<Args...>> ||
                                         !std::is_union_v<Template<Args...>>;
};

template <typename T>
inline constexpr bool is_alias_template_v = is_alias_template<T>::value;

template <typename T>
concept ClassTemplate = is_template_v<T> && std::is_class_v<T>;

template <typename T>
concept FunctionTemplate = requires(T t) {
    &T::operator();
    is_template_v<T>;
};

template <typename T>
using template_args_as_tuple_t = typename template_traits<T>::args_type;

template <typename T>
using template_args_as_type_list_t =
    typename template_traits<T>::type_list_args;

/**
 * @brief Count the number of occurrences of a type in a parameter pack
 * @tparam T Type to count
 * @tparam Args Parameter pack
 */
template <typename T, typename... Args>
constexpr std::size_t count_occurrences_v =
    (0 + ... + (std::is_same_v<T, Args> ? 1 : 0));

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

    if constexpr (value == std::numeric_limits<std::size_t>::max()) {
        if constexpr (std::is_same_v<T, void>) {
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

/**
 * @brief Extract reference wrapper or pointer types
 * @tparam T Type to extract from
 */
template <typename T>
struct extract_reference_wrapper {
    using type = std::conditional_t<
        is_specialization_of_v<std::reference_wrapper, std::remove_cvref_t<T>>,
        typename std::remove_cvref_t<T>::type, std::remove_reference_t<T>>;
};

template <typename T>
struct extract_reference_wrapper_type {
    using type = T;
};

template <typename T>
struct extract_reference_wrapper_type<std::reference_wrapper<T>> {
    using type = T;
};

template <typename T>
struct extract_reference_wrapper_type<T&> {
    using type = T;
};

template <typename T>
struct extract_reference_wrapper_type<const T&> {
    using type = const T;
};

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

template <typename T>
struct extract_pointer<T*> {
    using type = T;
    using element_type = T;
    static constexpr bool is_pointer = true;
    static constexpr bool is_smart_pointer = false;
};

template <typename T>
using extract_pointer_type_t = typename extract_pointer<T>::element_type;

/**
 * @brief Extract function traits including return type and parameters
 * @tparam T Function type
 */
template <typename T>
struct extract_function_traits;

template <typename R, typename... Args>
struct extract_function_traits<R(Args...)> {
    using return_type = R;
    using parameter_types = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);

    template <std::size_t I>
    using arg_t = std::tuple_element_t<I, parameter_types>;
};

template <typename R, typename... Args>
struct extract_function_traits<R (*)(Args...)>
    : extract_function_traits<R(Args...)> {};

template <typename C, typename R, typename... Args>
struct extract_function_traits<R (C::*)(Args...)> {
    using class_type = C;
    using return_type = R;
    using parameter_types = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);

    template <std::size_t I>
    using arg_t = std::tuple_element_t<I, parameter_types>;
};

template <typename C, typename R, typename... Args>
struct extract_function_traits<R (C::*)(Args...) const>
    : extract_function_traits<R (C::*)(Args...)> {};

template <typename R, typename... Args>
struct extract_function_traits<R(Args...) noexcept>
    : extract_function_traits<R(Args...)> {
    static constexpr bool is_noexcept = true;
};

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

template <typename T>
concept TupleLike = is_tuple_like_well_formed<T>();

/**
 * @brief Enhanced constraint level enum
 */
enum class constraint_level { none, nontrivial, nothrow, trivial };

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

template <template <typename...> class Base, typename Derived>
struct is_base_of_template_impl {
private:
    template <typename... U>
    static constexpr std::true_type test(const Base<U...>*);
    static constexpr std::false_type test(...);

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

template <typename Derived, template <typename...> class... Bases>
struct is_base_of_any_template {
    static constexpr bool value =
        (is_base_of_template_impl<Bases, Derived>::value || ...);

    template <template <typename...> class Base>
    static constexpr bool is_base =
        is_base_of_template_impl<Base, Derived>::value;

    static constexpr std::array<bool, sizeof...(Bases)> inheritance_map = {
        is_base<Bases>...};
};

template <template <typename...> class Base, typename Derived>
inline constexpr bool is_base_of_template_v =
    is_base_of_template_impl<Base, Derived>::value;

template <typename Derived, template <typename...> class... Bases>
inline constexpr bool is_base_of_any_template_v =
    is_base_of_any_template<Derived, Bases...>::value;

template <template <typename...> class Base, typename Derived>
concept DerivedFromTemplate = is_base_of_template_v<Base, Derived>;

template <typename Derived, template <typename...> class... Bases>
concept DerivedFromAnyTemplate = is_base_of_any_template_v<Derived, Bases...>;

template <typename T>
concept ThreadSafe = requires {
    typename T::is_thread_safe;
    { T::is_thread_safe::value } -> std::convertible_to<bool>;
    requires T::is_thread_safe::value;
};

/**
 * @brief Variant traits for working with std::variant
 * @tparam T Type to analyze
 */
template <typename T>
struct variant_traits {
    static constexpr bool is_variant =
        is_specialization_of_v<std::variant, std::remove_cvref_t<T>>;

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

/**
 * @brief Container traits for type analysis
 * @tparam T Type to analyze
 */
template <typename T>
struct container_traits {
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

    static constexpr bool is_fixed_size = requires(T) {
        { T::static_size } -> std::convertible_to<std::size_t>;
    };
};

template <bool Condition, const char* Message = nullptr>
struct static_check {
    static_assert(Condition, "Condition failed");
    static constexpr bool value = Condition;
};

template <const char* Message, auto Location = std::source_location::current()>
struct static_error {
    static_assert(false, "Static error triggered");
    static constexpr const char* message = Message;
    static constexpr auto location = Location;
};

template <typename T>
inline constexpr auto type_name = [] {
    std::string name = DemangleHelper::demangle(typeid(T).name());
    static std::string stored_name = name;
    return stored_name;
}();

}  // namespace atom::meta

namespace std {
template <size_t I, typename T, auto... Values>
auto get(const atom::meta::identity<T, Values...>&) {
    return atom::meta::identity<T, Values...>::template get<I>();
}
}  // namespace std

#endif
