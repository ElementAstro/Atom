/**
 * @file uv_coro.hpp
 * @brief Modern C++ coroutine wrapper for libuv with enhanced features
 * @version 2.0
 * @author Atom Framework Team
 */

#ifndef ATOM_EXTRA_UV_CORO_HPP
#define ATOM_EXTRA_UV_CORO_HPP

#include <uv.h>
#include <coroutine>
#include <exception>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <optional>
#include <span>
#include <cstring>
#include <algorithm>
#include <functional>

namespace uv_coro {

// Forward declarations
template <typename T = void>
class Task;

template <typename T = void>
class Generator;

class Scheduler;
class ConnectionPool;
class TimeoutAwaiter;
class TcpConnectAwaiter;
class TcpReadAwaiter;
class TcpWriteAwaiter;
class UdpSendAwaiter;
class UdpReceiveAwaiter;
class FileOpenAwaiter;
class FileReadAwaiter;
class FileWriteAwaiter;
class FileCloseAwaiter;
class ProcessAwaiter;
class HttpServerAwaiter;
class WebSocketAwaiter;

/**
 * @class UvError
 * @brief Enhanced exception class for libuv errors with context
 */
class UvError : public std::runtime_error {
public:
    explicit UvError(int err, const std::string& context = "")
        : std::runtime_error(format_error(err, context)),
          error_code_(err),
          context_(context) {}

    int error_code() const { return error_code_; }
    const std::string& context() const { return context_; }

    bool is_recoverable() const {
        return error_code_ == UV_EAGAIN || error_code_ == UV_EBUSY ||
               error_code_ == UV_ETIMEDOUT;
    }

private:
    int error_code_;
    std::string context_;

    static std::string format_error(int err, const std::string& context) {
        std::string msg = uv_strerror(err);
        if (!context.empty()) {
            msg += " (context: " + context + ")";
        }
        return msg;
    }
};

/**
 * @class ResourceManager
 * @brief RAII wrapper for libuv resources
 */
template<typename T>
class ResourceManager {
public:
    using DeleterFunc = std::function<void(T*)>;

    ResourceManager(T* resource, DeleterFunc deleter)
        : resource_(resource), deleter_(std::move(deleter)) {}

    ~ResourceManager() {
        if (resource_ && deleter_) {
            deleter_(resource_);
        }
    }

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    ResourceManager(ResourceManager&& other) noexcept
        : resource_(other.resource_), deleter_(std::move(other.deleter_)) {
        other.resource_ = nullptr;
    }

    ResourceManager& operator=(ResourceManager&& other) noexcept {
        if (this != &other) {
            if (resource_ && deleter_) {
                deleter_(resource_);
            }
            resource_ = other.resource_;
            deleter_ = std::move(other.deleter_);
            other.resource_ = nullptr;
        }
        return *this;
    }

    T* get() const { return resource_; }
    T* release() {
        T* temp = resource_;
        resource_ = nullptr;
        return temp;
    }

    explicit operator bool() const { return resource_ != nullptr; }
    T* operator->() const { return resource_; }
    T& operator*() const { return *resource_; }

private:
    T* resource_;
    DeleterFunc deleter_;
};

struct FinalAwaiter {
    bool await_ready() const noexcept { return false; }

    template <typename Promise>
    std::coroutine_handle<> await_suspend(
        std::coroutine_handle<Promise> h) noexcept {
        // If we have a continuation, resume it
        Promise& promise = h.promise();
        if (promise.continuation_) {
            return promise.continuation_;
        }
        // Otherwise return to caller
        return std::noop_coroutine();
    }

    void await_resume() noexcept {}
};

/**
 * @class Task
 * @brief Represents a coroutine task that can be awaited
 */
template <typename T>
class Task {
public:
    // Promise type required by the coroutine machinery
    class promise_type {
    public:
        Task get_return_object() {
            return Task(
                std::coroutine_handle<promise_type>::from_promise(*this));
        }

        std::suspend_never initial_suspend() { return {}; }

        auto final_suspend() noexcept { return FinalAwaiter{}; }

        void return_value(T value) {
            result_ = std::move(value);
            is_ready_ = true;
        }

        void unhandled_exception() {
            exception_ = std::current_exception();
            is_ready_ = true;
        }

        T& result() & {
            if (exception_) {
                std::rethrow_exception(exception_);
            }
            return result_;
        }

        T&& result() && {
            if (exception_) {
                std::rethrow_exception(exception_);
            }
            return std::move(result_);
        }

        void set_continuation(std::coroutine_handle<> handle) {
            continuation_ = handle;
        }

        bool is_ready() const { return is_ready_; }

    private:
        T result_;
        std::exception_ptr exception_;
        std::coroutine_handle<> continuation_;
        bool is_ready_ = false;

        friend struct FinalAwaiter;
    };

    // Awaiter class for co_await
    class Awaiter {
    public:
        explicit Awaiter(Task& task) : task_(task) {}

