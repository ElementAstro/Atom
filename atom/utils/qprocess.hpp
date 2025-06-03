#ifndef ATOM_UTILS_QPROCESS_HPP
#define ATOM_UTILS_QPROCESS_HPP

#include <chrono>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace atom::utils {

/**
 * @brief Concept for valid duration types
 */
template <typename T>
concept DurationType = requires(T t) {
    {
        std::chrono::duration_cast<std::chrono::milliseconds>(t)
    } -> std::same_as<std::chrono::milliseconds>;
};

/**
 * @brief A class to manage and interact with external processes.
 *
 * The `QProcess` class provides methods to start and control external
 * processes. It allows setting working directories, managing environment
 * variables, and reading from or writing to the process's standard output and
 * error streams.
 */
class QProcess {
public:
    /**
     * @brief Enum representing the possible states of a QProcess.
     */
    enum class ProcessState {
        NotRunning,  ///< The process is not running
        Starting,    ///< The process is starting but not yet running
        Running      ///< The process is running
    };

    /**
     * @brief Enum representing the possible error states of a QProcess.
     */
    enum class ProcessError {
        NoError,        ///< No error occurred
        FailedToStart,  ///< Process failed to start
        Crashed,        ///< Process crashed after starting
        Timedout,       ///< Process operation timed out
        ReadError,      ///< Error reading from the process
        WriteError,     ///< Error writing to the process
        UnknownError    ///< An unknown error occurred
    };

    /**
     * @brief Enum representing the exit status of a QProcess.
     */
    enum class ExitStatus {
        NormalExit,  ///< Process exited normally
        CrashExit    ///< Process crashed
    };

    /**
     * @brief Callback function type for process started events.
     */
    using StartedCallback = std::function<void()>;

    /**
     * @brief Callback function type for process finished events.
     *
     * @param exitCode The exit code of the process
     * @param exitStatus The exit status (normal or crash)
     */
    using FinishedCallback =
        std::function<void(int exitCode, ExitStatus exitStatus)>;

    /**
     * @brief Callback function type for process error events.
     *
     * @param error The error that occurred
     */
    using ErrorCallback = std::function<void(ProcessError error)>;

    /**
     * @brief Callback function type for reading standard output data.
     *
     * @param data The data read from standard output
     */
    using ReadyReadStandardOutputCallback =
        std::function<void(std::string_view data)>;

    /**
     * @brief Callback function type for reading standard error data.
     *
     * @param data The data read from standard error
     */
    using ReadyReadStandardErrorCallback =
        std::function<void(std::string_view data)>;

    /**
     * @brief Default constructor for `QProcess`.
     *
     * Initializes a new `QProcess` instance. The process is not yet started and
     * no environment or working directory is set.
     */
    QProcess();

    /**
     * @brief Destructor for `QProcess`.
     *
     * Cleans up resources used by the `QProcess` instance. If the process is
     * still running, it will be terminated.
     */
    ~QProcess() noexcept;

    QProcess(const QProcess&) = delete;
    QProcess& operator=(const QProcess&) = delete;

    QProcess(QProcess&&) noexcept;
    QProcess& operator=(QProcess&&) noexcept;

    /**
     * @brief Sets the working directory for the process.
     *
     * @param dir The path to the working directory for the process.
     * @throws std::invalid_argument If directory does not exist or is not
     * accessible.
     *
     * This method sets the directory where the process will start. If not set,
     * the process will use the current working directory.
     */
    void setWorkingDirectory(std::string_view dir);

    /**
     * @brief Gets the current working directory for the process.
     *
     * @return The current working directory or std::nullopt if not set.
     */
    [[nodiscard]] auto workingDirectory() const -> std::optional<std::string>;

    /**
     * @brief Sets the environment variables for the process.
     *
     * @param env A range of environment variables as strings.
     * @throws std::invalid_argument If any environment variables are malformed.
     *
     * This method sets the environment variables that will be used by the
     * process. If not set, the process will inherit the environment of the
     * parent process.
     */
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>,
                                     std::string> ||
                 std::convertible_to<std::ranges::range_value_t<R>, const char*>
    void setEnvironment(const R& env) {
        std::vector<std::string> env_vector;
        env_vector.reserve(std::ranges::size(env));
        for (const auto& item : env) {
            validateEnvironmentVariable(item);
            env_vector.emplace_back(item);
        }
        setEnvironmentImpl(std::move(env_vector));
    }

