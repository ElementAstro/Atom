/**
 * @file uv_process.hpp
 * @brief Modern C++ interface for libuv child process operations
 */

#ifndef ATOM_EXTRA_UV_SUBPROCESS_HPP
#define ATOM_EXTRA_UV_SUBPROCESS_HPP

#include <uv.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#undef ERROR
#endif

/**
 * @class UvProcess
 * @brief Class that encapsulates libuv child process functionality
 */
class UvProcess {
public:
    /**
     * @brief Exit callback function type
     */
    using ExitCallback =
        std::function<void(int64_t exit_status, int term_signal)>;

    /**
     * @brief Data callback function type
     */
    using DataCallback = std::function<void(const char* data, ssize_t size)>;

    /**
     * @brief Timeout callback function type
     */
    using TimeoutCallback = std::function<void()>;

    /**
     * @brief Error callback function type
     */
    using ErrorCallback = std::function<void(const std::string& error_message)>;

    /**
     * @brief Process options structure
     */
    struct ProcessOptions {
        std::string file;               // Executable path
        std::vector<std::string> args;  // Command line arguments
        std::string cwd;                // Working directory
        std::unordered_map<std::string, std::string>
            env;                // Environment variables
        bool detached = false;  // Run process detached
        std::chrono::milliseconds timeout{
            0};  // Process execution timeout (0 = no timeout)
        bool redirect_stderr_to_stdout = false;  // Redirect stderr to stdout
        bool inherit_parent_env = true;  // Inherit parent environment variables
        int stdio_count = 3;             // Number of stdio file descriptors
    };

    /**
     * @brief Process status enumeration
     */
    enum class ProcessStatus {
        IDLE,        // Process not started
        RUNNING,     // Process is running
        EXITED,      // Process exited normally
        TERMINATED,  // Process was terminated by signal
        TIMED_OUT,   // Process timed out
        ERROR        // Error occurred
    };

    /**
     * @brief Constructor
     * @param custom_loop Optional custom uv_loop_t (defaults to the default
     * loop)
     */
    explicit UvProcess(uv_loop_t* custom_loop = nullptr);

    /**
     * @brief Destructor
     */
    ~UvProcess();

    /**
     * @brief Delete copy constructor and assignment operator
     */
    UvProcess(const UvProcess&) = delete;
    UvProcess& operator=(const UvProcess&) = delete;

    /**
     * @brief Support for move constructor and assignment
     */
    UvProcess(UvProcess&& other) noexcept;
    UvProcess& operator=(UvProcess&& other) noexcept;

    /**
     * @brief Spawn a child process with basic options
     *
     * @param file Executable path
     * @param args Command line arguments
     * @param cwd Working directory (optional)
     * @param exit_callback Exit callback (optional)
     * @param stdout_callback Stdout callback (optional)
     * @param stderr_callback Stderr callback (optional)
     * @return bool Success status
     */
    bool spawn(const std::string& file, const std::vector<std::string>& args,
               const std::string& cwd = "",
               ExitCallback exit_callback = nullptr,
               DataCallback stdout_callback = nullptr,
               DataCallback stderr_callback = nullptr);

    /**
     * @brief Spawn a child process with advanced options
     *
     * @param options Process options
     * @param exit_callback Exit callback (optional)
     * @param stdout_callback Stdout callback (optional)
     * @param stderr_callback Stderr callback (optional)
     * @param timeout_callback Timeout callback (optional)
     * @param error_callback Error callback (optional)
     * @return bool Success status
     */
    bool spawnWithOptions(const ProcessOptions& options,
                          ExitCallback exit_callback = nullptr,
                          DataCallback stdout_callback = nullptr,
                          DataCallback stderr_callback = nullptr,
                          TimeoutCallback timeout_callback = nullptr,
                          ErrorCallback error_callback = nullptr);

    /**
     * @brief Write data to child process stdin
     *
     * @param data Data to write
     * @return bool Success status
     */
    bool writeToStdin(const std::string& data);

    /**
     * @brief Close child process stdin
     */
    void closeStdin();

    /**
     * @brief Send signal to child process
     *
     * @param signum Signal number (default: SIGTERM)
     * @return bool Success status
     */
    bool kill(int signum = SIGTERM);

    /**
     * @brief Kill process with SIGKILL (convenience method)
     *
     * @return bool Success status
     */
    bool killForcefully();

    /**
     * @brief Check if child process is running
     *
     * @return bool Is running
     */
    bool isRunning() const;

    /**
     * @brief Get child process ID
     *
     * @return int Process ID (-1 if not running)
     */
    int getPid() const;

    /**
     * @brief Get process status
     *
     * @return ProcessStatus Current status
     */
    ProcessStatus getStatus() const;

    /**
     * @brief Get process exit code
     *
     * @return int Exit code (-1 if process hasn't exited)
     */
    int getExitCode() const;

    /**
     * @brief Wait for process to exit
     *
     * @param timeout_ms Timeout in milliseconds (0 = wait forever)
     * @return bool True if process exited, false on timeout
     */
    bool waitForExit(uint64_t timeout_ms = 0);

    /**
     * @brief Reset the process object to allow reuse
     */
    void reset();

    /**
     * @brief Set custom error handler
     *
     * @param error_callback Error callback function
     */
    void setErrorCallback(ErrorCallback error_callback);

private:
    // Forward declarations of private implementation structures
    struct ReadContext;
    struct WriteRequest;
    struct TimeoutData;

    // Initialize pipes and process options
    void initializePipes();
    bool setupProcessOptions(uv_process_options_t& options,
                             const ProcessOptions& process_options);
    void prepareEnvironment(
        const std::unordered_map<std::string, std::string>& env_vars,
        bool inherit_parent);
    void startRead(uv_stream_t* stream, bool is_stdout);
    void cleanupArgs();
    void cleanup();
    void setupTimeout(const std::chrono::milliseconds& timeout);
    void cancelTimeout();
    void onExitInternal(int64_t exit_status, int term_signal);
    void handleError(const std::string& error_message);

    // libuv handlers
    uv_loop_t* loop_;
    std::unique_ptr<uv_process_t> process_;
    std::unique_ptr<uv_pipe_t> stdin_pipe_;
    std::unique_ptr<uv_pipe_t> stdout_pipe_;
    std::unique_ptr<uv_pipe_t> stderr_pipe_;
    std::unique_ptr<uv_timer_t> timeout_timer_;

    // Process state
    std::atomic<ProcessStatus> status_;
    std::atomic<bool> is_running_;
    std::atomic<int> exit_code_;
    std::mutex mutex_;
    std::vector<char*> args_;
    std::vector<char*> env_vars_;

    // Callback functions
    ExitCallback exit_callback_;
    DataCallback stdout_callback_;
    DataCallback stderr_callback_;
    TimeoutCallback timeout_callback_;
    ErrorCallback error_callback_;
};

#endif  // ATOM_EXTRA_UV_SUBPROCESS_HPP
