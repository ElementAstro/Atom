// rust_result.hpp
#pragma once

#include <string>
#include <variant>

#include "rust_error.hpp"

#include "atom/error/exception.hpp"

namespace atom::algorithm {

template <typename T>
class Result {
private:
    std::variant<T, Error> m_value;

public:
    Result(const T& value) : m_value(value) {}
    Result(const Error& error) : m_value(error) {}

    bool is_ok() const { return m_value.index() == 0; }
    bool is_err() const { return m_value.index() == 1; }

    const T& unwrap() const {
        if (is_ok()) {
            return std::get<0>(m_value);
        }
        THROW_RUNTIME_ERROR("Called unwrap() on an Err value: " +
                            std::get<1>(m_value).to_string());
    }

    T unwrap_or(const T& default_value) const {
        if (is_ok()) {
            return std::get<0>(m_value);
        }
        return default_value;
    }

    const Error& unwrap_err() const {
        if (is_err()) {
            return std::get<1>(m_value);
        }
        THROW_RUNTIME_ERROR("Called unwrap_err() on an Ok value");
    }

    template <typename F>
    auto map(F&& f) const -> Result<decltype(f(std::declval<T>()))> {
        using U = decltype(f(std::declval<T>()));

        if (is_ok()) {
            return Result<U>(f(std::get<0>(m_value)));
        }
        return Result<U>(std::get<1>(m_value));
    }

    template <typename E>
    T unwrap_or_else(E&& e) const {
        if (is_ok()) {
            return std::get<0>(m_value);
        }
        return e(std::get<1>(m_value));
    }

    static Result<T> ok(const T& value) { return Result<T>(value); }

    static Result<T> err(ErrorKind kind, const std::string& message) {
        return Result<T>(Error(kind, message));
    }
};

}  // namespace atom::algorithm