        bool await_ready() const { return task_.is_ready(); }

        void await_suspend(std::coroutine_handle<> handle) {
            task_.handle_.promise().set_continuation(handle);
        }

        T await_resume() { return std::move(task_.handle_.promise().result()); }

    private:
        Task& task_;
    };

    explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}

    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    // Disallow copying
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // Allow moving
    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    // Make the task co_awaitable
    Awaiter operator co_await() { return Awaiter(*this); }

    // Check if the task is completed
    bool is_ready() const { return handle_.done(); }

    // Get the result
    T get_result() { return std::move(handle_.promise().result()); }

private:
    std::coroutine_handle<promise_type> handle_;
};

// Task<void> 特化版本修复
template <>
class Task<void> {
public:
    // Promise type required by the coroutine machinery
    class promise_type {
    public:
        Task<void> get_return_object() {
            return Task<void>(
                std::coroutine_handle<promise_type>::from_promise(*this));
        }

        std::suspend_never initial_suspend() { return {}; }

        auto final_suspend() noexcept { return FinalAwaiter{}; }

        void return_void() { is_ready_ = true; }

        void unhandled_exception() {
            exception_ = std::current_exception();
            is_ready_ = true;
        }

        void result() {
            if (exception_) {
                std::rethrow_exception(exception_);
            }
        }

        void set_continuation(std::coroutine_handle<> handle) {
            continuation_ = handle;
        }

        bool is_ready() const { return is_ready_; }

    private:
        std::exception_ptr exception_;
        std::coroutine_handle<> continuation_;
        bool is_ready_ = false;

        friend struct FinalAwaiter;
    };

    // Awaiter class for co_await
    class Awaiter {
    public:
        explicit Awaiter(Task& task) : task_(task) {}

        bool await_ready() const { return task_.is_ready(); }

        void await_suspend(std::coroutine_handle<> handle) {
            task_.handle_.promise().set_continuation(handle);
        }

        void await_resume() { task_.handle_.promise().result(); }

    private:
        Task& task_;
    };

    explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}

    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    // Disallow copying
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // Allow moving
    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    Awaiter operator co_await() { return Awaiter(*this); }

    bool is_ready() const { return handle_.done(); }

    void get_result() { handle_.promise().result(); }

private:
    std::coroutine_handle<promise_type> handle_;
};

/**
 * @class Scheduler
 * @brief Controls the libuv event loop for coroutines
 */
class Scheduler {
public:
    explicit Scheduler(uv_loop_t* loop = nullptr)
        : loop_(loop ? loop : uv_default_loop()) {}

    uv_loop_t* get_loop() const { return loop_; }

    void run() { uv_run(loop_, UV_RUN_DEFAULT); }

    void run_once() { uv_run(loop_, UV_RUN_ONCE); }

    void stop() { uv_stop(loop_); }

private:
    uv_loop_t* loop_;
};

/**
 * @class TimeoutAwaiter
 * @brief Awaiter for sleeping or timeout operations
 */
class TimeoutAwaiter {
public:
    TimeoutAwaiter(uv_loop_t* loop, uint64_t timeout_ms)
        : loop_(loop), timeout_ms_(timeout_ms), timer_(new uv_timer_t) {
        uv_timer_init(loop_, timer_);
        timer_->data = this;
    }

    ~TimeoutAwaiter() {
        uv_close(reinterpret_cast<uv_handle_t*>(timer_),
                 [](uv_handle_t* handle) {
                     delete reinterpret_cast<uv_timer_t*>(handle);
                 });
    }

    bool await_ready() const { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        continuation_ = handle;
        uv_timer_start(timer_, timer_callback, timeout_ms_, 0);
    }

    void await_resume() {}

private:
    static void timer_callback(uv_timer_t* timer) {
        auto self = static_cast<TimeoutAwaiter*>(timer->data);
        self->continuation_.resume();
    }

    uv_loop_t* loop_;
    uint64_t timeout_ms_;
    uv_timer_t* timer_;
    std::coroutine_handle<> continuation_;
};

/**
 * @class TcpConnectAwaiter
 * @brief Awaiter for TCP connection operations
 */
class TcpConnectAwaiter {
public:
    TcpConnectAwaiter(uv_loop_t* loop, const std::string& host, int port)
        : loop_(loop),
          host_(host),
          port_(port),
          tcp_handle_(new uv_tcp_t),
          connect_req_(new uv_connect_t),
          result_(0) {
        uv_tcp_init(loop_, tcp_handle_);
        tcp_handle_->data = this;
        connect_req_->data = this;
    }

