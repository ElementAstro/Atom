#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// 包含 Timer 类的头文件
#include "atom/async/timer.hpp"

// 用于同步输出的互斥锁
std::mutex g_outputMutex;

// 辅助宏，用于格式化输出
#define SECTION(name) std::cout << "\n=== " << name << " ===\n"
#define LOG(msg)                                                        \
    {                                                                   \
        std::lock_guard<std::mutex> lock(g_outputMutex);                \
        std::cout << "[" << std::setw(4) << __LINE__ << "] "            \
                  << "[" << std::setw(10) << std::this_thread::get_id() \
                  << "] " << msg << std::endl;                          \
    }

// 获取当前时间的字符串表示
std::string getCurrentTimeStr() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%H:%M:%S") << '.'
       << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// 简单任务函数
void simpleTask() { LOG("执行简单任务 @ " + getCurrentTimeStr()); }

// 带参数的任务函数
void parameterizedTask(const std::string& message, int value) {
    LOG("执行带参数任务: " + message + ", 值: " + std::to_string(value) +
        " @ " + getCurrentTimeStr());
}

// 返回值的任务函数
int taskWithReturn(int a, int b) {
    int result = a + b;
    LOG("执行带返回值任务: " + std::to_string(a) + " + " + std::to_string(b) +
        " = " + std::to_string(result) + " @ " + getCurrentTimeStr());
    return result;
}

// 抛出异常的任务函数
void throwingTask() {
    LOG("执行将抛出异常的任务 @ " + getCurrentTimeStr());
    throw std::runtime_error("这是一个测试异常");
}

// 长时间运行的任务函数
void longRunningTask(int duration) {
    LOG("开始长时间运行任务, 持续 " + std::to_string(duration) + " 毫秒 @ " +
        getCurrentTimeStr());
    std::this_thread::sleep_for(std::chrono::milliseconds(duration));
    LOG("完成长时间运行任务 @ " + getCurrentTimeStr());
}

