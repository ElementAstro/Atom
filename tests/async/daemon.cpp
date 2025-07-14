// filepath: atom/async/test_daemon.hpp
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "atom/async/daemon.hpp"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace fs = std::filesystem;
using namespace atom::async;

// Helper function to create a dummy main callback
int dummyMainCallback(int argc, char** argv) {
    spdlog::info("Dummy main callback executed. argc: {}", argc);
    for (int i = 0; i < argc; ++i) {
        if (argv[i]) {
            spdlog::info("  argv[{}]: {}", i, argv[i]);
        }
    }
    return 0;
}

// Helper function for modern dummy main callback
int dummyMainCallbackModern(std::span<char*> args) {
    spdlog::info("Dummy modern main callback executed. args.size(): {}",
                 args.size());
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i]) {
            spdlog::info("  args[{}]: {}", i, args[i]);
        }
    }
    return 0;
}

class DaemonTest : public ::testing::Test {
protected:
    fs::path test_pid_dir;
    fs::path test_pid_file;
    std::shared_ptr<spdlog::logger> test_logger;

    void SetUp() override {
        // Set up spdlog for testing
        spdlog::drop_all();
        auto console_sink =
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        test_logger = std::make_shared<spdlog::logger>("daemon_test_logger",
                                                       console_sink);
        spdlog::set_default_logger(test_logger);
        spdlog::set_level(spdlog::level::info);

        test_pid_dir = fs::temp_directory_path() / "atom_daemon_test";
        test_pid_file = test_pid_dir / "test_daemon.pid";

        if (fs::exists(test_pid_dir)) {
            fs::remove_all(test_pid_dir);
        }
        fs::create_directories(test_pid_dir);

        // Reset global state for each test
        g_pid_file_path = test_pid_file;
        std::atomic_store_explicit(&g_is_daemon, false,
                                   std::memory_order_relaxed);
        setDaemonRestartInterval(10);  // Reset to default
    }

    void TearDown() override {
        // Clean up PID file and directory
        if (fs::exists(test_pid_file)) {
            fs::remove(test_pid_file);
        }
        if (fs::exists(test_pid_dir)) {
            fs::remove_all(test_pid_dir);
        }
        // Ensure ProcessCleanupManager is clean
        ProcessCleanupManager::cleanup();
    }

    // Helper to read PID from file
    long readPidFromFile(const fs::path& path) {
        if (!fs::exists(path)) {
            return 0;
        }
        std::ifstream ifs(path);
        long pid = 0;
        ifs >> pid;
        return pid;
    }
};

TEST_F(DaemonTest, ProcessIdValid) {
    ProcessId current_pid = ProcessId::current();
    EXPECT_TRUE(current_pid.valid());

    ProcessId invalid_pid;
    EXPECT_FALSE(invalid_pid.valid());

#ifdef _WIN32
    ProcessId win_invalid_handle(INVALID_HANDLE_VALUE);
    EXPECT_FALSE(win_invalid_handle.valid());
#endif

    current_pid.reset();
    EXPECT_FALSE(current_pid.valid());
}

TEST_F(DaemonTest, DaemonGuardToString) {
    DaemonGuard guard;
    std::string str = guard.toString();
    EXPECT_NE(str.find("parentId=0"),
              std::string::npos);                        // Default initialized
    EXPECT_NE(str.find("mainId=0"), std::string::npos);  // Default initialized
    EXPECT_NE(str.find("restartCount=0"), std::string::npos);
}

TEST_F(DaemonTest, WritePidFile) {
    writePidFile(test_pid_file);
    EXPECT_TRUE(fs::exists(test_pid_file));
    long pid = readPidFromFile(test_pid_file);
#ifdef _WIN32
    EXPECT_EQ(pid, GetCurrentProcessId());
#else
    EXPECT_EQ(pid, getpid());
#endif
}

TEST_F(DaemonTest, WritePidFileCreatesDirectory) {
    fs::path non_existent_dir_pid_file = test_pid_dir / "subdir" / "new.pid";
    writePidFile(non_existent_dir_pid_file);
    EXPECT_TRUE(fs::exists(non_existent_dir_pid_file));
    EXPECT_TRUE(fs::exists(test_pid_dir / "subdir"));
}

