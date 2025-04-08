/**
 * @file uv_process.cpp
 * @brief Implementation of the UvProcess class
 */

#include "subprocess.hpp"

#include <condition_variable>
#include <iostream>

// ===== Private Implementation Structures =====

/**
 * @brief Read context structure for stream data
 */
struct UvProcess::ReadContext {
    UvProcess* process;
    bool is_stdout;

    ReadContext(UvProcess* p, bool is_stdout_pipe)
        : process(p), is_stdout(is_stdout_pipe) {}
};

/**
 * @brief Write request structure for stdin data
 */
struct UvProcess::WriteRequest {
    uv_write_t req;
    std::unique_ptr<char[]> data;
    size_t length;

    WriteRequest(const std::string& str) : length(str.size()) {
        data = std::make_unique<char[]>(length);
        std::copy(str.begin(), str.end(), data.get());
        req.data = this;
    }
};

/**
 * @brief Timeout data structure
 */
struct UvProcess::TimeoutData {
    UvProcess* process;
    explicit TimeoutData(UvProcess* p) : process(p) {}
};

// ===== Constructor & Destructor =====

UvProcess::UvProcess(uv_loop_t* custom_loop)
    : loop_(custom_loop ? custom_loop : uv_default_loop()),
      status_(ProcessStatus::IDLE),
      is_running_(false),
      exit_code_(-1) {
    initializePipes();
}

UvProcess::~UvProcess() {
    // If process is still running, kill it forcefully
    if (is_running_) {
        killForcefully();
    }

    // Wait for process to fully exit
    while (is_running_) {
        uv_run(loop_, UV_RUN_NOWAIT);
    }

    // Clean up resources
    cleanup();
}

// ===== Move Semantics Implementation =====

UvProcess::UvProcess(UvProcess&& other) noexcept
    : loop_(other.loop_),
      status_(other.status_.load()),
      is_running_(other.is_running_.load()),
      exit_code_(other.exit_code_.load()),
      exit_callback_(std::move(other.exit_callback_)),
      stdout_callback_(std::move(other.stdout_callback_)),
      stderr_callback_(std::move(other.stderr_callback_)),
      timeout_callback_(std::move(other.timeout_callback_)),
      error_callback_(std::move(other.error_callback_)) {
    // Move ownership of handles
    process_ = std::move(other.process_);
    stdin_pipe_ = std::move(other.stdin_pipe_);
    stdout_pipe_ = std::move(other.stdout_pipe_);
    stderr_pipe_ = std::move(other.stderr_pipe_);
    timeout_timer_ = std::move(other.timeout_timer_);

    // Update data pointers
    if (process_)
        process_->data = this;
    if (stdin_pipe_)
        stdin_pipe_->data = this;
    if (stdout_pipe_)
        stdout_pipe_->data = this;
    if (stderr_pipe_)
        stderr_pipe_->data = this;
    if (timeout_timer_)
        timeout_timer_->data = this;

    // Move arguments and environment variables
    args_ = std::move(other.args_);
    env_vars_ = std::move(other.env_vars_);

    // Set other to default state
    other.is_running_ = false;
    other.status_ = ProcessStatus::IDLE;
    other.exit_code_ = -1;
}

UvProcess& UvProcess::operator=(UvProcess&& other) noexcept {
    if (this != &other) {
        // Clean up existing resources
        if (is_running_) {
            killForcefully();
        }
        cleanup();

        // Move from other
        loop_ = other.loop_;
        is_running_ = other.is_running_.load();
        status_ = other.status_.load();
        exit_code_ = other.exit_code_.load();

        // Move ownership of handles
        process_ = std::move(other.process_);
        stdin_pipe_ = std::move(other.stdin_pipe_);
        stdout_pipe_ = std::move(other.stdout_pipe_);
        stderr_pipe_ = std::move(other.stderr_pipe_);
        timeout_timer_ = std::move(other.timeout_timer_);

        // Update data pointers
        if (process_)
            process_->data = this;
        if (stdin_pipe_)
            stdin_pipe_->data = this;
        if (stdout_pipe_)
            stdout_pipe_->data = this;
        if (stderr_pipe_)
            stderr_pipe_->data = this;
        if (timeout_timer_)
            timeout_timer_->data = this;

        // Move callbacks
        exit_callback_ = std::move(other.exit_callback_);
        stdout_callback_ = std::move(other.stdout_callback_);
        stderr_callback_ = std::move(other.stderr_callback_);
        timeout_callback_ = std::move(other.timeout_callback_);
        error_callback_ = std::move(other.error_callback_);

        // Move arguments and environment variables
        args_ = std::move(other.args_);
        env_vars_ = std::move(other.env_vars_);

        // Set other to default state
        other.is_running_ = false;
        other.status_ = ProcessStatus::IDLE;
        other.exit_code_ = -1;
    }
    return *this;
}

