/**
 * @file uv_process.hpp
 * @brief Enhanced C++ interface for libuv child process operations with pooling and monitoring
 * @version 2.0
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
#include <queue>
#include <condition_variable>
#include <thread>
#include <optional>
#include <future>
#include <span>

#ifdef _WIN32
#undef ERROR
#endif

/**
 * @struct ProcessMetrics
 * @brief Comprehensive process monitoring metrics
 */
struct ProcessMetrics {
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    std::chrono::milliseconds execution_time{0};

    // Resource usage
    uint64_t peak_memory_usage = 0;      // Peak RSS in bytes
    uint64_t total_cpu_time = 0;         // Total CPU time in microseconds
    double cpu_usage_percent = 0.0;      // CPU usage percentage

    // I/O statistics
    uint64_t bytes_read = 0;
    uint64_t bytes_written = 0;
    uint64_t read_operations = 0;
    uint64_t write_operations = 0;

    // System calls and context switches
    uint64_t voluntary_context_switches = 0;
    uint64_t involuntary_context_switches = 0;

    // Exit information
    int exit_code = -1;
    int termination_signal = 0;
    bool was_killed = false;
    bool timed_out = false;

    void reset() {
        start_time = std::chrono::steady_clock::now();
        end_time = {};
        execution_time = std::chrono::milliseconds{0};
        peak_memory_usage = 0;
        total_cpu_time = 0;
        cpu_usage_percent = 0.0;
        bytes_read = 0;
        bytes_written = 0;
        read_operations = 0;
        write_operations = 0;
        voluntary_context_switches = 0;
        involuntary_context_switches = 0;
        exit_code = -1;
        termination_signal = 0;
        was_killed = false;
        timed_out = false;
    }
};

/**
 * @struct ProcessLimits
 * @brief Resource limits for process execution
 */
struct ProcessLimits {
    std::optional<uint64_t> max_memory;          // Maximum memory in bytes
    std::optional<std::chrono::seconds> max_cpu_time;  // Maximum CPU time
    std::optional<uint64_t> max_file_size;       // Maximum file size
    std::optional<uint32_t> max_open_files;      // Maximum open file descriptors
    std::optional<uint32_t> max_processes;       // Maximum child processes
    bool enforce_limits = true;
};

/**
 * @class UvProcess
 * @brief Enhanced class that encapsulates libuv child process functionality with monitoring
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
     * @brief Enhanced process options structure
     */
    struct ProcessOptions {
        std::string file;               // Executable path
        std::vector<std::string> args;  // Command line arguments
        std::string cwd;                // Working directory
        std::unordered_map<std::string, std::string> env; // Environment variables

        // Execution options
        bool detached = false;          // Run process detached
        std::chrono::milliseconds timeout{0}; // Process execution timeout (0 = no timeout)
        bool redirect_stderr_to_stdout = false; // Redirect stderr to stdout
        bool inherit_parent_env = true; // Inherit parent environment variables
        int stdio_count = 3;            // Number of stdio file descriptors

        // Security and sandboxing
        std::optional<uint32_t> uid;    // User ID to run as (Unix only)
        std::optional<uint32_t> gid;    // Group ID to run as (Unix only)
        std::string chroot_dir;         // Chroot directory (Unix only)
        bool create_new_session = false; // Create new session (Unix only)

        // Resource limits
        ProcessLimits limits;

        // Monitoring options
        bool enable_monitoring = true;
        std::chrono::milliseconds monitoring_interval{100};
        bool collect_detailed_metrics = false;

        // I/O options
        size_t buffer_size = 4096;
        bool use_line_buffering = false;
        std::string input_data;         // Data to write to stdin immediately

        // Retry and reliability
        uint32_t max_retries = 0;
        std::chrono::milliseconds retry_delay{1000};
        bool retry_on_failure = false;

        // Process priority (platform-specific)
        std::optional<int> priority;    // Process priority (-20 to 19 on Unix)

        // Custom signal handling
        std::unordered_map<int, std::function<void()>> signal_handlers;
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

    /**
     * @brief Get comprehensive process metrics
     *
     * @return ProcessMetrics Current metrics
     */
    ProcessMetrics getMetrics() const;

    /**
     * @brief Get real-time resource usage
     *
     * @return std::optional<ProcessMetrics> Current resource usage or nullopt if not available
     */
    std::optional<ProcessMetrics> getCurrentResourceUsage() const;

    /**
     * @brief Set resource limits for the process
     *
     * @param limits Resource limits to apply
     * @return bool Success status
     */
    bool setResourceLimits(const ProcessLimits& limits);

    /**
     * @brief Pause the process (send SIGSTOP on Unix)
     *
     * @return bool Success status
     */
    bool pause();

    /**
     * @brief Resume the process (send SIGCONT on Unix)
     *
     * @return bool Success status
     */
    bool resume();

    /**
     * @brief Send custom signal to process
     *
     * @param signal Signal number
     * @return bool Success status
     */
    bool sendSignal(int signal);

    /**
     * @brief Get process memory usage in bytes
     *
     * @return uint64_t Memory usage in bytes
     */
    uint64_t getMemoryUsage() const;

    /**
     * @brief Get process CPU usage percentage
     *
     * @return double CPU usage percentage (0.0 - 100.0)
     */
    double getCpuUsage() const;

    /**
     * @brief Check if process is responsive (can receive signals)
     *
     * @return bool True if responsive
     */
    bool isResponsive() const;

    /**
     * @brief Get process uptime
     *
     * @return std::chrono::milliseconds Process uptime
     */
    std::chrono::milliseconds getUptime() const;

    /**
     * @brief Enable/disable real-time monitoring
     *
     * @param enable Enable monitoring
     * @param interval Monitoring interval
     */
    void setMonitoring(bool enable, std::chrono::milliseconds interval = std::chrono::milliseconds(100));

    /**
     * @brief Get process command line
     *
     * @return std::vector<std::string> Command line arguments
     */
    std::vector<std::string> getCommandLine() const;

    /**
     * @brief Get process environment variables
     *
     * @return std::unordered_map<std::string, std::string> Environment variables
     */
    std::unordered_map<std::string, std::string> getEnvironment() const;

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

    // Enhanced monitoring members
    mutable std::mutex metrics_mutex_;
    ProcessMetrics metrics_;
    std::unique_ptr<uv_timer_t> monitoring_timer_;
    bool monitoring_enabled_;
    std::chrono::milliseconds monitoring_interval_;

    // Resource tracking
    ProcessLimits resource_limits_;
    std::chrono::steady_clock::time_point last_cpu_check_;
    uint64_t last_cpu_time_;

    // Enhanced monitoring methods
    void startMonitoring();
    void stopMonitoring();
    void updateMetrics();
    static void monitoring_callback(uv_timer_t* timer);
    bool checkResourceLimits();
    void enforceResourceLimits();
};