TEST_F(DaemonTest, CheckPidFile) {
    // Test non-existent file
    EXPECT_FALSE(checkPidFile(test_pid_file));

    // Test with current process PID
    writePidFile(test_pid_file);
    EXPECT_TRUE(checkPidFile(test_pid_file));

    // Test with a non-running PID (e.g., 99999, unlikely to be running)
    fs::path dummy_pid_file = test_pid_dir / "dummy.pid";
    std::ofstream ofs(dummy_pid_file);
    ofs << "99999";
    ofs.close();
    EXPECT_FALSE(checkPidFile(dummy_pid_file));

    // Test with empty/invalid content
    std::ofstream ofs_empty(dummy_pid_file);
    ofs_empty << "";
    ofs_empty.close();
    EXPECT_FALSE(checkPidFile(dummy_pid_file));

    std::ofstream ofs_invalid(dummy_pid_file);
    ofs_invalid << "abc";
    ofs_invalid.close();
    EXPECT_FALSE(checkPidFile(dummy_pid_file));
}

TEST_F(DaemonTest, SetAndGetDaemonRestartInterval) {
    setDaemonRestartInterval(60);
    EXPECT_EQ(getDaemonRestartInterval(), 60);

    EXPECT_THROW(setDaemonRestartInterval(0), std::invalid_argument);
    EXPECT_THROW(setDaemonRestartInterval(-5), std::invalid_argument);
}

TEST_F(DaemonTest, SignalHandlerCleanup) {
    // Register a dummy PID file
    writePidFile(test_pid_file);
    EXPECT_TRUE(fs::exists(test_pid_file));

    // Call signal handler directly (simulating a signal)
    // This will exit the process, so we can't directly test cleanup in the same
    // process. Instead, we test the registration with ProcessCleanupManager.
    // The actual cleanup is verified by the DaemonGuard destructor and
    // ProcessCleanupManager::cleanup() which is called by the signal handler.
    // For unit testing, we can manually call cleanup and check.
    ProcessCleanupManager::cleanup();
    EXPECT_FALSE(fs::exists(test_pid_file));  // Should be removed
}

TEST_F(DaemonTest, RegisterSignalHandlers) {
    // Test with common signals
#ifdef _WIN32
    std::vector<int> signals = {SIGINT, SIGTERM};
#else
    std::vector<int> signals = {SIGINT, SIGTERM, SIGHUP};
#endif
    EXPECT_TRUE(registerSignalHandlers(signals));

    // Test with an invalid signal (if applicable, though signal() and
    // sigaction() usually handle this) This might not throw an error but just
    // fail to register.
    std::vector<int> invalid_signal = {-1};
    // Expect true because Windows signal() doesn't always fail for invalid
    // signals in test context and Unix sigaction() might not fail for all
    // invalid numbers but rather for invalid usage. The current implementation
    // logs a warning but returns true.
    EXPECT_TRUE(registerSignalHandlers(invalid_signal));
}

TEST_F(DaemonTest, IsProcessBackground) {
    // This is hard to test reliably in a unit test environment as it depends on
    // how the test runner is launched (e.g., with or without a console/TTY).
    // We can at least call it and ensure it doesn't crash.
    bool is_bg = isProcessBackground();
    // We can't assert true/false as it's environment dependent.
    // Just ensure it runs without error.
    (void)is_bg;
}

TEST_F(DaemonTest, DaemonGuardRealStart) {
    DaemonGuard guard;
    guard.setPidFilePath(test_pid_file);

    char arg0[] = "test_program";
    char arg1[] = "arg1";
    char* argv[] = {arg0, arg1, nullptr};
    int argc = 2;

    int result = guard.realStart(argc, argv, dummyMainCallback);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(fs::exists(test_pid_file));
    EXPECT_TRUE(
        guard.isRunning());  // Should be running as it's the current process
}

TEST_F(DaemonTest, DaemonGuardRealStartModern) {
    DaemonGuard guard;
    guard.setPidFilePath(test_pid_file);

    char arg0[] = "test_program";
    char arg1[] = "arg1";
    std::vector<char*> args_vec = {arg0, arg1};
    std::span<char*> args(args_vec.data(), args_vec.size());

    int result = guard.realStartModern(args, dummyMainCallbackModern);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(fs::exists(test_pid_file));
    EXPECT_TRUE(guard.isRunning());
}

TEST_F(DaemonTest, DaemonGuardStartDaemonNonDaemonMode) {
    DaemonGuard guard;
    guard.setPidFilePath(test_pid_file);

    char arg0[] = "test_program";
    char arg1[] = "arg1";
    char* argv[] = {arg0, arg1, nullptr};
    int argc = 2;

    // Test non-daemon mode (isDaemonParam = false)
    int result = guard.startDaemon(argc, argv, dummyMainCallback, false);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(g_is_daemon.load(std::memory_order_relaxed));
    EXPECT_TRUE(fs::exists(test_pid_file));
    EXPECT_TRUE(guard.isRunning());
}

