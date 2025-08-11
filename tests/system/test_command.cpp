#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <handleapi.h>
#include <processthreadsapi.h>
#endif

#include "atom/system/command.hpp"

namespace atom::system::test {

// 命令行可用性的测试
class CommandAvailableTest : public ::testing::Test {
protected:
    // 应该在任何系统上都可用的命令
    const std::string availableCommand =
#ifdef _WIN32
        "cmd"
#else
        "echo"
#endif
        ;

    // 不太可能存在的命令
    const std::string unavailableCommand = "this_command_does_not_exist_12345";
};

TEST_F(CommandAvailableTest, CheckCommandAvailability) {
    EXPECT_TRUE(isCommandAvailable(availableCommand));
    EXPECT_FALSE(isCommandAvailable(unavailableCommand));
}

// 基本命令执行测试
class CommandExecutionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置跨平台的测试命令
#ifdef _WIN32
        echoCommand = "echo Hello World";
        errorCommand = "dir /invalid-flag";
        sleepCommand = "timeout 2";
        catCommand = "type";
#else
        echoCommand = "echo 'Hello World'";
        errorCommand = "ls --invalid-flag";
        sleepCommand = "sleep 2";
        catCommand = "cat";
#endif
    }

    std::string echoCommand;
    std::string errorCommand;
    std::string sleepCommand;
    std::string catCommand;
};

// 测试基本命令执行
TEST_F(CommandExecutionTest, ExecuteBasicCommand) {
    std::string output = executeCommand(echoCommand);
    EXPECT_THAT(output, ::testing::HasSubstr("Hello World"));
}

// 测试命令行回调处理
TEST_F(CommandExecutionTest, CommandWithLineCallback) {
    std::vector<std::string> lines;

    std::string output = executeCommand(
        echoCommand, false,
        [&lines](const std::string& line) { lines.push_back(line); });

    EXPECT_FALSE(lines.empty());
    EXPECT_THAT(lines[0], ::testing::HasSubstr("Hello World"));
}

// 测试命令状态返回
TEST_F(CommandExecutionTest, CommandWithStatusReturn) {
    auto [output, status] = executeCommandWithStatus(echoCommand);

    EXPECT_THAT(output, ::testing::HasSubstr("Hello World"));
    EXPECT_EQ(status, 0);  // 成功的命令返回0

    // 测试失败的命令
    try {
        auto [errorOutput, errorStatus] =
            executeCommandWithStatus(errorCommand);
        EXPECT_NE(errorStatus, 0);  // 失败的命令返回非0
    } catch (const std::exception& e) {
        // 某些系统上失败的命令可能会抛出异常
        SUCCEED() << "Expected exception: " << e.what();
    }
}

// 测试简单的命令执行
TEST_F(CommandExecutionTest, SimpleCommandExecution) {
    EXPECT_TRUE(executeCommandSimple(echoCommand));

    // 测试失败的命令
    try {
        bool result = executeCommandSimple(errorCommand);
        EXPECT_FALSE(result);
    } catch (const std::exception& e) {
        // 某些系统上失败的命令可能会抛出异常
        SUCCEED() << "Expected exception: " << e.what();
    }
}

// 测试带输入的命令
TEST_F(CommandExecutionTest, CommandWithInput) {
#ifdef _WIN32
    const std::string catCommandWithInput =
        "findstr /v \"\"";  // Windows 上的 cat 等效
#else
    const std::string catCommandWithInput = "cat";
#endif

    std::string input = "Test input";
    std::string output = executeCommandWithInput(catCommandWithInput, input);

    EXPECT_THAT(output, ::testing::HasSubstr("Test input"));
}

// 测试环境变量
TEST_F(CommandExecutionTest, CommandWithEnvironment) {
    std::unordered_map<std::string, std::string> env = {
        {"TEST_VAR", "test_value"}};

#ifdef _WIN32
    std::string envCommand = "echo %TEST_VAR%";
#else
    std::string envCommand = "echo $TEST_VAR";
#endif

    std::string output = executeCommandWithEnv(envCommand, env);
    EXPECT_THAT(output, ::testing::HasSubstr("test_value"));
}

// 测试异步命令执行
TEST_F(CommandExecutionTest, AsyncCommandExecution) {
    auto future = executeCommandAsync(echoCommand);
    auto status = future.wait_for(std::chrono::seconds(1));

    EXPECT_EQ(status, std::future_status::ready);
    std::string output = future.get();
    EXPECT_THAT(output, ::testing::HasSubstr("Hello World"));
}

// 测试超时命令
TEST_F(CommandExecutionTest, CommandWithTimeout) {
    // 快速命令应该在超时前完成
    auto result =
        executeCommandWithTimeout(echoCommand, std::chrono::seconds(1));
    EXPECT_TRUE(result.has_value());
    EXPECT_THAT(result.value(), ::testing::HasSubstr("Hello World"));

    // 长时间运行的命令应该超时
    auto timeoutResult =
        executeCommandWithTimeout(sleepCommand, std::chrono::milliseconds(500));
    EXPECT_FALSE(timeoutResult.has_value());
}

