#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// 引入 EventStack 头文件
#include "../atom/async/eventstack.hpp"

// 用于测试的简单事件类
class Event {
private:
    int id;
    std::string name;

public:
    Event() : id(0), name("Unknown") {}
    Event(int id, std::string name) : id(id), name(std::move(name)) {}

    int getId() const { return id; }
    const std::string& getName() const { return name; }

    // 用于 Serializable 概念
    std::string serialize() const { return std::to_string(id) + ":" + name; }

    // Deserialize from a string
    static Event deserialize(const std::string& str) {
        size_t pos = str.find(':');
        if (pos != std::string::npos) {
            int id = std::stoi(str.substr(0, pos));
            std::string name = str.substr(pos + 1);
            return Event(id, name);
        }
        return Event();
    }

    // 用于 Comparable 概念
    bool operator==(const Event& other) const {
        return id == other.id && name == other.name;
    }

    bool operator<(const Event& other) const {
        return id < other.id || (id == other.id && name < other.name);
    }

    // 便于调试输出
    friend std::ostream& operator<<(std::ostream& os, const Event& e) {
        return os << "Event{id=" << e.id << ", name='" << e.name << "'}";
    }
};

// 为Event类提供to_string和from_string函数，满足Serializable概念
namespace std {
string to_string(const Event& e) { return e.serialize(); }
}  // namespace std

namespace atom {
namespace async {
Event from_string(const std::string& str) { return Event::deserialize(str); }
}  // namespace async
}  // namespace atom

// 演示基本操作
void demonstrateBasicOperations() {
    std::cout << "\n===== 基本操作演示 =====\n";

    atom::async::EventStack<Event> stack;

    // 1. 检查初始状态
    std::cout << "初始栈是否为空: " << (stack.isEmpty() ? "是" : "否")
              << std::endl;
    std::cout << "初始栈大小: " << stack.size() << std::endl;

    // 2. 推入事件
    stack.pushEvent(Event(1, "FirstEvent"));
    stack.pushEvent(Event(2, "SecondEvent"));
    stack.pushEvent(Event(3, "ThirdEvent"));

    std::cout << "推入3个事件后栈大小: " << stack.size() << std::endl;
    std::cout << "栈是否为空: " << (stack.isEmpty() ? "是" : "否") << std::endl;

    // 3. 查看顶部事件
    auto topEvent = stack.peekTopEvent();
    if (topEvent) {
        std::cout << "栈顶事件: " << *topEvent << std::endl;
    }

    // 4. 弹出事件
    auto poppedEvent = stack.popEvent();
    if (poppedEvent) {
        std::cout << "弹出的事件: " << *poppedEvent << std::endl;
    }
    std::cout << "弹出一个事件后栈大小: " << stack.size() << std::endl;

    // 5. 清空栈
    stack.clearEvents();
    std::cout << "清空后栈大小: " << stack.size() << std::endl;
    std::cout << "清空后栈是否为空: " << (stack.isEmpty() ? "是" : "否")
              << std::endl;
}

// 演示边界情况
void demonstrateBoundaryConditions() {
    std::cout << "\n===== 边界情况演示 =====\n";

    atom::async::EventStack<Event> stack;

    // 1. 从空栈弹出
    std::cout << "尝试从空栈弹出:" << std::endl;
    auto emptyPop = stack.popEvent();
    std::cout << (emptyPop ? "成功弹出" : "弹出失败，返回 std::nullopt")
              << std::endl;

    // 2. 查看空栈顶部
    std::cout << "尝试查看空栈顶部:" << std::endl;
    auto emptyPeek = stack.peekTopEvent();
    std::cout << (emptyPeek ? "找到栈顶元素" : "无栈顶元素，返回 std::nullopt")
              << std::endl;

    // 3. 清空空栈
    std::cout << "清空空栈:" << std::endl;
    stack.clearEvents();
    std::cout << "操作后栈大小: " << stack.size() << std::endl;

    // 4. 添加大量事件
    std::cout << "添加1000个事件:" << std::endl;
    for (int i = 0; i < 1000; i++) {
        stack.pushEvent(Event(i, "Event" + std::to_string(i)));
    }
    std::cout << "添加后栈大小: " << stack.size() << std::endl;

    // 5. 复制大栈
    std::cout << "复制栈:" << std::endl;
    auto stackCopy = stack.copyStack();
    std::cout << "复制的栈大小: " << stackCopy.size() << std::endl;
}

// 演示并发安全性
void demonstrateConcurrency() {
    std::cout << "\n===== 并发操作演示 =====\n";

    atom::async::EventStack<Event> stack;

    // 1. 并发推入和弹出
    std::cout << "开始并发测试: 5个线程推入100个事件，5个线程弹出事件"
              << std::endl;

    std::vector<std::thread> pushThreads;
    std::vector<std::thread> popThreads;

    // 创建推入线程
    for (int t = 0; t < 5; ++t) {
        pushThreads.emplace_back([t, &stack]() {
            for (int i = 0; i < 100; ++i) {
                stack.pushEvent(
                    Event(t * 1000 + i, "Thread" + std::to_string(t) + "Event" +
                                            std::to_string(i)));
                std::this_thread::sleep_for(
                    std::chrono::microseconds(1));  // 添加微小延迟
            }
        });
    }

    // 创建弹出线程
    for (int t = 0; t < 5; ++t) {
        popThreads.emplace_back([&stack]() {
            int poppedCount = 0;
            while (poppedCount < 80) {  // 每个线程尝试弹出80个事件
                auto evt = stack.popEvent();
                if (evt) {
                    poppedCount++;
                }
                std::this_thread::sleep_for(
                    std::chrono::microseconds(2));  // 添加微小延迟
            }
        });
    }

    // 等待所有线程完成
    for (auto& t : pushThreads) {
        t.join();
    }
    for (auto& t : popThreads) {
        t.join();
    }

    std::cout << "并发操作后栈大小: " << stack.size() << std::endl;
    std::cout << "理论上的大小应该接近: " << 5 * 100 - 5 * 80
              << " (推入 - 弹出)" << std::endl;
}

