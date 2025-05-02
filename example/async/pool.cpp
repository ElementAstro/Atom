#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "atom/async/pool.hpp"

using namespace atom::async;
using namespace std::chrono_literals;

// 辅助函数：格式化输出当前时间
std::string getCurrentTimeStr() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    auto timer = std::chrono::system_clock::to_time_t(now);
    std::tm bt = *std::localtime(&timer);

    std::ostringstream oss;
    oss << std::put_time(&bt, "%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}

// 辅助函数：打印带有线程ID的消息
void printMessage(const std::string& message) {
    std::cout << "[" << getCurrentTimeStr() << "][线程 "
              << std::this_thread::get_id() << "] " << message << std::endl;
}

// 基本任务函数：休眠指定时间并返回一个值
int basicTask(int id, int sleepMs) {
    printMessage("开始执行任务 #" + std::to_string(id) + "，将睡眠 " +
                 std::to_string(sleepMs) + "ms");
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    printMessage("完成任务 #" + std::to_string(id));
    return id * 10;
}

// 抛出异常的任务
void errorTask() {
    printMessage("开始执行出错的任务");
    std::this_thread::sleep_for(100ms);
    printMessage("任务将抛出异常");
    throw std::runtime_error("这是一个测试异常");
}

// 递归强度计算任务（CPU密集型）
long long fibonacci(int n) {
    if (n <= 1)
        return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

// 示例1：基本用法
void basicUsageExample() {
    std::cout << "\n=== 示例1：基本线程池用法 ===\n" << std::endl;

    // 创建默认线程池
    ThreadPool pool;

    printMessage("主线程：创建了线程池，线程数量: " +
                 std::to_string(pool.getThreadCount()));

    // 提交几个简单任务
    using TaskFunc = int (*)(int, int);
    auto future1 = pool.submit(static_cast<TaskFunc>(basicTask), 1, 500);
    auto future2 = pool.submit(static_cast<TaskFunc>(basicTask), 2, 300);
    auto future3 = pool.submit(static_cast<TaskFunc>(basicTask), 3, 100);

    printMessage("主线程：已提交3个任务");

    // 等待并获取结果
    printMessage("主线程：等待结果...");
    int result1 = future1.get();
    int result2 = future2.get();
    int result3 = future3.get();

    printMessage("主线程：所有任务结果: " + std::to_string(result1) + ", " +
                 std::to_string(result2) + ", " + std::to_string(result3));
}

// 示例2：使用不同的线程池配置
void threadPoolConfigExample() {
    std::cout << "\n=== 示例2：不同线程池配置 ===\n" << std::endl;

    // 高性能线程池
    {
        ThreadPool::Options options =
            ThreadPool::Options::createHighPerformance();
        ThreadPool highPerfPool(options);

        printMessage("高性能线程池创建，线程数: " +
                     std::to_string(highPerfPool.getThreadCount()));

        auto future = highPerfPool.submit([]() {
            printMessage("在高性能线程池中执行计算密集型任务");
            return fibonacci(40);
        });

        printMessage("等待高性能计算结果...");
        long long result = future.get();
        printMessage("斐波那契计算结果: " + std::to_string(result));
    }

    // 低延迟线程池
    {
        ThreadPool::Options options = ThreadPool::Options::createLowLatency();
        ThreadPool lowLatencyPool(options);

        printMessage("低延迟线程池创建，线程数: " +
                     std::to_string(lowLatencyPool.getThreadCount()));

        auto future = lowLatencyPool.submit([]() {
            printMessage("在低延迟线程池中执行任务");
            std::this_thread::sleep_for(50ms);
            return "低延迟任务完成";
        });

        printMessage("等待低延迟任务结果...");
        std::string result = future.get();
        printMessage(result);
    }

    // 自定义配置线程池
    {
        ThreadPool::Options options;
        options.initialThreadCount = 2;
        options.maxThreadCount = 4;
        options.threadIdleTimeout = 1000ms;
        options.allowThreadGrowth = true;
        options.threadPriority =
            ThreadPool::Options::ThreadPriority::BelowNormal;

        ThreadPool customPool(options);

        printMessage("自定义线程池创建，初始线程数: " +
                     std::to_string(customPool.getThreadCount()));

        // 添加足够的任务使线程池增长
        std::vector<EnhancedFuture<int>> futures;

        for (int i = 0; i < 5; i++) {
            futures.push_back(customPool.submit([i]() {
                printMessage("自定义池任务 #" + std::to_string(i) + " 运行中");
                std::this_thread::sleep_for(500ms);
                return i;
            }));
        }

        // 观察线程池大小是否增长
        std::this_thread::sleep_for(100ms);
        printMessage("提交5个任务后，线程数: " +
                     std::to_string(customPool.getThreadCount()));

        // 等待所有任务完成
        for (auto& future : futures) {
            future.wait();
        }
    }
}

// 示例3：批量任务提交和处理
void batchTasksExample() {
    std::cout << "\n=== 示例3：批量任务提交 ===\n" << std::endl;

    ThreadPool pool;

    // 创建一些输入数据
    std::vector<int> inputs = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    printMessage("提交10个批量任务处理");

    // 使用submitBatch处理
    auto futures = pool.submitBatch(inputs.begin(), inputs.end(), [](int n) {
        printMessage("处理输入: " + std::to_string(n));
        std::this_thread::sleep_for(std::chrono::milliseconds(100 * n));
        return n * n;
    });

    printMessage("等待批量任务结果...");

    // 收集所有结果
    std::vector<int> results;
    for (auto& future : futures) {
        results.push_back(future.get());
    }

    printMessage("批量任务结果:");
    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << "  输入: " << inputs[i] << ", 结果: " << results[i]
                  << std::endl;
    }
}

// 示例4：Promise和Future高级用法
void promiseFutureExample() {
    std::cout << "\n=== 示例4：Promise和Future高级用法 ===\n" << std::endl;

    ThreadPool pool;

    // 使用Promise
    auto future = pool.submit(
        [](int a, int b) {
            printMessage("执行Promise任务计算 " + std::to_string(a) + " + " +
                         std::to_string(b));
            std::this_thread::sleep_for(300ms);
            return a + b;
        },
        10, 20);

    // 获取Future
    // 等待结果并处理
    try {
        auto result = future.get();
        printMessage("Promise任务回调1: 结果是 " + std::to_string(result));
        printMessage("Promise任务回调2: 新结果是 " +
                     std::to_string(result * 2));
    } catch (const std::exception& e) {
        printMessage("任务执行出错: " + std::string(e.what()));
    }

    printMessage("主线程：继续执行其他工作");
    std::this_thread::sleep_for(200ms);

    printMessage("主线程：等待Promise任务完成");
    future.wait();
    printMessage("Promise任务已完成");
}

// 示例5：错误处理
void errorHandlingExample() {
    std::cout << "\n=== 示例5：错误处理 ===\n" << std::endl;

    ThreadPool pool;

    // 提交一个会抛出异常的任务
    printMessage("提交将抛出异常的任务");
    using ErrorFunc = void (*)();
    auto errorFuture = pool.submit(static_cast<ErrorFunc>(errorTask));

    // 使用try-catch处理异常
    try {
        printMessage("主线程：等待出错任务结果...");
        errorFuture.get();
    } catch (const std::exception& e) {
        printMessage("捕获到异常: " + std::string(e.what()));
    }

    // 提交另一个异常任务，但使用Promise处理
    auto errorFuture2 = pool.submit([]() {
        printMessage("执行另一个出错任务");
        std::this_thread::sleep_for(200ms);
        throw std::runtime_error("Promise任务异常");
    });

    try {
        errorFuture2.get();
    } catch (const std::exception& e) {
        printMessage("捕获到任务异常: " + std::string(e.what()));
    }

    printMessage("错误处理示例完成");
}

// 示例6：线程池调整大小
void resizeExample() {
    std::cout << "\n=== 示例6：线程池大小调整 ===\n" << std::endl;

    // 创建初始有2个线程的池
    ThreadPool::Options options;
    options.initialThreadCount = 2;
    options.maxThreadCount = 8;
    options.allowThreadGrowth = true;
    options.allowThreadShrink = true;

    ThreadPool pool(options);

    printMessage("初始线程池大小: " + std::to_string(pool.getThreadCount()));

    // 手动增加线程数量
    printMessage("手动将线程池大小调整为6");
    pool.resize(6);
    printMessage("调整后线程池大小: " + std::to_string(pool.getThreadCount()));

    // 提交一些任务
    std::vector<EnhancedFuture<void>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.submit([i]() {
            printMessage("任务 " + std::to_string(i) + " 执行");
            std::this_thread::sleep_for(300ms);
        }));
    }

    // 等待任务完成
    for (auto& f : futures) {
        f.wait();
    }

    // 减少线程数量
    printMessage("减少线程池大小至3");
    pool.resize(3);
    std::this_thread::sleep_for(100ms);
    printMessage("调整后线程池大小: " + std::to_string(pool.getThreadCount()));

    // 尝试将大小设为0（这会导致异常）
    try {
        printMessage("尝试将线程池大小设为0（这应该会失败）");
        pool.resize(0);
    } catch (const std::exception& e) {
        printMessage("捕获到预期异常: " + std::string(e.what()));
    }
}

