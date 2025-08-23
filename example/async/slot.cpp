#include "../atom/async/slot.hpp"
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

// 辅助函数：打印分隔线
void printSeparator(const std::string& title) {
    std::cout << "\n===== " << title << " =====\n" << std::endl;
}

// 辅助类：用于演示信号槽的对象
class TestReceiver {
public:
    explicit TestReceiver(std::string name) : name_(std::move(name)) {}

    void handleSignal(int value) {
        std::cout << name_ << " received value: " << value << std::endl;
    }

    void handleMultipleArgs(int a, std::string b) {
        std::cout << name_ << " received: " << a << ", \"" << b << "\""
                  << std::endl;
    }

    void throwingHandler(int value) {
        std::cout << name_ << " will throw exception for value: " << value
                  << std::endl;
        throw std::runtime_error("Deliberate exception from handler");
    }

private:
    std::string name_;
};

// 1. 基本信号槽使用示例
void basicSignalExample() {
    printSeparator("基本Signal示例");

    // 创建信号
    atom::async::Signal<int> signal;

    // 创建接收者对象
    TestReceiver receiver1{"Receiver1"};
    TestReceiver receiver2{"Receiver2"};

    // 连接槽函数
    signal.connect([](int value) {
        std::cout << "Lambda received: " << value << std::endl;
    });
    signal.connect(std::bind(&TestReceiver::handleSignal, &receiver1,
                             std::placeholders::_1));
    signal.connect(std::bind(&TestReceiver::handleSignal, &receiver2,
                             std::placeholders::_1));

    // 发射信号
    std::cout << "发射信号值 42:" << std::endl;
    signal.emit(42);

    // 清除所有连接
    signal.clear();
    std::cout << "\n清除后的连接数: " << signal.size() << std::endl;

    // 发射信号，但不会有任何响应
    std::cout << "清除后发射信号值 100:" << std::endl;
    signal.emit(100);
}

// 2. 异步信号槽示例
void asyncSignalExample() {
    printSeparator("AsyncSignal异步示例");

    atom::async::AsyncSignal<int> asyncSignal;

    // 连接将在不同线程中运行的槽
    asyncSignal.connect([](int value) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "异步槽 1 收到值: " << value
                  << " (线程ID: " << std::this_thread::get_id() << ")"
                  << std::endl;
    });

    asyncSignal.connect([](int value) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::cout << "异步槽 2 收到值: " << value
                  << " (线程ID: " << std::this_thread::get_id() << ")"
                  << std::endl;
    });

    std::cout << "主线程ID: " << std::this_thread::get_id() << std::endl;
    std::cout << "发射异步信号..." << std::endl;

    // 发射信号 - 将等待所有异步槽完成
    asyncSignal.emit(123);

    std::cout << "所有异步槽已完成执行" << std::endl;
}

// 3. 自动断开连接信号示例
void autoDisconnectSignalExample() {
    printSeparator("AutoDisconnectSignal示例");

    atom::async::AutoDisconnectSignal<int> adSignal;
    TestReceiver receiver{"AutoDisconnect"};

    // 连接槽并保存连接ID
    auto id1 = adSignal.connect([](int value) {
        std::cout << "AutoDisconnect槽1收到: " << value << std::endl;
    });

    auto id2 = adSignal.connect(std::bind(&TestReceiver::handleSignal,
                                          &receiver, std::placeholders::_1));

    std::cout << "连接的槽数量: " << adSignal.size() << std::endl;

    // 发射信号到所有槽
    adSignal.emit(42);

    // 断开第一个槽的连接
    bool disconnected = adSignal.disconnect(id1);
    std::cout << "断开槽 #" << id1 << ": " << (disconnected ? "成功" : "失败")
              << std::endl;
    std::cout << "剩余槽数量: " << adSignal.size() << std::endl;

    // 再次发射信号 - 只有第二个槽会响应
    adSignal.emit(84);

    // 尝试断开不存在的槽
    bool nonExistentDisconnect = adSignal.disconnect(999);
    std::cout << "断开不存在的槽: " << (nonExistentDisconnect ? "成功" : "失败")
              << std::endl;
}

