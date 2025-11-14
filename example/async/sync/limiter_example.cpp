#include <chrono>
#include <coroutine>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "atom/async/limiter.hpp"
#include "atom/log/loguru.hpp"

// Helper struct for coroutine task
struct task {
    struct promise_type {
        task get_return_object() {
            return task(
                std::coroutine_handle<promise_type>::from_promise(*this));
        }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };

    std::coroutine_handle<promise_type> handle;

    task(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~task() {
        if (handle)
            handle.destroy();
    }
};

// 辅助宏，用于测试和输出
#define LOG(msg) std::cout << "[" << __LINE__ << "] " << msg << std::endl

// 示例协程函数
auto rate_limited_task(atom::async::RateLimiter& limiter,
                       const std::string& name) -> task {
    co_await limiter.acquire(name);
    LOG("执行函数: " + name);
}

int main() {
    using namespace std::chrono_literals;

    std::cout << "===== atom::async::limiter.hpp 使用示例 =====" << std::endl
              << std::endl;

    //==============================================================
    // 1. RateLimiter 基本用法
    //==============================================================
    LOG("1. RateLimiter 基本用法");
    {
        // 创建限流器并设置函数限制
        atom::async::RateLimiter limiter;
        limiter.setFunctionLimit("test_function", 3,
                                 2s);  // 每2秒最多允许3次调用

        LOG("尝试执行 test_function 5次 (限制为每2秒3次)");

        // 执行不会超过限制的调用
        for (int i = 0; i < 3; ++i) {
            rate_limited_task(limiter, "test_function");
        }

        // 尝试超出限制的调用
        try {
            rate_limited_task(limiter, "test_function");
            rate_limited_task(limiter, "test_function");
        } catch (const atom::async::RateLimitExceededException& e) {
            LOG("捕获到速率限制异常: " + std::string(e.what()));
        }

        // 等待时间窗口过去
        LOG("等待2秒后再次尝试...");
        std::this_thread::sleep_for(2s);

        // 现在应该可以再次调用了
        rate_limited_task(limiter, "test_function");
        LOG("成功执行!");

        // 检查被拒绝的请求数量
        auto rejected = limiter.getRejectedRequests("test_function");
        LOG("test_function 被拒绝的请求数: " + std::to_string(rejected));
    }

    std::cout << std::endl;

    //==============================================================
    // 2. RateLimiter 不同参数组合
    //==============================================================
    LOG("2. RateLimiter 不同参数组合");
    {
        atom::async::RateLimiter limiter;

        // 设置不同函数的不同限制
        limiter.setFunctionLimit("high_frequency", 10, 1s);  // 高频率：每秒10次
        limiter.setFunctionLimit("medium_frequency", 5,
                                 2s);  // 中频率：每2秒5次
        limiter.setFunctionLimit("low_frequency", 2, 5s);  // 低频率：每5秒2次

        LOG("设置了不同函数的不同限制:");
        LOG("- high_frequency: 每秒10次");
        LOG("- medium_frequency: 每2秒5次");
        LOG("- low_frequency: 每5秒2次");

        // 测试高频率函数
        LOG("\n测试高频率函数 (high_frequency):");
        for (int i = 0; i < 8; ++i) {
            rate_limited_task(limiter, "high_frequency");
        }

        // 测试中频率函数
        LOG("\n测试中频率函数 (medium_frequency):");
        for (int i = 0; i < 4; ++i) {
            rate_limited_task(limiter, "medium_frequency");
        }

        // 测试低频率函数
        LOG("\n测试低频率函数 (low_frequency):");
        try {
            rate_limited_task(limiter, "low_frequency");
            rate_limited_task(limiter, "low_frequency");
            rate_limited_task(limiter, "low_frequency");  // 这个应该会被限制
        } catch (const atom::async::RateLimitExceededException& e) {
            LOG("捕获到异常 (预期行为): " + std::string(e.what()));
        }

        // 批量设置函数限制
        std::vector<
            std::pair<std::string_view, atom::async::RateLimiter::Settings>>
            settings = {
                {"batch_func1", atom::async::RateLimiter::Settings(5, 3s)},
                {"batch_func2", atom::async::RateLimiter::Settings(3, 4s)}};

        limiter.setFunctionLimits(settings);
        LOG("\n批量设置了函数限制 (batch_func1, batch_func2)");

        // 批量获取限流器
        std::vector<std::string_view> func_names = {"batch_func1",
                                                    "batch_func2"};
        auto awaiters = limiter.acquireBatch(func_names);
        LOG("批量获取了限流器 awaiters.size() = " +
            std::to_string(awaiters.size()));
    }

    std::cout << std::endl;

    //==============================================================
    // 3. RateLimiter 暂停与恢复功能
    //==============================================================
    LOG("3. RateLimiter 暂停与恢复功能");
    {
        atom::async::RateLimiter limiter;
        limiter.setFunctionLimit("pausable_function", 2, 1s);

        rate_limited_task(limiter, "pausable_function");
        LOG("暂停限流器");
        limiter.pause();

        // 暂停时应该可以不受限制地调用
        rate_limited_task(limiter, "pausable_function");
        rate_limited_task(limiter, "pausable_function");
        rate_limited_task(limiter, "pausable_function");
        LOG("在暂停状态下成功执行了多次调用");

        LOG("恢复限流器");
        limiter.resume();

        // 恢复后应该会重新应用限制
        try {
            rate_limited_task(limiter, "pausable_function");
            rate_limited_task(limiter, "pausable_function");
            rate_limited_task(limiter, "pausable_function");  // 应该会被限制
        } catch (const atom::async::RateLimitExceededException& e) {
            LOG("恢复后捕获到限制异常: " + std::string(e.what()));
        }
    }

    std::cout << std::endl;

    //==============================================================
    // 4. RateLimiter 重置功能
    //==============================================================
    LOG("4. RateLimiter 重置功能");
    {
        atom::async::RateLimiter limiter;
        limiter.setFunctionLimit("reset_function", 1,
                                 10s);  // 严格限制：每10秒只能调用1次

        rate_limited_task(limiter, "reset_function");

        try {
            rate_limited_task(limiter, "reset_function");  // 应该会被限制
        } catch (const atom::async::RateLimitExceededException& e) {
            LOG("预期的异常: " + std::string(e.what()));
        }

        LOG("重置函数的限流计数器");
        limiter.resetFunction("reset_function");

        // 重置后应该可以再次调用
        rate_limited_task(limiter, "reset_function");
        LOG("重置后成功调用函数");

        // 再次尝试应该会被限制
        try {
            rate_limited_task(limiter, "reset_function");
        } catch (const atom::async::RateLimitExceededException& e) {
            LOG("重置后再次超限: " + std::string(e.what()));
        }

        LOG("重置所有限流计数器");
        limiter.resetAll();
        rate_limited_task(limiter, "reset_function");
        LOG("全部重置后成功调用函数");
    }

    std::cout << std::endl;

    //==============================================================
    // 5. RateLimiter 边缘情况
    //==============================================================
    LOG("5. RateLimiter 边缘情况");
    {
        atom::async::RateLimiter limiter;

        // 边缘情况1: 设置为0的限制
        try {
            limiter.setFunctionLimit("zero_limit", 0, 1s);
        } catch (const std::invalid_argument& e) {
            LOG("边缘情况1 - 设置为0的限制: " + std::string(e.what()));
        }

        // 边缘情况2: 设置为负的时间窗口
        try {
            limiter.setFunctionLimit("negative_window", 5, -1s);
        } catch (const std::invalid_argument& e) {
            LOG("边缘情况2 - 负的时间窗口: " + std::string(e.what()));
        }

        // 边缘情况3: 非常高的请求限制
        limiter.setFunctionLimit("very_high_limit", 1000000, 1s);
        LOG("边缘情况3 - 设置了非常高的请求限制: 1000000/秒");

        // 边缘情况4: 非常低的请求限制
        limiter.setFunctionLimit("very_low_limit", 1, 300s);
        LOG("边缘情况4 - 设置了非常低的请求限制: 1/300秒");

        // 边缘情况5: 对不存在的函数获取被拒绝的请求数
        auto rejected = limiter.getRejectedRequests("non_existent_function");
        LOG("边缘情况5 - 不存在的函数的被拒绝请求数: " +
            std::to_string(rejected));

        // 边缘情况6: 重置不存在的函数
        limiter.resetFunction("non_existent_function");
        LOG("边缘情况6 - 重置了不存在的函数");
    }

    std::cout << std::endl;

    //==============================================================
    // 6-11. Debounce/Throttle 示例位置变更
    //==============================================================
    LOG("Debounce/Throttle 示例已迁移至独立文件: example/async/lodash.cpp");
    LOG("该文件包含: "
        "Debounce(leading/trailing/maxWait/cancel/flush/reset/callCount)、");
    LOG("Throttle(leading/trailing/both/cancel/reset/"
        "callCount)、以及工厂类用法。");
    LOG("此 limiter.cpp 保留 RateLimiter 相关演示，避免篇幅过长。");

    std::cout << std::endl;

    //==============================================================
    // 12. 单例模式限流器
    //==============================================================
    LOG("12. RateLimiterSingleton 使用");
    {
        // 获取单例实例
        auto& limiter = atom::async::RateLimiterSingleton::instance();

        // 设置限流参数
        limiter.setFunctionLimit("singleton_func", 2, 1s);

        LOG("通过单例限流器执行函数");
        rate_limited_task(limiter, "singleton_func");
        rate_limited_task(limiter, "singleton_func");

        try {
            rate_limited_task(limiter, "singleton_func");  // 应该会被限制
        } catch (const atom::async::RateLimitExceededException& e) {
            LOG("单例限流器异常: " + std::string(e.what()));
        }

        // 重置后应该可以再次调用
        limiter.resetFunction("singleton_func");
        rate_limited_task(limiter, "singleton_func");
        LOG("重置后成功调用");
    }

    return 0;
}
