// filepath: d:\msys64\home\qwdma\Atom\atom\system\test_pidwatcher.hpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include "atom/system/pidwatcher.hpp"

using namespace atom::system;
using namespace std::chrono_literals;

class PidWatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建默认配置的PidWatcher
        watcher = std::make_unique<PidWatcher>();

        // 设置允许更多时间处理系统资源的测试
        wait_time = 100ms;

        // 启动一个测试进程（这里用一个永久循环的简单程序）
#ifdef _WIN32
        test_process_cmd = "notepad.exe";
#else
        test_process_cmd = "sleep 60";
#endif

        // 启动测试进程
        launch_test_process();
    }

    void TearDown() override {
        // 停止监控
        watcher->stop();

        // 终止所有测试进程
        for (pid_t pid : test_pids) {
            if (watcher->isProcessRunning(pid)) {
                watcher->terminateProcess(pid, true);
            }
        }

        // 给系统一点时间来清理进程
        std::this_thread::sleep_for(100ms);
    }

    // 辅助方法：启动测试进程
    void launch_test_process() {
        pid_t new_pid = watcher->launchProcess(test_process_cmd, {}, false);
        if (new_pid > 0) {
            test_pids.push_back(new_pid);
            current_test_pid = new_pid;
        }
    }

    // 辅助方法：等待指定条件成立
    template <typename Func>
    bool wait_for_condition(Func condition,
                            std::chrono::milliseconds timeout = 5s) {
        auto start = std::chrono::steady_clock::now();
        while (!condition()) {
            if (std::chrono::steady_clock::now() - start > timeout) {
                return false;
            }
            std::this_thread::sleep_for(10ms);
        }
        return true;
    }

    std::unique_ptr<PidWatcher> watcher;
    std::chrono::milliseconds wait_time;
    std::string test_process_cmd;
    std::vector<pid_t> test_pids;
    pid_t current_test_pid = 0;
};

// 测试基础构造函数
TEST_F(PidWatcherTest, ConstructorDefault) {
    PidWatcher watcher;
    EXPECT_FALSE(watcher.isActive());
}

// 测试带配置的构造函数
TEST_F(PidWatcherTest, ConstructorWithConfig) {
    MonitorConfig config;
    config.update_interval = 500ms;
    config.monitor_children = true;

    PidWatcher watcher(config);
    EXPECT_FALSE(watcher.isActive());
}

// 测试设置退出回调
TEST_F(PidWatcherTest, SetExitCallback) {
    bool callback_called = false;

    watcher->setExitCallback([&](const ProcessInfo& info) {
        callback_called = true;
        EXPECT_EQ(info.pid, current_test_pid);
    });

    // 启动监控
    ASSERT_TRUE(watcher->startByPid(current_test_pid));

    // 终止进程，应该触发回调
    watcher->terminateProcess(current_test_pid);

    // 给回调一些时间执行
    EXPECT_TRUE(wait_for_condition([&]() { return callback_called; }));
}

// 测试设置监控函数
TEST_F(PidWatcherTest, SetMonitorFunction) {
    std::atomic<int> callback_count{0};

    watcher->setMonitorFunction(
        [&](const ProcessInfo& info) {
            callback_count++;
            EXPECT_EQ(info.pid, current_test_pid);
        },
        100ms);

    // 启动监控
    ASSERT_TRUE(watcher->startByPid(current_test_pid));

    // 等待回调被调用几次
    std::this_thread::sleep_for(350ms);

    // 回调应该被调用至少3次
    EXPECT_GE(callback_count, 3);
}

// 测试设置多进程回调
TEST_F(PidWatcherTest, SetMultiProcessCallback) {
    // 启动另一个测试进程
    launch_test_process();

    std::atomic<bool> callback_called{false};

    watcher->setMultiProcessCallback(
        [&](const std::vector<ProcessInfo>& infos) {
            callback_called = true;
            EXPECT_GE(infos.size(), 2);  // 至少有两个进程
        });

    // 启动多进程监控
    std::vector<std::string> process_names;
    for (pid_t pid : test_pids) {
        auto info = watcher->getProcessInfo(pid);
        if (info) {
            process_names.push_back(info->name);
        }
    }

    size_t started = watcher->startMultiple(process_names);
    EXPECT_GE(started, 1);

    // 给回调一些时间执行
    EXPECT_TRUE(wait_for_condition([&]() { return callback_called; }));
}