int main() {
    using namespace std::chrono_literals;

    std::cout << "===== atom::async::Timer 使用示例 =====\n\n";

    //==============================================================
    // 1. 基本用法
    //==============================================================
    SECTION("1. 基本用法");
    {
        // 创建一个定时器
        atom::async::Timer timer;

        // 设置一个简单的定时任务（1000毫秒后执行一次）
        LOG("设置一个1000ms延时的任务");
        timer.setTimeout(simpleTask, 1000);

        // 设置一个带参数的定时任务（800毫秒后执行一次）
        LOG("设置一个800ms延时的带参数任务");
        timer.setTimeout(parameterizedTask, 800, "你好", 42);

        // 等待任务完成
        LOG("等待任务完成...");
        std::this_thread::sleep_for(1200ms);
        LOG("基本用法示例完成");
    }

    //==============================================================
    // 2. 设置回调函数
    //==============================================================
    SECTION("2. 设置回调函数");
    {
        atom::async::Timer timer;

        // 设置回调，每次任务执行完毕时调用
        timer.setCallback(
            []() { LOG("任务执行完毕回调被触发 @ " + getCurrentTimeStr()); });

        // 设置一个简单任务
        LOG("设置带有回调的任务");
        timer.setTimeout(simpleTask, 500);

        // 等待任务完成
        std::this_thread::sleep_for(600ms);
        LOG("回调示例完成");
    }

    //==============================================================
    // 3. setTimeout - 使用返回的Future获取结果
    //==============================================================
    SECTION("3. setTimeout - 使用返回的Future获取结果");
    {
        atom::async::Timer timer;

        // 设置一个有返回值的任务并获取Future
        LOG("设置一个有返回值的任务");
        auto future = timer.setTimeout(taskWithReturn, 500, 10, 20);

        // 等待任务完成并获取结果
        LOG("等待任务完成并获取结果...");
        auto result = future.get();
        LOG("从future获取的结果: " + std::to_string(result));

        // 使用EnhancedFuture的其他功能
        LOG("设置另一个任务，展示EnhancedFuture功能");
        auto future2 = timer.setTimeout(taskWithReturn, 300, 5, 7);

        // 使用then()添加连续操作
        future2
            .then([](int result) {
                LOG("然后(then)回调收到结果: " + std::to_string(result));
                return result * 2;
            })
            .then([](int result) {
                LOG("链式then回调收到结果: " + std::to_string(result));
            });

        // 等待所有任务完成
        std::this_thread::sleep_for(500ms);
    }

    //==============================================================
    // 4. setInterval - 定期执行任务
    //==============================================================
    SECTION("4. setInterval - 定期执行任务");
    {
        atom::async::Timer timer;

        // 设置一个重复执行的任务（每300ms执行一次，共执行3次）
        LOG("设置一个间隔300ms重复3次的任务");
        timer.setInterval(
            []() { LOG("重复执行任务 @ " + getCurrentTimeStr()); }, 300, 3, 0);

        // 等待任务完成
        LOG("等待重复任务完成...");
        std::this_thread::sleep_for(1000ms);
        LOG("重复任务示例完成");

        // 设置一个无限重复的任务，然后取消它
        LOG("设置一个无限重复的任务");
        timer.setInterval(
            []() { LOG("无限重复任务执行 @ " + getCurrentTimeStr()); }, 200, -1,
            0);

        // 让它执行几次
        std::this_thread::sleep_for(500ms);

        // 取消所有任务
        LOG("取消所有任务");
        timer.cancelAllTasks();

        std::this_thread::sleep_for(300ms);
        LOG("确认任务已被取消");
    }

    //==============================================================
    // 5. 任务优先级
    //==============================================================
    SECTION("5. 任务优先级");
    {
        atom::async::Timer timer;

        // 设置多个具有不同优先级的任务
        // 注意：尽管设置了不同的延迟，但优先级较高的任务会先执行
        LOG("设置多个不同优先级的任务");

        // 低优先级任务
        timer.setInterval(
            [](int priority) {
                LOG("优先级 " + std::to_string(priority) + " 的任务执行 @ " +
                    getCurrentTimeStr());
            },
            100, 1, 10, 10);

        // 中等优先级任务
        timer.setInterval(
            [](int priority) {
                LOG("优先级 " + std::to_string(priority) + " 的任务执行 @ " +
                    getCurrentTimeStr());
            },
            100, 1, 5, 5);

        // 高优先级任务
        timer.setInterval(
            [](int priority) {
                LOG("优先级 " + std::to_string(priority) + " 的任务执行 @ " +
                    getCurrentTimeStr());
            },
            100, 1, 1, 1);

        // 等待所有任务完成
        std::this_thread::sleep_for(200ms);
        LOG("优先级任务示例完成");
    }

    //==============================================================
    // 6. 暂停和恢复
    //==============================================================
    SECTION("6. 暂停和恢复");
    {
        atom::async::Timer timer;

        // 设置一个重复执行的任务
        LOG("设置一个间隔200ms的重复任务");
        timer.setInterval(
            []() { LOG("重复任务执行 @ " + getCurrentTimeStr()); }, 200, 10, 0);

        // 让它执行几次
        std::this_thread::sleep_for(500ms);

        // 暂停定时器
        LOG("暂停定时器");
        timer.pause();
        LOG("定时器已暂停，等待500ms");
        std::this_thread::sleep_for(500ms);

        // 恢复定时器
        LOG("恢复定时器");
        timer.resume();
        LOG("定时器已恢复，等待600ms");
        std::this_thread::sleep_for(600ms);

        // 停止定时器
        LOG("停止定时器");
        timer.stop();
        LOG("定时器已停止");
    }

    //==============================================================
    // 7. 错误处理
    //==============================================================
    SECTION("7. 错误处理");
    {
        atom::async::Timer timer;

        // 设置一个会抛出异常的任务
        LOG("设置一个会抛出异常的任务");
        auto exceptionFuture = timer.setTimeout(throwingTask, 100);

        // 等待任务执行并捕获异常
        try {
            LOG("等待异常任务完成...");
            exceptionFuture.get();
        } catch (const std::exception& e) {
            LOG("捕获到异常: " + std::string(e.what()));
        }

        // 尝试设置无效的任务参数
        LOG("尝试设置无效的任务参数");

        try {
            // 尝试设置nullptr作为函数
            std::function<void()> nullFunc = nullptr;
            timer.setTimeout(nullFunc, 100);
        } catch (const std::exception& e) {
            LOG("捕获到异常: " + std::string(e.what()));
        }

        try {
            // 尝试使用无效的重复次数
            timer.setInterval(simpleTask, 100, -2, 0);
        } catch (const std::exception& e) {
            LOG("捕获到异常: " + std::string(e.what()));
        }

        try {
            // 尝试使用0作为间隔
            timer.setInterval(simpleTask, 0, 1, 0);
        } catch (const std::exception& e) {
            LOG("捕获到异常: " + std::string(e.what()));
        }
    }

    //==============================================================
    // 8. 同时执行多个不同类型的任务
    //==============================================================
    SECTION("8. 同时执行多个不同类型的任务");
    {
        atom::async::Timer timer;

        LOG("设置多个不同类型的任务");

        // 简单的延迟任务
        timer.setTimeout(simpleTask, 100);

        // 带参数的任务
        timer.setTimeout(parameterizedTask, 150, "参数化任务", 123);

        // 带返回值的任务
        auto future = timer.setTimeout(taskWithReturn, 200, 30, 12);

        // 重复任务
        timer.setInterval(
            []() { LOG("短间隔重复任务 @ " + getCurrentTimeStr()); }, 100, 3,
            0);

        // 等待所有任务完成
        std::this_thread::sleep_for(300ms);

        // 获取返回值任务的结果
        auto result = future.get();
        LOG("返回值结果: " + std::to_string(result));
    }

    //==============================================================
    // 9. 边界情况测试
    //==============================================================
    SECTION("9. 边界情况测试");
    {
        atom::async::Timer timer;

        // 设置最小延迟的任务
        LOG("设置1毫秒延迟的任务");
        timer.setTimeout(
            []() { LOG("最小延迟任务执行 @ " + getCurrentTimeStr()); }, 1);

        // 设置较长延迟的任务
        LOG("设置较长延迟(2秒)的任务");
        timer.setTimeout(
            []() { LOG("较长延迟任务执行 @ " + getCurrentTimeStr()); }, 2000);

        // 设置任务数量限制测试
        LOG("设置大量短期任务");
        for (int i = 0; i < 20; ++i) {
            timer.setTimeout(
                [i]() { LOG("短期任务 #" + std::to_string(i) + " 执行"); },
                100 + i * 10);
        }

        // 等待任务完成
        LOG("等待较长延迟任务完成...");
        std::this_thread::sleep_for(2100ms);

        // 检查当前任务数量
        LOG("当前任务数量: " + std::to_string(timer.getTaskCount()));
    }

    //==============================================================
    // 10. 实际应用场景
    //==============================================================
    SECTION("10. 实际应用场景 - 模拟API限速");
    {
        atom::async::Timer timer;
        std::vector<std::string> apiRequests = {
            "获取用户数据", "更新配置", "上传文件", "下载报告", "验证凭证"};

        // 计数器和互斥锁
        int completedRequests = 0;
        std::mutex counterMutex;

        // 模拟API请求函数
        auto simulateAPIRequest = [&](const std::string& requestName) {
            LOG("发送API请求: " + requestName + " @ " + getCurrentTimeStr());

            // 模拟请求处理时间
            std::this_thread::sleep_for(
                std::chrono::milliseconds(50 + rand() % 100));

            // 更新完成计数
            {
                std::lock_guard<std::mutex> lock(counterMutex);
                ++completedRequests;
                LOG("已完成请求: " + requestName +
                    ", 总计: " + std::to_string(completedRequests));
            }
        };

        // 设置API限速 - 每200毫秒发送一个请求
        LOG("开始模拟API限速，每200ms发送一个请求");
        for (size_t i = 0; i < apiRequests.size(); ++i) {
            timer.setTimeout(simulateAPIRequest, i * 200, apiRequests[i]);
        }

        // 等待所有请求完成
        std::this_thread::sleep_for(1200ms);
        LOG("API限速模拟完成，共处理 " + std::to_string(completedRequests) +
            " 个请求");
    }

    //==============================================================
    // 11. 功能组合 - 超时处理和取消
    //==============================================================
    SECTION("11. 功能组合 - 超时处理和取消");
    {
        atom::async::Timer timer;

        // 设置一个长时间运行的任务
        LOG("设置一个长时间运行的任务(1500ms)");
        auto future = timer.setTimeout(longRunningTask, 100, 1500);

        // 设置一个超时检查，如果超过500ms还没完成，就取消所有任务
        LOG("设置500ms的超时检查");
        timer.setTimeout(
            [&timer]() {
                LOG("触发超时检查，取消所有任务");
                timer.cancelAllTasks();
                LOG("所有任务已取消");
            },
            500);

        // 等待一段时间
        std::this_thread::sleep_for(600ms);

        try {
            // 尝试获取结果
            future.get();
        } catch (const std::exception& e) {
            LOG("获取结果时捕获异常: " + std::string(e.what()));
        }
    }

#ifdef ATOM_USE_BOOST_LOCKFREE
    //==============================================================
    // 12. 无锁队列性能测试 (仅在定义了ATOM_USE_BOOST_LOCKFREE时可用)
    //==============================================================
    SECTION("12. 无锁队列性能测试");
    {
        atom::async::Timer timer;

        // 创建大量的短期任务
        const int taskCount = 1000;
        LOG("使用无锁队列设置 " + std::to_string(taskCount) + " 个短期任务");

        auto startTime = std::chrono::steady_clock::now();

        for (int i = 0; i < taskCount; ++i) {
            timer.setTimeout(
                [i]() {
                    // 任务内容很简单，只是递增一个计数器
                    static std::atomic<int> counter{0};
                    ++counter;
                    if (i % 100 == 0) {
                        LOG("完成第 " + std::to_string(i) +
                            " 个任务，计数: " + std::to_string(counter.load()));
                    }
                },
                1 + (i % 10));
        }

        // 等待所有任务完成
        LOG("等待所有任务完成...");
        std::this_thread::sleep_for(100ms);

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            endTime - startTime)
                            .count();

        LOG("无锁队列处理 " + std::to_string(taskCount) +
            " 个任务用时: " + std::to_string(duration) + " 毫秒");
    }
#endif

    std::cout << "\n===== 示例完成 =====\n";
    return 0;
}