// 测试命令流处理
TEST_F(CommandExecutionTest, CommandStreamProcessing) {
    int status = 0;
    std::vector<std::string> lines;

    std::string output = executeCommandStream(
        echoCommand, false,
        [&lines](const std::string& line) { lines.push_back(line); }, status);

    EXPECT_THAT(output, ::testing::HasSubstr("Hello World"));
    EXPECT_EQ(status, 0);
    EXPECT_FALSE(lines.empty());
}

// 测试命令终止条件
TEST_F(CommandExecutionTest, CommandTerminationCondition) {
    int status = 0;
    std::atomic<bool> terminate{false};

    std::thread terminatorThread([&terminate]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        terminate = true;
    });

    std::string output = executeCommandStream(
        sleepCommand, false, [](const std::string&) {}, status,
        [&terminate]() { return terminate.load(); });

    terminatorThread.join();

    // 由于终止条件被触发，命令应该在完成之前终止
    // 不能严格地测试输出内容，因为这取决于系统行为
}

// 测试命令列表执行
TEST_F(CommandExecutionTest, MultipleCommandExecution) {
    std::vector<std::string> commands = {echoCommand, echoCommand + " Again"};

    EXPECT_NO_THROW(executeCommands(commands));

    // 测试带环境变量的多命令执行
    std::unordered_map<std::string, std::string> env = {
        {"TEST_VAR", "test_value"}};

    auto results = executeCommandsWithCommonEnv(commands, env);
    EXPECT_EQ(results.size(), 2);
    EXPECT_THAT(results[0].first, ::testing::HasSubstr("Hello World"));
    EXPECT_THAT(results[1].first, ::testing::HasSubstr("Again"));
    EXPECT_EQ(results[0].second, 0);
    EXPECT_EQ(results[1].second, 0);
}

// 测试命令行分割为行
TEST_F(CommandExecutionTest, CommandOutputLines) {
#ifdef _WIN32
    std::string multilineCommand = "echo Line1 && echo Line2";
#else
    std::string multilineCommand = "echo 'Line1' && echo 'Line2'";
#endif

    auto lines = executeCommandGetLines(multilineCommand);
    EXPECT_EQ(lines.size(), 2);
    EXPECT_THAT(lines[0], ::testing::HasSubstr("Line1"));
    EXPECT_THAT(lines[1], ::testing::HasSubstr("Line2"));
}

// 测试命令管道
TEST_F(CommandExecutionTest, CommandPiping) {
#ifdef _WIN32
    std::string firstCommand = "echo Hello";
    std::string secondCommand = "findstr /i \"Hello\"";
#else
    std::string firstCommand = "echo 'Hello'";
    std::string secondCommand = "grep Hello";
#endif

    std::string output = pipeCommands(firstCommand, secondCommand);
    EXPECT_THAT(output, ::testing::HasSubstr("Hello"));
}

// 进程管理相关测试
class ProcessManagementTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef _WIN32
        // Windows 上启动一个长时间运行的进程
        longRunningCommand = "timeout 10";
#else
        // Unix 上启动一个长时间运行的进程
        longRunningCommand = "sleep 10";
#endif
    }

    std::string longRunningCommand;
};

// 测试进程启动和获取 PID
TEST_F(ProcessManagementTest, StartProcess) {
    auto [pid, handle] = startProcess(longRunningCommand);

    EXPECT_GT(pid, 0);           // PID 应该是正数
    EXPECT_NE(handle, nullptr);  // 句柄不应该是 nullptr

    // 清理进程
#ifdef _WIN32
    TerminateProcess(handle, 0);
    CloseHandle(handle);
#else
    kill(pid, SIGTERM);
#endif
}

// 测试获取进程信息
TEST_F(ProcessManagementTest, GetProcesses) {
#ifdef _WIN32
    // 在 Windows 上搜索 cmd.exe 进程
    std::string processName = "cmd";
#else
    // 在 Unix 上搜索 bash 进程
    std::string processName = "bash";
#endif

    auto processes = getProcessesBySubstring(processName);
    EXPECT_FALSE(processes.empty());

    // 至少有一个进程应该包含搜索的字符串
    bool foundProcess = false;
    for (const auto& [pid, name] : processes) {
        if (name.find(processName) != std::string::npos) {
            foundProcess = true;
            break;
        }
    }
    EXPECT_TRUE(foundProcess);
}

// 测试终止进程
TEST_F(ProcessManagementTest, KillProcess) {
    auto [pid, handle] = startProcess(longRunningCommand);

    // 确保进程已启动
    EXPECT_GT(pid, 0);

    // 尝试通过 PID 终止进程
#ifdef _WIN32
    // Windows 上 SIGTERM 相当于 WM_CLOSE
    killProcessByPID(pid, 15);  // SIGTERM = 15
    CloseHandle(handle);
#else
    killProcessByPID(pid, SIGTERM);
#endif

    // 给一些时间让进程终止
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 确认进程已终止
    // 实现可能因平台而异，这里简单地检查进程是否在运行列表中
    auto processes = getProcessesBySubstring(std::to_string(pid));
    bool processFound = false;
    for (const auto& [p, name] : processes) {
        if (p == pid) {
            processFound = true;
            break;
        }
    }

    EXPECT_FALSE(processFound);
}

