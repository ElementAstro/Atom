#ifndef ATOM_META_AWAITABLE_HPP
#define ATOM_META_AWAITABLE_HPP

#if __cpp_lib_coroutine
#include <coroutine>
#include <functional>
#include <tuple>
#include <type_traits>

namespace atom::meta {

/*!
 * \brief Simple awaitable wrapper for function calls
 * \tparam F Function type
 * \tparam Args Argument types
 */
template <typename Function, typename... StoredArgs>
class SimpleAwaitable {
public:
    using result_type = std::invoke_result_t<Function&, StoredArgs...>;

    template <typename F, typename... Args>
    SimpleAwaitable(F&& func, Args&&... args)
        : func_(std::forward<F>(func)), args_(std::forward<Args>(args)...) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) const noexcept {
        // For this simple implementation, we just resume immediately
        handle.resume();
    }

    result_type await_resume() { return std::apply(func_, args_); }

private:
    Function func_;
    std::tuple<StoredArgs...> args_;
};

/*!
 * \brief Create an awaitable from a function and its arguments
 * \tparam F Function type
 * \tparam Args Argument types
 * \param func Function to make awaitable
 * \param args Arguments to pass to the function
 * \return Awaitable object
 */
template <typename F, typename... Args>
[[nodiscard]] auto makeAwaitable(F&& func, Args&&... args) {
    return SimpleAwaitable<std::decay_t<F>, std::decay_t<Args>...>(
        std::forward<F>(func), std::forward<Args>(args)...);
}

template <typename F, typename... Args>
SimpleAwaitable(F&&, Args&&...)
    -> SimpleAwaitable<std::decay_t<F>, std::decay_t<Args>...>;

//==============================================================================
// C++23 Enhanced Awaitable Utilities
//==============================================================================

/**
 * @brief Concept for awaitable types
 */
template <typename T>
concept Awaitable = requires(T t) {
    { t.await_ready() } -> std::convertible_to<bool>;
    { t.await_suspend(std::declval<std::coroutine_handle<>>()) };
    { t.await_resume() };
};

/**
 * @brief Delayed awaitable with timeout
 */
template <typename Function, typename... StoredArgs>
class DelayedAwaitable {
    Function func_;
    std::tuple<StoredArgs...> args_;
    std::chrono::milliseconds delay_;

public:
    using result_type = std::invoke_result_t<Function&, StoredArgs...>;

    template <typename F, typename... Args>
    DelayedAwaitable(std::chrono::milliseconds delay, F&& func, Args&&... args)
        : func_(std::forward<F>(func)),
          args_(std::forward<Args>(args)...),
          delay_(delay) {}

    bool await_ready() const noexcept { return delay_.count() == 0; }

    void await_suspend(std::coroutine_handle<> handle) const noexcept {
        std::this_thread::sleep_for(delay_);
        handle.resume();
    }

    result_type await_resume() { return std::apply(func_, args_); }
};

/**
 * @brief Create a delayed awaitable
 */
template <typename F, typename... Args>
auto makeDelayedAwaitable(std::chrono::milliseconds delay, F&& func,
                          Args&&... args) {
    return DelayedAwaitable<std::decay_t<F>, std::decay_t<Args>...>(
        delay, std::forward<F>(func), std::forward<Args>(args)...);
}

/**
 * @brief Conditional awaitable
 */
template <typename Condition, typename Function, typename... StoredArgs>
class ConditionalAwaitable {
    Condition condition_;
    Function func_;
    std::tuple<StoredArgs...> args_;

public:
    using result_type =
        std::optional<std::invoke_result_t<Function&, StoredArgs...>>;

    template <typename C, typename F, typename... Args>
    ConditionalAwaitable(C&& cond, F&& func, Args&&... args)
        : condition_(std::forward<C>(cond)),
          func_(std::forward<F>(func)),
          args_(std::forward<Args>(args)...) {}

    bool await_ready() const noexcept { return !condition_(); }

    void await_suspend(std::coroutine_handle<> handle) const noexcept {
        handle.resume();
    }

    result_type await_resume() {
        if (condition_()) {
            return std::apply(func_, args_);
        }
        return std::nullopt;
    }
};

/**
 * @brief Create a conditional awaitable
 */
template <typename C, typename F, typename... Args>
auto makeConditionalAwaitable(C&& cond, F&& func, Args&&... args) {
    return ConditionalAwaitable<std::decay_t<C>, std::decay_t<F>,
                                std::decay_t<Args>...>(
        std::forward<C>(cond), std::forward<F>(func),
        std::forward<Args>(args)...);
}

/**
 * @brief Awaitable that transforms result
 */
template <typename Function, typename Transform, typename... StoredArgs>
class TransformingAwaitable {
    Function func_;
    Transform transform_;
    std::tuple<StoredArgs...> args_;

public:
    using intermediate_type = std::invoke_result_t<Function&, StoredArgs...>;
    using result_type = std::invoke_result_t<Transform&, intermediate_type>;

    template <typename F, typename T, typename... Args>
    TransformingAwaitable(F&& func, T&& transform, Args&&... args)
        : func_(std::forward<F>(func)),
          transform_(std::forward<T>(transform)),
          args_(std::forward<Args>(args)...) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) const noexcept {
        handle.resume();
    }

    result_type await_resume() { return transform_(std::apply(func_, args_)); }
};

/**
 * @brief Create a transforming awaitable
 */
template <typename F, typename T, typename... Args>
auto makeTransformingAwaitable(F&& func, T&& transform, Args&&... args) {
    return TransformingAwaitable<std::decay_t<F>, std::decay_t<T>,
                                 std::decay_t<Args>...>(
        std::forward<F>(func), std::forward<T>(transform),
        std::forward<Args>(args)...);
}

/**
 * @brief Immediate awaitable (no suspension)
 */
template <typename T>
class ImmediateAwaitable {
    T value_;

public:
    explicit ImmediateAwaitable(T value) : value_(std::move(value)) {}

    bool await_ready() const noexcept { return true; }
    void await_suspend(std::coroutine_handle<>) const noexcept {}
    T await_resume() { return std::move(value_); }
};

/**
 * @brief Create an immediate awaitable
 */
template <typename T>
auto makeImmediateAwaitable(T&& value) {
    return ImmediateAwaitable<std::decay_t<T>>(std::forward<T>(value));
}

}  // namespace atom::meta

#endif  // __cpp_lib_coroutine

#endif  // ATOM_META_AWAITABLE_HPP
