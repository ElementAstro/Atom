#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "atom/async/async_executor.hpp"

using namespace atom::async;
using namespace std::chrono_literals;

// 格式化时间为字符串的辅助函数
std::string formatTime(const std::chrono::system_clock::time_point& timePoint) {
    auto time = std::chrono::system_clock::to_time_t(timePoint);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  timePoint.time_since_epoch() % 1s)
                  .count();

    std::tm tm_buf;
#if defined(_WIN32)
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif

    std::stringstream ss;
    ss << std::put_time(&tm_buf, "%H:%M:%S") << '.' << std::setw(3)
       << std::setfill('0') << ms;
    return ss.str();
}

// 打印带时间戳和线程ID的消息
void log(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto threadId = std::this_thread::get_id();

    std::stringstream ss;
    ss << "[" << formatTime(now) << "][线程 " << threadId << "] " << message;

    std::cout << ss.str() << std::endl;
}

// 示例基础任务：休眠并返回一个值
int basicTask(int id, int sleepMs) {
    log("执行任务 #" + std::to_string(id) + "，休眠 " +
        std::to_string(sleepMs) + "ms");
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    log("任务 #" + std::to_string(id) + " 完成");
    return id * 10;
}

// 产生错误的任务
void errorTask() {
    log("开始执行会失败的任务");
    std::this_thread::sleep_for(100ms);
    log("抛出异常");
    throw std::runtime_error("示例错误");
}

// 计算密集型任务
long long fibonacciTask(int n) {
    if (n <= 1)
        return n;
    log("计算斐波那契数 " + std::to_string(n));
    auto result = fibonacciTask(n - 1) + fibonacciTask(n - 2);
    return result;
}

// 1. 基本用法示例
void basicUsageExample() {
    log("\n=== 1. 基本用法示例 ===");

    // 创建并启动异步执行器
    AsyncExecutor::Configuration config;
    config.minThreads = 4;
    config.maxThreads = 4;
    AsyncExecutor executor(config);  // 4个工作线程
    executor.start();
    log("创建了异步执行器，线程数: 4");

    // 使用execute方法执行任务
    log("提交3个任务");
    auto future1 = executor.execute([]() { return basicTask(1, 500); },
                                    AsyncExecutor::Priority::Normal);
    auto future2 = executor.execute([]() { return basicTask(2, 300); },
                                    AsyncExecutor::Priority::High);
    auto future3 = executor.execute([]() { return basicTask(3, 100); },
                                    AsyncExecutor::Priority::Low);

    // 获取结果
    log("等待结果");
    int result1 = future1.get();
    int result2 = future2.get();
    int result3 = future3.get();

    log("所有任务完成，结果: " + std::to_string(result1) + ", " +
        std::to_string(result2) + ", " + std::to_string(result3));

    log("基本用法示例完成");
}

// 2. 优先级任务示例
void priorityTasksExample() {
    log("\n=== 2. 优先级任务示例 ===");

    AsyncExecutor::Configuration config;
    config.minThreads = 1;
    config.maxThreads = 1;
    AsyncExecutor executor(config);  // 只使用1个线程，使优先级效果更明显
    log("创建了异步执行器，线程数: 1");

    // 创建多个不同优先级的任务
    log("提交不同优先级的任务 (低、普通、高、关键)");

    // 为了确保任务在队列中排队，先批量提交
    std::vector<std::future<int>> futures;

    // 低优先级
    futures.push_back(executor.execute(
        []() {
            log("执行低优先级任务");
            std::this_thread::sleep_for(100ms);
            return 1;
        },
        AsyncExecutor::Priority::Low));

    // 普通优先级
    futures.push_back(executor.execute(
        []() {
            log("执行普通优先级任务");
            std::this_thread::sleep_for(100ms);
            return 2;
        },
        AsyncExecutor::Priority::Normal));

    // 高优先级
    futures.push_back(executor.execute(
        []() {
            log("执行高优先级任务");
            std::this_thread::sleep_for(100ms);
            return 3;
        },
        AsyncExecutor::Priority::High));

    // 关键优先级
    futures.push_back(executor.execute(
        []() {
            log("执行关键优先级任务");
            std::this_thread::sleep_for(100ms);
            return 4;
        },
        AsyncExecutor::Priority::Critical));

    // 等待所有任务完成
    log("等待所有优先级任务完成");
    for (auto& future : futures) {
        future.wait();
    }

    log("所有优先级任务已完成");
}

