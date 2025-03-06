#ifndef ATOM_UTILS_QPROCESS_HPP
#define ATOM_UTILS_QPROCESS_HPP

#include <chrono>
#include <concepts>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace atom::utils {

// Concept for valid duration types
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

    // Prevent copying to avoid resource management issues
    QProcess(const QProcess&) = delete;
    QProcess& operator=(const QProcess&) = delete;

    // Allow moving
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
        requires std::convertible_to<std::ranges::range_value_t<R>, std::string> || std::convertible_to<std::ranges::range_value_t<R>, const char *> 
    void setEnvironment(const R& env) {
        std::vector<std::string> env_vector;
        for (const auto& item : env) {
            validateEnvironmentVariable(item);
            env_vector.push_back(std::string(item));
        }
        setEnvironmentImpl(std::move(env_vector));
    }

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
        for (const auto& arg : args) {
            args_vector.push_back(std::string(arg));
        }
        startImpl(std::string(program), std::move(args_vector));
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
     * @brief Checks if the process is currently running.
     *
     * @return `true` if the process is running, `false` otherwise.
     */
    [[nodiscard]] auto isRunning() const noexcept -> bool;

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

private:
    // Validate environment variable format
    static void validateEnvironmentVariable(std::string_view var);

    // Implementation methods for template functions
    void setEnvironmentImpl(std::vector<std::string>&& env);
    void startImpl(std::string&& program, std::vector<std::string>&& args);
    auto waitForStartedImpl(int timeoutMs) -> bool;
    auto waitForFinishedImpl(int timeoutMs) -> bool;

    class Impl;  ///< Forward declaration of the implementation class
    std::unique_ptr<Impl> impl_;  ///< Pointer to the implementation details
};

}  // namespace atom::utils

#endif  // ATOM_UTILS_QPROCESS_HPP