// 示例7：自动增长和工作窃取
void autoGrowthExample() {
    std::cout << "\n=== 示例7：自动增长和工作窃取 ===\n" << std::endl;

    // 创建初始有1个线程但可自动增长的池
    ThreadPool::Options options;
    options.initialThreadCount = 1;
    options.maxThreadCount = 4;
    options.allowThreadGrowth = true;
    options.useWorkStealing = true;

    ThreadPool pool(options);

    printMessage("创建可自动增长的线程池，初始大小: " +
                 std::to_string(pool.getThreadCount()));

    // 创建一些计算密集型任务
    std::vector<EnhancedFuture<long long>> futures;

    printMessage("提交4个计算密集型任务");
    for (int i = 0; i < 4; ++i) {
        int n = 38 + i;  // 增大斐波那契数列计算值
        futures.push_back(pool.submit([n]() {
            printMessage("开始计算斐波那契(" + std::to_string(n) + ")");
            auto result = fibonacci(n);
            printMessage("完成斐波那契(" + std::to_string(n) +
                         ") = " + std::to_string(result));
            return result;
        }));
    }

    // 观察线程池是否自动增长
    std::this_thread::sleep_for(500ms);
    printMessage("提交任务后线程池大小: " +
                 std::to_string(pool.getThreadCount()));
    printMessage("活跃线程数: " + std::to_string(pool.getActiveThreadCount()));

    // 等待所有计算完成
    printMessage("等待所有计算完成...");
    for (auto& future : futures) {
        future.wait();
    }
    printMessage("所有计算已完成");
}