    ~TcpConnectAwaiter() {
        if (tcp_handle_) {
            if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(tcp_handle_))) {
                uv_close(reinterpret_cast<uv_handle_t*>(tcp_handle_),
                         [](uv_handle_t* handle) {
                             delete reinterpret_cast<uv_tcp_t*>(handle);
                         });
            }
        }
        delete connect_req_;
    }

    bool await_ready() const { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        continuation_ = handle;

        struct sockaddr_in addr;
        uv_ip4_addr(host_.c_str(), port_, &addr);

        result_ = uv_tcp_connect(connect_req_, tcp_handle_,
                                 reinterpret_cast<const sockaddr*>(&addr),
                                 connect_callback);

        if (result_ != 0) {
            // Connection failed, resume immediately
            continuation_.resume();
        }
    }

    uv_tcp_t* await_resume() {
        if (result_ != 0) {
            throw UvError(result_);
        }
        auto* handle = tcp_handle_;
        tcp_handle_ = nullptr;  // Transfer ownership
        return handle;
    }

private:
    static void connect_callback(uv_connect_t* req, int status) {
        auto self = static_cast<TcpConnectAwaiter*>(req->data);
        self->result_ = status;
        self->continuation_.resume();
    }

    uv_loop_t* loop_;
    std::string host_;
    int port_;
    uv_tcp_t* tcp_handle_;
    uv_connect_t* connect_req_;
    int result_;
    std::coroutine_handle<> continuation_;
};

/**
 * @class TcpReadAwaiter
 * @brief Awaiter for TCP read operations
 */
class TcpReadAwaiter {
public:
    TcpReadAwaiter(uv_tcp_t* tcp) : tcp_(tcp), result_(-1), data_len_(0) {
        tcp_->data = this;
    }

    bool await_ready() const { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        continuation_ = handle;

        result_ = uv_read_start(reinterpret_cast<uv_stream_t*>(tcp_),
                                alloc_callback, read_callback);

        if (result_ != 0) {
            // Read failed to start, resume immediately
            continuation_.resume();
        }
    }

    std::string await_resume() {
        if (result_ < 0 && result_ != UV_EOF) {
            throw UvError(result_);
        }
        return std::move(data_);
    }

private:
    static void alloc_callback(uv_handle_t* /*handle*/, size_t suggested_size,
                               uv_buf_t* buf) {
        buf->base = new char[suggested_size];
        buf->len = suggested_size;
    }

    static void read_callback(uv_stream_t* stream, ssize_t nread,
                              const uv_buf_t* buf) {
        auto self = static_cast<TcpReadAwaiter*>(stream->data);

        if (nread > 0) {
            self->data_.append(buf->base, nread);
            self->data_len_ += nread;
            self->result_ = 0;
        } else {
            // EOF or error
            self->result_ = nread;
        }

        // Stop reading
        uv_read_stop(stream);

        // Free buffer
        delete[] buf->base;

        // Resume coroutine
        self->continuation_.resume();
    }

    uv_tcp_t* tcp_;
    int result_;
    std::string data_;
    size_t data_len_;
    std::coroutine_handle<> continuation_;
};

/**
 * @class TcpWriteAwaiter
 * @brief Awaiter for TCP write operations
 */
class TcpWriteAwaiter {
public:
    TcpWriteAwaiter(uv_tcp_t* tcp, const std::string& data)
        : tcp_(tcp),
          data_(new char[data.size()]),
          data_len_(data.size()),
          result_(0) {
        memcpy(data_, data.c_str(), data_len_);
        write_req_.data = this;
    }

    ~TcpWriteAwaiter() { delete[] data_; }

    bool await_ready() const { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        continuation_ = handle;

        buf_ = uv_buf_init(data_, data_len_);

        result_ = uv_write(&write_req_, reinterpret_cast<uv_stream_t*>(tcp_),
                           &buf_, 1, write_callback);

        if (result_ != 0) {
            // Write failed to start, resume immediately
            continuation_.resume();
        }
    }

    void await_resume() {
        if (result_ != 0) {
            throw UvError(result_);
        }
    }

private:
    static void write_callback(uv_write_t* req, int status) {
        auto self = static_cast<TcpWriteAwaiter*>(req->data);
        self->result_ = status;
        self->continuation_.resume();
    }

    uv_tcp_t* tcp_;
    char* data_;
    size_t data_len_;
    uv_buf_t buf_;
    uv_write_t write_req_;
    int result_;
    std::coroutine_handle<> continuation_;
};

/**
 * @class FileOpenAwaiter
 * @brief Awaiter for file open operations
 */
class FileOpenAwaiter {
public:
    FileOpenAwaiter(uv_loop_t* loop, const std::string& path, int flags,
                    int mode)
        : loop_(loop), path_(path), flags_(flags), mode_(mode), result_(-1) {
        open_req_.data = this;
    }

    bool await_ready() const { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        continuation_ = handle;

        result_ = uv_fs_open(loop_, &open_req_, path_.c_str(), flags_, mode_,
                             open_callback);

        if (result_ < 0) {
            // Open failed, resume immediately
            continuation_.resume();
        }
    }

    uv_file await_resume() {
        if (result_ < 0) {
            throw UvError(result_);
        }
        return open_req_.result;
    }

private:
    static void open_callback(uv_fs_t* req) {
        auto self = static_cast<FileOpenAwaiter*>(req->data);
        self->result_ = req->result;
        uv_fs_req_cleanup(req);
        self->continuation_.resume();
    }