// ===== Private Helpers =====

void UvProcess::initializePipes() {
    // Initialize process handle
    process_ = std::make_unique<uv_process_t>();
    process_->data = this;

    // Initialize standard input/output/error pipes
    stdin_pipe_ = std::make_unique<uv_pipe_t>();
    stdout_pipe_ = std::make_unique<uv_pipe_t>();
    stderr_pipe_ = std::make_unique<uv_pipe_t>();

    uv_pipe_init(loop_, stdin_pipe_.get(), 0);
    uv_pipe_init(loop_, stdout_pipe_.get(), 0);
    uv_pipe_init(loop_, stderr_pipe_.get(), 0);

    stdin_pipe_->data = this;
    stdout_pipe_->data = this;
    stderr_pipe_->data = this;
}

void UvProcess::cleanupArgs() {
    for (char* arg : args_) {
        if (arg)
            free(arg);
    }
    args_.clear();

    for (char* env : env_vars_) {
        if (env)
            free(env);
    }
    env_vars_.clear();
}

void UvProcess::cleanup() {
    // Cancel any active timeout
    cancelTimeout();

    // Clean up command line arguments and environment variables
    cleanupArgs();

    // Close all active handles
    if (stdin_pipe_ &&
        !uv_is_closing(reinterpret_cast<uv_handle_t*>(stdin_pipe_.get()))) {
        uv_close(reinterpret_cast<uv_handle_t*>(stdin_pipe_.get()), nullptr);
    }

    if (stdout_pipe_ &&
        !uv_is_closing(reinterpret_cast<uv_handle_t*>(stdout_pipe_.get()))) {
        uv_close(reinterpret_cast<uv_handle_t*>(stdout_pipe_.get()), nullptr);
    }

    if (stderr_pipe_ &&
        !uv_is_closing(reinterpret_cast<uv_handle_t*>(stderr_pipe_.get()))) {
        uv_close(reinterpret_cast<uv_handle_t*>(stderr_pipe_.get()), nullptr);
    }

    if (process_ &&
        !uv_is_closing(reinterpret_cast<uv_handle_t*>(process_.get()))) {
        uv_close(reinterpret_cast<uv_handle_t*>(process_.get()), nullptr);
    }

    if (timeout_timer_ &&
        !uv_is_closing(reinterpret_cast<uv_handle_t*>(timeout_timer_.get()))) {
        uv_close(reinterpret_cast<uv_handle_t*>(timeout_timer_.get()), nullptr);
    }

    // Run event loop to ensure all close callbacks are executed
    uv_run(loop_, UV_RUN_NOWAIT);
}

