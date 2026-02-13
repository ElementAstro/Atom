// rust_option.hpp
#pragma once

#include "atom/error/exception.hpp"

namespace atom::algorithm {

template <typename T>
class Option {
private:
    bool m_has_value;
    T m_value;

public:
    Option() : m_has_value(false), m_value() {}
    explicit Option(T value) : m_has_value(true), m_value(value) {}

    bool has_value() const { return m_has_value; }
    bool is_some() const { return m_has_value; }
    bool is_none() const { return !m_has_value; }

    T value() const {
        if (!m_has_value) {
            THROW_RUNTIME_ERROR("Called value() on a None option");
        }
        return m_value;
    }

    T unwrap() const {
        if (!m_has_value) {
            THROW_RUNTIME_ERROR("Called unwrap() on a None option");
        }
        return m_value;
    }

    T unwrap_or(T default_value) const {
        return m_has_value ? m_value : default_value;
    }

    template <typename F>
    T unwrap_or_else(F&& f) const {
        return m_has_value ? m_value : f();
    }

    template <typename F>
    auto map(F&& f) const -> Option<decltype(f(std::declval<T>()))> {
        using U = decltype(f(std::declval<T>()));

        if (m_has_value) {
            return Option<U>(f(m_value));
        }
        return Option<U>();
    }

    template <typename F>
    auto and_then(F&& f) const -> decltype(f(std::declval<T>())) {
        using ReturnType = decltype(f(std::declval<T>()));

        if (m_has_value) {
            return f(m_value);
        }
        return ReturnType();
    }

    static Option<T> some(T value) { return Option<T>(value); }

    static Option<T> none() { return Option<T>(); }
};

}  // namespace atom::algorithm
