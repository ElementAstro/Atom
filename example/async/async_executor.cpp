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

    // 创建异步执行器
    AsyncExecutor executor(4);  // 4个工作线程
    log("创建了异步执行器，线程数: 4");

    // 使用IMMEDIATE策略执行任务
    log("使用IMMEDIATE策略提交3个任务");
    auto future1 =
        executor.schedule(AsyncExecutor::ExecutionStrategy::IMMEDIATE,
                          ExecutorTask::Priority::NORMAL, basicTask, 1, 500);

    auto future2 =
        executor.schedule(AsyncExecutor::ExecutionStrategy::IMMEDIATE,
                          ExecutorTask::Priority::HIGH, basicTask, 2, 300);

    auto future3 =
        executor.schedule(AsyncExecutor::ExecutionStrategy::IMMEDIATE,
                          ExecutorTask::Priority::LOW, basicTask, 3, 100);

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

    AsyncExecutor executor(1);  // 只使用1个线程，使优先级效果更明显
    log("创建了异步执行器，线程数: 1");

    // 创建多个不同优先级的任务
    log("提交不同优先级的任务 (低、普通、高、关键)");

    // 为了确保任务在队列中排队，先批量提交
    std::vector<std::future<int>> futures;

    // 低优先级
    futures.push_back(
        executor.schedule(AsyncExecutor::ExecutionStrategy::IMMEDIATE,
                          ExecutorTask::Priority::LOW, []() {
                              log("执行低优先级任务");
                              std::this_thread::sleep_for(100ms);
                              return 1;
                          }));

    // 普通优先级
    futures.push_back(
        executor.schedule(AsyncExecutor::ExecutionStrategy::IMMEDIATE,
                          ExecutorTask::Priority::NORMAL, []() {
                              log("执行普通优先级任务");
                              std::this_thread::sleep_for(100ms);
                              return 2;
                          }));

    // 高优先级
    futures.push_back(
        executor.schedule(AsyncExecutor::ExecutionStrategy::IMMEDIATE,
                          ExecutorTask::Priority::HIGH, []() {
                              log("执行高优先级任务");
                              std::this_thread::sleep_for(100ms);
                              return 3;
                          }));

    // 关键优先级
    futures.push_back(
        executor.schedule(AsyncExecutor::ExecutionStrategy::IMMEDIATE,
                          ExecutorTask::Priority::CRITICAL, []() {
                              log("执行关键优先级任务");
                              std::this_thread::sleep_for(100ms);
                              return 4;
                          }));

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

    AsyncExecutor executor;
    log("创建了异步执行器");

    // 添加延迟执行任务
    log("添加3个延迟执行任务");

    auto future1 = executor.schedule(AsyncExecutor::ExecutionStrategy::DEFERRED,
                                     ExecutorTask::Priority::NORMAL, []() {
                                         log("执行延迟任务 #1");
                                         std::this_thread::sleep_for(100ms);
                                         return "延迟任务1结果";
                                     });

    auto future2 = executor.schedule(AsyncExecutor::ExecutionStrategy::DEFERRED,
                                     ExecutorTask::Priority::HIGH, []() {
                                         log("执行延迟任务 #2");
                                         std::this_thread::sleep_for(150ms);
                                         return "延迟任务2结果";
                                     });

    auto future3 = executor.schedule(AsyncExecutor::ExecutionStrategy::DEFERRED,
                                     ExecutorTask::Priority::LOW, []() {
                                         log("执行延迟任务 #3");
                                         std::this_thread::sleep_for(50ms);
                                         return "延迟任务3结果";
                                     });

    log("延迟任务已添加但尚未执行");
    log("队列中任务数: " + std::to_string(executor.queueSize()));
    log("活动任务数: " + std::to_string(executor.activeTaskCount()));

    std::this_thread::sleep_for(200ms);

    // 执行延迟任务
    log("现在执行所有延迟任务");
    executor.executeDeferredTasks();

    // 获取结果
    log("等待延迟任务结果");
    std::string result1 = future1.get();
    std::string result2 = future2.get();
    std::string result3 = future3.get();

    log("所有延迟任务完成，结果:");
    log("任务1: " + result1);
    log("任务2: " + result2);
    log("任务3: " + result3);
}

