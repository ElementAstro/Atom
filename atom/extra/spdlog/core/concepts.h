#pragma once

#include <any>
#include <format>
#include <string_view>


/**
 * @file concepts.h
 * @brief Concepts for formatting, serialization, logging, event handling, and
 * ranges in modern_log.
 */

namespace modern_log {

/**
 * @brief Concept for types that can be formatted using std::format.
 *
 * A type T satisfies Formattable if it can be passed to std::format with the
 * "{}" format string.
 *
 * @tparam T The type to check for formattability.
 */
template <typename T>
concept Formattable = requires(T t) { std::format("{}", t); };

/**
 * @brief Concept for types that can be serialized to JSON.
 *
 * A type T satisfies Serializable if it provides a to_json() method returning a
 * std::string, or if it is Formattable.
 *
 * @tparam T The type to check for serializability.
 */
template <typename T>
concept Serializable = requires(T t) {
    { t.to_json() } -> std::convertible_to<std::string>;
} || Formattable<T>;

/**
 * @brief Concept for log filter functions.
 *
 * A type Func satisfies LogFilterFunc if it is callable with a std::string_view
 * message and an int log level, and returns a bool indicating whether the log
 * should be accepted.
 *
 * @tparam Func The function type to check.
 */
template <typename Func>
concept LogFilterFunc = requires(Func f, std::string_view msg, int level) {
    { f(msg, level) } -> std::convertible_to<bool>;
};

/**
 * @brief Concept for event handler functions.
 *
 * A type Handler satisfies EventHandler if it is callable with a
 * std::string_view event name and an std::any data payload.
 *
 * @tparam Handler The function type to check.
 */
template <typename Handler>
concept EventHandler = requires(Handler h, std::string_view event,
                                std::any data) { h(event, data); };

/**
 * @brief Concept for iterable ranges whose value type is Formattable.
 *
 * A type R satisfies Range if it is a std::ranges::range and its value type is
 * Formattable.
 *
 * @tparam R The range type to check.
 */
template <typename R>
concept Range =
    std::ranges::range<R> && Formattable<std::ranges::range_value_t<R>>;

}  // namespace modern_log