    uv_loop_t* loop_;
    std::string path_;
    int flags_;
    int mode_;
    uv_fs_t open_req_;
    int result_;
    std::coroutine_handle<> continuation_;
};

/**
 * @class FileReadAwaiter
 * @brief Awaiter for file read operations
 */
class FileReadAwaiter {
public:
    FileReadAwaiter(uv_loop_t* loop, uv_file file, size_t buffer_size)
        : loop_(loop),
          file_(file),
          buffer_(new char[buffer_size]),
          buffer_size_(buffer_size),
          result_(-1) {
        read_req_.data = this;
    }

    ~FileReadAwaiter() { delete[] buffer_; }

    bool await_ready() const { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        continuation_ = handle;

        uv_buf_t buf = uv_buf_init(buffer_, buffer_size_);

        result_ =
            uv_fs_read(loop_, &read_req_, file_, &buf, 1, -1, read_callback);

        if (result_ < 0) {
            // Read failed, resume immediately
            continuation_.resume();
        }
    }

    std::string await_resume() {
        if (result_ < 0) {
            throw UvError(result_);
        }
        return std::string(buffer_, read_req_.result);
    }

private:
    static void read_callback(uv_fs_t* req) {
        auto self = static_cast<FileReadAwaiter*>(req->data);
        self->result_ = req->result;
        uv_fs_req_cleanup(req);
        self->continuation_.resume();
    }

    uv_loop_t* loop_;
    uv_file file_;
    char* buffer_;
    size_t buffer_size_;
    uv_fs_t read_req_;
    int result_;
    std::coroutine_handle<> continuation_;
};

/**
 * @class FileWriteAwaiter
 * @brief Awaiter for file write operations
 */
class FileWriteAwaiter {
public:
    FileWriteAwaiter(uv_loop_t* loop, uv_file file, const std::string& data)
        : loop_(loop),
          file_(file),
          buffer_(new char[data.size()]),
          buffer_size_(data.size()),
          result_(-1) {
        memcpy(buffer_, data.c_str(), buffer_size_);
        write_req_.data = this;
    }

    ~FileWriteAwaiter() { delete[] buffer_; }

    bool await_ready() const { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        continuation_ = handle;

        uv_buf_t buf = uv_buf_init(buffer_, buffer_size_);

        result_ =
            uv_fs_write(loop_, &write_req_, file_, &buf, 1, -1, write_callback);

        if (result_ < 0) {
            // Write failed, resume immediately
            continuation_.resume();
        }
    }

    size_t await_resume() {
        if (result_ < 0) {
            throw UvError(result_);
        }
        return write_req_.result;
    }

private:
    static void write_callback(uv_fs_t* req) {
        auto self = static_cast<FileWriteAwaiter*>(req->data);
        self->result_ = req->result;
        uv_fs_req_cleanup(req);
        self->continuation_.resume();
    }

    uv_loop_t* loop_;
    uv_file file_;
    char* buffer_;
    size_t buffer_size_;
    uv_fs_t write_req_;
    int result_;
    std::coroutine_handle<> continuation_;
};

/**
 * @class FileCloseAwaiter
 * @brief Awaiter for file close operations
 */
class FileCloseAwaiter {
public:
    FileCloseAwaiter(uv_loop_t* loop, uv_file file)
        : loop_(loop), file_(file), result_(-1) {
        close_req_.data = this;
    }

    bool await_ready() const { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        continuation_ = handle;

        result_ = uv_fs_close(loop_, &close_req_, file_, close_callback);

        if (result_ < 0) {
            // Close failed, resume immediately
            continuation_.resume();
        }
    }

    void await_resume() {
        if (result_ < 0) {
            throw UvError(result_);
        }
    }

private:
    static void close_callback(uv_fs_t* req) {
        auto self = static_cast<FileCloseAwaiter*>(req->data);
        self->result_ = req->result;
        uv_fs_req_cleanup(req);
        self->continuation_.resume();
    }

    uv_loop_t* loop_;
    uv_file file_;
    uv_fs_t close_req_;
    int result_;
    std::coroutine_handle<> continuation_;
};

/**
 * @class TcpClient
 * @brief High-level TCP client with coroutine-based interface
 */
class TcpClient {
public:
    explicit TcpClient(uv_loop_t* loop) : loop_(loop), tcp_(nullptr) {}

    ~TcpClient() {
        if (tcp_ && !uv_is_closing(reinterpret_cast<uv_handle_t*>(tcp_))) {
            uv_close(reinterpret_cast<uv_handle_t*>(tcp_),
                     [](uv_handle_t* handle) {
                         delete reinterpret_cast<uv_tcp_t*>(handle);
                     });
        }
    }