TEST_F(DaemonTest, DaemonGuardStartDaemonModernNonDaemonMode) {
    DaemonGuard guard;
    guard.setPidFilePath(test_pid_file);

    char arg0[] = "test_program";
    char arg1[] = "arg1";
    std::vector<char*> args_vec = {arg0, arg1};
    std::span<char*> args(args_vec.data(), args_vec.size());

    // Test non-daemon mode (isDaemonParam = false)
    int result = guard.startDaemonModern(args, dummyMainCallbackModern, false);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(g_is_daemon.load(std::memory_order_relaxed));
    EXPECT_TRUE(fs::exists(test_pid_file));
    EXPECT_TRUE(guard.isRunning());
}

// Daemon mode tests are tricky because they involve forking/detaching
// processes. These tests typically require a separate executable or careful
// mocking. For now, we'll test the parent's behavior and the initial setup. The
// actual child process execution is hard to verify in a single unit test.

TEST_F(DaemonTest, DaemonGuardRealDaemonParentBehavior) {
    DaemonGuard guard;
    guard.setPidFilePath(test_pid_file);

    char arg0[] = "test_program";
    char* argv[] = {arg0, nullptr};
    int argc = 1;

#ifdef _WIN32
    // On Windows, CreateProcessA is called, and the parent exits.
    // We can't verify the child's state directly here.
    // The return value should be 0 for the parent.
    int result = guard.realDaemon(argc, argv, dummyMainCallback);
    EXPECT_EQ(result, 0);
    // PID file is written by the child, so it won't exist immediately in parent
    EXPECT_FALSE(fs::exists(test_pid_file));
#else
    // On Unix, fork() is called. The parent process returns 0 and exits.
    // The child process continues.
    // To test this, we need to mock fork() or run this in a separate process.
    // For a basic unit test, we can only check the return value.
    // If fork fails, it throws. If it succeeds, parent returns 0.
    // The actual daemonization (setsid, chdir, close stdio) happens in the
    // child.
    int result = guard.realDaemon(argc, argv, dummyMainCallback);
    EXPECT_EQ(result, 0);
    // PID file is written by the child, so it won't exist immediately in parent
    EXPECT_FALSE(fs::exists(test_pid_file));
#endif
}

TEST_F(DaemonTest, DaemonGuardRealDaemonModernParentBehavior) {
    DaemonGuard guard;
    guard.setPidFilePath(test_pid_file);

    char arg0[] = "test_program";
    std::vector<char*> args_vec = {arg0};
    std::span<char*> args(args_vec.data(), args_vec.size());

#ifdef _WIN32
    int result = guard.realDaemonModern(args, dummyMainCallbackModern);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(fs::exists(test_pid_file));
#else
    int result = guard.realDaemonModern(args, dummyMainCallbackModern);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(fs::exists(test_pid_file));
#endif
}

TEST_F(DaemonTest, DaemonGuardIsRunning) {
    DaemonGuard guard;
    // Initially not running
    EXPECT_FALSE(guard.isRunning());

    // Simulate a running process by setting m_mainId to current process
    // This is a hack for testing, as m_mainId is usually set by
    // realStart/realDaemon
    ProcessId current_pid = ProcessId::current();
    guard.setMainId(current_pid);  // Use the new setter

    EXPECT_TRUE(guard.isRunning());

    // Simulate an invalid process ID
    guard.setMainId({});  // Use the new setter with a default-constructed
                          // (invalid) ProcessId
    EXPECT_FALSE(guard.isRunning());
}

TEST_F(DaemonTest, DaemonGuardPidFilePath) {
    DaemonGuard guard;
    EXPECT_FALSE(guard.getPidFilePath().has_value());

    guard.setPidFilePath(test_pid_file);
    EXPECT_TRUE(guard.getPidFilePath().has_value());
    EXPECT_EQ(guard.getPidFilePath().value(), test_pid_file);
}

TEST_F(DaemonTest, DaemonGuardDestructorCleanupInfo) {
    // This test primarily checks if the destructor logs correctly when a PID
    // file exists. The actual file removal is handled by ProcessCleanupManager,
    // which is called by signalHandler. We can't directly test the destructor
    // removing the file here because it's designed to defer to the global
    // cleanup manager.

    // Create a PID file
    writePidFile(test_pid_file);
    EXPECT_TRUE(fs::exists(test_pid_file));

    {
        DaemonGuard guard;
        guard.setPidFilePath(test_pid_file);
        // When guard goes out of scope, its destructor will be called.
        // It should log that cleanup is deferred.
    }
    // The file should still exist because ProcessCleanupManager::cleanup() was
    // not called.
    EXPECT_TRUE(fs::exists(test_pid_file));
    // Manually clean up for the next test
    ProcessCleanupManager::cleanup();
    EXPECT_FALSE(fs::exists(test_pid_file));
}