// 3. 延迟执行示例
void deferredTasksExample() {
    log("\n=== 3. 延迟执行示例 ===");

    // Use the global instance instead
    auto& executor = AsyncExecutor::getInstance();
    log("获取了异步执行器实例");

    // 执行任务
    log("提交3个任务");

    auto future1 = executor.execute(
        []() {
            log("执行任务 #1");
            std::this_thread::sleep_for(100ms);
            return std::string("任务1结果");
        },
        AsyncExecutor::Priority::Normal);

    auto future2 = executor.execute(
        []() {
            log("执行任务 #2");
            std::this_thread::sleep_for(150ms);
            return std::string("任务2结果");
        },
        AsyncExecutor::Priority::High);

    auto future3 = executor.execute(
        []() {
            log("执行任务 #3");
            std::this_thread::sleep_for(50ms);
            return std::string("任务3结果");
        },
        AsyncExecutor::Priority::Low);

    // 获取结果
    log("等待任务结果");
    std::string result1 = future1.get();
    std::string result2 = future2.get();
    std::string result3 = future3.get();

    log("所有任务完成，结果:");
    log("任务1: " + result1);
    log("任务2: " + result2);
    log("任务3: " + result3);
}

// 4. 定时任务示例（使用 Timer 工具）
#include "atom/async/timer.hpp"
void scheduledTasksExample() {
    log("\n=== 4. 定时任务示例 ===");

    atom::async::Timer timer;

    // 创建定时任务
    log("安排3个定时任务");

    // 1秒后执行
    auto future1 = timer.setTimeout(
        []() {
            log("执行定时任务 #1 (1秒后)");
            return std::string("定时任务1结果");
        },
        1000);

    // 2秒后执行
    auto future2 = timer.setTimeout(
        []() {
            log("执行定时任务 #2 (2秒后)");
            return std::string("定时任务2结果");
        },
        2000);

    // 使用setTimeout模拟 scheduleAfter(3s)
    auto future3 = timer.setTimeout(
        []() {
            log("执行定时任务 #3 (3秒后)");
            return std::string("定时任务3结果");
        },
        3000);

    log("已安排所有定时任务");

    // 等待结果
    log("等待所有定时任务执行和完成");
    std::string result1 = future1.wait();
    log("任务1完成: " + result1);

    std::string result2 = future2.wait();
    log("任务2完成: " + result2);

    std::string result3 = future3.wait();
    log("任务3完成: " + result3);

    log("所有定时任务已完成");
}

// 5. 错误处理示例
void errorHandlingExample() {
    log("\n=== 5. 错误处理示例 ===");

    auto& executor = AsyncExecutor::getInstance();

    // 提交一个会抛出异常的任务
    log("提交会抛出异常的任务");
    auto errorFuture = executor.execute([]() {
        errorTask();
        return 0;  // never reached
    });

    // 使用try-catch处理异常
    try {
        log("等待结果 (预期会有异常)");
        errorFuture.get();
        log("这行不应该被打印");
    } catch (const std::exception& e) {
        log("捕获到异常: " + std::string(e.what()));
    }

    // 测试异常传播
    log("提交一个lambda中抛出异常的任务");
    auto lambdaErrorFuture = executor.execute([]() -> std::string {
        log("在lambda中执行抛出异常的任务");
        throw std::runtime_error("Lambda错误");
        return "不会返回";
    });

    try {
        lambdaErrorFuture.get();
    } catch (const std::exception& e) {
        log("从lambda任务捕获到异常: " + std::string(e.what()));
    }

    // 测试延迟任务中的异常（使用普通提交模拟延迟行为）
    log("创建一个延迟任务，其中包含异常");
    auto deferredErrorFuture = executor.execute([]() {
        std::this_thread::sleep_for(50ms);
        log("执行延迟任务中的错误代码");
        throw std::runtime_error("延迟任务错误");
        return 0;
    });

    try {
        deferredErrorFuture.get();
    } catch (const std::exception& e) {
        log("从延迟任务捕获到异常: " + std::string(e.what()));
    }
}