/**
 * @class ProcessPool
 * @brief Pool of reusable processes for improved performance
 */
class ProcessPool {
public:
    struct PoolConfig {
        size_t max_processes = 10;
        size_t min_processes = 2;
        std::chrono::seconds idle_timeout{300}; // 5 minutes
        std::chrono::seconds startup_timeout{30};
        bool enable_prewarming = true;
        std::string pool_name = "default";
    };

    struct PoolStats {
        std::atomic<size_t> total_processes{0};
        std::atomic<size_t> active_processes{0};
        std::atomic<size_t> idle_processes{0};
        std::atomic<size_t> failed_processes{0};
        std::atomic<size_t> total_executions{0};
        std::atomic<size_t> successful_executions{0};
        std::atomic<size_t> failed_executions{0};
        std::chrono::steady_clock::time_point start_time{std::chrono::steady_clock::now()};

        double success_rate() const {
            auto total = total_executions.load();
            return total > 0 ? (double)successful_executions.load() / total * 100.0 : 0.0;
        }
    };

    explicit ProcessPool(const PoolConfig& config = {}, uv_loop_t* loop = nullptr);
    ~ProcessPool();

    /**
     * @brief Execute a command using a pooled process
     *
     * @param options Process options
     * @return std::future<ProcessMetrics> Future containing execution results
     */
    std::future<ProcessMetrics> execute(const UvProcess::ProcessOptions& options);

    /**
     * @brief Execute a simple command
     *
     * @param command Command to execute
     * @param args Command arguments
     * @param timeout Execution timeout
     * @return std::future<ProcessMetrics> Future containing execution results
     */
    std::future<ProcessMetrics> execute(const std::string& command,
                                       const std::vector<std::string>& args = {},
                                       std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    /**
     * @brief Get pool statistics
     *
     * @return PoolStats Current pool statistics
     */
    PoolStats getStats() const { return stats_; }

    /**
     * @brief Shutdown the pool gracefully
     *
     * @param timeout Maximum time to wait for shutdown
     */
    void shutdown(std::chrono::seconds timeout = std::chrono::seconds(30));

    /**
     * @brief Resize the pool
     *
     * @param new_size New pool size
     */
    void resize(size_t new_size);

    /**
     * @brief Warm up the pool by pre-creating processes
     */
    void warmup();

private:
    struct PooledProcess {
        std::unique_ptr<UvProcess> process;
        std::chrono::steady_clock::time_point last_used;
        bool in_use = false;
        size_t execution_count = 0;

        PooledProcess() : last_used(std::chrono::steady_clock::now()) {}
    };

    PoolConfig config_;
    uv_loop_t* loop_;
    mutable PoolStats stats_;
    std::atomic<bool> shutdown_requested_{false};

    mutable std::mutex pool_mutex_;
    std::vector<std::unique_ptr<PooledProcess>> processes_;
    std::queue<std::promise<std::unique_ptr<PooledProcess>>> waiting_queue_;

    std::thread cleanup_thread_;
    std::condition_variable pool_condition_;

    // Pool management methods
    std::unique_ptr<PooledProcess> acquireProcess();
    void releaseProcess(std::unique_ptr<PooledProcess> process);
    void cleanupIdleProcesses();
    void cleanupLoop();
    std::unique_ptr<PooledProcess> createProcess();
    bool isProcessHealthy(const PooledProcess& process) const;
};

#endif  // ATOM_EXTRA_UV_SUBPROCESS_HPP