    /**
     * @brief Gets the current environment variables for the process.
     *
     * @return The current environment variables.
     */
    [[nodiscard]] auto environment() const -> std::vector<std::string>;

    /**
     * @brief Starts the external process with the given program and arguments.
     *
     * @param program The path to the executable program to start.
     * @param args A range of arguments to pass to the program.
     * @throws std::runtime_error If process start fails or is already running.
     * @throws std::invalid_argument If program path is invalid.
     *
     * This method starts the process with the specified program and arguments.
     * The process will begin execution asynchronously.
     */
    template <std::ranges::input_range R = std::vector<std::string>>
        requires std::convertible_to<std::ranges::range_value_t<R>, std::string>
    void start(std::string_view program, const R& args = {}) {
        if (program.empty()) {
            throw std::invalid_argument("Program path cannot be empty");
        }

        std::vector<std::string> args_vector;
        args_vector.reserve(std::ranges::size(args));
        for (const auto& arg : args) {
            args_vector.emplace_back(arg);
        }
        startImpl(std::string(program), std::move(args_vector));
    }

    /**
     * @brief Starts the external process in detached mode.
     *
     * @param program The path to the executable program to start.
     * @param args A range of arguments to pass to the program.
     * @return true if the process was started successfully, false otherwise.
     *
     * In detached mode, the process will run independently of the parent
     * process and will not be terminated when the parent process exits.
     */
    template <std::ranges::input_range R = std::vector<std::string>>
        requires std::convertible_to<std::ranges::range_value_t<R>, std::string>
    bool startDetached(std::string_view program, const R& args = {}) {
        if (program.empty()) {
            return false;
        }

        std::vector<std::string> args_vector;
        args_vector.reserve(std::ranges::size(args));
        for (const auto& arg : args) {
            args_vector.emplace_back(arg);
        }
        return startDetachedImpl(std::string(program), std::move(args_vector));
    }

    /**
     * @brief Waits for the process to start.
     *
     * @tparam Rep The representation type of the duration count
     * @tparam Period The period of the duration
     * @param timeout The maximum amount of time to wait. Negative values wait
     * indefinitely.
     *
     * @return `true` if the process has started within the specified timeout,
     * `false` otherwise.
     */
    template <typename Rep, typename Period>
    auto waitForStarted(std::chrono::duration<Rep, Period> timeout =
                            std::chrono::milliseconds(-1)) -> bool {
        auto timeoutMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(timeout)
                .count();
        return waitForStartedImpl(static_cast<int>(timeoutMs));
    }

    /**
     * @brief Waits for the process to finish.
     *
     * @tparam Rep The representation type of the duration count
     * @tparam Period The period of the duration
     * @param timeout The maximum amount of time to wait. Negative values wait
     * indefinitely.
     *
     * @return `true` if the process has finished within the specified timeout,
     * `false` otherwise.
     */
    template <typename Rep, typename Period>
    auto waitForFinished(std::chrono::duration<Rep, Period> timeout =
                             std::chrono::milliseconds(-1)) -> bool {
        auto timeoutMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(timeout)
                .count();
        return waitForFinishedImpl(static_cast<int>(timeoutMs));
    }

    /**
     * @brief Executes a program synchronously.
     *
     * @tparam Rep The representation type of the duration count
     * @tparam Period The period of the duration
     * @param program The path to the executable program to start.
     * @param args A range of arguments to pass to the program.
     * @param timeout The maximum amount of time to wait for the process to
     * finish.
     * @return The exit code of the process, or -1 if the process times out or
     * fails to start.
     *
     * This is a convenience method that starts the process, waits for it to
     * finish, and returns the exit code.
     */
    template <std::ranges::input_range R = std::vector<std::string>,
              typename Rep = int, typename Period = std::ratio<1>>
        requires std::convertible_to<std::ranges::range_value_t<R>, std::string>
    int execute(
        std::string_view program, const R& args = {},
        std::chrono::duration<Rep, Period> timeout = std::chrono::seconds(-1)) {
        start(program, args);
        if (!waitForStarted(std::chrono::seconds(30))) {
            return -1;
        }

        if (!waitForFinished(timeout)) {
            terminate();
            return -1;
        }

        return exitCode();
    }