// 6. 线程池调整大小示例（基于当前API能力进行简化）
void resizeExample() {
    log("\n=== 6. 线程池调整大小示例 ===");

    // 使用全局执行器（当前实现不支持动态 resize/查询队列等）
    auto& executor = AsyncExecutor::getInstance();

    log("提交多个长时间运行的任务");
    std::vector<std::future<int>> futures;

    for (int i = 0; i < 6; i++) {
        futures.push_back(executor.execute(
            [i]() {
                log("开始执行任务 " + std::to_string(i));
                std::this_thread::sleep_for(500ms);
                log("完成任务 " + std::to_string(i));
                return i;  // 返回一个值以获得 future
            },
            AsyncExecutor::Priority::Normal));
    }

    // 等待所有任务完成
    for (auto& future : futures) {
        future.wait();
    }
    log("所有任务已完成");

    // 提交一个简单任务确认执行器可继续使用
    auto future = executor.execute(
        []() {
            log("在所有任务完成后执行一个确认任务");
            std::this_thread::sleep_for(100ms);
            return std::string("完成");
        },
        AsyncExecutor::Priority::Normal);

    log("结果: " + future.get());
}

// 7. 边界情况和异常场景（基于现有API进行调整）
void edgeCasesExample() {
    log("\n=== 7. 边界情况和异常场景 ===");

    // 使用计时器验证长延迟任务安排
    atom::async::Timer timer;
    log("安排一个10年后执行的任务 (不等待)");
    auto futureFarAway = timer.setTimeout(
        []() {
            log("10年后的任务执行了");
            return true;
        },
        static_cast<unsigned int>(24 * 365 * 10ULL * 60ULL * 60ULL * 1000ULL));

    log("远期任务已安排 (但不会在本示例中等待)");

    // 测试快速任务并等待
    log("提交几个快速任务然后等待所有完成");
    auto& executor = AsyncExecutor::getInstance();
    std::vector<std::future<int>> quick;
    for (int i = 0; i < 3; i++) {
        quick.push_back(executor.execute([i]() {
            log("执行快速任务 " + std::to_string(i));
            std::this_thread::sleep_for(50ms);
            return i;
        }));
    }
    for (auto& f : quick)
        f.wait();

    log("所有任务已完成");
}

// 8. 复杂任务组合示例（用现有API实现）
void complexTasksExample() {
    log("\n=== 8. 复杂任务组合示例 ===");

    auto& executor = AsyncExecutor::getInstance();

    log("开始复杂任务流程");

    // 第1阶段：生成数据
    log("阶段1: 生成数据 (立即执行)");
    auto dataFuture = executor.execute(
        []() {
            log("生成随机数据");
            std::vector<int> data;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 100);

            for (int i = 0; i < 10; i++) {
                data.push_back(dis(gen));
            }

            std::stringstream ss;
            ss << "生成的数据: ";
            for (int val : data) {
                ss << val << " ";
            }
            log(ss.str());

            return data;
        },
        AsyncExecutor::Priority::Normal);

    // 第2阶段：处理数据（在另一个任务中串联）
    log("阶段2: 数据处理");
    auto processingFuture = executor.execute(
        [&dataFuture]() {
            auto data = dataFuture.get();
            log("处理数据");

            std::vector<int> processed;
            for (int val : data) {
                processed.push_back(val * val);
            }

            std::stringstream ss;
            ss << "处理后的数据: ";
            for (int val : processed) {
                ss << val << " ";
            }
            log(ss.str());

            return processed;
        },
        AsyncExecutor::Priority::High);

    // 第3阶段：结果整合（用Timer延迟1秒）
    log("阶段3: 结果整合 (定时执行，1秒后)");
    atom::async::Timer timer;
    auto resultFuture = timer.setTimeout(
        [&processingFuture]() {
            auto processed = processingFuture.get();
            log("整合最终结果");

            int sum = 0;
            int product = 1;
            for (int val : processed) {
                sum += val;
                product *= val;
            }

            std::stringstream ss;
            ss << "最终结果 - 总和: " << sum << ", 乘积: " << product;
            log(ss.str());

            return std::make_pair(sum, product);
        },
        1000);

    // 获取最终结果
    log("等待整个流程完成");
    auto [sum, product] = resultFuture.wait();

    log("复杂任务流程已完成");
    log("最终总和: " + std::to_string(sum));
    log("最终乘积: " + std::to_string(product));
}

// 主函数
int main() {
    std::cout << "=======================================" << std::endl;
    std::cout << "    AsyncExecutor 使用示例    " << std::endl;
    std::cout << "=======================================" << std::endl;

    try {
        // 执行各种示例
        basicUsageExample();
        priorityTasksExample();
        deferredTasksExample();
        scheduledTasksExample();
        errorHandlingExample();
        resizeExample();
        edgeCasesExample();
        complexTasksExample();

        std::cout << "\n所有示例已完成!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "主函数捕获到未处理的异常: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
