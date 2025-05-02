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
                                 2s);                      // 中频率：每2秒5次
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
    // 6. Debounce 基本用法
    //==============================================================
    LOG("6. Debounce 基本用法");
    {
        int call_count = 0;
        auto debounced_function = atom::async::Debounce<std::function<void()>>(
            [&call_count]() {
                call_count++;
                LOG("Debounced 函数被调用! 当前计数: " +
                    std::to_string(call_count));
            },
            100ms  // 100毫秒的去抖动延迟
        );

        LOG("快速连续调用debounced_function 5次");
        for (int i = 0; i < 5; ++i) {
            debounced_function();
            std::this_thread::sleep_for(20ms);  // 间隔小于去抖动时间
        }

        LOG("等待200毫秒让去抖动时间过去...");
        std::this_thread::sleep_for(200ms);

        LOG("再次连续调用4次");
        for (int i = 0; i < 4; ++i) {
            debounced_function();
            std::this_thread::sleep_for(20ms);
        }

        LOG("等待200毫秒...");
        std::this_thread::sleep_for(200ms);

        LOG("最终调用计数: " + std::to_string(debounced_function.callCount()));
        LOG("预期结果应该是2，因为应该只在每一组连续调用后执行一次");
    }

    std::cout << std::endl;

    //==============================================================
    // 7. Debounce 不同参数组合
    //==============================================================
    LOG("7. Debounce 不同参数组合");
    {
        LOG("7.1 前缘触发(leading=true)的去抖动:");
        {
            int call_count = 0;
            auto leading_debounce =
                atom::async::Debounce<std::function<void()>>(
                    [&call_count]() {
                        call_count++;
                        LOG("前缘触发Debounce被调用! 计数: " +
                            std::to_string(call_count));
                    },
                    150ms,  // 150毫秒的去抖动延迟
                    true    // 前缘触发
                );

            LOG("第一次调用 (应该立即执行)");
            leading_debounce();
            std::this_thread::sleep_for(50ms);

            LOG("连续快速调用3次 (不应该执行)");
            for (int i = 0; i < 3; ++i) {
                leading_debounce();
                std::this_thread::sleep_for(30ms);
            }

            LOG("等待去抖动时间过去...");
            std::this_thread::sleep_for(200ms);

            LOG("再次调用 (应该立即执行)");
            leading_debounce();
            std::this_thread::sleep_for(200ms);

            LOG("最终调用计数: " +
                std::to_string(leading_debounce.callCount()));
            LOG("预期应该是2，因为只有首次调用会立即执行");
        }

        LOG("\n7.2 带有最大等待时间的去抖动:");
        {
            int call_count = 0;
            auto max_wait_debounce =
                atom::async::Debounce<std::function<void()>>(
                    [&call_count]() {
                        call_count++;
                        LOG("最大等待时间Debounce被调用! 计数: " +
                            std::to_string(call_count));
                    },
                    500ms,                // 500毫秒的去抖动延迟
                    false,                // 不是前缘触发
                    std::optional(300ms)  // 300毫秒的最大等待时间
                );

            LOG("开始持续调用...");
            for (int i = 0; i < 10; ++i) {
                max_wait_debounce();
                LOG("调用 #" + std::to_string(i + 1));
                std::this_thread::sleep_for(50ms);  // 每50毫秒调用一次
            }

            LOG("等待1秒钟...");
            std::this_thread::sleep_for(1s);

            LOG("最终调用计数: " +
                std::to_string(max_wait_debounce.callCount()));
            LOG("预期应该大于1，因为即使不断调用，最大等待时间也会强制调用");
        }
    }

    std::cout << std::endl;

    //==============================================================
    // 8. Debounce 其它方法
    //==============================================================
    LOG("8. Debounce 其它方法");
    {
        int call_count = 0;
        auto debounced = atom::async::Debounce<std::function<void()>>(
            [&call_count]() {
                call_count++;
                LOG("Debounced 函数被调用! 计数: " +
                    std::to_string(call_count));
            },
            300ms  // 300毫秒的去抖动延迟
        );

        LOG("调用函数3次");
        debounced();
        debounced();
        debounced();

        LOG("立即刷新 (使用flush方法)");
        debounced.flush();

        LOG("再次调用2次然后取消");
        debounced();
        debounced();
        LOG("调用cancel()方法取消挂起的调用");
        debounced.cancel();

        LOG("等待500毫秒...");
        std::this_thread::sleep_for(500ms);

        LOG("再次调用然后重置");
        debounced();
        LOG("调用reset()方法重置去抖动器");
        debounced.reset();

        LOG("最终调用计数: " + std::to_string(debounced.callCount()));
        LOG("预期应该是1，因为只有flush()调用了函数");
    }

    std::cout << std::endl;

    //==============================================================
    // 9. Throttle 基本用法
    //==============================================================
    LOG("9. Throttle 基本用法");
    {
        int call_count = 0;
        auto throttled_function = atom::async::Throttle<std::function<void()>>(
            [&call_count]() {
                call_count++;
                LOG("Throttled 函数被调用! 计数: " +
                    std::to_string(call_count));
            },
            200ms  // 200毫秒的节流时间间隔
        );

        LOG("快速连续调用throttled_function 10次");
        for (int i = 0; i < 10; ++i) {
            throttled_function();
            std::this_thread::sleep_for(30ms);  // 间隔小于节流时间
        }

        LOG("等待500毫秒...");
        std::this_thread::sleep_for(500ms);

        LOG("最终调用计数: " + std::to_string(throttled_function.callCount()));
        LOG("预期应该是2或3，因为函数应该大约每200毫秒被调用一次");
    }

    std::cout << std::endl;

    //==============================================================
    // 10. Throttle 不同参数组合
    //==============================================================
    LOG("10. Throttle 不同参数组合");
    {
        LOG("10.1 前缘触发(leading=true)的节流:");
        {
            int call_count = 0;
            auto leading_throttle =
                atom::async::Throttle<std::function<void()>>(
                    [&call_count]() {
                        call_count++;
                        LOG("前缘触发Throttle被调用! 计数: " +
                            std::to_string(call_count));
                    },
                    300ms,  // 300毫秒的节流时间间隔
                    true    // 前缘触发
                );

            LOG("第一次调用 (应该立即执行)");
            leading_throttle();
            std::this_thread::sleep_for(50ms);

            LOG("连续快速调用5次 (应该被节流)");
            for (int i = 0; i < 5; ++i) {
                leading_throttle();
                std::this_thread::sleep_for(50ms);
            }

            LOG("等待400毫秒...");
            std::this_thread::sleep_for(400ms);

            LOG("再次调用 (应该立即执行，因为间隔已过)");
            leading_throttle();

            LOG("最终调用计数: " +
                std::to_string(leading_throttle.callCount()));
        }

        LOG("\n10.2 带有最大等待时间的节流:");
        {
            int call_count = 0;
            auto max_wait_throttle =
                atom::async::Throttle<std::function<void()>>(
                    [&call_count]() {
                        call_count++;
                        LOG("最大等待时间Throttle被调用! 计数: " +
                            std::to_string(call_count));
                    },
                    500ms,                // 500毫秒的节流时间间隔
                    false,                // 不是前缘触发
                    std::optional(300ms)  // 300毫秒的最大等待时间
                );

            LOG("开始持续调用...");
            for (int i = 0; i < 8; ++i) {
                max_wait_throttle();
                LOG("调用 #" + std::to_string(i + 1));
                std::this_thread::sleep_for(100ms);  // 每100毫秒调用一次
            }

            LOG("等待1秒钟...");
            std::this_thread::sleep_for(1s);

            LOG("最终调用计数: " +
                std::to_string(max_wait_throttle.callCount()));
            LOG("预期应该大于2，因为最大等待时间会保证额外的调用");
        }
    }

    std::cout << std::endl;

    //==============================================================
    // 11. 工厂类使用示例
    //==============================================================
    LOG("11. 工厂类使用示例");
    {
        LOG("11.1 ThrottleFactory:");
        // 创建节流工厂，设置共享参数
        atom::async::ThrottleFactory throttle_factory(200ms, true);

        int count1 = 0;
        auto throttled1 = throttle_factory.create([&count1]() {
            count1++;
            LOG("Throttled1 被调用: " + std::to_string(count1));
        });

        int count2 = 0;
        auto throttled2 = throttle_factory.create([&count2]() {
            count2++;
            LOG("Throttled2 被调用: " + std::to_string(count2));
        });

        LOG("连续调用两个不同的节流函数");
        for (int i = 0; i < 5; ++i) {
            throttled1();
            throttled2();
            std::this_thread::sleep_for(50ms);
        }

        LOG("等待300毫秒...");
        std::this_thread::sleep_for(300ms);

        LOG("Throttled1 调用计数: " + std::to_string(throttled1.callCount()));
        LOG("Throttled2 调用计数: " + std::to_string(throttled2.callCount()));

        LOG("\n11.2 DebounceFactory:");
        // 创建去抖动工厂
        atom::async::DebounceFactory debounce_factory(200ms, false);

        int count3 = 0;
        auto debounced1 = debounce_factory.create([&count3]() {
            count3++;
            LOG("Debounced1 被调用: " + std::to_string(count3));
        });

        int count4 = 0;
        auto debounced2 = debounce_factory.create([&count4]() {
            count4++;
            LOG("Debounced2 被调用: " + std::to_string(count4));
        });

        LOG("连续调用两个不同的去抖动函数");
        for (int i = 0; i < 3; ++i) {
            debounced1();
            debounced2();
            std::this_thread::sleep_for(50ms);
        }

        LOG("等待500毫秒...");
        std::this_thread::sleep_for(500ms);

        LOG("Debounced1 调用计数: " + std::to_string(debounced1.callCount()));
        LOG("Debounced2 调用计数: " + std::to_string(debounced2.callCount()));
    }

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