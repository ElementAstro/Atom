/**
 * @file test_skip.hpp
 * @brief Conditional test skipping and timeout support
 * @details Provides skip conditions, platform detection, and timeout utilities
 *
 * @author Max Qian
 * @copyright GPL3 License
 */

#ifndef ATOM_TEST_UTILITIES_TEST_SKIP_HPP
#define ATOM_TEST_UTILITIES_TEST_SKIP_HPP

#include <chrono>
#include <cstdlib>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <utility>

#include "atom/tests/core/test.hpp"

namespace atom::test {

/**
 * @brief Skip reason container
 */
struct SkipInfo {
    bool shouldSkip{false};
    std::string reason;

    explicit operator bool() const { return shouldSkip; }
};

/**
 * @brief Check if a condition is met for skipping
 * @param condition The condition to check
 * @param reason The reason for skipping
 * @return SkipInfo with skip status and reason
 */
inline auto skipIf(bool condition, std::string reason = "") -> SkipInfo {
    return {condition, std::move(reason)};
}

/**
 * @brief Check if a condition is NOT met for skipping
 * @param condition The condition to check
 * @param reason The reason for skipping
 * @return SkipInfo with skip status and reason
 */
inline auto skipUnless(bool condition, std::string reason = "") -> SkipInfo {
    return {!condition, std::move(reason)};
}

/**
 * @brief Platform detection helpers
 */
namespace platform {

#ifdef _WIN32
constexpr bool isWindows = true;
#else
constexpr bool isWindows = false;
#endif

#ifdef __linux__
constexpr bool isLinux = true;
#else
constexpr bool isLinux = false;
#endif

#ifdef __APPLE__
constexpr bool isMacOS = true;
#else
constexpr bool isMacOS = false;
#endif

#ifdef __unix__
constexpr bool isUnix = true;
#else
constexpr bool isUnix = false;
#endif

#ifdef _DEBUG
constexpr bool isDebug = true;
#else
constexpr bool isDebug = false;
#endif

#ifdef NDEBUG
constexpr bool isRelease = true;
#else
constexpr bool isRelease = false;
#endif

#if defined(__x86_64__) || defined(_M_X64)
constexpr bool is64Bit = true;
#else
constexpr bool is64Bit = false;
#endif

#if defined(__i386__) || defined(_M_IX86)
constexpr bool is32Bit = true;
#else
constexpr bool is32Bit = false;
#endif

#if defined(__arm__) || defined(_M_ARM)
constexpr bool isArm = true;
#else
constexpr bool isArm = false;
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
constexpr bool isArm64 = true;
#else
constexpr bool isArm64 = false;
#endif

#if defined(__GNUC__)
constexpr bool isGcc = true;
#else
constexpr bool isGcc = false;
#endif

#if defined(__clang__)
constexpr bool isClang = true;
#else
constexpr bool isClang = false;
#endif

#if defined(_MSC_VER)
constexpr bool isMsvc = true;
#else
constexpr bool isMsvc = false;
#endif

}  // namespace platform

/**
 * @brief Skip test on Windows
 */
inline auto skipOnWindows(std::string reason = "Not supported on Windows")
    -> SkipInfo {
    return skipIf(platform::isWindows, std::move(reason));
}

/**
 * @brief Skip test on Linux
 */
inline auto skipOnLinux(std::string reason = "Not supported on Linux")
    -> SkipInfo {
    return skipIf(platform::isLinux, std::move(reason));
}

/**
 * @brief Skip test on macOS
 */
inline auto skipOnMacOS(std::string reason = "Not supported on macOS")
    -> SkipInfo {
    return skipIf(platform::isMacOS, std::move(reason));
}

/**
 * @brief Skip test in debug builds
 */
inline auto skipInDebug(std::string reason = "Skipped in debug builds")
    -> SkipInfo {
    return skipIf(platform::isDebug, std::move(reason));
}

/**
 * @brief Skip test in release builds
 */
inline auto skipInRelease(std::string reason = "Skipped in release builds")
    -> SkipInfo {
    return skipIf(platform::isRelease, std::move(reason));
}

/**
 * @brief Skip test on 32-bit systems
 */
inline auto skipOn32Bit(std::string reason = "Skipped on 32-bit systems")
    -> SkipInfo {
    return skipIf(platform::is32Bit, std::move(reason));
}

/**
 * @brief Skip test on 64-bit systems
 */
inline auto skipOn64Bit(std::string reason = "Skipped on 64-bit systems")
    -> SkipInfo {
    return skipIf(platform::is64Bit, std::move(reason));
}

/**
 * @brief Environment variable check for skipping
 * @param envVar Environment variable name
 * @param expectedValue Expected value (empty means just check existence)
 * @return SkipInfo
 */
inline auto skipIfEnvSet(const std::string& envVar,
                         const std::string& expectedValue = "") -> SkipInfo {
    const char* value = std::getenv(envVar.c_str());
    bool shouldSkip = false;

    if (expectedValue.empty()) {
        shouldSkip = (value != nullptr);
    } else {
        shouldSkip = (value != nullptr && std::string(value) == expectedValue);
    }

    return {shouldSkip, "Environment variable " + envVar + " is set"};
}

/**
 * @brief Skip if environment variable is NOT set
 */
inline auto skipIfEnvNotSet(const std::string& envVar) -> SkipInfo {
    const char* value = std::getenv(envVar.c_str());
    return {value == nullptr, "Environment variable " + envVar + " is not set"};
}

/**
 * @brief Skip if CI environment detected
 */
inline auto skipInCI(std::string reason = "Skipped in CI environment")
    -> SkipInfo {
    bool isCI = std::getenv("CI") != nullptr ||
                std::getenv("GITHUB_ACTIONS") != nullptr ||
                std::getenv("TRAVIS") != nullptr ||
                std::getenv("JENKINS_HOME") != nullptr ||
                std::getenv("GITLAB_CI") != nullptr;
    return skipIf(isCI, std::move(reason));
}

/**
 * @brief Skip unless in CI environment
 */
inline auto skipUnlessCI(std::string reason = "Only runs in CI environment")
    -> SkipInfo {
    bool isCI = std::getenv("CI") != nullptr ||
                std::getenv("GITHUB_ACTIONS") != nullptr ||
                std::getenv("TRAVIS") != nullptr ||
                std::getenv("JENKINS_HOME") != nullptr ||
                std::getenv("GITLAB_CI") != nullptr;
    return skipUnless(isCI, std::move(reason));
}

/**
 * @brief Timeout configuration for tests
 */
struct TimeoutConfig {
    std::chrono::milliseconds duration{5000};
    bool failOnTimeout{true};
    std::string timeoutMessage{"Test timed out"};
};

/**
 * @brief Run a test function with timeout
 * @param func The test function to run
 * @param timeout Timeout duration
 * @return TestResult with timeout status
 */
template <typename Func>
auto runWithTimeout(Func&& func, std::chrono::milliseconds timeout)
    -> std::pair<bool, std::string> {
    std::promise<void> promise;
    auto future = promise.get_future();

    std::exception_ptr exceptionPtr;

    std::thread testThread([&]() {
        try {
            std::forward<Func>(func)();
            promise.set_value();
        } catch (...) {
            exceptionPtr = std::current_exception();
            promise.set_value();
        }
    });

    auto status = future.wait_for(timeout);

    if (status == std::future_status::timeout) {
        // Test timed out - detach thread (it will continue running)
        testThread.detach();
        return {false, "Test timed out after " +
                           std::to_string(timeout.count()) + "ms"};
    }

    testThread.join();

    if (exceptionPtr) {
        try {
            std::rethrow_exception(exceptionPtr);
        } catch (const std::exception& e) {
            return {false, std::string("Exception: ") + e.what()};
        } catch (...) {
            return {false, "Unknown exception"};
        }
    }

    return {true, ""};
}

/**
 * @brief Conditional test wrapper
 */
class ConditionalTest {
public:
    explicit ConditionalTest(std::string name) : name_(std::move(name)) {}