// 4. 链式信号示例
void chainedSignalExample() {
    printSeparator("ChainedSignal示例");

    atom::async::ChainedSignal<int> rootSignal;
    atom::async::ChainedSignal<int> childSignal1;
    atom::async::ChainedSignal<int> childSignal2;

    // 为每个信号连接槽
    rootSignal.connect(
        [](int value) { std::cout << "根信号槽收到: " << value << std::endl; });

    childSignal1.connect([](int value) {
        std::cout << "子信号1槽收到: " << value << std::endl;
    });

    childSignal2.connect([](int value) {
        std::cout << "子信号2槽收到: " << value << std::endl;
    });

    // 创建信号链
    rootSignal.addChain(childSignal1);
    childSignal1.addChain(childSignal2);

    // 使用智能指针创建另一个信号链
    auto childSignal3 = std::make_shared<atom::async::ChainedSignal<int>>();
    childSignal3->connect([](int value) {
        std::cout << "子信号3槽收到: " << value << std::endl;
    });

    rootSignal.addChain(childSignal3);

    // 从根信号发射信号会触发所有链
    std::cout << "从根信号发射值 42:" << std::endl;
    rootSignal.emit(42);

    // 从中间信号发射只会触发该信号及其后续信号
    std::cout << "\n从子信号1发射值 99:" << std::endl;
    childSignal1.emit(99);
}

// 5. 线程安全信号示例
void threadSafeSignalExample() {
    printSeparator("ThreadSafeSignal示例");

    atom::async::ThreadSafeSignal<int> tsSignal;

    // 添加多个槽
    for (int i = 0; i < 10; i++) {
        tsSignal.connect([i](int value) {
            // 故意增加一些延迟以便观察并行执行
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::cout << "ThreadSafe槽 #" << i << " 收到: " << value
                      << " (线程ID: " << std::this_thread::get_id() << ")\n";
        });
    }

    std::cout << "主线程ID: " << std::this_thread::get_id() << std::endl;
    std::cout << "发射线程安全信号 (可能并行执行)..." << std::endl;

    // 发射信号 - 槽可能并行执行
    tsSignal.emit(42);

    std::cout << "发射完成" << std::endl;
}

// 6. 有限次数信号示例
void limitedSignalExample() {
    printSeparator("LimitedSignal示例");

    // 创建一个限制为3次调用的信号
    atom::async::LimitedSignal<int> limitedSignal(3);

    // 连接槽
    limitedSignal.connect([](int value) {
        std::cout << "LimitedSignal槽收到: " << value << std::endl;
    });

    // 发射信号几次
    for (int i = 1; i <= 5; ++i) {
        bool emitted = limitedSignal.emit(i * 10);
        std::cout << "发射 #" << i
                  << " 结果: " << (emitted ? "成功" : "已达到限制")
                  << ", 剩余调用次数: " << limitedSignal.remainingCalls()
                  << std::endl;
    }

    // 重置计数器
    std::cout << "\n重置LimitedSignal..." << std::endl;
    limitedSignal.reset();

    // 再次尝试发射
    bool emitted = limitedSignal.emit(100);
    std::cout << "重置后发射结果: " << (emitted ? "成功" : "失败")
              << ", 剩余调用次数: " << limitedSignal.remainingCalls()
              << std::endl;

    // 尝试创建无效限制的信号
    try {
        atom::async::LimitedSignal<int> invalidSignal(0);  // 应抛出异常
    } catch (const std::exception& e) {
        std::cout << "创建无效LimitedSignal时捕获异常: " << e.what()
                  << std::endl;
    }
}

// 7. 协程信号示例
void coroutineSignalExample() {
    printSeparator("CoroutineSignal示例");

    atom::async::CoroutineSignal<int> coroSignal;

    // 连接几个槽
    coroSignal.connect([](int value) {
        std::cout << "协程槽 1 收到: " << value << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    });

    coroSignal.connect([](int value) {
        std::cout << "协程槽 2 收到: " << value << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    });

    // 发射协程信号
    std::cout << "发射协程信号..." << std::endl;
    auto task = coroSignal.emit(42);
    std::cout << "协程信号已调度，但可能仍在执行中..." << std::endl;

    // 等待协程完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "所有协程槽应该已完成" << std::endl;
}