    Task<void> connect(const std::string& host, int port) {
        if (tcp_ && !uv_is_closing(reinterpret_cast<uv_handle_t*>(tcp_))) {
            uv_close(reinterpret_cast<uv_handle_t*>(tcp_),
                     [](uv_handle_t* handle) {
                         delete reinterpret_cast<uv_tcp_t*>(handle);
                     });
            tcp_ = nullptr;
        }

        tcp_ = co_await TcpConnectAwaiter(loop_, host, port);
    }

    Task<std::string> read() {
        if (!tcp_) {
            throw UvError(UV_EBADF);
        }

        std::string result = co_await TcpReadAwaiter(tcp_);
        co_return result;
    }

    Task<void> write(const std::string& data) {
        if (!tcp_) {
            throw UvError(UV_EBADF);
        }

        co_await TcpWriteAwaiter(tcp_, data);
    }

    void close() {
        if (tcp_ && !uv_is_closing(reinterpret_cast<uv_handle_t*>(tcp_))) {
            uv_close(reinterpret_cast<uv_handle_t*>(tcp_),
                     [](uv_handle_t* handle) {
                         delete reinterpret_cast<uv_tcp_t*>(handle);
                     });
            tcp_ = nullptr;
        }
    }

    uv_tcp_t* get_handle() const { return tcp_; }

private:
    uv_loop_t* loop_;
    uv_tcp_t* tcp_;
};

/**
 * @class FileSystem
 * @brief High-level file system operations with coroutine-based interface
 */
class FileSystem {
public:
    explicit FileSystem(uv_loop_t* loop) : loop_(loop) {}

    Task<std::string> read_file(const std::string& path) {
        uv_file file = co_await FileOpenAwaiter(loop_, path, O_RDONLY, 0);

        std::string content;
        const size_t buffer_size = 4096;

        try {
            while (true) {
                std::string chunk =
                    co_await FileReadAwaiter(loop_, file, buffer_size);
                if (chunk.empty()) {
                    break;  // EOF
                }
                content += chunk;
            }

            co_await FileCloseAwaiter(loop_, file);
        } catch (...) {
            // Ensure file is closed on error
            uv_fs_t close_req;
            uv_fs_close(loop_, &close_req, file, nullptr);
            uv_fs_req_cleanup(&close_req);
            throw;  // Re-throw the original exception
        }

        co_return content;
    }

    Task<void> write_file(const std::string& path, const std::string& content) {
        uv_file file = co_await FileOpenAwaiter(
            loop_, path, O_WRONLY | O_CREAT | O_TRUNC, 0666);

        co_await FileWriteAwaiter(loop_, file, content);
        co_await FileCloseAwaiter(loop_, file);
    }

    Task<void> append_file(const std::string& path,
                           const std::string& content) {
        uv_file file = co_await FileOpenAwaiter(
            loop_, path, O_WRONLY | O_CREAT | O_APPEND, 0666);

        co_await FileWriteAwaiter(loop_, file, content);
        co_await FileCloseAwaiter(loop_, file);
    }

private:
    uv_loop_t* loop_;
};

/**
 * @class ConnectionPool
 * @brief Connection pool for TCP connections with automatic management
 */
class ConnectionPool {
public:
    struct PoolConfig {
        size_t max_connections = 10;
        std::chrono::seconds idle_timeout{30};
        std::chrono::seconds connect_timeout{5};
        bool enable_keepalive = true;
    };

    explicit ConnectionPool(uv_loop_t* loop, const PoolConfig& config = {})
        : loop_(loop), config_(config), shutdown_(false) {
        cleanup_timer_ = std::make_unique<uv_timer_t>();
        uv_timer_init(loop_, cleanup_timer_.get());
        cleanup_timer_->data = this;

        // Start cleanup timer
        uv_timer_start(cleanup_timer_.get(), cleanup_callback,
                      config_.idle_timeout.count() * 1000,
                      config_.idle_timeout.count() * 1000);
    }

    ~ConnectionPool() {
        shutdown();
    }

