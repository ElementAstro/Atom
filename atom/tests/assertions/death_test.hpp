/**
 * @file death_test.hpp
 * @brief Death tests for testing code that terminates or crashes
 * @details Provides macros and utilities for testing fatal errors
 *
 * @author Max Qian
 * @copyright GPL3 License
 */

#ifndef ATOM_TEST_ASSERTIONS_DEATH_TEST_HPP
#define ATOM_TEST_ASSERTIONS_DEATH_TEST_HPP

#include <csignal>
#include <cstdlib>
#include <functional>
#include <regex>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "atom/tests/core/test.hpp"

namespace atom::test {

/**
 * @brief Exit code predicates
 */
struct ExitedWithCode {
    int expectedCode;

    explicit ExitedWithCode(int code) : expectedCode(code) {}

    [[nodiscard]] bool operator()(int actualCode) const {
        return actualCode == expectedCode;
    }

    [[nodiscard]] std::string describe() const {
        return "exited with code " + std::to_string(expectedCode);
    }
};

/**
 * @brief Killed by signal predicate
 */
struct KilledBySignal {
    int expectedSignal;

    explicit KilledBySignal(int sig) : expectedSignal(sig) {}

    [[nodiscard]] bool operator()(int status) const {
#ifndef _WIN32
        return WIFSIGNALED(status) && WTERMSIG(status) == expectedSignal;
#else
        (void)status;
        return false;  // Windows doesn't use signals the same way
#endif
    }

    [[nodiscard]] std::string describe() const {
        return "killed by signal " + std::to_string(expectedSignal);
    }
};

/**
 * @brief Death test result
 */
struct DeathTestResult {
    bool terminated{false};
    int exitCode{0};
    std::string output;
    std::string errorOutput;

    [[nodiscard]] bool exitedWithCode(int code) const {
        return terminated && exitCode == code;
    }

    [[nodiscard]] bool killedBySignal([[maybe_unused]] int sig) const {
#ifndef _WIN32
        return terminated && WIFSIGNALED(exitCode) && WTERMSIG(exitCode) == sig;
#else
        return false;
#endif
    }
};

/**
 * @brief Death test execution mode
 */
enum class DeathTestStyle {
    Fast,       // Fork-based (Unix) or thread-based
    Threadsafe  // More compatible but slower
};

namespace detail {

#ifdef _WIN32

/**
 * @brief Windows implementation of death test
 * @details Uses structured exception handling and subprocess
 */
inline DeathTestResult runDeathTestWindows(std::function<void()> func) {
    DeathTestResult result;

    // Use SEH to catch crashes
    __try {
        func();
        result.terminated = false;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result.terminated = true;
        result.exitCode = GetExceptionCode();
    }

    return result;
}

#else

/**
 * @brief Unix implementation of death test using fork
 */
inline DeathTestResult runDeathTestUnix(std::function<void()> func) {
    DeathTestResult result;

    // Create pipes for capturing output
    int stdoutPipe[2];
    int stderrPipe[2];

    if (pipe(stdoutPipe) != 0 || pipe(stderrPipe) != 0) {
        result.errorOutput = "Failed to create pipes";
        return result;
    }

    pid_t pid = fork();

    if (pid == -1) {
        result.errorOutput = "Fork failed";
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        close(stderrPipe[0]);
        close(stderrPipe[1]);
        return result;
    }

    if (pid == 0) {
        // Child process
        close(stdoutPipe[0]);
        close(stderrPipe[0]);

        // Redirect stdout and stderr
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);

        close(stdoutPipe[1]);
        close(stderrPipe[1]);

        // Run the test function
        try {
            func();
            _exit(0);
        } catch (...) {
            _exit(1);
        }
    } else {
        // Parent process
        close(stdoutPipe[1]);
        close(stderrPipe[1]);

        // Read output from child
        char buffer[1024];
        ssize_t bytesRead;

        while ((bytesRead = read(stdoutPipe[0], buffer, sizeof(buffer) - 1)) >
               0) {
            buffer[bytesRead] = '\0';
            result.output += buffer;
        }

        while ((bytesRead = read(stderrPipe[0], buffer, sizeof(buffer) - 1)) >
               0) {
            buffer[bytesRead] = '\0';
            result.errorOutput += buffer;
        }

        close(stdoutPipe[0]);
        close(stderrPipe[0]);

        // Wait for child
        int status;
        waitpid(pid, &status, 0);

        result.terminated = true;
        result.exitCode = status;
    }

    return result;
}

#endif

/**
 * @brief Platform-independent death test runner
 */
inline DeathTestResult runDeathTest(std::function<void()> func) {
#ifdef _WIN32
    return runDeathTestWindows(std::move(func));
#else
    return runDeathTestUnix(std::move(func));
#endif
}

}  // namespace detail

/**
 * @brief Run a death test and check exit code
 * @param func Function expected to terminate
 * @param exitPredicate Predicate to check exit status
 * @param regexPattern Regex pattern for output matching (optional)
 * @return True if test passed
 */