    /**
     * @brief Kills the process with immediate effect.
     *
     * This method forcefully kills the process if it is running.
     * It's more aggressive than terminate().
     */
    void kill() noexcept;

    /**
     * @brief Checks if the process is currently running.
     *
     * @return `true` if the process is running, `false` otherwise.
     */
    [[nodiscard]] auto isRunning() const noexcept -> bool;

    /**
     * @brief Gets the current state of the process.
     *
     * @return The current process state.
     */
    [[nodiscard]] auto state() const noexcept -> ProcessState;

    /**
     * @brief Gets the last error that occurred.
     *
     * @return The last process error.
     */
    [[nodiscard]] auto error() const noexcept -> ProcessError;

    /**
     * @brief Gets the exit code of the process.
     *
     * @return The exit code, or -1 if the process is still running or never
     * started.
     */
    [[nodiscard]] auto exitCode() const noexcept -> int;

    /**
     * @brief Gets the exit status of the process.
     *
     * @return The exit status.
     */
    [[nodiscard]] auto exitStatus() const noexcept -> ExitStatus;

    /**
     * @brief Writes data to the process's standard input.
     *
     * @param data The data to write to the process's standard input.
     * @throws std::runtime_error If write operation fails.
     *
     * This method sends data to the process's standard input stream.
     */
    void write(std::string_view data);

    /**
     * @brief Closes the process's standard input.
     *
     * This method closes the write channel to the process,
     * indicating no more data will be sent.
     */
    void closeWriteChannel();

    /**
     * @brief Reads all available data from the process's standard output.
     *
     * @return A string containing all data read from the process's standard
     * output.
     * @throws std::runtime_error If read operation fails.
     */
    [[nodiscard]] auto readAllStandardOutput() -> std::string;

    /**
     * @brief Reads all available data from the process's standard error.
     *
     * @return A string containing all data read from the process's standard
     * error.
     * @throws std::runtime_error If read operation fails.
     */
    [[nodiscard]] auto readAllStandardError() -> std::string;

    /**
     * @brief Terminates the process.
     *
     * This method forcefully terminates the process if it is still running.
     */
    void terminate() noexcept;

    /**
     * @brief Sets the callback function for process started events.
     *
     * @param callback The function to call when the process starts.
     */
    void setStartedCallback(StartedCallback callback);

    /**
     * @brief Sets the callback function for process finished events.
     *
     * @param callback The function to call when the process finishes.
     */
    void setFinishedCallback(FinishedCallback callback);

    /**
     * @brief Sets the callback function for process error events.
     *
     * @param callback The function to call when an error occurs.
     */
    void setErrorCallback(ErrorCallback callback);

    /**
     * @brief Sets the callback function for standard output data.
     *
     * @param callback The function to call when data is available on standard
     * output.
     */
    void setReadyReadStandardOutputCallback(
        ReadyReadStandardOutputCallback callback);

    /**
     * @brief Sets the callback function for standard error data.
     *
     * @param callback The function to call when data is available on standard
     * error.
     */
    void setReadyReadStandardErrorCallback(
        ReadyReadStandardErrorCallback callback);

private:
    static void validateEnvironmentVariable(std::string_view var);

    void setEnvironmentImpl(std::vector<std::string>&& env);
    void startImpl(std::string&& program, std::vector<std::string>&& args);
    bool startDetachedImpl(std::string&& program,
                           std::vector<std::string>&& args);
    auto waitForStartedImpl(int timeoutMs) -> bool;
    auto waitForFinishedImpl(int timeoutMs) -> bool;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace atom::utils

#endif  // ATOM_UTILS_QPROCESS_HPP
