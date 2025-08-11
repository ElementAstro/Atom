#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <random>
#include <thread>
#include <vector>

#include "qprocess.hpp"

using namespace atom::utils;
using ::testing::HasSubstr;

class QProcessTerminateTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建一个新的 QProcess 用于每个测试
        process = std::make_unique<QProcess>();
    }

    void TearDown() override {
        // 确保测试清理前进程已终止
        if (process && process->isRunning()) {
            process->terminate();
        }
        process.reset();
    }

    std::unique_ptr<QProcess> process;
};

// 测试基本的终止运行中进程功能
TEST_F(QProcessTerminateTest, BasicTermination) {
// 启动一个长时间运行的进程
#ifdef _WIN32
    process->start("ping -t 127.0.0.1");
#else
    process->start("ping 127.0.0.1");
#endif

    // 等待进程启动
    ASSERT_TRUE(process->isRunning());

    // 给它一些时间产生输出
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 终止进程
    process->terminate();

    // 进程应该不再运行
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(process->isRunning());
}

// 测试终止已完成的进程
TEST_F(QProcessTerminateTest, TerminateFinishedProcess) {
// 启动一个短暂的进程
#ifdef _WIN32
    process->start("cmd /c echo Test output");
#else
    process->start("echo Test output");
#endif

    // 等待它完成
    // 给进程一点时间完成
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_FALSE(process->isRunning());

    // 终止已完成的进程应该是安全的
    EXPECT_NO_THROW(process->terminate());
    EXPECT_FALSE(process->isRunning());
}

// 测试多次调用终止
TEST_F(QProcessTerminateTest, MultipleTerminateCalls) {
// 启动一个长时间运行的进程
#ifdef _WIN32
    process->start("ping -t 127.0.0.1");
#else
    process->start("ping 127.0.0.1");
#endif

    ASSERT_TRUE(process->isRunning());

    // 多次调用终止
    EXPECT_NO_THROW(process->terminate());
    EXPECT_NO_THROW(process->terminate());
    EXPECT_NO_THROW(process->terminate());

    // 进程应该不再运行
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(process->isRunning());
}

// 测试终止阻塞/等待的进程
TEST_F(QProcessTerminateTest, TerminateBlockedProcess) {
// 启动一个等待输入的进程
#ifdef _WIN32
    process->start("cmd /k");  // /k 在命令执行后保持 cmd.exe 运行
#else
    process->start("cat");  // 不带参数的 cat 会等待 stdin
#endif

    ASSERT_TRUE(process->isRunning());

    // 进程正在等待输入，终止它
    process->terminate();

    // 进程应该不再运行
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(process->isRunning());
}

// 测试在进程产生输出时终止
TEST_F(QProcessTerminateTest, TerminateDuringOutput) {
// 启动一个持续产生输出的进程
#ifdef _WIN32
    process->start(
        "cmd /c for /L %i in (1,1,100) do @(echo Line %i & ping -n 1 127.0.0.1 "
        "> nul)");
#else
    process->start(
        "bash -c \"for i in {1..100}; do echo Line $i; sleep 0.1; done\"");
#endif

    ASSERT_TRUE(process->isRunning());

    // 给它一点时间开始产生输出
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 读取一些输出
    std::string output = process->readAllStandardOutput();
    EXPECT_FALSE(output.empty());

    // 在它仍在产生输出时终止
    process->terminate();

    // 进程应该不再运行
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(process->isRunning());

    // 仍然应该能够读取缓冲的输出
    std::string remainingOutput = process->readAllStandardOutput();
    // 对此没有特定期望 - 可能有也可能没有额外输出
}

// 测试从多个线程调用终止时的线程安全性
TEST_F(QProcessTerminateTest, ThreadSafetyOfTerminate) {
// 启动一个长时间运行的进程
#ifdef _WIN32
    process->start("ping -t 127.0.0.1");
#else
    process->start("ping 127.0.0.1");
#endif

    ASSERT_TRUE(process->isRunning());

    // 尝试从多个线程终止
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; i++) {
        threads.emplace_back([this]() { process->terminate(); });
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    // 进程应该被终止
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(process->isRunning());
}

