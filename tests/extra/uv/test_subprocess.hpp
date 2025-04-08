#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <string>
#include <thread>

#include "atom/extra/uv/subprocess.hpp"

using namespace std::chrono_literals;
using ::testing::HasSubstr;
using ::testing::StartsWith;

class UvProcessTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary script file for testing
#ifdef _WIN32
        // Windows test script
        test_script_path_ = "test_script.bat";
        std::ofstream script(test_script_path_);
        script << "@echo off\n"
               << "echo Hello World\n"
               << "echo Test Error 1>&2\n"
               << "if \"%1\"==\"loop\" (\n"
               << "  for /L %%i in (1,1,5) do (\n"
               << "    echo Count: %%i\n"
               << "    timeout /t 1 > nul\n"
               << "  )\n"
               << ")\n"
               << "if \"%1\"==\"sleep\" timeout /t %2 > nul\n"
               << "if \"%1\"==\"exit\" exit %2\n"
               << "if \"%1\"==\"stdin\" (\n"
               << "  set /p INPUT=\n"
               << "  echo You entered: %INPUT%\n"
               << ")\n";
#else
        // Unix/Linux test script
        test_script_path_ = "./test_script.sh";
        std::ofstream script(test_script_path_);
        script << "#!/bin/sh\n"
               << "echo \"Hello World\"\n"
               << "echo \"Test Error\" >&2\n"
               << "if [ \"$1\" = \"loop\" ]; then\n"
               << "  for i in $(seq 1 5); do\n"
               << "    echo \"Count: $i\"\n"
               << "    sleep 1\n"
               << "  done\n"
               << "fi\n"
               << "if [ \"$1\" = \"sleep\" ]; then sleep \"$2\"; fi\n"
               << "if [ \"$1\" = \"exit\" ]; then exit \"$2\"; fi\n"
               << "if [ \"$1\" = \"stdin\" ]; then\n"
               << "  read INPUT\n"
               << "  echo \"You entered: $INPUT\"\n"
               << "fi\n";
        system("chmod +x ./test_script.sh");
#endif
        script.close();
    }

    void TearDown() override {
        // Remove the temporary script file
#ifdef _WIN32
        system(("del " + test_script_path_).c_str());
#else
        system(("rm " + test_script_path_).c_str());
#endif
    }

    std::string test_script_path_;
};

// Test basic process spawning
TEST_F(UvProcessTest, BasicSpawn) {
    UvProcess process;

    std::string stdout_data;
    std::string stderr_data;
    bool exit_called = false;
    int exit_code = -1;

    ASSERT_TRUE(process.spawn(
        test_script_path_, {}, "",
        [&exit_called, &exit_code](int64_t status, int signal) {
            exit_called = true;
            exit_code = static_cast<int>(status);
        },
        [&stdout_data](const char* data, ssize_t size) {
            stdout_data.append(data, size);
        },
        [&stderr_data](const char* data, ssize_t size) {
            stderr_data.append(data, size);
        }));

    EXPECT_TRUE(process.isRunning());

    // Wait for process to exit
    ASSERT_TRUE(process.waitForExit(5000));

    // Verify exit callback was called
    EXPECT_TRUE(exit_called);
    EXPECT_EQ(0, exit_code);

    // Verify stdout and stderr
    EXPECT_THAT(stdout_data, HasSubstr("Hello World"));
    EXPECT_THAT(stderr_data, HasSubstr("Test Error"));
}

// Test process with command line arguments
TEST_F(UvProcessTest, ProcessWithArgs) {
    UvProcess process;

    std::string stdout_data;
    std::atomic<bool> process_exited{false};
    std::condition_variable cv;
    std::mutex mutex;

    ASSERT_TRUE(process.spawn(
        test_script_path_, {"exit", "42"}, "",
        [&process_exited, &cv](int64_t status, int signal) {
            process_exited = true;
            cv.notify_one();
        },
        [&stdout_data](const char* data, ssize_t size) {
            stdout_data.append(data, size);
        }));

    // Wait for process to exit
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, 5s,
                    [&process_exited]() { return process_exited.load(); });
    }

    // Verify exit code
    EXPECT_EQ(42, process.getExitCode());
    EXPECT_EQ(UvProcess::ProcessStatus::EXITED, process.getStatus());
}