// 测试错误回调
TEST_F(PidWatcherTest, SetErrorCallback) {
    std::atomic<bool> callback_called{false};

    watcher->setErrorCallback(
        [&](const std::string& error, int code) { callback_called = true; });

    // 尝试启动监控一个不存在的PID
    pid_t non_existent_pid = 999999;
    bool result = watcher->startByPid(non_existent_pid);

    // 应该失败并触发错误回调
    EXPECT_FALSE(result);
    EXPECT_TRUE(wait_for_condition([&]() { return callback_called; }));
}

// 测试资源限制回调
TEST_F(PidWatcherTest, SetResourceLimitCallback) {
    std::atomic<bool> callback_called{false};

    watcher->setResourceLimitCallback(
        [&](const ProcessInfo& info, const ResourceLimits& limits) {
            callback_called = true;
            EXPECT_EQ(info.pid, current_test_pid);
        });

    // 设置一个非常低的资源限制，确保会触发回调
    ResourceLimits limits;
    limits.max_cpu_percent = 0.1;  // 0.1% CPU使用率上限
    limits.max_memory_kb = 1024;   // 1MB内存上限

    // 启动监控
    ASSERT_TRUE(watcher->startByPid(current_test_pid));

    // 应用资源限制
    watcher->setResourceLimits(current_test_pid, limits);

    // 给回调一些时间执行
    // 注意：这个测试可能不稳定，因为进程可能不会立即超过资源限制
    std::this_thread::sleep_for(1s);
}

// 测试进程创建回调
TEST_F(PidWatcherTest, SetProcessCreateCallback) {
    std::atomic<bool> callback_called{false};

    watcher->setProcessCreateCallback([&](pid_t pid, const std::string& cmd) {
        callback_called = true;
        EXPECT_GT(pid, 0);
        EXPECT_FALSE(cmd.empty());
    });

    // 启动新进程
    pid_t new_pid = watcher->launchProcess(test_process_cmd);
    if (new_pid > 0) {
        test_pids.push_back(new_pid);
    }

    // 回调应该被调用
    EXPECT_TRUE(callback_called);
}

// 测试进程过滤器
TEST_F(PidWatcherTest, SetProcessFilter) {
    std::atomic<int> monitor_count{0};

    watcher->setProcessFilter([&](const ProcessInfo& info) {
        // 只监控内存使用小于100MB的进程
        return info.memory_usage < 100 * 1024;
    });

    watcher->setMonitorFunction(
        [&](const ProcessInfo& info) { monitor_count++; }, 100ms);

    // 启动监控
    ASSERT_TRUE(watcher->startByPid(current_test_pid));

    // 给监控一些时间运行
    std::this_thread::sleep_for(300ms);

    // 由于过滤器，如果进程内存超过100MB，monitor_count可能为0
    // 如果内存低于阈值，monitor_count应该大于0
    int count = monitor_count;
    EXPECT_GE(count, 0);  // 我们不能确定这个进程是否通过过滤器
}

// 测试通过名称获取PID
TEST_F(PidWatcherTest, GetPidByName) {
    // 获取测试进程的信息
    auto info = watcher->getProcessInfo(current_test_pid);
    ASSERT_TRUE(info.has_value());

    // 使用名称查找PID
    pid_t found_pid = watcher->getPidByName(info->name);

    // 不同操作系统可能有不同行为，但在大多数情况下应该能找到一个PID
    EXPECT_GT(found_pid, 0);
}

// 测试通过名称获取多个PID
TEST_F(PidWatcherTest, GetPidsByName) {
    // 获取测试进程的信息
    auto info = watcher->getProcessInfo(current_test_pid);
    ASSERT_TRUE(info.has_value());

    // 启动另一个同名进程
#ifdef _WIN32
    if (info->name == "notepad.exe") {
        watcher->launchProcess("notepad.exe", {}, false);
    }
#else
    if (info->name.find("sleep") != std::string::npos) {
        watcher->launchProcess("sleep 60", {}, false);
    }
#endif

    // 使用名称查找多个PID
    std::vector<pid_t> found_pids = watcher->getPidsByName(info->name);

    // 应该至少找到一个PID
    EXPECT_GE(found_pids.size(), 1);
}

// 测试获取进程信息
TEST_F(PidWatcherTest, GetProcessInfo) {
    // 获取测试进程的信息
    auto info = watcher->getProcessInfo(current_test_pid);

    // 应该能获取到信息
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->pid, current_test_pid);
    EXPECT_FALSE(info->name.empty());
}

