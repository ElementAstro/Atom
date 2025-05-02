#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// 如果需要使用 Boost 锁定功能，取消下面的注释
// #define ATOM_USE_BOOST_LOCKS

// 如果需要使用 Boost 无锁队列功能，取消下面的注释
// #define ATOM_USE_BOOST_LOCKFREE

#include "atom/async/trigger.hpp"

// 辅助宏，用于格式化输出
#define SECTION(name) std::cout << "\n=== " << name << " ===\n"
#define LOG(msg) std::cout << "[" << __LINE__ << "] " << msg << std::endl

// 简单事件数据结构
struct EventData {
    std::string message;
    int value;

    // 支持复制构造和赋值
    EventData(const std::string& msg = "", int val = 0)
        : message(msg), value(val) {}
};

// 打印事件数据
void printEventData(const EventData& data) {
    std::cout << "  消息: \"" << data.message << "\", 值: " << data.value
              << std::endl;
}

int main() {
    using namespace std::chrono_literals;

    std::cout << "===== atom::async::Trigger 使用示例 =====\n\n";

    //==============================================================
    // 1. 基本用法
    //==============================================================
    SECTION("1. 基本用法");
    {
        // 创建一个使用 EventData 类型参数的触发器
        atom::async::Trigger<EventData> trigger;

        // 注册一个基本回调函数
        auto callbackId =
            trigger.registerCallback("basic_event", [](const EventData& data) {
                std::cout << "收到基本事件: ";
                printEventData(data);
            });

        LOG("注册了回调ID: " + std::to_string(callbackId));

        // 触发事件
        EventData basicData{"这是基本事件", 42};
        size_t count = trigger.trigger("basic_event", basicData);
        LOG("触发的回调数量: " + std::to_string(count));

        // 检查回调计数
        LOG("事件有回调: " +
            std::string(trigger.hasCallbacks("basic_event") ? "是" : "否"));
        LOG("回调数量: " +
            std::to_string(trigger.callbackCount("basic_event")));

        // 注销回调
        bool unregistered =
            trigger.unregisterCallback("basic_event", callbackId);
        LOG("注销成功: " + std::string(unregistered ? "是" : "否"));
        LOG("注销后回调数量: " +
            std::to_string(trigger.callbackCount("basic_event")));

        // 再次触发，应该没有回调被执行
        count = trigger.trigger("basic_event", basicData);
        LOG("再次触发的回调数量: " + std::to_string(count));
    }

    //==============================================================
    // 2. 带优先级的回调
    //==============================================================
    SECTION("2. 带优先级的回调");
    {
        atom::async::Trigger<EventData> trigger;

        // 注册不同优先级的回调
        [[maybe_unused]] auto lowId = trigger.registerCallback(
            "priority_event",
            [](const EventData& data) {
                std::cout << "  低优先级回调: ";
                printEventData(data);
            },
            atom::async::Trigger<EventData>::CallbackPriority::Low);

        [[maybe_unused]] auto normalId = trigger.registerCallback(
            "priority_event",
            [](const EventData& data) {
                std::cout << "  普通优先级回调: ";
                printEventData(data);
            },
            atom::async::Trigger<EventData>::CallbackPriority::Normal);

        [[maybe_unused]] auto highId = trigger.registerCallback(
            "priority_event",
            [](const EventData& data) {
                std::cout << "  高优先级回调: ";
                printEventData(data);
            },
            atom::async::Trigger<EventData>::CallbackPriority::High);

        // 触发事件，应该按优先级执行回调（高 -> 普通 -> 低）
        LOG("触发具有不同优先级的回调：");
        EventData data{"优先级测试", 100};
        trigger.trigger("priority_event", data);
    }

    //==============================================================
    // 3. 延时触发
    //==============================================================
    SECTION("3. 延时触发");
    {
        atom::async::Trigger<EventData> trigger;

        // 注册回调
        [[maybe_unused]] auto callbackId = trigger.registerCallback(
            "delayed_event", [](const EventData& data) {
                std::cout << "  收到延时事件: ";
                printEventData(data);
                std::cout << "  接收时间: "
                          << std::chrono::system_clock::now()
                                 .time_since_epoch()
                                 .count()
                          << std::endl;
            });

        EventData delayData{"延时500毫秒", 500};

        // 记录当前时间
        auto now = std::chrono::system_clock::now();
        std::cout << "  触发时间: " << now.time_since_epoch().count()
                  << std::endl;

        // 计划500毫秒后触发
        auto cancelFlag =
            trigger.scheduleTrigger("delayed_event", delayData, 500ms);
        LOG("安排了延时触发，等待...");

        // 等待触发完成
        std::this_thread::sleep_for(600ms);

        // 再次计划，但这次立即取消
        LOG("安排另一个延时触发，但立即取消");
        auto cancelFlag2 = trigger.scheduleTrigger(
            "delayed_event", EventData{"这不应被触发", 999}, 300ms);
        *cancelFlag2 = true;  // 直接设置取消标志

        // 确保没有被触发
        std::this_thread::sleep_for(400ms);
        LOG("取消的触发器不应该执行");
    }

    //==============================================================
    // 4. 异步触发
    //==============================================================
    SECTION("4. 异步触发");
    {
        atom::async::Trigger<EventData> trigger;

        // 注册几个回调
        for (int i = 1; i <= 3; ++i) {
            [[maybe_unused]] auto callbackId = trigger.registerCallback(
                "async_event", [i](const EventData& data) {
                    std::cout << "  异步回调 #" << i << ": ";
                    printEventData(data);
                });
        }

        // 异步触发
        LOG("开始异步触发");
        auto future = trigger.scheduleAsyncTrigger("async_event",
                                                   EventData{"异步执行", 42});

        // 获取结果
        auto count = future.get();  // 等待异步操作完成
        LOG("异步触发完成，执行的回调数量: " + std::to_string(count));
    }

    //==============================================================
    // 5. 取消触发
    //==============================================================
    SECTION("5. 取消触发");
    {
        atom::async::Trigger<EventData> trigger;

        // 注册回调
        [[maybe_unused]] auto callbackId =
            trigger.registerCallback("cancel_event", [](const EventData& data) {
                std::cout << "  取消事件回调: ";
                printEventData(data);
            });

        // 安排多个延时触发
        LOG("安排多个延时触发");
        auto flag1 = trigger.scheduleTrigger("cancel_event",
                                             EventData{"延时1", 1}, 500ms);
        auto flag2 = trigger.scheduleTrigger("cancel_event",
                                             EventData{"延时2", 2}, 700ms);
        auto flag3 = trigger.scheduleTrigger("cancel_event",
                                             EventData{"延时3", 3}, 900ms);

        // 取消特定事件的所有触发
        size_t canceled = trigger.cancelTrigger("cancel_event");
        LOG("取消的触发器数量: " + std::to_string(canceled));

        // 等待足够长的时间，确保触发器不会执行
        std::this_thread::sleep_for(1000ms);
        LOG("等待后，所有触发器应该已被取消");

        // 安排另一组触发器
        LOG("安排另一组触发器");
        auto eventFlag1 =
            trigger.scheduleTrigger("event1", EventData{"事件1", 1}, 300ms);
        auto eventFlag2 =
            trigger.scheduleTrigger("event2", EventData{"事件2", 2}, 300ms);

        // 取消所有触发器
        canceled = trigger.cancelAllTriggers();
        LOG("取消所有触发器，取消数量: " + std::to_string(canceled));

        // 等待，确保没有触发器执行
        std::this_thread::sleep_for(500ms);
    }

    //==============================================================
    // 6. 多事件触发
    //==============================================================
    SECTION("6. 多事件触发");
    {
        atom::async::Trigger<EventData> trigger;

        // 为多个事件注册回调
        [[maybe_unused]] auto callbackA =
            trigger.registerCallback("event_a", [](const EventData& data) {
                std::cout << "  事件A回调: ";
                printEventData(data);
            });

        [[maybe_unused]] auto callbackB =
            trigger.registerCallback("event_b", [](const EventData& data) {
                std::cout << "  事件B回调: ";
                printEventData(data);
            });

        [[maybe_unused]] auto callbackC =
            trigger.registerCallback("event_c", [](const EventData& data) {
                std::cout << "  事件C回调: ";
                printEventData(data);
            });

        // 触发多个不同的事件
        LOG("触发多个不同的事件");
        trigger.trigger("event_a", EventData{"来自事件A", 10});
        trigger.trigger("event_b", EventData{"来自事件B", 20});
        trigger.trigger("event_c", EventData{"来自事件C", 30});
    }

    //==============================================================
    // 7. 错误处理
    //==============================================================
    SECTION("7. 错误处理");
    {
        atom::async::Trigger<EventData> trigger;

        // 注册一个会抛出异常的回调
        [[maybe_unused]] auto errorCallbackId = trigger.registerCallback(
            "error_event", [](const EventData& /* data */) {
                std::cout << "  尝试抛出异常的回调" << std::endl;
                throw std::runtime_error("这个错误应该被捕获");
            });

        // 注册一个正常的回调，应该在异常之后仍然执行
        [[maybe_unused]] auto normalCallbackId =
            trigger.registerCallback("error_event", [](const EventData& data) {
                std::cout << "  正常回调应该在异常之后执行: ";
                printEventData(data);
            });

        // 触发事件
        LOG("触发可能抛出异常的事件");
        try {
            size_t count =
                trigger.trigger("error_event", EventData{"错误处理", 500});
            LOG("成功执行的回调数量: " + std::to_string(count) +
                " (异常被内部捕获)");
        } catch (const std::exception& e) {
            LOG("捕获了异常: " + std::string(e.what()) + " (不应该发生)");
        }

        // 尝试注册空回调函数
        LOG("尝试注册空回调函数");
        try {
            [[maybe_unused]] auto id =
                trigger.registerCallback("empty_callback", nullptr);
        } catch (const atom::async::TriggerException& e) {
            LOG("捕获了预期的异常: " + std::string(e.what()));
        }

        // 尝试注册到空事件名
        LOG("尝试注册到空事件名");
        try {
            [[maybe_unused]] auto id =
                trigger.registerCallback("", [](const EventData&) {});
        } catch (const atom::async::TriggerException& e) {
            LOG("捕获了预期的异常: " + std::string(e.what()));
        }

        // 尝试使用负延时触发
        LOG("尝试使用负延时触发");
        try {
            [[maybe_unused]] auto flag =
                trigger.scheduleTrigger("negative_delay", EventData{}, -100ms);
        } catch (const atom::async::TriggerException& e) {
            LOG("捕获了预期的异常: " + std::string(e.what()));
        }
    }

    //==============================================================
    // 8. 边界情况
    //==============================================================
    SECTION("8. 边界情况");
    {
        atom::async::Trigger<EventData> trigger;

        // 测试触发不存在的事件
        LOG("触发不存在的事件");
        size_t count = trigger.trigger("nonexistent_event", EventData{});
        LOG("执行的回调数量: " + std::to_string(count) + " (应该为0)");

        // 测试注销不存在的回调
        LOG("注销不存在的回调");
        bool result = trigger.unregisterCallback("nonexistent_event", 999);
        LOG("注销结果: " + std::string(result ? "成功" : "失败") +
            " (应该失败)");

        // 测试使用零延时触发
        LOG("使用零延时触发");
        auto zeroDelayFlag =
            trigger.scheduleTrigger("zero_delay", EventData{"零延时", 0}, 0ms);
        std::this_thread::sleep_for(100ms);
        LOG("零延时触发应该立即执行");

        // 测试空参数
        LOG("空参数测试");
        [[maybe_unused]] auto emptyParamId =
            trigger.registerCallback("empty_param", [](const EventData& data) {
                std::cout << "  收到空参数事件: ";
                printEventData(data);
            });
        trigger.trigger("empty_param", EventData{});
    }

#ifdef ATOM_USE_BOOST_LOCKFREE
    //==============================================================
    // 9. 无锁队列功能 (仅在ATOM_USE_BOOST_LOCKFREE定义时可用)
    //==============================================================
    SECTION("9. 无锁队列功能");
    {
        atom::async::Trigger<EventData> trigger;

        // 为测试事件注册回调
        [[maybe_unused]] auto lockfreeCallbackId = trigger.registerCallback(
            "lockfree_event", [](const EventData& data) {
                std::cout << "  无锁队列事件回调: ";
                printEventData(data);
            });

        // 创建无锁触发队列
        LOG("创建无锁触发队列");
        auto queue = trigger.createLockFreeTriggerQueue(100);

        // 添加事件到队列
        bool pushed =
            queue->push({"lockfree_event", EventData{"无锁队列消息", 42}});
        LOG("添加到队列: " + std::string(pushed ? "成功" : "失败"));

        // 处理队列中的事件
        LOG("处理队列中的事件");
        size_t processed = trigger.processLockFreeTriggers(*queue);
        LOG("处理的事件数量: " + std::to_string(processed));

        // 添加多个事件
        LOG("添加多个事件到队列");
        queue->push({"lockfree_event", EventData{"批处理1", 1}});
        queue->push({"lockfree_event", EventData{"批处理2", 2}});
        queue->push({"lockfree_event", EventData{"批处理3", 3}});

        // 使用maxEvents参数处理部分事件
        LOG("处理部分事件 (maxEvents=2)");
        processed = trigger.processLockFreeTriggers(*queue, 2);
        LOG("处理的事件数量: " + std::to_string(processed) + " (应该为2)");

        // 处理剩余事件
        LOG("处理剩余事件");
        processed = trigger.processLockFreeTriggers(*queue);
        LOG("处理的事件数量: " + std::to_string(processed) + " (应该为1)");
    }
#endif

    //==============================================================
    // 10. 复杂场景：多线程处理
    //==============================================================
    SECTION("10. 复杂场景：多线程处理");
    {
        atom::async::Trigger<EventData> trigger;

        // 注册多个回调
        for (int i = 1; i <= 5; ++i) {
            [[maybe_unused]] auto threadCallbackId = trigger.registerCallback(
                "thread_event", [i](const EventData& data) {
                    std::cout << "  线程 " << std::this_thread::get_id()
                              << " 处理回调 #" << i << ": " << data.message
                              << ", 值: " << data.value << std::endl;
                    // 模拟处理时间
                    std::this_thread::sleep_for(50ms);
                });
        }

        LOG("从多个线程触发事件");

        // 创建多个线程，同时触发事件
        std::vector<std::thread> threads;
        for (int i = 1; i <= 3; ++i) {
            threads.emplace_back([&trigger, i]() {
                EventData data{"线程" + std::to_string(i), i * 100};
                size_t count = trigger.trigger("thread_event", data);
                std::cout << "  线程 " << std::this_thread::get_id()
                          << " 触发了 " << count << " 个回调" << std::endl;
            });
        }

        // 等待所有线程完成
        for (auto& t : threads) {
            t.join();
        }

        LOG("多线程触发完成");
    }

    //==============================================================
    // 11. 使用自定义类型的Trigger
    //==============================================================
    SECTION("11. 使用自定义类型的Trigger");
    {
        // 使用简单类型的触发器
        atom::async::Trigger<int> intTrigger;
        [[maybe_unused]] auto intCallbackId =
            intTrigger.registerCallback("int_event", [](int value) {
                std::cout << "  Int触发器收到值: " << value << std::endl;
            });

        intTrigger.trigger("int_event", 42);

        // 使用字符串的触发器
        atom::async::Trigger<std::string> stringTrigger;
        [[maybe_unused]] auto stringCallbackId = stringTrigger.registerCallback(
            "string_event", [](const std::string& msg) {
                std::cout << "  String触发器收到: " << msg << std::endl;
            });

        stringTrigger.trigger("string_event", "Hello, World!");
    }

    std::cout << "\n===== 示例完成 =====\n";
    return 0;
}