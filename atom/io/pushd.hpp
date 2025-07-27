#ifndef ATOM_IO_PUSHD_HPP
#define ATOM_IO_PUSHD_HPP

#include <atomic>
#include <chrono>
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

// Forward declarations
struct DirectoryStackOptions;
struct DirectoryStackStats;

// Callback types
using ProgressCallback = std::function<void(size_t processed, size_t total, double percentage)>;
using StackChangeCallback = std::function<void(const std::filesystem::path& old_dir, const std::filesystem::path& new_dir)>;

template <typename T>
concept PathLike = std::convertible_to<T, std::filesystem::path>;

/**
 * @brief Enhanced options for directory stack operations
 */
struct DirectoryStackOptions {
    bool enable_logging{true};          ///< Enable detailed logging
    bool enable_statistics{false};      ///< Enable performance statistics
    bool enable_validation{true};       ///< Enable path validation
    bool enable_history{false};         ///< Enable operation history
    size_t max_stack_size{100};         ///< Maximum stack size
    size_t max_history_size{50};        ///< Maximum history size
    std::chrono::milliseconds timeout{30000}; ///< Operation timeout

    // Callbacks
    StackChangeCallback change_callback; ///< Directory change callback

    /**
     * @brief Creates options optimized for performance
     */
    static DirectoryStackOptions createFastOptions() {
        DirectoryStackOptions options;
        options.enable_logging = false;
        options.enable_statistics = false;
        options.enable_validation = false;
        options.enable_history = false;
        return options;
    }

    /**
     * @brief Creates options for comprehensive operations
     */
    static DirectoryStackOptions createDetailedOptions() {
        DirectoryStackOptions options;
        options.enable_logging = true;
        options.enable_statistics = true;
        options.enable_validation = true;
        options.enable_history = true;
        return options;
    }
};

/**
 * @brief Statistics for directory stack operations
 */
struct DirectoryStackStats {
    std::atomic<uint64_t> pushd_operations{0};
    std::atomic<uint64_t> popd_operations{0};
    std::atomic<uint64_t> failed_operations{0};
    std::atomic<uint64_t> validation_failures{0};
    std::chrono::steady_clock::time_point start_time;

    void reset() {
        pushd_operations = 0;
        popd_operations = 0;
        failed_operations = 0;
        validation_failures = 0;
        start_time = std::chrono::steady_clock::now();
    }

    double getOperationsPerSecond() const {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
        if (duration.count() > 0) {
            auto total_ops = pushd_operations.load() + popd_operations.load();
            return static_cast<double>(total_ops) / duration.count();
        }
        return 0.0;
    }
};

class DirectoryStack {
public:
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
    static inline asio::io_context::executor_type executor_;
#endif

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

#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
            auto get_executor() { return DirectoryStack::executor_; }
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

    /**
     * @brief Constructs a DirectoryStack with optional async executor
     * @param io_context IO context for async operations (nullptr if sync only)
     */
    explicit DirectoryStack(
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        asio::io_context& io_context
#else
        void* io_context = nullptr
#endif
    );

    ~DirectoryStack() noexcept;

    DirectoryStack(const DirectoryStack& other) = delete;
    auto operator=(const DirectoryStack& other) -> DirectoryStack& = delete;

    DirectoryStack(DirectoryStack&& other) noexcept;
    auto operator=(DirectoryStack&& other) noexcept -> DirectoryStack&;

    /**
     * @brief Asynchronously push current directory and change to new directory
     * @tparam P Path-like type
     * @param new_dir Directory to change to
     * @param handler Completion handler
     */
    template <PathLike P>
    void asyncPushd(const P& new_dir,
                    const std::function<void(const std::error_code&)>& handler);

    /**
     * @brief Coroutine version of pushd
     * @tparam P Path-like type
     * @param new_dir Directory to change to
     * @return Task for completion
     */
    template <PathLike P>
    [[nodiscard]] auto pushd(const P& new_dir) -> Task<void>;

    /**
     * @brief Asynchronously pop directory from stack and change to it
     * @param handler Completion handler
     */
    void asyncPopd(const std::function<void(const std::error_code&)>& handler);

    /**
     * @brief Coroutine version of popd
     * @return Task for completion
     */
    [[nodiscard]] auto popd() -> Task<void>;

    /**
     * @brief Get top directory from stack without popping
     * @return Top directory path
     */
    [[nodiscard]] auto peek() const -> std::filesystem::path;

