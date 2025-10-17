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

}  // namespace atom::meta

#endif  // __cpp_lib_coroutine

#endif  // ATOM_META_AWAITABLE_HPP