// Test process timeout
TEST_F(UvProcessTest, ProcessTimeout) {
    UvProcess process;

    bool timeout_called = false;
    std::atomic<bool> process_exited{false};
    std::condition_variable cv;
    std::mutex mutex;

    UvProcess::ProcessOptions options;
    options.file = test_script_path_;
    options.args = {"sleep", "10"};
    options.timeout = 500ms;

    ASSERT_TRUE(process.spawnWithOptions(
        options,
        [&process_exited, &cv](int64_t status, int signal) {
            process_exited = true;
            cv.notify_one();
        },
        nullptr, nullptr, [&timeout_called]() { timeout_called = true; }));

    // Wait for process to exit
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, 2s,
                    [&process_exited]() { return process_exited.load(); });
    }

    // Verify timeout callback was called
    EXPECT_TRUE(timeout_called);
    EXPECT_EQ(UvProcess::ProcessStatus::TIMED_OUT, process.getStatus());
}

// Test writing to process stdin
TEST_F(UvProcessTest, WriteToStdin) {
    UvProcess process;

    std::string stdout_data;
    std::atomic<bool> process_exited{false};
    std::condition_variable cv;
    std::mutex mutex;

    ASSERT_TRUE(process.spawn(
        test_script_path_, {"stdin"}, "",
        [&process_exited, &cv](int64_t status, int signal) {
            process_exited = true;
            cv.notify_one();
        },
        [&stdout_data](const char* data, ssize_t size) {
            stdout_data.append(data, size);
        }));

    // Write to stdin
    std::string input = "Hello from test\n";
    EXPECT_TRUE(process.writeToStdin(input));

    // Close stdin to signal we're done writing
    process.closeStdin();

    // Wait for process to exit
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, 5s,
                    [&process_exited]() { return process_exited.load(); });
    }

    // Verify stdout contains our input
    EXPECT_THAT(stdout_data, HasSubstr("You entered: Hello from test"));
}

// Test killing a process
TEST_F(UvProcessTest, KillProcess) {
    UvProcess process;

    std::atomic<bool> process_exited{false};
    std::condition_variable cv;
    std::mutex mutex;
    int term_signal = 0;

    ASSERT_TRUE(process.spawn(
        test_script_path_, {"loop"}, "",
        [&process_exited, &cv, &term_signal](int64_t status, int signal) {
            process_exited = true;
            term_signal = signal;
            cv.notify_one();
        }));

    // Let the process run for a bit
    std::this_thread::sleep_for(1s);

    // Kill the process
    EXPECT_TRUE(process.kill());

    // Wait for process to exit
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, 5s,
                    [&process_exited]() { return process_exited.load(); });
    }

    // Verify process was terminated
    EXPECT_TRUE(process_exited);
    EXPECT_NE(0, term_signal);
    EXPECT_EQ(UvProcess::ProcessStatus::TERMINATED, process.getStatus());
}

// Test force killing a process
TEST_F(UvProcessTest, KillForcefullyProcess) {
    UvProcess process;

    std::atomic<bool> process_exited{false};
    std::condition_variable cv;
    std::mutex mutex;

    ASSERT_TRUE(
        process.spawn(test_script_path_, {"loop"}, "",
                      [&process_exited, &cv](int64_t status, int signal) {
                          process_exited = true;
                          cv.notify_one();
                      }));

    // Let the process run for a bit
    std::this_thread::sleep_for(1s);

    // Kill the process forcefully
    EXPECT_TRUE(process.killForcefully());

    // Wait for process to exit
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, 5s,
                    [&process_exited]() { return process_exited.load(); });
    }

    // Verify process was terminated
    EXPECT_TRUE(process_exited);
    EXPECT_EQ(UvProcess::ProcessStatus::TERMINATED, process.getStatus());
}

