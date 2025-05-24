#ifndef ATOM_IO_PUSHD_HPP
#define ATOM_IO_PUSHD_HPP

#include <concepts>
#include <coroutine>
#include <filesystem>
#include <functional>
#include <memory>
#include <system_error>
#include <utility>

#ifdef ATOM_USE_BOOST
#include <boost/asio.hpp>
namespace asio = boost::asio;
#elif defined(ATOM_USE_ASIO)
#include <asio.hpp>
#endif

#include "atom/containers/high_performance.hpp"

using atom::containers::String;
using atom::containers::Vector;

namespace atom::io {

class DirectoryStackImpl;

template <typename T>
concept PathLike = std::convertible_to<T, std::filesystem::path>;

class DirectoryStack {
public:
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
    static inline asio::io_context::executor_type executor_;
#endif
    template <typename T>
    class [[nodiscard]] Task {
    public:        struct promise_type {
            T result_;
            std::exception_ptr exception_;

            auto get_return_object() -> Task {
                return Task{
                    std::coroutine_handle<promise_type>::from_promise(*this)};
            }
            auto initial_suspend() noexcept -> std::suspend_always {
                return {};
            }
            auto final_suspend() noexcept -> std::suspend_always { return {}; }
            void unhandled_exception() {
                exception_ = std::current_exception();
            }
            void return_value(T value) { result_ = std::move(value); }
            
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
            auto get_executor() {
                return DirectoryStack::executor_;
            }
#endif
        };

        explicit Task(std::coroutine_handle<promise_type> h) : coro_(h) {}
        ~Task() {
            if (coro_)
                coro_.destroy();
        }

        Task(const Task&) = delete;
        auto operator=(const Task&) -> Task& = delete;

        Task(Task&& other) noexcept : coro_(other.coro_) { other.coro_ = {}; }
        auto operator=(Task&& other) noexcept -> Task& {
            if (this != &other) {
                if (coro_)
                    coro_.destroy();
                coro_ = other.coro_;
                other.coro_ = {};
            }
            return *this;
        }

        auto operator co_await() {
            struct Awaiter {
                std::coroutine_handle<promise_type> coro;

                bool await_ready() const noexcept { return false; }
                auto await_resume() -> T {
                    if (coro.promise().exception_) {
                        std::rethrow_exception(coro.promise().exception_);
                    }
                    return std::move(coro.promise().result_);
                }
                void await_suspend(std::coroutine_handle<> h) const {
                    auto resume_coro = coro;
                    auto resume_awaiting = h;
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
                    asio::post(coro.promise().get_executor(),
                               [resume_coro, resume_awaiting]() mutable {
                                   resume_coro.resume();
                                   resume_awaiting.resume();
                               });
#else
                    resume_coro.resume();
                    resume_awaiting.resume();
#endif
                }
            };
            return Awaiter{coro_};
        }

    private:
        std::coroutine_handle<promise_type> coro_;
    };

    explicit DirectoryStack(
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
    asio::io_context& io_context
#else
    void* io_context
#endif
    );

    ~DirectoryStack() noexcept;

    DirectoryStack(const DirectoryStack& other) = delete;
    auto operator=(const DirectoryStack& other) -> DirectoryStack& = delete;

    DirectoryStack(DirectoryStack&& other) noexcept;
    auto operator=(DirectoryStack&& other) noexcept -> DirectoryStack&;

    template <PathLike P>
    void asyncPushd(const P& new_dir,
                    const std::function<void(const std::error_code&)>& handler);

    template <PathLike P>
    [[nodiscard]] auto pushd(const P& new_dir) -> Task<void>;

    void asyncPopd(const std::function<void(const std::error_code&)>& handler);
    [[nodiscard]] auto popd() -> Task<void>;

    [[nodiscard]] auto peek() const -> std::filesystem::path;
    [[nodiscard]] auto dirs() const noexcept -> Vector<std::filesystem::path>;

    void clear() noexcept;
    void swap(size_t index1, size_t index2);
    void remove(size_t index);

    void asyncGotoIndex(
        size_t index,
        const std::function<void(const std::error_code&)>& handler);
    [[nodiscard]] auto gotoIndex(size_t index) -> Task<void>;

    void asyncSaveStackToFile(
        const String& filename,
        const std::function<void(const std::error_code&)>& handler);
    [[nodiscard]] auto saveStackToFile(const String& filename) -> Task<void>;

    void asyncLoadStackFromFile(
        const String& filename,
        const std::function<void(const std::error_code&)>& handler);
    [[nodiscard]] auto loadStackFromFile(const String& filename) -> Task<void>;

    [[nodiscard]] auto size() const noexcept -> size_t;
    [[nodiscard]] auto isEmpty() const noexcept -> bool;

    void asyncGetCurrentDirectory(
        const std::function<void(const std::filesystem::path&,
                                 const std::error_code&)>& handler) const;
    [[nodiscard]] auto getCurrentDirectory() const
        -> Task<std::filesystem::path>;

private:
    std::unique_ptr<DirectoryStackImpl> impl_;
};

template <>
class DirectoryStack::Task<void> {
public:    struct promise_type {
        std::exception_ptr exception_;

        auto get_return_object() -> Task {
            return Task{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        auto initial_suspend() noexcept -> std::suspend_always { return {}; }
        auto final_suspend() noexcept -> std::suspend_always { return {}; }
        void unhandled_exception() { exception_ = std::current_exception(); }
        void return_void() {}
        
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        auto get_executor() {
            return DirectoryStack::executor_;
        }
#endif
    };

    explicit Task(std::coroutine_handle<promise_type> h) : coro_(h) {}
    ~Task() {
        if (coro_)
            coro_.destroy();
    }

    Task(const Task&) = delete;
    auto operator=(const Task&) -> Task& = delete;

    Task(Task&& other) noexcept : coro_(other.coro_) { other.coro_ = {}; }
    auto operator=(Task&& other) noexcept -> Task& {
        if (this != &other) {
            if (coro_)
                coro_.destroy();
            coro_ = other.coro_;
            other.coro_ = {};
        }
        return *this;
    }

    auto operator co_await() {
        struct Awaiter {
            std::coroutine_handle<promise_type> coro;

            bool await_ready() const noexcept { return false; }
            void await_resume() {
                if (coro.promise().exception_) {
                    std::rethrow_exception(coro.promise().exception_);
                }
            }
            void await_suspend(std::coroutine_handle<> h) const {
                auto resume_coro = coro;
                auto resume_awaiting = h;
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
                asio::post(coro.promise().get_executor(),
                           [resume_coro, resume_awaiting]() mutable {
                               resume_coro.resume();
                               resume_awaiting.resume();
                           });
#else
                resume_coro.resume();
                resume_awaiting.resume();
#endif
            }
        };
        return Awaiter{coro_};
    }

private:
    std::coroutine_handle<promise_type> coro_;
};

}  // namespace atom::io

#endif  // ATOM_IO_PUSHD_HPP