void UvProcess::startRead(uv_stream_t* stream, bool is_stdout) {
    auto* context = new ReadContext(this, is_stdout);
    stream->data = context;

    int r = uv_read_start(
        stream,
        [](uv_handle_t* /*handle*/, size_t suggested_size, uv_buf_t* buf) {
            // Allocate buffer
            buf->base = new char[suggested_size];
            buf->len = suggested_size;
        },
        [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
            auto* context = static_cast<ReadContext*>(stream->data);
            auto* self = context->process;

            // Handle error or EOF
            if (nread < 0) {
                if (nread != UV_EOF) {
                    self->handleError(std::string("Read error: ") +
                                      uv_strerror(nread));
                }

                // Stop reading
                uv_read_stop(stream);

                // Free context
                delete context;
                stream->data = nullptr;

                // Free buffer
                if (buf->base) {
                    delete[] buf->base;
                }

                return;
            }

            // No data, just free buffer
            if (nread == 0) {
                delete[] buf->base;
                return;
            }

            // Call appropriate callback
            if (context->is_stdout && self->stdout_callback_) {
                self->stdout_callback_(buf->base, nread);
            } else if (!context->is_stdout && self->stderr_callback_) {
                self->stderr_callback_(buf->base, nread);
            }

            // Free buffer
            delete[] buf->base;
        });

    if (r != 0) {
        handleError(std::string("Failed to start reading: ") + uv_strerror(r));
        delete context;
    }
}

bool UvProcess::setupProcessOptions(uv_process_options_t& options,
                                    const ProcessOptions& process_options) {
    // Clean up previous command line arguments and environment variables
    cleanupArgs();

    // Prepare new command line arguments
    args_.push_back(strdup(process_options.file.c_str()));
    for (const auto& arg : process_options.args) {
        args_.push_back(strdup(arg.c_str()));
    }
    args_.push_back(nullptr);  // Argument list must end with nullptr

    // Prepare environment variables if specified
    if (!process_options.env.empty() || !process_options.inherit_parent_env) {
        prepareEnvironment(process_options.env,
                           process_options.inherit_parent_env);
    }

    // Configure process options
    memset(&options, 0, sizeof(options));
    options.exit_cb = [](uv_process_t* req, int64_t exit_status,
                         int term_signal) {
        auto* self = static_cast<UvProcess*>(req->data);
        self->onExitInternal(exit_status, term_signal);
    };

    options.file = process_options.file.c_str();
    options.args = args_.data();

    if (!process_options.cwd.empty()) {
        options.cwd = process_options.cwd.c_str();
    }

    options.stdio_count = process_options.stdio_count;

    // Set environment if prepared
    if (!env_vars_.empty()) {
        options.env = env_vars_.data();
    }

    // Set detached flag if requested
    if (process_options.detached) {
        options.flags |= UV_PROCESS_DETACHED;
    }

    return true;
}

void UvProcess::prepareEnvironment(
    const std::unordered_map<std::string, std::string>& env_vars,
    bool inherit_parent) {
    // Start with parent environment if requested
    if (inherit_parent) {
        for (char** env = environ; *env != nullptr; env++) {
            env_vars_.push_back(strdup(*env));
        }
    }

    // Add or override with specified environment variables
    for (const auto& [key, value] : env_vars) {
        std::string env_str = key + "=" + value;
        env_vars_.push_back(strdup(env_str.c_str()));
    }

    // Terminate with nullptr
    env_vars_.push_back(nullptr);
}

void UvProcess::setupTimeout(const std::chrono::milliseconds& timeout) {
    if (timeout.count() <= 0) {
        return;  // No timeout
    }

    // Create timeout timer if not exists
    if (!timeout_timer_) {
        timeout_timer_ = std::make_unique<uv_timer_t>();
        uv_timer_init(loop_, timeout_timer_.get());

        auto* timeout_data = new TimeoutData(this);
        timeout_timer_->data = timeout_data;
    }

    // Start timer
    uv_timer_start(
        timeout_timer_.get(),
        [](uv_timer_t* handle) {
            auto* timeout_data = static_cast<TimeoutData*>(handle->data);
            auto* self = timeout_data->process;

            // Set status and kill process
            self->status_ = ProcessStatus::TIMED_OUT;
            self->killForcefully();

            // Call timeout callback
            if (self->timeout_callback_) {
                self->timeout_callback_();
            }
        },
        timeout.count(), 0);  // one-shot timer
}

void UvProcess::cancelTimeout() {
    if (timeout_timer_ &&
        !uv_is_closing(reinterpret_cast<uv_handle_t*>(timeout_timer_.get()))) {
        uv_timer_stop(timeout_timer_.get());

        // Clean up timeout data
        if (timeout_timer_->data) {
            delete static_cast<TimeoutData*>(timeout_timer_->data);
            timeout_timer_->data = nullptr;
        }
    }
}