    /**
     * @brief Add a skip condition
     */
    auto skipIf(bool condition, std::string reason = "") -> ConditionalTest& {
        if (condition && !skipInfo_.shouldSkip) {
            skipInfo_ = {true, std::move(reason)};
        }
        return *this;
    }

    /**
     * @brief Add a skip condition with SkipInfo
     */
    auto skipIf(const SkipInfo& info) -> ConditionalTest& {
        if (info.shouldSkip && !skipInfo_.shouldSkip) {
            skipInfo_ = info;
        }
        return *this;
    }

    /**
     * @brief Set timeout for the test
     */
    auto withTimeout(std::chrono::milliseconds timeout) -> ConditionalTest& {
        timeout_ = timeout;
        return *this;
    }

    /**
     * @brief Set timeout in seconds
     */
    auto withTimeoutSeconds(int seconds) -> ConditionalTest& {
        timeout_ = std::chrono::seconds(seconds);
        return *this;
    }

    /**
     * @brief Set timeout in milliseconds
     */
    auto withTimeoutMs(int ms) -> ConditionalTest& {
        timeout_ = std::chrono::milliseconds(ms);
        return *this;
    }

    /**
     * @brief Run the test with the configured conditions
     */
    template <typename Func>
    void run(Func&& func) {
        if (skipInfo_.shouldSkip) {
            // Register as skipped test
            atom::test::registerTest(
                name_,
                [reason = skipInfo_.reason]() {
                    throw std::runtime_error("SKIPPED: " + reason);
                },
                false, 0.0, true);  // Mark as skipped
            return;
        }

        if (timeout_.has_value()) {
            auto wrappedFunc = [f = std::forward<Func>(func),
                                timeout = *timeout_]() {
                auto [success, message] = runWithTimeout(f, timeout);
                if (!success) {
                    throw std::runtime_error(message);
                }
            };
            atom::test::registerTest(name_, std::move(wrappedFunc));
        } else {
            atom::test::registerTest(name_, std::forward<Func>(func));
        }
    }

private:
    std::string name_;
    SkipInfo skipInfo_;
    std::optional<std::chrono::milliseconds> timeout_;
};

/**
 * @brief Create a conditional test
 */
inline auto conditionalTest(std::string name) -> ConditionalTest {
    return ConditionalTest(std::move(name));
}

/**
 * @brief Timeout assertion helper
 */
template <typename Func>
auto expectCompletesWithin(Func&& func, std::chrono::milliseconds timeout,
                           const char* file, int line) -> Expect {
    auto [success, message] = runWithTimeout(std::forward<Func>(func), timeout);
    return {success, file, line,
            success ? "Completed within timeout" : "Timeout: " + message};
}

/**
 * @brief Expect a function to timeout
 */
template <typename Func>
auto expectTimeout(Func&& func, std::chrono::milliseconds timeout,
                   const char* file, int line) -> Expect {
    auto [success, message] = runWithTimeout(std::forward<Func>(func), timeout);
    // We expect it to timeout, so success means failure
    return {!success, file, line,
            !success ? "Timed out as expected"
                     : "Expected timeout but completed successfully"};
}

/**
 * @brief Retry a function until success or max attempts
 */
template <typename Func>
auto retryUntilSuccess(Func&& func, size_t maxAttempts,
                       std::chrono::milliseconds delayBetweenAttempts = {})
    -> bool {
    for (size_t i = 0; i < maxAttempts; ++i) {
        try {
            std::forward<Func>(func)();
            return true;
        } catch (...) {
            if (i + 1 < maxAttempts && delayBetweenAttempts.count() > 0) {
                std::this_thread::sleep_for(delayBetweenAttempts);
            }
        }
    }
    return false;
}

/**
 * @brief Flaky test wrapper - retries on failure
 */
template <typename Func>
void flakyTest(Func&& func, size_t maxRetries = 3) {
    std::string lastError;
    for (size_t i = 0; i <= maxRetries; ++i) {
        try {
            std::forward<Func>(func)();
            return;  // Success
        } catch (const std::exception& e) {
            lastError = e.what();
            if (i < maxRetries) {
                std::cerr << "Test failed (attempt " << (i + 1) << "/"
                          << (maxRetries + 1) << "): " << lastError
                          << ". Retrying...\n";
            }
        }
    }
    throw std::runtime_error("Flaky test failed after " +
                             std::to_string(maxRetries + 1) +
                             " attempts. Last error: " + lastError);
}

}  // namespace atom::test