// 4. 定时任务示例
void scheduledTasksExample() {
    log("\n=== 4. 定时任务示例 ===");

    AsyncExecutor executor;
    log("创建了异步执行器");

    auto now = std::chrono::system_clock::now();

    // 创建定时任务
    log("安排3个定时任务");

    // 1秒后执行
    auto future1 =
        executor.scheduleAt(now + 1s, ExecutorTask::Priority::NORMAL, []() {
            log("执行定时任务 #1 (1秒后)");
            return "定时任务1结果";
        });

    // 2秒后执行
    auto future2 =
        executor.scheduleAt(now + 2s, ExecutorTask::Priority::HIGH, []() {
            log("执行定时任务 #2 (2秒后)");
            return "定时任务2结果";
        });

    // 使用scheduleAfter
    auto future3 =
        executor.scheduleAfter(3s, ExecutorTask::Priority::LOW, []() {
            log("执行定时任务 #3 (3秒后)");
            return "定时任务3结果";
        });

    log("已安排所有定时任务");

    // 等待结果
    log("等待所有定时任务执行和完成");
    std::string result1 = future1.get();
    log("任务1完成: " + result1);

    std::string result2 = future2.get();
    log("任务2完成: " + result2);

    std::string result3 = future3.get();
    log("任务3完成: " + result3);

    log("所有定时任务已完成");
}

// 5. 错误处理示例
void errorHandlingExample() {
    log("\n=== 5. 错误处理示例 ===");

    AsyncExecutor executor;
    log("创建了异步执行器");

    // 提交一个会抛出异常的任务
    log("提交会抛出异常的任务");
    auto errorFuture =
        executor.schedule(AsyncExecutor::ExecutionStrategy::IMMEDIATE,
                          ExecutorTask::Priority::NORMAL, errorTask);

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
    auto lambdaErrorFuture =
        executor.schedule(AsyncExecutor::ExecutionStrategy::IMMEDIATE,
                          ExecutorTask::Priority::HIGH, []() -> std::string {
                              log("在lambda中执行抛出异常的任务");
                              throw std::runtime_error("Lambda错误");
                              return "不会返回";
                          });

    try {
        lambdaErrorFuture.get();
    } catch (const std::exception& e) {
        log("从lambda任务捕获到异常: " + std::string(e.what()));
    }

    // 测试延迟任务中的异常
    log("创建一个延迟任务，其中包含异常");
    auto deferredErrorFuture =
        executor.schedule(AsyncExecutor::ExecutionStrategy::DEFERRED,
                          ExecutorTask::Priority::NORMAL, []() {
                              log("执行延迟任务中的错误代码");
                              throw std::runtime_error("延迟任务错误");
                              return 0;
                          });

    executor.executeDeferredTasks();

    try {
        deferredErrorFuture.get();
    } catch (const std::exception& e) {
        log("从延迟任务捕获到异常: " + std::string(e.what()));
    }
}

// 6. 线程池调整大小示例
void resizeExample() {
    log("\n=== 6. 线程池调整大小示例 ===");

    // 创建小的线程池
    AsyncExecutor executor(2);
    log("创建了线程池，初始大小: 2");

    // 检查初始大小
    log("提交多个长时间运行的任务");
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 6; i++) {
        futures.push_back(
            executor.schedule(AsyncExecutor::ExecutionStrategy::IMMEDIATE,
                              ExecutorTask::Priority::NORMAL, [i]() {
                                  log("开始执行任务 " + std::to_string(i));
                                  std::this_thread::sleep_for(500ms);
                                  log("完成任务 " + std::to_string(i));
                              }));
    }

    // 给一点时间开始执行
    std::this_thread::sleep_for(200ms);
    log("当前活动任务数: " + std::to_string(executor.activeTaskCount()));
    log("队列中任务数: " + std::to_string(executor.queueSize()));

    // 增加线程池大小
    log("将线程池大小增加到4");
    executor.resize(4);

    std::this_thread::sleep_for(200ms);
    log("调整后活动任务数: " + std::to_string(executor.activeTaskCount()));
    log("调整后队列中任务数: " + std::to_string(executor.queueSize()));

    // 等待所有任务完成
    for (auto& future : futures) {
        future.wait();
    }
    log("所有任务已完成");

    // 减小线程池大小
    log("将线程池大小减少到1");
    executor.resize(1);

    // 测试线程池大小减少后的行为
    auto future = executor.schedule(AsyncExecutor::ExecutionStrategy::IMMEDIATE,
                                    ExecutorTask::Priority::NORMAL, []() {
                                        log("在调整大小后的线程池中执行任务");
                                        std::this_thread::sleep_for(100ms);
                                        return "完成";
                                    });

    log("结果: " + std::string(future.get()));
}