void UvProcess::onExitInternal(int64_t exit_status, int term_signal) {
    // Update process state
    is_running_ = false;
    exit_code_ = static_cast<int>(exit_status);

    // Determine exit status
    if (term_signal == 0) {
        status_ = ProcessStatus::EXITED;
    } else {
        status_ = ProcessStatus::TERMINATED;
    }

    // Cancel timeout if active
    cancelTimeout();

    // Call user exit callback
    if (exit_callback_) {
        exit_callback_(exit_status, term_signal);
    }
}

void UvProcess::handleError(const std::string& error_message) {
    status_ = ProcessStatus::ERROR;

    if (error_callback_) {
        error_callback_(error_message);
    } else {
        std::cerr << "UvProcess error: " << error_message << std::endl;
    }
}

// ===== Public API Implementation =====

bool UvProcess::spawn(const std::string& file,
                      const std::vector<std::string>& args,
                      const std::string& cwd, ExitCallback exit_callback,
                      DataCallback stdout_callback,
                      DataCallback stderr_callback) {
    // Configure basic process options
    ProcessOptions options;
    options.file = file;
    options.args = args;
    options.cwd = cwd;

    return spawnWithOptions(options, exit_callback, stdout_callback,
                            stderr_callback);
}

bool UvProcess::spawnWithOptions(const ProcessOptions& options,
                                 ExitCallback exit_callback,
                                 DataCallback stdout_callback,
                                 DataCallback stderr_callback,
                                 TimeoutCallback timeout_callback,
                                 ErrorCallback error_callback) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (is_running_) {
        handleError("Process is already running");
        return false;
    }

    // Reset state
    status_ = ProcessStatus::IDLE;
    exit_code_ = -1;

    // Save callback functions
    exit_callback_ = std::move(exit_callback);
    stdout_callback_ = std::move(stdout_callback);
    stderr_callback_ = std::move(stderr_callback);
    timeout_callback_ = std::move(timeout_callback);
    error_callback_ = std::move(error_callback);

    // Set up standard input/output/error stream redirection
    uv_stdio_container_t child_stdio[3];
    child_stdio[0].flags =
        static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_READABLE_PIPE);
    child_stdio[0].data.stream =
        reinterpret_cast<uv_stream_t*>(stdin_pipe_.get());

    child_stdio[1].flags =
        static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_WRITABLE_PIPE);
    child_stdio[1].data.stream =
        reinterpret_cast<uv_stream_t*>(stdout_pipe_.get());

    if (options.redirect_stderr_to_stdout) {
        child_stdio[2].flags = static_cast<uv_stdio_flags>(UV_INHERIT_STREAM);
        child_stdio[2].data.stream =
            reinterpret_cast<uv_stream_t*>(stdout_pipe_.get());
    } else {
        child_stdio[2].flags =
            static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_WRITABLE_PIPE);
        child_stdio[2].data.stream =
            reinterpret_cast<uv_stream_t*>(stderr_pipe_.get());
    }

    // Set up process options
    uv_process_options_t process_options;
    if (!setupProcessOptions(process_options, options)) {
        return false;
    }

    process_options.stdio_count = 3;
    process_options.stdio = child_stdio;

    // Spawn child process
    int r = uv_spawn(loop_, process_.get(), &process_options);
    if (r != 0) {
        std::string error =
            std::string("Failed to spawn process: ") + uv_strerror(r);
        handleError(error);
        return false;
    }

    is_running_ = true;
    status_ = ProcessStatus::RUNNING;

    // Start reading standard output and standard error
    if (stdout_callback_) {
        startRead(reinterpret_cast<uv_stream_t*>(stdout_pipe_.get()), true);
    }

    if (stderr_callback_ && !options.redirect_stderr_to_stdout) {
        startRead(reinterpret_cast<uv_stream_t*>(stderr_pipe_.get()), false);
    }

    // Set up timeout if specified
    if (options.timeout.count() > 0) {
        setupTimeout(options.timeout);
    }

    return true;
}