// 演示高级操作
void demonstrateAdvancedOperations() {
    std::cout << "\n===== 高级操作演示 =====\n";

    atom::async::EventStack<Event> stack;

    // 准备数据
    for (int i = 1; i <= 10; ++i) {
        stack.pushEvent(Event(i, "Event" + std::to_string(i)));
    }
    // 添加一些重复项
    stack.pushEvent(Event(5, "Event5"));
    stack.pushEvent(Event(7, "Event7"));

    std::cout << "初始栈大小: " << stack.size() << std::endl;

    // 1. 过滤事件
    std::cout << "过滤 ID > 5 的事件:" << std::endl;
    stack.filterEvents([](const Event& e) { return e.getId() > 5; });
    std::cout << "过滤后栈大小: " << stack.size() << std::endl;

    // 2. 移除重复项
    std::cout << "移除重复项:" << std::endl;
    stack.removeDuplicates();
    std::cout << "移除重复项后栈大小: " << stack.size() << std::endl;

    // 3. 排序事件
    std::cout << "按照ID降序排序:" << std::endl;
    stack.sortEvents(
        [](const Event& a, const Event& b) { return a.getId() > b.getId(); });

    // 4. 反转事件
    std::cout << "反转栈:" << std::endl;
    stack.reverseEvents();

    // 5. 计数和查找
    int count =
        stack.countEvents([](const Event& e) { return e.getId() % 2 == 0; });
    std::cout << "偶数ID的事件数: " << count << std::endl;

    auto found = stack.findEvent([](const Event& e) { return e.getId() == 6; });
    if (found) {
        std::cout << "找到ID为6的事件: " << *found << std::endl;
    } else {
        std::cout << "未找到ID为6的事件" << std::endl;
    }

    // 6. 任意和所有事件判断
    bool any = stack.anyEvent([](const Event& e) {
        return e.getName().find("Event6") != std::string::npos;
    });
    std::cout << "是否存在包含'Event6'的事件: " << (any ? "是" : "否")
              << std::endl;

    bool all = stack.allEvents([](const Event& e) { return e.getId() > 5; });
    std::cout << "是否所有事件ID都>5: " << (all ? "是" : "否") << std::endl;

    // 7. 转换事件
    std::cout << "将所有事件ID乘以10:" << std::endl;
    stack.transformEvents([](Event& e) {
        return Event(e.getId() * 10, e.getName() + "_transformed");
    });

    // 使用forEach遍历并打印
    std::cout << "转换后的事件:" << std::endl;
    stack.forEach([](const Event& e) { std::cout << " - " << e << std::endl; });
}

// 演示序列化和反序列化
void demonstrateSerializationDeserialization() {
    std::cout << "\n===== 序列化与反序列化演示 =====\n";

    atom::async::EventStack<Event> stack1;

    // 添加事件
    stack1.pushEvent(Event(1, "EventA"));
    stack1.pushEvent(Event(2, "EventB"));
    stack1.pushEvent(Event(3, "EventC"));

    // 序列化
    try {
        std::string serialized =
            stack1.serialize();  // 修改这里：使用serialize()方法
        std::cout << "序列化结果: " << serialized << std::endl;

        // 创建新栈并反序列化
        atom::async::EventStack<Event> stack2;
        stack2.deserialize(serialized);  // 修改这里：使用deserialize()方法

        std::cout << "反序列化后栈大小: " << stack2.size() << std::endl;

        // 验证内容
        auto event = stack2.popEvent();
        if (event) {
            std::cout << "反序列化后第一个事件: " << *event << std::endl;
        }
    } catch (const atom::async::EventStackSerializationException& e) {
        std::cout << "序列化/反序列化异常: " << e.what() << std::endl;
    }
}

// 演示错误处理
void demonstrateErrorHandling() {
    std::cout << "\n===== 错误处理演示 =====\n";

    atom::async::EventStack<Event> stack;

    // 1. 异常捕获
    try {
        // 尝试创建一个无效的事件
        Event invalidEvent(-1, "");
        std::vector<Event> events(1000000000,
                                  invalidEvent);  // 尝试分配巨大内存

        for (const auto& e : events) {
            stack.pushEvent(e);  // 可能因内存不足而抛出异常
        }
    } catch (const atom::async::EventStackException& e) {
        std::cout << "捕获事件栈异常: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "捕获标准异常: " << e.what() << std::endl;
    }

    // 2. 序列化错误
    try {
        std::string invalidData = "this:is:invalid:data;";
        stack.deserialize(invalidData);  // 修改这里：使用deserialize()方法
    } catch (const atom::async::EventStackSerializationException& e) {
        std::cout << "捕获序列化异常: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "EventStack 类使用示例\n";
    std::cout << "====================\n";

    try {
        // 基本操作
        demonstrateBasicOperations();

        // 边界情况
        demonstrateBoundaryConditions();

        // 并发安全
        demonstrateConcurrency();

        // 高级操作
        demonstrateAdvancedOperations();

        // 序列化和反序列化
        demonstrateSerializationDeserialization();

        // 错误处理
        demonstrateErrorHandling();

    } catch (const std::exception& e) {
        std::cerr << "未捕获的异常: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n所有测试完成!\n";
    return 0;
}