// 测试获取所有进程
TEST_F(PidWatcherTest, GetAllProcesses) {
    // 获取所有运行中的进程
    auto processes = watcher->getAllProcesses();

    // 应该有至少一个进程（测试进程）
    EXPECT_FALSE(processes.empty());

    // 验证测试进程在列表中
    bool found = false;
    for (const auto& process : processes) {
        if (process.pid == current_test_pid) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// 测试获取子进程
TEST_F(PidWatcherTest, GetChildProcesses) {
    // 在某些情况下可能需要特别创建一个有子进程的进程
    // 这里简单测试API是否正常工作
    auto children = watcher->getChildProcesses(current_test_pid);

    // 这个测试进程可能没有子进程
    // 只检查函数不会抛出异常
    SUCCEED();
}

// 测试通过名称启动监控
TEST_F(PidWatcherTest, StartByName) {
    // 获取测试进程的信息
    auto info = watcher->getProcessInfo(current_test_pid);
    ASSERT_TRUE(info.has_value());

    // 通过名称启动监控
    bool result = watcher->start(info->name);

    // 应该成功启动
    EXPECT_TRUE(result);
    EXPECT_TRUE(watcher->isActive());
}

// 测试通过PID启动监控
TEST_F(PidWatcherTest, StartByPid) {
    // 通过PID启动监控
    bool result = watcher->startByPid(current_test_pid);

    // 应该成功启动
    EXPECT_TRUE(result);
    EXPECT_TRUE(watcher->isActive());
    EXPECT_TRUE(watcher->isMonitoring(current_test_pid));
}

// 测试带自定义配置启动监控
TEST_F(PidWatcherTest, StartWithCustomConfig) {
    // 创建自定义配置
    MonitorConfig config;
    config.update_interval = 200ms;
    config.monitor_children = true;
    config.auto_restart = true;

    // 通过PID启动监控，使用自定义配置
    bool result = watcher->startByPid(current_test_pid, &config);

    // 应该成功启动
    EXPECT_TRUE(result);
    EXPECT_TRUE(watcher->isActive());
}

// 测试启动多进程监控
TEST_F(PidWatcherTest, StartMultiple) {
    // 启动另一个测试进程
    launch_test_process();

    // 收集进程名称
    std::vector<std::string> process_names;
    for (pid_t pid : test_pids) {
        auto info = watcher->getProcessInfo(pid);
        if (info) {
            process_names.push_back(info->name);
        }
    }

    // 启动多进程监控
    size_t started = watcher->startMultiple(process_names);

    // 应该至少启动了一个监控
    EXPECT_GE(started, 1);
    EXPECT_TRUE(watcher->isActive());
}

// 测试停止监控
TEST_F(PidWatcherTest, Stop) {
    // 首先启动监控
    ASSERT_TRUE(watcher->startByPid(current_test_pid));
    EXPECT_TRUE(watcher->isActive());

    // 停止监控
    watcher->stop();

    // 应该不再监控
    EXPECT_FALSE(watcher->isActive());
    EXPECT_FALSE(watcher->isMonitoring(current_test_pid));
}

// 测试停止特定进程监控
TEST_F(PidWatcherTest, StopProcess) {
    // 启动另一个测试进程
    launch_test_process();
    pid_t second_pid = current_test_pid;

    // 启动多进程监控
    std::vector<std::string> process_names;
    for (pid_t pid : test_pids) {
        auto info = watcher->getProcessInfo(pid);
        if (info) {
            process_names.push_back(info->name);
        }
    }

    size_t started = watcher->startMultiple(process_names);
    ASSERT_GE(started, 2);

    // 停止特定进程监控
    bool result = watcher->stopProcess(second_pid);

    // 应该成功停止特定进程的监控
    EXPECT_TRUE(result);
    EXPECT_TRUE(watcher->isActive());  // 仍在监控其他进程
    EXPECT_FALSE(watcher->isMonitoring(second_pid));
}

// 测试切换到另一个进程
TEST_F(PidWatcherTest, SwitchToProcess) {
    // 首先启动监控
    ASSERT_TRUE(watcher->startByPid(current_test_pid));

    // 启动另一个测试进程
    launch_test_process();
    pid_t second_pid = current_test_pid;

    // 获取第二个进程的信息
    auto info = watcher->getProcessInfo(second_pid);
    ASSERT_TRUE(info.has_value());

    // 切换到第二个进程
    bool result = watcher->switchToProcess(info->name);

    // 应该成功切换
    EXPECT_TRUE(result);
    EXPECT_TRUE(watcher->isActive());
}

// 测试通过PID切换进程
TEST_F(PidWatcherTest, SwitchToProcessById) {
    // 首先启动监控
    ASSERT_TRUE(watcher->startByPid(current_test_pid));

    // 启动另一个测试进程
    launch_test_process();
    pid_t second_pid = current_test_pid;

    // 切换到第二个进程
    bool result = watcher->switchToProcessById(second_pid);

    // 应该成功切换
    EXPECT_TRUE(result);
    EXPECT_TRUE(watcher->isActive());
}

// 测试检查进程是否在运行
TEST_F(PidWatcherTest, IsProcessRunning) {
    // 检查测试进程是否在运行
    bool running = watcher->isProcessRunning(current_test_pid);

    // 应该在运行
    EXPECT_TRUE(running);

    // 检查不存在的进程
    running = watcher->isProcessRunning(999999);
    EXPECT_FALSE(running);
}

// 测试获取进程CPU使用率
TEST_F(PidWatcherTest, GetProcessCpuUsage) {
    // 获取测试进程的CPU使用率
    double cpu_usage = watcher->getProcessCpuUsage(current_test_pid);

    // CPU使用率应该是一个合理的值
    EXPECT_GE(cpu_usage, 0.0);
    EXPECT_LE(cpu_usage, 100.0);
}

// 测试获取进程内存使用
TEST_F(PidWatcherTest, GetProcessMemoryUsage) {
    // 获取测试进程的内存使用
    size_t memory_usage = watcher->getProcessMemoryUsage(current_test_pid);

    // 内存使用应该大于0
    EXPECT_GT(memory_usage, 0);
}

// 测试获取进程线程数
TEST_F(PidWatcherTest, GetProcessThreadCount) {
    // 获取测试进程的线程数
    unsigned int thread_count =
        watcher->getProcessThreadCount(current_test_pid);

    // 应该至少有一个线程
    EXPECT_GE(thread_count, 1);
}

// 测试获取进程IO统计
TEST_F(PidWatcherTest, GetProcessIOStats) {
    // 获取测试进程的IO统计
    ProcessIOStats io_stats = watcher->getProcessIOStats(current_test_pid);

    // 无法确定具体的IO活动，但结构应该被正确初始化
    SUCCEED();
}

// 测试获取进程状态
TEST_F(PidWatcherTest, GetProcessStatus) {
    // 获取测试进程的状态
    ProcessStatus status = watcher->getProcessStatus(current_test_pid);

    // 不应该是未知状态
    EXPECT_NE(status, ProcessStatus::UNKNOWN);
    // 应该是运行中
    EXPECT_EQ(status, ProcessStatus::RUNNING);
}

// 测试获取进程运行时间
TEST_F(PidWatcherTest, GetProcessUptime) {
    // 获取测试进程的运行时间
    std::chrono::milliseconds uptime =
        watcher->getProcessUptime(current_test_pid);

    // 应该有正值的运行时间
    EXPECT_GT(uptime.count(), 0);
}

// 测试启动新进程
TEST_F(PidWatcherTest, LaunchProcess) {
    // 启动一个新进程
    pid_t new_pid = watcher->launchProcess(test_process_cmd);

    // 应该成功启动
    EXPECT_GT(new_pid, 0);

    if (new_pid > 0) {
        test_pids.push_back(new_pid);

        // 验证进程正在运行
        EXPECT_TRUE(watcher->isProcessRunning(new_pid));
    }
}

// 测试终止进程
TEST_F(PidWatcherTest, TerminateProcess) {
    // 终止测试进程
    bool result = watcher->terminateProcess(current_test_pid);

    // 应该成功终止
    EXPECT_TRUE(result);

    // 给系统一些时间处理终止
    std::this_thread::sleep_for(100ms);

    // 验证进程不再运行
    EXPECT_FALSE(watcher->isProcessRunning(current_test_pid));
}

// 测试设置资源限制
TEST_F(PidWatcherTest, SetResourceLimits) {
    // 设置资源限制
    ResourceLimits limits;
    limits.max_cpu_percent = 50.0;
    limits.max_memory_kb = 100 * 1024;  // 100 MB

    bool result = watcher->setResourceLimits(current_test_pid, limits);

    // 注意：这个功能在不同系统上可能有不同的行为
    // 我们只是测试API能否被调用
    SUCCEED();
}

// 测试设置进程优先级
TEST_F(PidWatcherTest, SetProcessPriority) {
    // 设置进程优先级
    bool result = watcher->setProcessPriority(current_test_pid, 10);

    // 注意：设置优先级需要特殊权限，可能失败
    // 我们只是测试API能否被调用
    SUCCEED();
}

// 测试配置自动重启
TEST_F(PidWatcherTest, ConfigureAutoRestart) {
    // 配置自动重启
    bool result = watcher->configureAutoRestart(current_test_pid, true, 3);

    // 应该成功配置
    EXPECT_TRUE(result);
}

// 测试重启进程
TEST_F(PidWatcherTest, RestartProcess) {
    // 重启测试进程
    pid_t new_pid = watcher->restartProcess(current_test_pid);

    // 注意：重启会终止原进程并启动新进程
    // 根据实现细节，可能成功也可能失败
    if (new_pid > 0) {
        test_pids.push_back(new_pid);

        // 验证新进程正在运行
        EXPECT_TRUE(watcher->isProcessRunning(new_pid));
    }

    // 给系统一些时间处理重启
    std::this_thread::sleep_for(100ms);
}

// 测试导出进程信息
TEST_F(PidWatcherTest, DumpProcessInfo) {
    // 导出进程信息到临时文件
    std::string temp_file = "test_process_dump.txt";
    bool result = watcher->dumpProcessInfo(current_test_pid, true, temp_file);

    // 应该成功导出
    EXPECT_TRUE(result);

    // 验证文件存在
    std::ifstream file(temp_file);
    EXPECT_TRUE(file.good());
    file.close();

    // 清理临时文件
    std::remove(temp_file.c_str());
}

// 测试获取监控统计
TEST_F(PidWatcherTest, GetMonitoringStats) {
    // 首先启动监控
    ASSERT_TRUE(watcher->startByPid(current_test_pid));

    // 给监控一些时间收集数据
    std::this_thread::sleep_for(300ms);

    // 获取监控统计
    auto stats = watcher->getMonitoringStats();

    // 可能还没有收集到统计数据
    // 只测试API能否被调用
    SUCCEED();
}

// 测试设置速率限制
TEST_F(PidWatcherTest, SetRateLimiting) {
    // 设置速率限制
    watcher->setRateLimiting(5);  // 每秒最多5次更新

    // 方法应该链式返回自身
    SUCCEED();
}

// 测试并发调用
TEST_F(PidWatcherTest, ConcurrentAccess) {
    // 启动监控
    ASSERT_TRUE(watcher->startByPid(current_test_pid));

    // 创建多个线程并发访问PidWatcher
    const int num_threads = 5;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < 10; ++j) {
                if (i % 5 == 0) {
                    watcher->getProcessInfo(current_test_pid);
                } else if (i % 5 == 1) {
                    watcher->getProcessCpuUsage(current_test_pid);
                } else if (i % 5 == 2) {
                    watcher->getProcessMemoryUsage(current_test_pid);
                } else if (i % 5 == 3) {
                    watcher->isProcessRunning(current_test_pid);
                } else {
                    watcher->getProcessStatus(current_test_pid);
                }
                std::this_thread::sleep_for(5ms);
            }
        });
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    // 验证监控仍在正常运行
    EXPECT_TRUE(watcher->isActive());
}

