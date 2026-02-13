/**
 * @file clipboard_utils.hpp
 * @brief Utility classes for clipboard operations.
 *
 * Contains ScopeGuard for exception-safe resource management.
 */

#ifndef ATOM_SYSTEM_CLIPBOARD_UTILS_HPP
#define ATOM_SYSTEM_CLIPBOARD_UTILS_HPP

#include <type_traits>
#include <utility>

namespace clip {

/**
 * @brief Scope guard for exception safety
 */
template <typename F>
class ScopeGuard {
private:
    F function_;
    bool should_execute_;

public:
    explicit ScopeGuard(F&& f) noexcept
        : function_(std::forward<F>(f)), should_execute_(true) {}

    ~ScopeGuard() noexcept {
        if (should_execute_) {
            try {
                function_();
            } catch (...) {
                // Suppress exceptions in destructor
            }
        }
    }

    void dismiss() noexcept { should_execute_ = false; }

    // Non-copyable, movable
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    ScopeGuard(ScopeGuard&& other) noexcept
        : function_(std::move(other.function_)),
          should_execute_(other.should_execute_) {
        other.should_execute_ = false;
    }

    ScopeGuard& operator=(ScopeGuard&& other) noexcept {
        if (this != &other) {
            function_ = std::move(other.function_);
            should_execute_ = other.should_execute_;
            other.should_execute_ = false;
        }
        return *this;
    }
};

/**
 * @brief Helper function to create a scope guard
 */
template <typename F>
[[nodiscard]] auto make_scope_guard(F&& f) noexcept {
    return ScopeGuard<std::decay_t<F>>(std::forward<F>(f));
}

}  // namespace clip

#endif  // ATOM_SYSTEM_CLIPBOARD_UTILS_HPP