    Task<uv_tcp_t*> get_connection(const std::string& host, int port) {
        std::string key = host + ":" + std::to_string(port);

        std::lock_guard<std::mutex> lock(pool_mutex_);

        auto it = connections_.find(key);
        if (it != connections_.end() && !it->second.empty()) {
            auto conn = std::move(it->second.front());
            it->second.pop();

            // Verify connection is still valid
            if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(conn.get()))) {
                co_return conn.release();
            }
        }

        // Create new connection
        if (active_connections_[key] >= config_.max_connections) {
            throw UvError(UV_EBUSY, "Connection pool exhausted for " + key);
        }

        active_connections_[key]++;

        try {
            uv_tcp_t* tcp = co_await TcpConnectAwaiter(loop_, host, port);
            co_return tcp;
        } catch (...) {
            active_connections_[key]--;
            throw;
        }
    }

    void return_connection(const std::string& host, int port, uv_tcp_t* tcp) {
        if (!tcp || uv_is_closing(reinterpret_cast<uv_handle_t*>(tcp))) {
            return;
        }

        std::string key = host + ":" + std::to_string(port);

        std::lock_guard<std::mutex> lock(pool_mutex_);

        if (connections_[key].size() < config_.max_connections / 2) {
            auto managed_tcp = std::unique_ptr<uv_tcp_t, std::function<void(uv_tcp_t*)>>(
                tcp, [](uv_tcp_t* t) {
                    if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(t))) {
                        uv_close(reinterpret_cast<uv_handle_t*>(t),
                                [](uv_handle_t* handle) {
                                    delete reinterpret_cast<uv_tcp_t*>(handle);
                                });
                    }
                });

            connections_[key].push(std::move(managed_tcp));
            last_used_[key] = std::chrono::steady_clock::now();
        } else {
            // Pool is full, close connection
            uv_close(reinterpret_cast<uv_handle_t*>(tcp),
                     [](uv_handle_t* handle) {
                         delete reinterpret_cast<uv_tcp_t*>(handle);
                     });
        }

        active_connections_[key]--;
    }

    void shutdown() {
        shutdown_ = true;

        if (cleanup_timer_) {
            uv_timer_stop(cleanup_timer_.get());
            uv_close(reinterpret_cast<uv_handle_t*>(cleanup_timer_.get()), nullptr);
        }

        std::lock_guard<std::mutex> lock(pool_mutex_);
        connections_.clear();
        active_connections_.clear();
        last_used_.clear();
    }

private:
    static void cleanup_callback(uv_timer_t* timer) {
        auto* pool = static_cast<ConnectionPool*>(timer->data);
        pool->cleanup_idle_connections();
    }

    void cleanup_idle_connections() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(pool_mutex_);

        for (auto it = last_used_.begin(); it != last_used_.end();) {
            if (now - it->second > config_.idle_timeout) {
                connections_.erase(it->first);
                active_connections_.erase(it->first);
                it = last_used_.erase(it);
            } else {
                ++it;
            }
        }
    }

    uv_loop_t* loop_;
    PoolConfig config_;
    std::atomic<bool> shutdown_;
    std::unique_ptr<uv_timer_t> cleanup_timer_;

    std::mutex pool_mutex_;
    std::unordered_map<std::string, std::queue<std::unique_ptr<uv_tcp_t, std::function<void(uv_tcp_t*)>>>> connections_;
    std::unordered_map<std::string, size_t> active_connections_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_used_;
};

/**
 * @class HttpClient
 * @brief Enhanced HTTP client with connection pooling and better error handling
 */
class HttpClient {
public:
    struct HttpResponse {
        int status_code = 0;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
        std::chrono::milliseconds response_time{0};
    };

    struct HttpRequest {
        std::string method = "GET";
        std::string url;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
        std::chrono::seconds timeout{30};
    };

    explicit HttpClient(uv_loop_t* loop)
        : loop_(loop), connection_pool_(std::make_unique<ConnectionPool>(loop)) {}

    Task<HttpResponse> request(const HttpRequest& req) {
        auto start_time = std::chrono::steady_clock::now();

        // Parse URL
        std::string host;
        std::string path = "/";
        int port = 80;
        bool use_ssl = false;

        // Simple URL parsing
        size_t protocol_end = req.url.find("://");
        if (protocol_end != std::string::npos) {
            std::string protocol = req.url.substr(0, protocol_end);
            if (protocol == "https") {
                use_ssl = true;
                port = 443;
            }
            protocol_end += 3;  // Skip "://"
        } else {
            protocol_end = 0;
        }

        size_t path_start = req.url.find("/", protocol_end);
        if (path_start != std::string::npos) {
            host = req.url.substr(protocol_end, path_start - protocol_end);
            path = req.url.substr(path_start);
        } else {
            host = req.url.substr(protocol_end);
        }

        // Check for port
        size_t port_start = host.find(":");
        if (port_start != std::string::npos) {
            port = std::stoi(host.substr(port_start + 1));
            host = host.substr(0, port_start);
        }

        if (use_ssl) {
            throw std::runtime_error("HTTPS not implemented in this example");
        }

        // Get connection from pool
        uv_tcp_t* tcp = nullptr;
        try {
            tcp = co_await connection_pool_->get_connection(host, port);

            // Build HTTP request
            std::string request_str = req.method + " " + path + " HTTP/1.1\r\n";
            request_str += "Host: " + host + "\r\n";

            for (const auto& [key, value] : req.headers) {
                request_str += key + ": " + value + "\r\n";
            }

            if (!req.body.empty()) {
                request_str += "Content-Length: " + std::to_string(req.body.size()) + "\r\n";
            }

            request_str += "Connection: keep-alive\r\n\r\n";
            request_str += req.body;

            // Send request
            co_await TcpWriteAwaiter(tcp, request_str);

            // Read response
            std::string response_text;
            while (true) {
                try {
                    std::string chunk = co_await TcpReadAwaiter(tcp);
                    if (chunk.empty()) {
                        break;
                    }
                    response_text += chunk;
                } catch (const UvError& e) {
                    if (e.error_code() == UV_EOF) {
                        break;
                    }
                    throw;
                }
            }

            // Return connection to pool
            connection_pool_->return_connection(host, port, tcp);

            // Parse response
            HttpResponse response;
            auto end_time = std::chrono::steady_clock::now();
            response.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time);

            size_t header_end = response_text.find("\r\n\r\n");
            if (header_end == std::string::npos) {
                throw std::runtime_error("Invalid HTTP response");
            }

            std::string headers_text = response_text.substr(0, header_end);
            response.body = response_text.substr(header_end + 4);

            // Parse status line
            size_t first_line_end = headers_text.find("\r\n");
            if (first_line_end != std::string::npos) {
                std::string status_line = headers_text.substr(0, first_line_end);
                size_t space1 = status_line.find(" ");
                if (space1 != std::string::npos) {
                    size_t space2 = status_line.find(" ", space1 + 1);
                    if (space2 != std::string::npos) {
                        response.status_code = std::stoi(status_line.substr(
                            space1 + 1, space2 - space1 - 1));
                    }
                }
            }

            // Parse headers
            size_t pos = first_line_end + 2;
            while (pos < headers_text.size()) {
                size_t line_end = headers_text.find("\r\n", pos);
                if (line_end == std::string::npos) {
                    line_end = headers_text.size();
                }

                std::string line = headers_text.substr(pos, line_end - pos);
                size_t colon = line.find(":");
                if (colon != std::string::npos) {
                    std::string name = line.substr(0, colon);
                    std::string value = line.substr(colon + 1);

                    // Trim whitespace
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);

                    response.headers[name] = value;
                }

                pos = line_end + 2;
            }