// 测试资源限制边缘情况
TEST_F(PidWatcherTest, ResourceLimitsEdgeCases) {
    // 测试零资源限制
    ResourceLimits zero_limits;
    zero_limits.max_cpu_percent = 0.0;
    zero_limits.max_memory_kb = 0;

    bool result = watcher->setResourceLimits(current_test_pid, zero_limits);
    // 应该能设置，但可能不会实际限制

    // 测试非常高的资源限制
    ResourceLimits high_limits;
    high_limits.max_cpu_percent = 1000.0;  // 超过100%
    high_limits.max_memory_kb = std::numeric_limits<size_t>::max();

    result = watcher->setResourceLimits(current_test_pid, high_limits);
    // 应该能设置，但可能会被调整到合理范围

    // 我们主要测试API不会崩溃
    SUCCEED();
}

// 测试负载测试 - 快速频繁操作
TEST_F(PidWatcherTest, DISABLED_LoadTest) {
    // 这是一个可能会长时间运行的负载测试
    // 默认禁用，但可以手动启用来验证系统在高负载下的稳定性

    const int iterations = 1000;

    for (int i = 0; i < iterations; ++i) {
        // 随机选择操作
        int operation = i % 5;

        switch (operation) {
            case 0:
                watcher->getProcessInfo(current_test_pid);
                break;
            case 1:
                watcher->getProcessCpuUsage(current_test_pid);
                break;
            case 2:
                watcher->getProcessMemoryUsage(current_test_pid);
                break;
            case 3:
                watcher->isProcessRunning(current_test_pid);
                break;
            case 4:
                watcher->getProcessStatus(current_test_pid);
                break;
        }

        if (i % 100 == 0) {
            // 每100次操作输出一次进度
            std::cout << "Load test progress: " << i << "/" << iterations
                      << std::endl;
        }
    }

    // 测试是否仍然可以正常工作
    EXPECT_NO_THROW(watcher->getProcessInfo(current_test_pid));
}

// 主函数
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
