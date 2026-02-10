/*
 * common.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-26

Description: Common definitions for async messaging components

**************************************************/

#ifndef ATOM_ASYNC_MESSAGING_COMMON_HPP
#define ATOM_ASYNC_MESSAGING_COMMON_HPP

#include <concepts>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>

// Platform detection macros
#if defined(__GNUC__) || defined(__clang__)
#define ATOM_LIKELY(x) __builtin_expect(!!(x), 1)
#define ATOM_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define ATOM_FORCE_INLINE __attribute__((always_inline)) inline
#define ATOM_NO_INLINE __attribute__((noinline))
#define ATOM_RESTRICT __restrict__
#elif defined(_MSC_VER)
#define ATOM_LIKELY(x) (x)
#define ATOM_UNLIKELY(x) (x)
#define ATOM_FORCE_INLINE __forceinline
#define ATOM_NO_INLINE __declspec(noinline)
#define ATOM_RESTRICT __restrict
#else
#define ATOM_LIKELY(x) (x)
#define ATOM_UNLIKELY(x) (x)
#define ATOM_FORCE_INLINE inline
#define ATOM_NO_INLINE
#define ATOM_RESTRICT
#endif

// Cache line size for alignment
#ifndef ATOM_CACHE_LINE_SIZE
#if defined(__aarch64__) || defined(_M_ARM64)
#define ATOM_CACHE_LINE_SIZE 128
#elif defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)
#define ATOM_CACHE_LINE_SIZE 64
#else
#define ATOM_CACHE_LINE_SIZE 64
#endif
#endif

#define ATOM_CACHELINE_ALIGN alignas(ATOM_CACHE_LINE_SIZE)

namespace atom::async {

// ============================================================================
// Common Concepts
// ============================================================================

/**
 * @brief Concept for message types that can be used in messaging systems
 */
template <typename T>
concept MessageConcept = std::copyable<T> && std::movable<T> &&
                         !std::is_pointer_v<T> && !std::is_reference_v<T>;

/**
 * @brief Concept for movable types used in queues
 */
template <typename T>
concept Movable = std::move_constructible<T> && std::assignable_from<T&, T>;

/**
 * @brief Concept for serializable types
 */
template <typename T>
concept Serializable = requires(T a) {
    { std::to_string(a) } -> std::convertible_to<std::string>;
} || std::same_as<T, std::string>;

/**
 * @brief Concept for comparable types
 */
template <typename T>
concept Comparable = requires(T a, T b) {
    { a == b } -> std::convertible_to<bool>;
    { a < b } -> std::convertible_to<bool>;
};

/**
 * @brief Concept for extractable predicates
 */
template <typename UnaryPredicate, typename T>
concept ExtractableWith = requires(UnaryPredicate pred, T t) {
    { pred(t) } -> std::convertible_to<bool>;
};

// ============================================================================
// Common Exception Classes
// ============================================================================

/**
 * @brief Base exception class for all messaging-related errors
 */
class MessagingException : public std::runtime_error {
public:
    explicit MessagingException(
        const std::string& message,
        const std::source_location& location = std::source_location::current())
        : std::runtime_error(formatMessage(message, location)),
          location_(location) {}

    [[nodiscard]] const std::source_location& location() const noexcept {
        return location_;
    }

private:
    std::source_location location_;

    static std::string formatMessage(const std::string& message,
                                     const std::source_location& location) {
        return message + " at " + location.file_name() + ":" +
               std::to_string(location.line()) + " in " +
               location.function_name();
    }
};

/**
 * @brief Exception for queue-related errors
 */
class QueueException : public MessagingException {
public:
    explicit QueueException(
        const std::string& message,
        const std::source_location& location = std::source_location::current())
        : MessagingException(message, location) {}
};

/**
 * @brief Exception for empty container operations
 */
class EmptyContainerException : public MessagingException {
public:
    explicit EmptyContainerException(
        const std::string& message = "Operation on empty container",
        const std::source_location& location = std::source_location::current())
        : MessagingException(message, location) {}
};

/**
 * @brief Exception for timeout errors
 */
class TimeoutException : public MessagingException {
public:
    explicit TimeoutException(
        const std::string& message = "Operation timed out",
        const std::source_location& location = std::source_location::current())
        : MessagingException(message, location) {}
};

/**
 * @brief Exception for subscriber-related errors
 */
class SubscriberException : public MessagingException {
public:
    explicit SubscriberException(
        const std::string& message,
        const std::source_location& location = std::source_location::current())
        : MessagingException(message, location) {}
};

/**
 * @brief Exception for serialization errors
 */
class SerializationException : public MessagingException {
public:
    explicit SerializationException(
        const std::string& message,
        const std::source_location& location = std::source_location::current())
        : MessagingException("Serialization error: " + message, location) {}
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_MESSAGING_COMMON_HPP