// 示例8：全局线程池和辅助函数
void globalPoolExample() {
    std::cout << "\n=== 示例8：全局线程池和辅助函数 ===\n" << std::endl;

    // 使用全局线程池
    printMessage("使用全局线程池");
    auto future1 = async([]() {
        printMessage("在全局线程池中执行任务");
        std::this_thread::sleep_for(300ms);
        return "全局线程池任务完成";
    });

    // 使用高性能线程池
    printMessage("使用高性能线程池");
    auto future2 = asyncHighPerformance([]() {
        printMessage("在高性能线程池中执行任务");
        std::this_thread::sleep_for(200ms);
        return "高性能线程池任务完成";
    });

    // 使用低延迟线程池
    printMessage("使用低延迟线程池");
    auto future3 = asyncLowLatency([]() {
        printMessage("在低延迟线程池中执行任务");
        std::this_thread::sleep_for(100ms);
        return "低延迟线程池任务完成";
    });

    // 使用节能线程池
    printMessage("使用节能线程池");
    auto future4 = asyncEnergyEfficient([]() {
        printMessage("在节能线程池中执行任务");
        std::this_thread::sleep_for(150ms);
        return "节能线程池任务完成";
    });

    // 等待并打印所有结果
    printMessage(future1.get());
    printMessage(future2.get());
    printMessage(future3.get());
    printMessage(future4.get());
}

// 示例9：边界情况和极限测试
void edgeCasesExample() {
    std::cout << "\n=== 示例9：边界情况和极限测试 ===\n" << std::endl;

    // 创建有限队列大小的线程池
    ThreadPool::Options options;
    options.initialThreadCount = 2;
    options.maxQueueSize = 5;  // 最多允许5个任务排队

    ThreadPool pool(options);
    printMessage("创建队列大小有限的线程池");

    // 尝试提交超过队列大小的任务
    printMessage("尝试提交超过队列容量的任务");

    std::vector<EnhancedFuture<int>> futures;
    try {
        // 提交足够多的长时间运行任务来填满队列
        for (int i = 0; i < 10; ++i) {
            printMessage("提交任务 #" + std::to_string(i));
            futures.push_back(pool.submit([i]() {
                printMessage("执行任务 #" + std::to_string(i));
                std::this_thread::sleep_for(500ms);
                return i;
            }));
        }
    } catch (const std::exception& e) {
        printMessage("捕获到预期异常: " + std::string(e.what()));
    }

    // 测试非法参数
    try {
        printMessage("尝试创建线程数为0的线程池");
        ThreadPool::Options invalidOptions;
        invalidOptions.initialThreadCount = 0;
        invalidOptions.maxThreadCount = 0;
        ThreadPool invalidPool(invalidOptions);
    } catch (const std::exception& e) {
        printMessage("捕获到异常: " + std::string(e.what()));
    }

    // 等待已提交任务完成
    for (auto& future : futures) {
        try {
            future.wait();
        } catch (...) {
            // 忽略任何错误
        }
    }
}

// 主函数
int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "        线程池 (ThreadPool) 使用示例        " << std::endl;
    std::cout << "============================================" << std::endl;

    // 运行所有示例
    basicUsageExample();
    threadPoolConfigExample();
    batchTasksExample();
    promiseFutureExample();
    errorHandlingExample();
    resizeExample();
    autoGrowthExample();
    globalPoolExample();
    edgeCasesExample();

    std::cout << "\n所有示例已完成\n" << std::endl;
    return 0;
}