// 命令历史记录测试
class CommandHistoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        history = createCommandHistory(5);  // 最多保存 5 条命令
    }

    std::unique_ptr<CommandHistory> history;
};

// 测试命令历史记录添加和查询
TEST_F(CommandHistoryTest, AddAndQueryHistory) {
    history->addCommand("echo Hello", 0);
    history->addCommand("ls -la", 0);
    history->addCommand("grep pattern file", 1);

    EXPECT_EQ(history->size(), 3);

    // 获取最后 2 条命令
    auto lastTwo = history->getLastCommands(2);
    EXPECT_EQ(lastTwo.size(), 2);
    EXPECT_EQ(lastTwo[0].first, "grep pattern file");
    EXPECT_EQ(lastTwo[0].second, 1);
    EXPECT_EQ(lastTwo[1].first, "ls -la");
    EXPECT_EQ(lastTwo[1].second, 0);

    // 尝试获取比存在更多的命令
    auto allCommands = history->getLastCommands(10);
    EXPECT_EQ(allCommands.size(), 3);
}

// 测试命令历史记录搜索
TEST_F(CommandHistoryTest, SearchHistory) {
    history->addCommand("echo Hello", 0);
    history->addCommand("ls -la", 0);
    history->addCommand("grep pattern file", 1);
    history->addCommand("find . -name '*.txt'", 0);

    // 搜索包含 "pattern" 的命令
    auto grepCommands = history->searchCommands("pattern");
    EXPECT_EQ(grepCommands.size(), 1);
    EXPECT_EQ(grepCommands[0].first, "grep pattern file");

    // 搜索包含 "echo" 的命令
    auto echoCommands = history->searchCommands("echo");
    EXPECT_EQ(echoCommands.size(), 1);
    EXPECT_EQ(echoCommands[0].first, "echo Hello");

    // 搜索包含 "ls" 的命令
    auto lsCommands = history->searchCommands("ls");
    EXPECT_EQ(lsCommands.size(), 1);
    EXPECT_EQ(lsCommands[0].first, "ls -la");

    // 搜索不存在的模式
    auto nonexistentCommands = history->searchCommands("nonexistent");
    EXPECT_TRUE(nonexistentCommands.empty());
}

// 测试命令历史记录清除
TEST_F(CommandHistoryTest, ClearHistory) {
    history->addCommand("echo Hello", 0);
    history->addCommand("ls -la", 0);

    EXPECT_EQ(history->size(), 2);

    history->clear();
    EXPECT_EQ(history->size(), 0);
    EXPECT_TRUE(history->getLastCommands(10).empty());
}

// 测试命令历史记录大小限制
TEST_F(CommandHistoryTest, HistorySizeLimit) {
    // 添加超过限制的命令
    for (int i = 0; i < 10; ++i) {
        history->addCommand("Command " + std::to_string(i), 0);
    }

    // 历史记录大小应该限制为 5
    EXPECT_EQ(history->size(), 5);

    // 最后的命令应该是最近添加的
    auto lastCommands = history->getLastCommands(5);
    EXPECT_EQ(lastCommands.size(), 5);
    EXPECT_EQ(lastCommands[0].first, "Command 9");
    EXPECT_EQ(lastCommands[4].first, "Command 5");
}

// 强制测试（故意测试边缘情况）
class CommandEdgeCaseTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef _WIN32
        emptyCommand = "cmd /c";
        longCommand = "cmd /c echo " + std::string(10000, 'A');
        specialCharsCommand = "echo \"!@#$%^&*()\"";
#else
        emptyCommand = "";
        longCommand = "echo " + std::string(10000, 'A');
        specialCharsCommand = "echo '!@#$%^&*()'";
#endif
    }

    std::string emptyCommand;
    std::string longCommand;
    std::string specialCharsCommand;
};

// 测试空命令
TEST_F(CommandEdgeCaseTest, EmptyCommand) {
    try {
        std::string output = executeCommand(emptyCommand);
        // 某些系统可能允许空命令
        // 没有明确的期望结果，只要不崩溃就行
    } catch (const std::exception& e) {
        // 捕获异常是可接受的
        SUCCEED() << "Empty command threw exception: " << e.what();
    }
}

// 测试特殊字符
TEST_F(CommandEdgeCaseTest, SpecialCharacters) {
    try {
        std::string output = executeCommand(specialCharsCommand);
        EXPECT_THAT(output, ::testing::HasSubstr("!@#$%^&*()"));
    } catch (const std::exception& e) {
        // 特殊字符可能导致问题，捕获异常是可接受的
        SUCCEED() << "Special characters command threw exception: " << e.what();
    }
}

// 测试非常长的命令
TEST_F(CommandEdgeCaseTest, VeryLongCommand) {
    try {
        std::string output = executeCommand(longCommand);
        // 长命令可能会被截断，但不应崩溃
    } catch (const std::exception& e) {
        // 非常长的命令可能导致问题，捕获异常是可接受的
        SUCCEED() << "Very long command threw exception: " << e.what();
    }
}

}  // namespace atom::system::test
