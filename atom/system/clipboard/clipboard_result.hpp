/**
 * @file clipboard_result.hpp
 * @brief Result type for clipboard operations that may fail.
 *
 * Provides ClipboardResult<T> template and void specialization for
 * exception-safe error handling in clipboard operations.
 */

#ifndef ATOM_SYSTEM_CLIPBOARD_RESULT_HPP
#define ATOM_SYSTEM_CLIPBOARD_RESULT_HPP

#include <optional>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace clip {

/**
 * @brief Result type for operations that may fail
 */
template <typename T>
class ClipboardResult {
private:
    std::optional<T> m_value;
    std::error_code m_error;

public:
    ClipboardResult(T&& value) noexcept : m_value(std::move(value)) {}
    ClipboardResult(const T& value) : m_value(value) {}
    ClipboardResult(std::error_code error) noexcept : m_error(error) {}

    bool has_value() const noexcept { return m_value.has_value(); }
    explicit operator bool() const noexcept { return has_value(); }

    const T& value() const& {
        if (!has_value())
            throw std::runtime_error("ClipboardResult has no value");
        return *m_value;
    }

    T& value() & {
        if (!has_value())
            throw std::runtime_error("ClipboardResult has no value");
        return *m_value;
    }

    T&& value() && {
        if (!has_value())
            throw std::runtime_error("ClipboardResult has no value");
        return std::move(*m_value);
    }

    const T& operator*() const& noexcept { return *m_value; }
    T& operator*() & noexcept { return *m_value; }
    T&& operator*() && noexcept { return std::move(*m_value); }

    const T* operator->() const noexcept { return &*m_value; }
    T* operator->() noexcept { return &*m_value; }

    std::error_code error() const noexcept { return m_error; }

    template <typename U>
    T value_or(U&& default_value) const& {
        return has_value() ? *m_value
                           : static_cast<T>(std::forward<U>(default_value));
    }

    template <typename U>
    T value_or(U&& default_value) && {
        return has_value() ? std::move(*m_value)
                           : static_cast<T>(std::forward<U>(default_value));
    }
};

/**
 * @brief Result type specialization for void operations
 */
template <>
class ClipboardResult<void> {
private:
    std::error_code m_error;

public:
    ClipboardResult() noexcept = default;
    ClipboardResult(std::error_code error) noexcept : m_error(error) {}

    bool has_value() const noexcept { return !m_error; }
    explicit operator bool() const noexcept { return has_value(); }

    void value() const {
        if (!has_value())
            throw std::runtime_error("ClipboardResult has error");
    }

    std::error_code error() const noexcept { return m_error; }
};

}  // namespace clip

#endif  // ATOM_SYSTEM_CLIPBOARD_RESULT_HPP