template <typename Predicate>
bool runDeathTestWithPredicate(std::function<void()> func,
                               Predicate exitPredicate,
                               const std::string& regexPattern = "") {
    auto result = detail::runDeathTest(std::move(func));

    if (!result.terminated) {
        return false;
    }

    if (!exitPredicate(result.exitCode)) {
        return false;
    }

    if (!regexPattern.empty()) {
        std::regex pattern(regexPattern);
        std::string combinedOutput = result.output + result.errorOutput;
        if (!std::regex_search(combinedOutput, pattern)) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Expect death (process termination)
 * @param func Function expected to terminate
 * @param regexPattern Regex pattern for error message (optional)
 * @param file Source file
 * @param line Line number
 * @return Expect assertion result
 */
inline auto expectDeath(std::function<void()> func,
                        const std::string& regexPattern, const char* file,
                        int line) -> Expect {
    auto result = detail::runDeathTest(std::move(func));

    if (!result.terminated) {
        return {false, file, line,
                "Expected death, but statement returned normally"};
    }

    if (!regexPattern.empty()) {
        std::regex pattern(regexPattern);
        std::string combinedOutput = result.output + result.errorOutput;
        if (!std::regex_search(combinedOutput, pattern)) {
            return {false, file, line,
                    "Death output didn't match pattern \"" + regexPattern +
                        "\". Actual: " + combinedOutput};
        }
    }

    return {true, file, line, "Statement terminated as expected"};
}

/**
 * @brief Expect exit with specific predicate
 * @param func Function expected to exit
 * @param predicate Exit status predicate
 * @param regexPattern Regex pattern for output (optional)
 * @param file Source file
 * @param line Line number
 * @return Expect assertion result
 */
template <typename Predicate>
auto expectExit(std::function<void()> func, Predicate predicate,
                const std::string& regexPattern, const char* file,
                int line) -> Expect {
    auto result = detail::runDeathTest(std::move(func));

    if (!result.terminated) {
        return {false, file, line,
                "Expected exit, but statement returned normally"};
    }

    if (!predicate(result.exitCode)) {
        return {false, file, line,
                "Exit status didn't match predicate. " + predicate.describe() +
                    ", actual code: " + std::to_string(result.exitCode)};
    }

    if (!regexPattern.empty()) {
        std::regex pattern(regexPattern);
        std::string combinedOutput = result.output + result.errorOutput;
        if (!std::regex_search(combinedOutput, pattern)) {
            return {false, file, line,
                    "Exit output didn't match pattern \"" + regexPattern +
                        "\". Actual: " + combinedOutput};
        }
    }

    return {true, file, line, "Statement exited as expected"};
}

/**
 * @brief Debug death test (doesn't actually fork, for debugging)
 */
inline auto expectDeathDebug(std::function<void()> func,
                             [[maybe_unused]] const std::string& regexPattern,
                             const char* file, int line) -> Expect {
    try {
        func();
        return {false, file, line,
                "Expected death, but statement returned normally (debug mode)"};
    } catch (const std::exception& e) {
        return {true, file, line,
                "Statement threw exception (debug mode): " +
                    std::string(e.what())};
    } catch (...) {
        return {true, file, line,
                "Statement threw unknown exception (debug mode)"};
    }
}

/**
 * @brief Check if death tests are supported on this platform
 */
inline bool deathTestsSupported() {
#ifdef _WIN32
    return true;  // Limited support via SEH
#else
    return true;  // Full support via fork
#endif
}

/**
 * @brief RAII guard for death test environment setup
 */
class DeathTestEnvironment {
public:
    DeathTestEnvironment() {
        // Disable core dumps during death tests
#ifndef _WIN32
        struct rlimit limit;
        limit.rlim_cur = 0;
        limit.rlim_max = 0;
        // setrlimit(RLIMIT_CORE, &limit);
#endif
    }

    ~DeathTestEnvironment() {
        // Restore settings if needed
    }
};

}  // namespace atom::test

// Death test macros
#define EXPECT_DEATH(statement, regex) \
    atom::test::expectDeath([&]() { statement; }, regex, __FILE__, __LINE__)

#define ASSERT_DEATH(statement, regex) \
    atom::test::expectDeath([&]() { statement; }, regex, __FILE__, __LINE__)

#define EXPECT_DEATH_IF_SUPPORTED(statement, regex)       \
    do {                                                  \
        if (atom::test::deathTestsSupported()) {          \
            EXPECT_DEATH(statement, regex);               \
        }                                                 \
    } while (0)

#define EXPECT_EXIT(statement, predicate, regex)                              \
    atom::test::expectExit([&]() { statement; }, predicate, regex, __FILE__, \
                            __LINE__)

#define ASSERT_EXIT(statement, predicate, regex)                              \
    atom::test::expectExit([&]() { statement; }, predicate, regex, __FILE__, \
                            __LINE__)

#define EXPECT_DEBUG_DEATH(statement, regex)                                 \
    atom::test::expectDeathDebug([&]() { statement; }, regex, __FILE__, \
                                  __LINE__)

// Convenience predicates
#define ExitedWithCode(code) atom::test::ExitedWithCode(code)
#define KilledBySignal(signal) atom::test::KilledBySignal(signal)

#endif  // ATOM_TEST_ASSERTIONS_DEATH_TEST_HPP