// 8. 作用域信号槽示例
void scopedSignalExample() {
    printSeparator("ScopedSignal示例");

    atom::async::ScopedSignal<int> scopedSignal;

    // 创建智能指针管理的槽函数
    auto slot1 = std::make_shared<atom::async::ScopedSignal<int>::SlotType>(
        [](int value) {
            std::cout << "作用域槽 1 收到: " << value << std::endl;
        });

    auto slot2 = std::make_shared<atom::async::ScopedSignal<int>::SlotType>(
        [](int value) {
            std::cout << "作用域槽 2 收到: " << value << std::endl;
        });

    // 连接槽
    scopedSignal.connect(slot1);
    scopedSignal.connect(slot2);
    scopedSignal.connect([](int value) {
        std::cout << "作用域内联槽收到: " << value << std::endl;
    });

    // 发射信号
    std::cout << "发射作用域信号..." << std::endl;
    scopedSignal.emit(42);

    // 当我们重置其中一个智能指针时，相应的槽会失效
    std::cout << "\n重置槽1的智能指针..." << std::endl;
    slot1.reset();

    // 再次发射信号 - 只有槽2和内联槽会响应
    std::cout << "再次发射作用域信号..." << std::endl;
    scopedSignal.emit(84);
}

// 9. 错误处理示例
void errorHandlingExample() {
    printSeparator("错误处理示例");

    // 1. 尝试连接无效槽
    atom::async::Signal<int> signal1;
    try {
        std::function<void(int)> nullSlot;
        signal1.connect(nullSlot);  // 应抛出SlotConnectionError
    } catch (const atom::async::SlotConnectionError& e) {
        std::cout << "捕获SlotConnectionError: " << e.what() << std::endl;
    }

    // 2. 处理槽执行期间的异常
    atom::async::Signal<int> signal2;
    TestReceiver errorReceiver{"ErrorReceiver"};

    signal2.connect(std::bind(&TestReceiver::throwingHandler, &errorReceiver,
                              std::placeholders::_1));

    try {
        signal2.emit(42);  // 应抛出SlotEmissionError
    } catch (const atom::async::SlotEmissionError& e) {
        std::cout << "捕获SlotEmissionError: " << e.what() << std::endl;
    }

    // 3. 异步信号中的错误处理
    atom::async::AsyncSignal<int> asyncSignal;
    asyncSignal.connect(
        [](int value) { throw std::runtime_error("异步槽故意抛出的异常"); });

    try {
        asyncSignal.emit(42);  // 应抛出SlotEmissionError
    } catch (const atom::async::SlotEmissionError& e) {
        std::cout << "捕获异步SlotEmissionError: " << e.what() << std::endl;
    }
}

// 10. 多参数信号示例
void multiParameterSignalExample() {
    printSeparator("多参数信号示例");

    // 创建接收多个参数的信号
    atom::async::Signal<int, std::string> multiSignal;
    TestReceiver receiver{"MultiParam"};

    // 连接槽
    multiSignal.connect([](int a, const std::string& b) {
        std::cout << "Lambda收到: " << a << ", \"" << b << "\"" << std::endl;
    });

    multiSignal.connect(std::bind(&TestReceiver::handleMultipleArgs, &receiver,
                                  std::placeholders::_1,
                                  std::placeholders::_2));

    // 发射信号
    multiSignal.emit(42, "Hello World");

    // 各种参数值组合
    std::cout << "\n各种参数组合:" << std::endl;
    multiSignal.emit(0, "零值测试");
    multiSignal.emit(-1, "负值测试");
    multiSignal.emit(9999, "大整数测试");
    multiSignal.emit(42, "");  // 空字符串
}

int main() {
    std::cout << "==== atom::async 信号槽系统示例 ====\n" << std::endl;

    try {
        basicSignalExample();
        asyncSignalExample();
        autoDisconnectSignalExample();
        chainedSignalExample();
        threadSafeSignalExample();
        limitedSignalExample();
        coroutineSignalExample();
        scopedSignalExample();
        errorHandlingExample();
        multiParameterSignalExample();
    } catch (const std::exception& e) {
        std::cerr << "未捕获的异常: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n所有示例已完成!" << std::endl;
    return 0;
}