    /**
     * @brief Get all directories in stack
     * @return Vector of directory paths
     */
    [[nodiscard]] auto dirs() const noexcept -> Vector<std::filesystem::path>;

    /**
     * @brief Clear all directories from stack
     */
    void clear() noexcept;

    /**
     * @brief Swap two directories in stack
     * @param index1 First index
     * @param index2 Second index
     */
    void swap(size_t index1, size_t index2);

    /**
     * @brief Remove directory at index
     * @param index Index to remove
     */
    void remove(size_t index);

    /**
     * @brief Asynchronously go to directory at index
     * @param index Stack index
     * @param handler Completion handler
     */
    void asyncGotoIndex(
        size_t index,
        const std::function<void(const std::error_code&)>& handler);

    /**
     * @brief Coroutine version of goto index
     * @param index Stack index
     * @return Task for completion
     */
    [[nodiscard]] auto gotoIndex(size_t index) -> Task<void>;

    /**
     * @brief Asynchronously save stack to file
     * @param filename File to save to
     * @param handler Completion handler
     */
    void asyncSaveStackToFile(
        const String& filename,
        const std::function<void(const std::error_code&)>& handler);

    /**
     * @brief Coroutine version of save stack to file
     * @param filename File to save to
     * @return Task for completion
     */
    [[nodiscard]] auto saveStackToFile(const String& filename) -> Task<void>;

    /**
     * @brief Asynchronously load stack from file
     * @param filename File to load from
     * @param handler Completion handler
     */
    void asyncLoadStackFromFile(
        const String& filename,
        const std::function<void(const std::error_code&)>& handler);

    /**
     * @brief Coroutine version of load stack from file
     * @param filename File to load from
     * @return Task for completion
     */
    [[nodiscard]] auto loadStackFromFile(const String& filename) -> Task<void>;

    /**
     * @brief Get stack size
     * @return Number of directories in stack
     */
    [[nodiscard]] auto size() const noexcept -> size_t;

    /**
     * @brief Check if stack is empty
     * @return True if empty
     */
    [[nodiscard]] auto isEmpty() const noexcept -> bool;

    /**
     * @brief Asynchronously get current directory
     * @param handler Completion handler
     */
    void asyncGetCurrentDirectory(
        const std::function<void(const std::filesystem::path&,
                                 const std::error_code&)>& handler) const;

    /**
     * @brief Coroutine version of get current directory
     * @return Task with current directory path
     */
    [[nodiscard]] auto getCurrentDirectory() const
        -> Task<std::filesystem::path>;

    // Enhanced methods with options and statistics

    /**
     * @brief Set options for directory stack operations
     * @param options New options to apply
     */
    void setOptions(const DirectoryStackOptions& options);

    /**
     * @brief Get current options
     * @return Current options
     */
    [[nodiscard]] auto getOptions() const -> DirectoryStackOptions;

    /**
     * @brief Get operation statistics
     * @return Current statistics
     */
    [[nodiscard]] auto getStats() const -> DirectoryStackStats;

    /**
     * @brief Reset operation statistics
     */
    void resetStats();

    /**
     * @brief Get operation history
     * @return Vector of recent operations
     */
    [[nodiscard]] auto getHistory() const -> Vector<std::string>;

    /**
     * @brief Validate stack integrity
     * @return True if stack is valid
     */
    [[nodiscard]] auto validateStack() const -> bool;

    /**
     * @brief Batch push multiple directories
     * @param directories Vector of directories to push
     * @param progress_callback Progress callback
     * @return Task for completion
     */
    [[nodiscard]] auto batchPushd(const Vector<std::filesystem::path>& directories,
                                 ProgressCallback progress_callback = nullptr) -> Task<void>;

    /**
     * @brief Find directory in stack
     * @param path Directory path to find
     * @return Index if found, -1 otherwise
     */
    [[nodiscard]] auto findDirectory(const std::filesystem::path& path) const -> int;

    /**
     * @brief Get stack as JSON string
     * @return JSON representation of stack
     */
    [[nodiscard]] auto toJson() const -> std::string;

    /**
     * @brief Load stack from JSON string
     * @param json JSON string to load from
     * @return True if successful
     */
    auto fromJson(const std::string& json) -> bool;

private:
    std::unique_ptr<DirectoryStackImpl> impl_;
};

template <>
class DirectoryStack::Task<void> {
public:
    struct promise_type {
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
        auto get_executor() { return DirectoryStack::executor_; }
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
