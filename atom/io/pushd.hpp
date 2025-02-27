#ifndef ATOM_IO_PUSHD_HPP
#define ATOM_IO_PUSHD_HPP

#include <concepts>
#include <coroutine>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <asio.hpp>

namespace atom::io {

// Forward declaration of the implementation class
class DirectoryStackImpl;

/**
 * @brief Custom concept to validate path-like types
 */
template <typename T>
concept PathLike = std::convertible_to<T, std::filesystem::path>;

/**
 * @class DirectoryStack
 * @brief A class for managing a stack of directory paths, allowing push, pop,
 * and various operations on the stack, with asynchronous support using Asio.
 */
class DirectoryStack {
public:
    /**
     * @brief Represents an asynchronous operation that can be awaited
     */
    template <typename T>
    class [[nodiscard]] Task {
    public:
        struct promise_type {
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
                    coro.resume();
                    h.resume();
                }
            };
            return Awaiter{coro_};
        }

    private:
        std::coroutine_handle<promise_type> coro_;
    };

    /**
     * @brief Specialization for void return type
     */
    template <>
    class Task<void> {
    public:
        struct promise_type {
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
            void return_void() {}
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
                    coro.resume();
                    h.resume();
                }
            };
            return Awaiter{coro_};
        }

    private:
        std::coroutine_handle<promise_type> coro_;
    };

    /**
     * @brief Constructor
     * @param io_context The Asio io_context to use for asynchronous operations
     * @throws std::bad_alloc if memory allocation fails
     */
    explicit DirectoryStack(asio::io_context& io_context);

    /**
     * @brief Destructor
     */
    ~DirectoryStack() noexcept;

    /**
     * @brief Copy constructor (deleted)
     */
    DirectoryStack(const DirectoryStack& other) = delete;

    /**
     * @brief Copy assignment operator (deleted)
     */
    auto operator=(const DirectoryStack& other) -> DirectoryStack& = delete;

    /**
     * @brief Move constructor
     */
    DirectoryStack(DirectoryStack&& other) noexcept;

    /**
     * @brief Move assignment operator
     */
    auto operator=(DirectoryStack&& other) noexcept -> DirectoryStack&;

    /**
     * @brief Push the current directory onto the stack and change to the
     * specified directory asynchronously.
     * @param new_dir The directory to change to.
     * @param handler The completion handler to be called when the operation
     * completes.
     * @throws std::invalid_argument if the path is invalid
     */
    void asyncPushd(const std::filesystem::path& new_dir,
                    const std::function<void(const std::error_code&)>& handler);

    /**
     * @brief Push the current directory onto the stack and change to the
     * specified directory using C++20 coroutines.
     * @param new_dir The directory to change to.
     * @return A task that completes when the operation is done
     */
    [[nodiscard]] auto pushd(const std::filesystem::path& new_dir)
        -> Task<void>;

    /**
     * @brief Pop the directory from the stack and change back to it
     * asynchronously.
     * @param handler The completion handler to be called when the operation
     * completes.
     */
    void asyncPopd(const std::function<void(const std::error_code&)>& handler);

    /**
     * @brief Pop the directory from the stack and change back to it
     * using C++20 coroutines.
     * @return A task that completes when the operation is done
     */
    [[nodiscard]] auto popd() -> Task<void>;

    /**
     * @brief View the top directory in the stack without changing to it.
     * @return The top directory in the stack.
     * @throws std::runtime_error if the stack is empty
     */
    [[nodiscard]] auto peek() const -> std::filesystem::path;

    /**
     * @brief Display the current stack of directories.
     * @return A vector of directory paths in the stack.
     */
    [[nodiscard]] auto dirs() const noexcept
        -> std::vector<std::filesystem::path>;

    /**
     * @brief Clear the directory stack.
     */
    void clear() noexcept;

    /**
     * @brief Swap two directories in the stack given their indices.
     * @param index1 The first index.
     * @param index2 The second index.
     * @throws std::out_of_range if indices are out of bounds
     */
    void swap(size_t index1, size_t index2);

    /**
     * @brief Remove a directory from the stack at the specified index.
     * @param index The index of the directory to remove.
     * @throws std::out_of_range if index is out of bounds
     */
    void remove(size_t index);

    /**
     * @brief Change to the directory at the specified index in the stack
     * asynchronously.
     * @param index The index of the directory to change to.
     * @param handler The completion handler to be called when the operation
     * completes.
     */
    void asyncGotoIndex(
        size_t index,
        const std::function<void(const std::error_code&)>& handler);

    /**
     * @brief Change to the directory at the specified index using C++20
     * coroutines
     * @param index The index of the directory to change to
     * @return A task that completes when the operation is done
     */
    [[nodiscard]] auto gotoIndex(size_t index) -> Task<void>;

    /**
     * @brief Save the directory stack to a file asynchronously.
     * @param filename The name of the file to save the stack to.
     * @param handler The completion handler to be called when the operation
     * completes.
     */
    void asyncSaveStackToFile(
        const std::string& filename,
        const std::function<void(const std::error_code&)>& handler);

    /**
     * @brief Save the stack to file using C++20 coroutines
     * @param filename The name of the file to save to
     * @return A task that completes when the operation is done
     */
    [[nodiscard]] auto saveStackToFile(const std::string& filename)
        -> Task<void>;

    /**
     * @brief Load the directory stack from a file asynchronously.
     * @param filename The name of the file to load the stack from.
     * @param handler The completion handler to be called when the operation
     * completes.
     */
    void asyncLoadStackFromFile(
        const std::string& filename,
        const std::function<void(const std::error_code&)>& handler);

    /**
     * @brief Load the stack from file using C++20 coroutines
     * @param filename The name of the file to load from
     * @return A task that completes when the operation is done
     */
    [[nodiscard]] auto loadStackFromFile(const std::string& filename)
        -> Task<void>;

    /**
     * @brief Get the size of the directory stack.
     * @return The number of directories in the stack.
     */
    [[nodiscard]] auto size() const noexcept -> size_t;

    /**
     * @brief Check if the directory stack is empty.
     * @return True if the stack is empty, false otherwise.
     */
    [[nodiscard]] auto isEmpty() const noexcept -> bool;

    /**
     * @brief Get the current directory path asynchronously.
     * @param handler The completion handler to be called with the current
     * directory path.
     */
    void asyncGetCurrentDirectory(
        const std::function<void(const std::filesystem::path&)>& handler) const;

    /**
     * @brief Get the current directory path using C++20 coroutines
     * @return A task that resolves to the current directory path
     */
    [[nodiscard]] auto getCurrentDirectory() const
        -> Task<std::filesystem::path>;

private:
    std::unique_ptr<DirectoryStackImpl>
        impl_;  ///< Pointer to the implementation.
};
}  // namespace atom::io

#endif  // ATOM_IO_PUSHD_HPP