#define SKIP_IF(condition, reason)                                         \
    do {                                                                   \
        if (condition) {                                                   \
            throw std::runtime_error(std::string("SKIPPED: ") + (reason)); \
        }                                                                  \
    } while (0)

#define SKIP_UNLESS(condition, reason) SKIP_IF(!(condition), reason)

#define SKIP_ON_WINDOWS() \
    SKIP_IF(atom::test::platform::isWindows, "Not supported on Windows")
#define SKIP_ON_LINUX() \
    SKIP_IF(atom::test::platform::isLinux, "Not supported on Linux")
#define SKIP_ON_MACOS() \
    SKIP_IF(atom::test::platform::isMacOS, "Not supported on macOS")
#define SKIP_IN_DEBUG() \
    SKIP_IF(atom::test::platform::isDebug, "Skipped in debug builds")
#define SKIP_IN_RELEASE() \
    SKIP_IF(atom::test::platform::isRelease, "Skipped in release builds")
#define SKIP_ON_32BIT() \
    SKIP_IF(atom::test::platform::is32Bit, "Skipped on 32-bit systems")
#define SKIP_ON_64BIT() \
    SKIP_IF(atom::test::platform::is64Bit, "Skipped on 64-bit systems")
#define SKIP_IN_CI() \
    SKIP_IF(std::getenv("CI") != nullptr, "Skipped in CI environment")