// Test process with custom working directory
TEST_F(UvProcessTest, CustomWorkingDirectory) {
    UvProcess process;

    std::string stdout_data;
    std::atomic<bool> process_exited{false};
    std::condition_variable cv;
    std::mutex mutex;

    // Create a temporary directory and a script to print current directory
#ifdef _WIN32
    std::string temp_dir = "test_dir";
    system("mkdir test_dir 2>nul");
    std::string cwd_script = "test_dir\\cwd_test.bat";
    std::ofstream script(cwd_script);
    script << "@echo off\n"
           << "echo Current directory: %CD%\n";
#else
    std::string temp_dir = "./test_dir";
    system("mkdir -p ./test_dir");
    std::string cwd_script = "./test_dir/cwd_test.sh";
    std::ofstream script(cwd_script);
    script << "#!/bin/sh\n"
           << "echo \"Current directory: $(pwd)\"\n";
    system("chmod +x ./test_dir/cwd_test.sh");
#endif
    script.close();

    ASSERT_TRUE(process.spawn(
#ifdef _WIN32
        "test_dir\\cwd_test.bat",
#else
        "./cwd_test.sh",
#endif
        {}, temp_dir,
        [&process_exited, &cv](int64_t status, int signal) {
            process_exited = true;
            cv.notify_one();
        },
        [&stdout_data](const char* data, ssize_t size) {
            stdout_data.append(data, size);
        }));

    // Wait for process to exit
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, 5s,
                    [&process_exited]() { return process_exited.load(); });
    }

    // Verify working directory in output
    EXPECT_THAT(stdout_data, HasSubstr(temp_dir));

    // Clean up
#ifdef _WIN32
    system("rmdir /s /q test_dir");
#else
    system("rm -rf ./test_dir");
#endif
}

// Test environment variable setting
TEST_F(UvProcessTest, EnvironmentVariables) {
    UvProcess process;

    std::string stdout_data;
    std::atomic<bool> process_exited{false};
    std::condition_variable cv;
    std::mutex mutex;

    // Create a script to print an environment variable
#ifdef _WIN32
    std::string env_script = "env_test.bat";
    std::ofstream script(env_script);
    script << "@echo off\n"
           << "echo TEST_VAR is: %TEST_VAR%\n";
#else
    std::string env_script = "./env_test.sh";
    std::ofstream script(env_script);
    script << "#!/bin/sh\n"
           << "echo \"TEST_VAR is: $TEST_VAR\"\n";
    system("chmod +x ./env_test.sh");
#endif
    script.close();

    UvProcess::ProcessOptions options;
    options.file = env_script;
    options.env = {{"TEST_VAR", "test_value"}};
    options.inherit_parent_env = true;

    ASSERT_TRUE(process.spawnWithOptions(
        options,
        [&process_exited, &cv](int64_t status, int signal) {
            process_exited = true;
            cv.notify_one();
        },
        [&stdout_data](const char* data, ssize_t size) {
            stdout_data.append(data, size);
        }));

    // Wait for process to exit
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, 5s,
                    [&process_exited]() { return process_exited.load(); });
    }

    // Verify environment variable in output
    EXPECT_THAT(stdout_data, HasSubstr("TEST_VAR is: test_value"));

    // Clean up
#ifdef _WIN32
    system("del env_test.bat");
#else
    system("rm ./env_test.sh");
#endif
}

// Test process error handling
TEST_F(UvProcessTest, ErrorHandling) {
    UvProcess process;

    std::string error_message;

    // Try to spawn a non-existent executable
    process.setErrorCallback(
        [&error_message](const std::string& err) { error_message = err; });
    EXPECT_FALSE(process.spawn("non_existent_executable", {}, "", nullptr,
                               nullptr, nullptr));

    // Verify error callback was called
    EXPECT_FALSE(error_message.empty());
    EXPECT_EQ(UvProcess::ProcessStatus::ERROR, process.getStatus());
}

// Test process cleanup and reset
TEST_F(UvProcessTest, ProcessReset) {
    UvProcess process;

    std::atomic<bool> process_exited{false};
    std::condition_variable cv;
    std::mutex mutex;

    // First process
    ASSERT_TRUE(
        process.spawn(test_script_path_, {}, "",
                      [&process_exited, &cv](int64_t status, int signal) {
                          process_exited = true;
                          cv.notify_one();
                      }));

    // Wait for first process to exit
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, 5s,
                    [&process_exited]() { return process_exited.load(); });
    }

    // Reset the process
    process.reset();
    process_exited = false;

    // Second process
    ASSERT_TRUE(
        process.spawn(test_script_path_, {"exit", "123"}, "",
                      [&process_exited, &cv](int64_t status, int signal) {
                          process_exited = true;
                          cv.notify_one();
                      }));

    // Wait for second process to exit
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, 5s,
                    [&process_exited]() { return process_exited.load(); });
    }

    // Verify second process exit code
    EXPECT_EQ(123, process.getExitCode());
}