bool UvProcess::writeToStdin(const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_running_ ||
        uv_is_closing(reinterpret_cast<uv_handle_t*>(stdin_pipe_.get()))) {
        return false;
    }

    // Create write request and data buffer
    auto* write_req = new WriteRequest(data);
    uv_buf_t buf = uv_buf_init(write_req->data.get(), write_req->length);

    int r = uv_write(
        &write_req->req, reinterpret_cast<uv_stream_t*>(stdin_pipe_.get()),
        &buf, 1, [](uv_write_t* req, int status) {
            auto* write_req = static_cast<WriteRequest*>(req->data);
            auto* self = static_cast<UvProcess*>(
                reinterpret_cast<uv_pipe_t*>(req->handle)->data);

            if (status != 0 && self) {
                self->handleError(std::string("Write error: ") +
                                  uv_strerror(status));
            }

            delete write_req;  // Automatically releases req and data buffer
        });

    if (r != 0) {
        handleError(std::string("Failed to write to stdin: ") + uv_strerror(r));
        delete write_req;
        return false;
    }

    return true;
}

void UvProcess::closeStdin() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (is_running_ &&
        !uv_is_closing(reinterpret_cast<uv_handle_t*>(stdin_pipe_.get()))) {
        uv_close(reinterpret_cast<uv_handle_t*>(stdin_pipe_.get()), nullptr);
    }
}

bool UvProcess::kill(int signum) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_running_) {
        return false;
    }

    int r = uv_process_kill(process_.get(), signum);
    if (r != 0) {
        handleError(std::string("Failed to kill process: ") + uv_strerror(r));
        return false;
    }

    return true;
}

bool UvProcess::killForcefully() { return kill(SIGKILL); }

bool UvProcess::isRunning() const { return is_running_; }

int UvProcess::getPid() const {
    if (!is_running_) {
        return -1;
    }
    return process_->pid;
}

UvProcess::ProcessStatus UvProcess::getStatus() const { return status_; }

int UvProcess::getExitCode() const { return exit_code_; }

bool UvProcess::waitForExit(uint64_t timeout_ms) {
    if (!is_running_) {
        return true;  // Already exited
    }

    std::condition_variable cv;
    std::mutex wait_mutex;
    bool exited = false;

    // Save current exit callback
    auto original_callback = exit_callback_;

    // Set up new exit callback that will signal our condition variable
    exit_callback_ = [&](int64_t exit_status, int term_signal) {
        // Call original callback if present
        if (original_callback) {
            original_callback(exit_status, term_signal);
        }

        // Signal waiting thread
        {
            std::lock_guard<std::mutex> lock(wait_mutex);
            exited = true;
        }
        cv.notify_one();
    };

    // Wait for signal or timeout
    std::unique_lock<std::mutex> lock(wait_mutex);
    if (timeout_ms == 0) {
        // Wait indefinitely
        while (is_running_ && !exited) {
            cv.wait(lock);
        }
        return true;
    } else {
        // Wait with timeout
        auto wait_until = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);

        while (is_running_ && !exited) {
            if (cv.wait_until(lock, wait_until) == std::cv_status::timeout) {
                // Restore original callback
                exit_callback_ = original_callback;
                return false;  // Timeout
            }
        }
        return true;  // Exited within timeout
    }
}

void UvProcess::reset() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (is_running_) {
        killForcefully();
        // Wait for process to fully exit
        while (is_running_) {
            uv_run(loop_, UV_RUN_NOWAIT);
        }
    }

    // Clean up resources
    cleanup();

    // Reinitialize handles
    initializePipes();

    // Reset state
    is_running_ = false;
    status_ = ProcessStatus::IDLE;
    exit_code_ = -1;

    // Clear callbacks
    exit_callback_ = nullptr;
    stdout_callback_ = nullptr;
    stderr_callback_ = nullptr;
    timeout_callback_ = nullptr;
    error_callback_ = nullptr;
}

void UvProcess::setErrorCallback(ErrorCallback error_callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_callback_ = std::move(error_callback);
}