            co_return response;

        } catch (...) {
            if (tcp) {
                // Close connection on error
                uv_close(reinterpret_cast<uv_handle_t*>(tcp),
                         [](uv_handle_t* handle) {
                             delete reinterpret_cast<uv_tcp_t*>(handle);
                         });
            }
            throw;
        }
    }

    Task<HttpResponse> get(const std::string& url) {
        HttpRequest req;
        req.url = url;
        co_return co_await request(req);
    }

    Task<HttpResponse> post(const std::string& url, const std::string& body,
                           const std::string& content_type = "application/json") {
        HttpRequest req;
        req.method = "POST";
        req.url = url;
        req.body = body;
        req.headers["Content-Type"] = content_type;
        co_return co_await request(req);
    }

private:
    uv_loop_t* loop_;
    std::unique_ptr<ConnectionPool> connection_pool_;
};

/**
 * @class Generator
 * @brief Coroutine generator for producing sequences of values
 */
template <typename T>
class Generator {
public:
    class promise_type {
    public:
        Generator get_return_object() {
            return Generator(std::coroutine_handle<promise_type>::from_promise(*this));
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        std::suspend_always yield_value(T value) {
            current_value_ = std::move(value);
            return {};
        }

        void return_void() {}
        void unhandled_exception() { exception_ = std::current_exception(); }

        T& value() { return current_value_; }

        void rethrow_if_exception() {
            if (exception_) {
                std::rethrow_exception(exception_);
            }
        }

    private:
        T current_value_;
        std::exception_ptr exception_;
    };

    class iterator {
    public:
        explicit iterator(std::coroutine_handle<promise_type> handle)
            : handle_(handle) {}

        iterator& operator++() {
            handle_.resume();
            if (handle_.done()) {
                handle_.promise().rethrow_if_exception();
            }
            return *this;
        }

        T& operator*() { return handle_.promise().value(); }

        bool operator==(const iterator& other) const {
            return handle_.done() == other.handle_.done();
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

    private:
        std::coroutine_handle<promise_type> handle_;
    };

    explicit Generator(std::coroutine_handle<promise_type> handle)
        : handle_(handle) {}

    ~Generator() {
        if (handle_) {
            handle_.destroy();
        }
    }

    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    Generator(Generator&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    iterator begin() {
        if (handle_) {
            handle_.resume();
            if (handle_.done()) {
                handle_.promise().rethrow_if_exception();
            }
        }
        return iterator{handle_};
    }

    iterator end() {
        return iterator{nullptr};
    }

private:
    std::coroutine_handle<promise_type> handle_;
};

/**
 * @class AsyncMutex
 * @brief Coroutine-friendly mutex implementation
 */
class AsyncMutex {
public:
    class LockAwaiter {
    public:
        explicit LockAwaiter(AsyncMutex& mutex) : mutex_(mutex) {}

        bool await_ready() const {
            return mutex_.try_lock();
        }

        void await_suspend(std::coroutine_handle<> handle) {
            std::lock_guard<std::mutex> lock(mutex_.queue_mutex_);
            mutex_.waiting_queue_.push(handle);
        }

        void await_resume() {}

    private:
        AsyncMutex& mutex_;
    };