#define expect_completes_within(func, timeout_ms) \
    atom::test::expectCompletesWithin(            \
        func, std::chrono::milliseconds(timeout_ms), __FILE__, __LINE__)

#define expect_times_out(func, timeout_ms)                                 \
    atom::test::expectTimeout(func, std::chrono::milliseconds(timeout_ms), \
                              __FILE__, __LINE__)

/**
 * @brief Define a test with timeout
 */
#define TEST_WITH_TIMEOUT(suite_name, test_name, timeout_ms)            \
    static void suite_name##_##test_name##_TestBody();                  \
    static struct suite_name##_##test_name##_Registrar {                \
        suite_name##_##test_name##_Registrar() {                        \
            atom::test::registerTest(#suite_name "." #test_name, []() { \
                auto [success, message] = atom::test::runWithTimeout(   \
                    suite_name##_##test_name##_TestBody,                \
                    std::chrono::milliseconds(timeout_ms));             \
                if (!success) {                                         \
                    throw std::runtime_error(message);                  \
                }                                                       \
            });                                                         \
        }                                                               \
    } suite_name##_##test_name##_registrar_instance;                    \
    static void suite_name##_##test_name##_TestBody()

/**
 * @brief Define a conditional test that may be skipped
 */
#define TEST_CONDITIONAL(suite_name, test_name, skip_condition, skip_reason)   \
    static void suite_name##_##test_name##_TestBody();                         \
    static struct suite_name##_##test_name##_Registrar {                       \
        suite_name##_##test_name##_Registrar() {                               \
            if (skip_condition) {                                              \
                atom::test::registerTest(                                      \
                    #suite_name "." #test_name,                                \
                    []() {                                                     \
                        throw std::runtime_error(std::string("SKIPPED: ") +    \
                                                 skip_reason);                 \
                    },                                                         \
                    false, 0.0, true);                                         \
            } else {                                                           \
                atom::test::registerTest(#suite_name "." #test_name,           \
                                         suite_name##_##test_name##_TestBody); \
            }                                                                  \
        }                                                                      \
    } suite_name##_##test_name##_registrar_instance;                           \
    static void suite_name##_##test_name##_TestBody()

/**
 * @brief Define a flaky test that retries on failure
 */
#define TEST_FLAKY(suite_name, test_name, max_retries)                     \
    static void suite_name##_##test_name##_TestBody();                     \
    static struct suite_name##_##test_name##_Registrar {                   \
        suite_name##_##test_name##_Registrar() {                           \
            atom::test::registerTest(#suite_name "." #test_name, []() {    \
                atom::test::flakyTest(suite_name##_##test_name##_TestBody, \
                                      max_retries);                        \
            });                                                            \
        }                                                                  \
    } suite_name##_##test_name##_registrar_instance;                       \
    static void suite_name##_##test_name##_TestBody()

#endif  // ATOM_TEST_UTILITIES_TEST_SKIP_HPP
