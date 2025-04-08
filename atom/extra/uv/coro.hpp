/**
 * @file uv_coro.hpp
 * @brief Modern C++ coroutine wrapper for libuv
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

namespace uv_coro {

// Forward declarations
template <typename T = void>
class Task;

class Scheduler;
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

/**
 * @class UvError
 * @brief Exception class for libuv errors
 */
class UvError : public std::runtime_error {
public:
    explicit UvError(int err)
        : std::runtime_error(uv_strerror(err)), error_code_(err) {}

    int error_code() const { return error_code_; }

private:
    int error_code_;
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
 * @class HttpClient
 * @brief Simple HTTP client built using TcpClient
 */
class HttpClient {
public:
    struct HttpResponse {
        int status_code = 0;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
    };

    explicit HttpClient(uv_loop_t* loop) : loop_(loop) {}

    Task<HttpResponse> get(const std::string& url) {
        // Parse URL
        std::string host;
        std::string path = "/";
        int port = 80;
        bool use_ssl = false;

        // Simple URL parsing
        size_t protocol_end = url.find("://");
        if (protocol_end != std::string::npos) {
            std::string protocol = url.substr(0, protocol_end);
            if (protocol == "https") {
                use_ssl = true;
                port = 443;
            }
            protocol_end += 3;  // Skip "://"
        } else {
            protocol_end = 0;
        }

        size_t path_start = url.find("/", protocol_end);
        if (path_start != std::string::npos) {
            host = url.substr(protocol_end, path_start - protocol_end);
            path = url.substr(path_start);
        } else {
            host = url.substr(protocol_end);
        }

        // Check for port
        size_t port_start = host.find(":");
        if (port_start != std::string::npos) {
            port = std::stoi(host.substr(port_start + 1));
            host = host.substr(0, port_start);
        }

        if (use_ssl) {
            // SSL not implemented in this simple example
            throw std::runtime_error("HTTPS not implemented in this example");
        }

        // Create TCP client and connect
        TcpClient client(loop_);
        try {
            co_await client.connect(host, port);

            // Send HTTP request
            std::string request = "GET " + path +
                                  " HTTP/1.1\r\n"
                                  "Host: " +
                                  host +
                                  "\r\n"
                                  "Connection: close\r\n\r\n";

            co_await client.write(request);

            // Read response
            std::string response_text;
            while (true) {
                try {
                    std::string chunk = co_await client.read();
                    if (chunk.empty()) {
                        break;
                    }
                    response_text += chunk;
                } catch (const UvError& e) {
                    if (e.error_code() == UV_EOF) {
                        break;  // End of response
                    }
                    throw;  // Re-throw other errors
                }
            }

            // Parse response
            HttpResponse response;

            size_t header_end = response_text.find("\r\n\r\n");
            if (header_end == std::string::npos) {
                throw std::runtime_error("Invalid HTTP response");
            }

            std::string headers_text = response_text.substr(0, header_end);
            response.body = response_text.substr(header_end + 4);

            // Parse status line
            size_t first_line_end = headers_text.find("\r\n");
            if (first_line_end != std::string::npos) {
                std::string status_line =
                    headers_text.substr(0, first_line_end);
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

            client.close();
            co_return response;

        } catch (...) {
            client.close();
            throw;
        }
    }

private:
    uv_loop_t* loop_;
};

// Global scheduler
inline Scheduler& get_scheduler() {
    static Scheduler scheduler;
    return scheduler;
}

// Convenience functions
inline TimeoutAwaiter sleep_for(uint64_t timeout_ms) {
    return TimeoutAwaiter(get_scheduler().get_loop(), timeout_ms);
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
}  // namespace uv_coro

#endif  // ATOM_EXTRA_UV_CORO_HPP