// Test move semantics
TEST_F(UvProcessTest, MoveSemantics) {
    auto process_creator = [this]() -> UvProcess {
        UvProcess process;

        std::string stdout_data;

        process.spawn(test_script_path_, {"loop"}, "", nullptr,
                      [&stdout_data](const char* data, ssize_t size) {
                          stdout_data.append(data, size);
                      });

        // Let the process run for a bit
        std::this_thread::sleep_for(1s);

        // Verify process is running and has output
        EXPECT_TRUE(process.isRunning());
        EXPECT_THAT(stdout_data, HasSubstr("Count: 1"));

        return process;
    };

    // Move construct
    UvProcess moved_process = process_creator();

    // Verify moved process is still running
    EXPECT_TRUE(moved_process.isRunning());

    // Kill the moved process
    EXPECT_TRUE(moved_process.killForcefully());

    // Wait for process to exit
    ASSERT_TRUE(moved_process.waitForExit(5000));

    // Test move assignment
    UvProcess another_process;

    {
        UvProcess temp_process;

        ASSERT_TRUE(temp_process.spawn(test_script_path_, {"sleep", "5"}, ""));

        EXPECT_TRUE(temp_process.isRunning());

        // Move assign
        another_process = std::move(temp_process);

        // temp_process is now in a valid but unspecified state, don't use it
    }

    // Verify another_process is running
    EXPECT_TRUE(another_process.isRunning());

    // Kill the process
    EXPECT_TRUE(another_process.killForcefully());
}

// Test getting process ID
TEST_F(UvProcessTest, GetProcessId) {
    UvProcess process;

    ASSERT_TRUE(process.spawn(test_script_path_, {"sleep", "2"}, ""));

    // Get and verify PID
    int pid = process.getPid();
    EXPECT_GT(pid, 0);

    // Kill the process
    EXPECT_TRUE(process.killForcefully());

    // Wait for process to exit
    ASSERT_TRUE(process.waitForExit(5000));

    // After exit, getPid should return -1
    EXPECT_EQ(-1, process.getPid());
}

// Test redirect stderr to stdout
TEST_F(UvProcessTest, RedirectStderrToStdout) {
    UvProcess process;

    std::string stdout_data;
    std::atomic<bool> process_exited{false};
    std::condition_variable cv;
    std::mutex mutex;

    UvProcess::ProcessOptions options;
    options.file = test_script_path_;
    options.redirect_stderr_to_stdout = true;

    ASSERT_TRUE(process.spawnWithOptions(
        options,
        [&process_exited, &cv](int64_t status, int signal) {
            process_exited = true;
            cv.notify_one();
        },
        [&stdout_data](const char* data, ssize_t size) {
            stdout_data.append(data, size);
        }));

    // Wait for process to exit
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, 5s,
                    [&process_exited]() { return process_exited.load(); });
    }

    // Verify both stdout and stderr content is in stdout_data
    EXPECT_THAT(stdout_data, HasSubstr("Hello World"));
    EXPECT_THAT(stdout_data, HasSubstr("Test Error"));
}

// Test detached process
TEST_F(UvProcessTest, DetachedProcess) {
    UvProcess process;

    UvProcess::ProcessOptions options;
    options.file = test_script_path_;
    options.args = {"sleep", "1"};
    options.detached = true;

    ASSERT_TRUE(process.spawnWithOptions(options));

    // Process should be running
    EXPECT_TRUE(process.isRunning());

    // Get PID for later verification
    int pid = process.getPid();
    EXPECT_GT(pid, 0);

    // Kill the process (even though it's detached, we can still control it)
    EXPECT_TRUE(process.killForcefully());

    // Wait for process to exit
    ASSERT_TRUE(process.waitForExit(5000));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}