    LockAwaiter lock() {
        return LockAwaiter(*this);
    }

    void unlock() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        locked_ = false;

        if (!waiting_queue_.empty()) {
            auto handle = waiting_queue_.front();
            waiting_queue_.pop();
            locked_ = true;
            handle.resume();
        }
    }

private:
    bool try_lock() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (!locked_) {
            locked_ = true;
            return true;
        }
        return false;
    }

    std::mutex queue_mutex_;
    std::queue<std::coroutine_handle<>> waiting_queue_;
    bool locked_ = false;

    friend class LockAwaiter;
};

// Global scheduler
inline Scheduler& get_scheduler() {
    static Scheduler scheduler;
    return scheduler;
}

// Enhanced convenience functions
inline TimeoutAwaiter sleep_for(uint64_t timeout_ms) {
    return TimeoutAwaiter(get_scheduler().get_loop(), timeout_ms);
}

inline TimeoutAwaiter sleep_for(std::chrono::milliseconds timeout) {
    return TimeoutAwaiter(get_scheduler().get_loop(), timeout.count());
}

inline TcpClient make_tcp_client() {
    return TcpClient(get_scheduler().get_loop());
}

inline HttpClient make_http_client() {
    return HttpClient(get_scheduler().get_loop());
}

inline FileSystem make_file_system() {
    return FileSystem(get_scheduler().get_loop());
}

/**
 * @brief Run multiple tasks concurrently and wait for all to complete
 */
template<typename... Tasks>
Task<std::tuple<typename Tasks::value_type...>> when_all(Tasks&&... tasks) {
    std::tuple<typename Tasks::value_type...> results;

    // Helper to await each task and store result
    auto await_task = [](auto& task, auto& result) -> Task<void> {
        result = co_await task;
    };

    // Create tasks for each input
    std::vector<Task<void>> await_tasks;
    std::apply([&](auto&... args) {
        (await_tasks.emplace_back(await_task(tasks, args)), ...);
    }, results);

    // Wait for all tasks to complete
    for (auto& task : await_tasks) {
        co_await task;
    }

    co_return results;
}

/**
 * @brief Run multiple tasks concurrently and return the first to complete
 */
template<typename T>
Task<T> when_any(std::vector<Task<T>>& tasks) {
    if (tasks.empty()) {
        throw std::invalid_argument("when_any requires at least one task");
    }

    std::atomic<bool> completed{false};
    std::optional<T> result;
    std::exception_ptr exception;

    std::vector<Task<void>> wrapper_tasks;
    wrapper_tasks.reserve(tasks.size());

    for (auto& task : tasks) {
        wrapper_tasks.emplace_back([&]() -> Task<void> {
            try {
                T value = co_await task;
                if (!completed.exchange(true)) {
                    result = std::move(value);
                }
            } catch (...) {
                if (!completed.exchange(true)) {
                    exception = std::current_exception();
                }
            }
        }());
    }

    // Wait for first completion
    while (!completed.load()) {
        co_await sleep_for(1);
    }

    if (exception) {
        std::rethrow_exception(exception);
    }

    co_return std::move(*result);
}

/**
 * @brief Create a timeout wrapper for any task
 */
template<typename T>
Task<T> with_timeout(Task<T> task, std::chrono::milliseconds timeout) {
    std::atomic<bool> completed{false};
    std::optional<T> result;
    std::exception_ptr exception;

    // Start the main task
    auto main_task = [&]() -> Task<void> {
        try {
            T value = co_await task;
            if (!completed.exchange(true)) {
                result = std::move(value);
            }
        } catch (...) {
            if (!completed.exchange(true)) {
                exception = std::current_exception();
            }
        }
    }();

    // Start the timeout task
    auto timeout_task = [&]() -> Task<void> {
        co_await sleep_for(timeout);
        if (!completed.exchange(true)) {
            exception = std::make_exception_ptr(
                UvError(UV_ETIMEDOUT, "Task timed out"));
        }
    }();

    // Wait for either to complete
    while (!completed.load()) {
        co_await sleep_for(1);
    }

    if (exception) {
        std::rethrow_exception(exception);
    }

    co_return std::move(*result);
}

/**
 * @brief Retry a task with exponential backoff
 */
template<typename T>
Task<T> retry_with_backoff(std::function<Task<T>()> task_factory,
                          int max_attempts = 3,
                          std::chrono::milliseconds initial_delay = std::chrono::milliseconds(100)) {
    std::chrono::milliseconds delay = initial_delay;

    for (int attempt = 1; attempt <= max_attempts; ++attempt) {
        try {
            co_return co_await task_factory();
        } catch (const UvError& e) {
            if (attempt == max_attempts || !e.is_recoverable()) {
                throw;
            }

            co_await sleep_for(delay);
            delay *= 2; // Exponential backoff
        }
    }

    throw UvError(UV_ECANCELED, "All retry attempts failed");
}
}  // namespace uv_coro

#endif  // ATOM_EXTRA_UV_CORO_HPP