// 7. 边界情况和异常场景
void edgeCasesExample() {
    log("\n=== 7. 边界情况和异常场景 ===");

    // 尝试创建线程数为0的执行器
    log("尝试创建线程数为0的执行器 (应该失败)");
    try {
        AsyncExecutor invalidExecutor(0);
        log("这行不应该被打印");
    } catch (const std::exception& e) {
        log("捕获到异常: " + std::string(e.what()));
    }

    // 正常创建执行器
    AsyncExecutor executor(1);
    log("成功创建了线程数为1的执行器");

    // 尝试不支持的执行策略
    log("尝试直接使用SCHEDULED策略 (应该失败)");
    try {
        executor.schedule(AsyncExecutor::ExecutionStrategy::SCHEDULED,
                          ExecutorTask::Priority::NORMAL,
                          []() { return true; });
        log("这行不应该被打印");
    } catch (const std::exception& e) {
        log("捕获到异常: " + std::string(e.what()));
    }

    // 测试调整大小到0
    log("尝试将线程池大小调整为0 (应该失败)");
    try {
        executor.resize(0);
        log("这行不应该被打印");
    } catch (const std::exception& e) {
        log("捕获到异常: " + std::string(e.what()));
    }

    // 测试非常远的未来的定时任务
    log("安排一个10年后执行的任务");
    auto futureFarAway =
        executor.scheduleAfter(std::chrono::hours(24 * 365 * 10),  // 10年
                               ExecutorTask::Priority::LOW, []() {
                                   log("10年后的任务执行了");
                                   return true;
                               });

    log("远期任务已安排 (但不会在本示例中等待)");

    // 测试waitForAll
    log("提交几个快速任务然后等待所有完成");

    for (int i = 0; i < 3; i++) {
        executor.schedule(AsyncExecutor::ExecutionStrategy::IMMEDIATE,
                          ExecutorTask::Priority::NORMAL, [i]() {
                              log("执行快速任务 " + std::to_string(i));
                              std::this_thread::sleep_for(50ms);
                          });
    }

    log("调用waitForAll()");
    executor.waitForAll();
    log("所有任务已完成");
}

// 8. 复杂任务组合示例
void complexTasksExample() {
    log("\n=== 8. 复杂任务组合示例 ===");

    AsyncExecutor executor(4);
    log("创建了异步执行器，线程数: 4");

    // 模拟一个多阶段处理流程
    // 1. 初始数据生成 (立即执行)
    // 2. 数据处理 (延迟执行)
    // 3. 结果整合 (定时执行)

    log("开始复杂任务流程");

    // 第1阶段：生成数据
    log("阶段1: 生成数据 (立即执行)");
    auto dataFuture =
        executor.schedule(AsyncExecutor::ExecutionStrategy::IMMEDIATE,
                          ExecutorTask::Priority::NORMAL, []() {
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
                          });

    // 第2阶段：处理数据 (延迟执行)
    log("阶段2: 数据处理 (延迟执行)");
    auto processingFuture = executor.schedule(
        AsyncExecutor::ExecutionStrategy::DEFERRED,
        ExecutorTask::Priority::HIGH, [&dataFuture]() {
            // 获取第1阶段的数据
            auto data = dataFuture.get();
            log("处理数据");

            std::vector<int> processed;
            for (int val : data) {
                processed.push_back(val * val);  // 简单处理：平方
            }

            std::stringstream ss;
            ss << "处理后的数据: ";
            for (int val : processed) {
                ss << val << " ";
            }
            log(ss.str());

            return processed;
        });

    // 第3阶段：结果整合 (定时执行)
    log("阶段3: 结果整合 (定时执行，1秒后)");
    auto resultFuture = executor.scheduleAfter(
        1s, ExecutorTask::Priority::CRITICAL, [&processingFuture, &executor]() {
            // 确保先执行延迟任务
            executor.executeDeferredTasks();

            // 获取第2阶段的数据
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
        });

    // 获取最终结果
    log("等待整个流程完成");
    auto [sum, product] = resultFuture.get();

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