TEST_F(DaemonTest, ProcessCleanupManager) {
    // Ensure cleanup manager is empty initially
    ProcessCleanupManager::cleanup();  // Clear any previous registrations

    fs::path pid_file1 = test_pid_dir / "pid1.pid";
    fs::path pid_file2 = test_pid_dir / "pid2.pid";

    // Register files
    writePidFile(pid_file1);  // This registers it internally
    ProcessCleanupManager::registerPidFile(pid_file2);  // Manually register

    EXPECT_TRUE(fs::exists(pid_file1));
    // pid_file2 won't exist until writePidFile is called for it
    // For this test, we just care about registration and cleanup
    std::ofstream ofs(pid_file2);
    ofs << "123";
    ofs.close();
    EXPECT_TRUE(fs::exists(pid_file2));

    // Perform cleanup
    ProcessCleanupManager::cleanup();

    // Both files should be removed
    EXPECT_FALSE(fs::exists(pid_file1));
    EXPECT_FALSE(fs::exists(pid_file2));

    // Calling cleanup again should be safe and do nothing
    ProcessCleanupManager::cleanup();
}

TEST_F(DaemonTest, DaemonExceptionSourceLocation) {
    try {
        throw DaemonException("Test exception");
    } catch (const DaemonException& e) {
        std::string what_str = e.what();
        EXPECT_NE(what_str.find("Test exception"), std::string::npos);
        EXPECT_NE(what_str.find("test_daemon.hpp"), std::string::npos);
        EXPECT_NE(what_str.find("DaemonExceptionSourceLocation"),
                  std::string::npos);
    }
}

// Test invalid argc/argv for realStart and startDaemon
TEST_F(DaemonTest, InvalidArgsRealStart) {
    DaemonGuard guard;
    char** null_argv = nullptr;
    EXPECT_THROW(guard.realStart(1, null_argv, dummyMainCallback),
                 DaemonException);
    // Test with argc = 0 and argv = nullptr (should be fine)
    EXPECT_NO_THROW(guard.realStart(0, null_argv, dummyMainCallback));
}

TEST_F(DaemonTest, InvalidArgsRealStartModern) {
    DaemonGuard guard;
    std::span<char*> empty_args;
    EXPECT_THROW(guard.realStartModern(empty_args, dummyMainCallbackModern),
                 DaemonException);

    char* null_arg = nullptr;
    std::span<char*> null_first_arg(&null_arg, 1);
    EXPECT_THROW(guard.realStartModern(null_first_arg, dummyMainCallbackModern),
                 DaemonException);
}

TEST_F(DaemonTest, InvalidArgsStartDaemon) {
    DaemonGuard guard;
    char** null_argv = nullptr;
    EXPECT_THROW(guard.startDaemon(1, null_argv, dummyMainCallback, false),
                 DaemonException);

    // Test with argc = 0 and argv = nullptr (should be fine)
    EXPECT_NO_THROW(guard.startDaemon(0, null_argv, dummyMainCallback, false));

    // Test with negative argc (should warn and set to 0)
    char arg0[] = "test_program";
    char* argv[] = {arg0, nullptr};
    EXPECT_NO_THROW(guard.startDaemon(-5, argv, dummyMainCallback, false));
}

TEST_F(DaemonTest, InvalidArgsStartDaemonModern) {
    DaemonGuard guard;
    std::span<char*> empty_args;
    EXPECT_THROW(
        guard.startDaemonModern(empty_args, dummyMainCallbackModern, false),
        DaemonException);

    char* null_arg = nullptr;
    std::span<char*> null_first_arg(&null_arg, 1);
    EXPECT_THROW(
        guard.startDaemonModern(null_first_arg, dummyMainCallbackModern, false),
        DaemonException);
}

// Note: Testing `realDaemon` and `startDaemon` in daemon mode (where
// `isDaemonParam` is true) is inherently difficult in a standard unit test
// framework because these functions are designed to fork/detach the process and
// potentially exit the parent. This would terminate the test runner. Proper
// testing of daemonization usually involves:
// 1. Running the daemon logic in a separate, isolated process.
// 2. Using process control mechanisms (e.g., `waitpid` on Unix,
// `OpenProcess`/`GetExitCodeProcess` on Windows)
//    to monitor the child process from the test runner.
// 3. Checking for the existence of PID files and other side effects.
// 4. Potentially sending signals to the daemon to test shutdown.
// These are more akin to integration tests than unit tests.
// The current tests cover the non-daemon path and the initial setup/error
// handling of the daemon path in the parent process.