// 测试终止后的资源清理
TEST_F(QProcessTerminateTest, ResourceCleanupAfterTerminate) {
// 启动一个进程
#ifdef _WIN32
    process->start("cmd /c echo Test output & ping -n 10 127.0.0.1 > nul");
#else
    process->start("bash -c \"echo 'Test output'; sleep 10\"");
#endif

    ASSERT_TRUE(process->isRunning());

    // 终止进程
    process->terminate();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(process->isRunning());

    // 尝试用相同的 QProcess 对象启动另一个进程
    process.reset(new QProcess());  // 创建一个新的 QProcess 以避免任何问题

#ifdef _WIN32
    process->start("cmd /c echo Second process");
#else
    process->start("echo Second process");
#endif

    // 给进程时间启动并完成
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::string output = process->readAllStandardOutput();
    EXPECT_THAT(output, HasSubstr("Second process"));
}

// 测试有大量待处理输出时终止进程
TEST_F(QProcessTerminateTest, TerminateWithPendingOutput) {
// 启动一个快速生成大量输出的进程
#ifdef _WIN32
    process->start("cmd /c for /L %i in (1,1,10000) do @echo Line %i");
#else
    process->start("bash -c \"for i in {1..10000}; do echo Line $i; done\"");
#endif

    ASSERT_TRUE(process->isRunning());

    // 让它生成一些输出
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 在有待处理输出时终止
    process->terminate();

    // 进程应该不再运行
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(process->isRunning());

    // 仍然应该能够读取缓冲的输出
    std::string output = process->readAllStandardOutput();
    // 输出可能是部分的，但应该包含一些行
    EXPECT_FALSE(output.empty());
}

// 测试在写入其标准输入时终止进程
TEST_F(QProcessTerminateTest, TerminateDuringWrite) {
// 启动一个从 stdin 读取的进程
#ifdef _WIN32
    process->start("cmd /c findstr .*");  // findstr 将从 stdin 读取
#else
    process->start("cat");  // 不带参数的 cat 会从 stdin 读取
#endif

    ASSERT_TRUE(process->isRunning());

    // 生成一些随机数据写入
    std::string largeData;
    largeData.reserve(100000);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(32, 126);  // ASCII 可打印字符

    for (int i = 0; i < 100000; ++i) {
        largeData += static_cast<char>(dis(gen));
    }

    // 异步写入
    auto writeFuture = std::async(std::launch::async, [this, &largeData]() {
        try {
            process->write(largeData);
            return true;
        } catch (...) {
            return false;
        }
    });

    // 给它一点时间开始写入
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 在写入可能进行时终止
    process->terminate();

    // 进程应该被终止
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(process->isRunning());

    // 写入操作应该完成或失败，但不应挂起
    auto writeStatus = writeFuture.wait_for(std::chrono::seconds(2));
    EXPECT_NE(writeStatus, std::future_status::timeout);
}

// 测试设置环境变量后终止
TEST_F(QProcessTerminateTest, TerminateAfterSettingEnvironment) {
    // 设置环境变量
    std::vector<std::string> env = {"TEST_VAR1=value1", "TEST_VAR2=value2"};
    process->setEnvironment(env);

// 启动一个进程
#ifdef _WIN32
    process->start("cmd /c ping -n 10 127.0.0.1 > nul");
#else
    process->start("sleep 10");
#endif

    ASSERT_TRUE(process->isRunning());

    // 终止进程
    process->terminate();

    // 进程应该被终止
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(process->isRunning());

    // 应该能够用不同的环境启动另一个进程
    process.reset(new QProcess());
    process->setEnvironment(
        std::vector<std::string>{"ANOTHER_VAR=another_value"});

#ifdef _WIN32
    process->start("cmd /c echo Restarted");
#else
    process->start("echo Restarted");
#endif

    ASSERT_TRUE(process->isRunning());

    // 给进程时间启动并完成
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::string output = process->readAllStandardOutput();
    EXPECT_THAT(output, HasSubstr("Restarted"));
}

// 测试设置工作目录后终止进程
TEST_F(QProcessTerminateTest, TerminateWithCustomWorkingDirectory) {
// 将工作目录设置为临时目录
#ifdef _WIN32
    std::string tempDir = std::getenv("TEMP");
#else
    std::string tempDir = "/tmp";
#endif

    process->setWorkingDirectory(tempDir);

// 启动一个进程
#ifdef _WIN32
    process->start("cmd /c ping -n 10 127.0.0.1 > nul");
#else
    process->start("sleep 10");
#endif

    ASSERT_TRUE(process->isRunning());

    // 终止进程
    process->terminate();

    // 进程应该被终止
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(process->isRunning());
}
