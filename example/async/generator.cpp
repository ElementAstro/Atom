#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "atom/async/generator.hpp"

template <typename T>
void print_generator(const std::string& description,
                     atom::async::Generator<T>& gen) {
    std::cout << description << ": ";
    for (const auto& value : gen) {
        std::cout << value << " ";
    }
    std::cout << std::endl;
}

// 1. 基本 Generator 使用示例
void basic_generator_examples() {
    std::cout << "\n=== 基本 Generator 示例 ===\n";

    // 简单的整数生成器
    auto int_generator = []() -> atom::async::Generator<int> {
        for (int i = 1; i <= 5; ++i) {
            co_yield i;
        }
    };

    auto gen = int_generator();
    print_generator("整数生成器", gen);

    // 字符串生成器
    auto string_generator = []() -> atom::async::Generator<std::string> {
        co_yield "Hello";
        co_yield "World";
        co_yield "C++20";
        co_yield "Coroutines";
    };

    auto str_gen = string_generator();
    print_generator("字符串生成器", str_gen);

    // 使用 range 辅助函数
    auto range_gen = atom::async::range(1, 6);
    print_generator("range(1, 6)", range_gen);

    // 使用不同步长
    auto step_gen = atom::async::range(0, 10, 2);
    print_generator("range(0, 10, 2)", step_gen);

    // 从现有容器创建生成器
    std::vector<double> values = {1.1, 2.2, 3.3, 4.4, 5.5};
    auto from_range_gen = atom::async::from_range(values);
    print_generator("from_range(vector)", from_range_gen);
}

// 2. infinite_range 示例（带有有界限制）
void infinite_range_examples() {
    std::cout << "\n=== infinite_range 示例 ===\n";

    // 创建无限生成器但限制迭代次数
    auto inf_gen = atom::async::infinite_range(1);

    std::cout << "infinite_range(1) 的前 10 个元素: ";
    int count = 0;
    for (const auto& value : inf_gen) {
        std::cout << value << " ";
        if (++count >= 10)
            break;  // 避免无限循环
    }
    std::cout << std::endl;

    // 带步长的无限生成器
    auto step_inf_gen = atom::async::infinite_range(0, 5);

    std::cout << "infinite_range(0, 5) 的前 8 个元素: ";
    count = 0;
    for (const auto& value : step_inf_gen) {
        std::cout << value << " ";
        if (++count >= 8)
            break;
    }
    std::cout << std::endl;
}

// 3. TwoWayGenerator 示例
void two_way_generator_examples() {
    std::cout << "\n=== TwoWayGenerator 示例 ===\n";

    // 创建一个双向通信生成器（计算发送值的平方）
    auto square_generator = []() -> atom::async::TwoWayGenerator<int, int> {
        int received = 0;
        while (true) {
            received = co_yield received * received;
        }
    };

    auto two_way_gen = square_generator();

    // 发送值到生成器并获取结果
    try {
        std::cout << "双向生成器示例 (计算平方):" << std::endl;
        std::cout << "发送 5，接收: " << two_way_gen.next(5)
                  << std::endl;  // 初始值为0，返回0
        std::cout << "发送 3，接收: " << two_way_gen.next(3)
                  << std::endl;  // 返回5²=25
        std::cout << "发送 7，接收: " << two_way_gen.next(7)
                  << std::endl;  // 返回3²=9
        std::cout << "发送 10，接收: " << two_way_gen.next(10)
                  << std::endl;  // 返回7²=49
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
    }

    // 不接收值的 TwoWayGenerator 示例
    auto counter_generator = []() -> atom::async::TwoWayGenerator<int, void> {
        int count = 0;
        while (true) {
            co_yield count++;
        }
    };

    auto counter_gen = counter_generator();

    std::cout << "\n计数生成器示例:" << std::endl;
    for (int i = 0; i < 5; ++i) {
        std::cout << "next() 返回: " << counter_gen.next() << std::endl;
    }
}

// 4. 错误处理示例
void error_handling_examples() {
    std::cout << "\n=== 错误处理示例 ===\n";

    // 生成器抛出异常示例
    auto throwing_generator = []() -> atom::async::Generator<int> {
        co_yield 1;
        co_yield 2;
        throw std::runtime_error("生成器异常示例");
        co_yield 3;  // 不会执行到这里
    };

    std::cout << "处理生成器异常:" << std::endl;
    try {
        auto gen = throwing_generator();
        for (const auto& value : gen) {
            std::cout << "值: " << value << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "捕获异常: " << e.what() << std::endl;
    }

    // TwoWayGenerator 耗尽后的错误
    auto finite_two_way_gen = []() -> atom::async::TwoWayGenerator<int, int> {
        co_yield 1;
        co_yield 2;
        // 结束协程
    };

    std::cout << "\n双向生成器耗尽示例:" << std::endl;
    try {
        auto gen = finite_two_way_gen();
        std::cout << "第一个值: " << gen.next(0) << std::endl;
        std::cout << "第二个值: " << gen.next(0) << std::endl;
        std::cout << "尝试获取更多值..." << std::endl;
        std::cout << "第三个值: " << gen.next(0) << std::endl;  // 会抛出异常
    } catch (const std::exception& e) {
        std::cout << "捕获异常: " << e.what() << std::endl;
    }
}

// 5. 边界情况示例
void edge_cases_examples() {
    std::cout << "\n=== 边界情况示例 ===\n";

    // 空生成器
    auto empty_generator = []() -> atom::async::Generator<int> {
        // 不生成任何值
    };

    std::cout << "空生成器示例:" << std::endl;
    auto empty_gen = empty_generator();
    bool is_empty = true;
    for (const auto& value : empty_gen) {
        std::cout << value << " ";
        is_empty = false;
    }
    std::cout << (is_empty ? "生成器为空" : "生成器不为空") << std::endl;

    // 只生成一个值的生成器
    auto single_value_generator = []() -> atom::async::Generator<std::string> {
        co_yield "单值";
    };

    std::cout << "\n单值生成器: ";
    auto single_gen = single_value_generator();
    int count = 0;
    for (const auto& value : single_gen) {
        std::cout << value << " ";
        count++;
    }
    std::cout << "(总数: " << count << ")" << std::endl;

    // 使用特殊值的 range
    std::cout << "\n边界范围值:" << std::endl;
    auto zero_range = atom::async::range(0, 0);
    print_generator("range(0, 0)", zero_range);

    auto negative_range = atom::async::range(-5, -1);
    print_generator("range(-5, -1)", negative_range);

    auto reverse_range = atom::async::range(5, 1, -1);
    print_generator("range(5, 1, -1)", reverse_range);
}

// 6. 复杂使用示例 - 斐波那契数列生成器
void fibonacci_generator_example() {
    std::cout << "\n=== 斐波那契数列生成器 ===\n";

    auto fibonacci = []() -> atom::async::Generator<uint64_t> {
        uint64_t a = 0, b = 1;
        while (true) {  // 无限生成斐波那契数列
            co_yield a;
            uint64_t next = a + b;
            a = b;
            b = next;

            // 为了安全，防止溢出
            if (b < a) {
                co_yield b;  // 最后一个可以表示的值
                break;
            }
        }
    };

    std::cout << "斐波那契数列的前 20 个数：" << std::endl;
    auto fib_gen = fibonacci();
    int count = 0;
    for (const auto& value : fib_gen) {
        std::cout << value << " ";
        if (++count >= 20)
            break;
    }
    std::cout << std::endl;
}

#ifdef ATOM_USE_BOOST_LOCKS
// 7. 线程安全生成器示例（仅当 ATOM_USE_BOOST_LOCKS 定义时可用）
void thread_safe_generator_examples() {
    std::cout << "\n=== 线程安全生成器示例 ===\n";

    auto counter = []() -> atom::async::ThreadSafeGenerator<int> {
        for (int i = 0; i < 100; ++i) {
            co_yield i;
        }
    };

    auto gen = counter();

    // 创建多个线程从同一个生成器消费值
    std::vector<std::thread> threads;
    std::mutex cout_mutex;

    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&gen, i, &cout_mutex]() {
            int count = 0;
            for (const auto& value : gen) {
                {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "线程 " << i << " 获得值: " << value
                              << std::endl;
                }
                count++;
                if (count >= 10)
                    break;  // 每个线程只消费10个值
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
}
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
// 8. 无锁并发生成器示例（仅当 ATOM_USE_BOOST_LOCKFREE 定义时可用）
void lock_free_generator_examples() {
    std::cout << "\n=== 无锁并发生成器示例 ===\n";

    // 创建一个生成整数的函数
    auto producer = []() -> atom::async::Generator<int> {
        for (int i = 0; i < 100; ++i) {
            co_yield i;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };

    // 创建并发生成器
    auto concurrent_gen = atom::async::make_concurrent_generator(producer);

    // 创建多个消费者线程
    std::vector<std::thread> consumers;
    std::mutex cout_mutex;

    for (int i = 0; i < 3; ++i) {
        consumers.emplace_back([&concurrent_gen, i, &cout_mutex]() {
            try {
                for (int j = 0; j < 10; ++j) {
                    int value;
                    while (!concurrent_gen.try_next(value) &&
                           !concurrent_gen.done()) {
                        std::this_thread::yield();
                    }

                    if (concurrent_gen.done())
                        break;

                    {
                        std::lock_guard<std::mutex> lock(cout_mutex);
                        std::cout << "消费者 " << i << " 接收到值: " << value
                                  << std::endl;
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "消费者 " << i << " 错误: " << e.what()
                          << std::endl;
            }
        });
    }

    for (auto& c : consumers) {
        c.join();
    }

    // 无锁双向生成器示例
    auto calculator = []() -> atom::async::TwoWayGenerator<std::string, int> {
        int value;
        while (true) {
            value = co_yield "结果: " + std::to_string(value * value);
        }
    };

    atom::async::LockFreeTwoWayGenerator<std::string, int> two_way_gen(
        calculator);

    std::cout << "\n无锁双向生成器示例:" << std::endl;
    for (int i = 1; i <= 5; ++i) {
        std::string result = two_way_gen.send(i);
        std::cout << "发送 " << i << ", 收到: " << result << std::endl;
    }
}
#endif

int main() {
    std::cout << "===== atom::async::generator.hpp 使用示例 =====" << std::endl;

    // 运行所有示例
    basic_generator_examples();
    infinite_range_examples();
    two_way_generator_examples();
    error_handling_examples();
    edge_cases_examples();
    fibonacci_generator_example();

#ifdef ATOM_USE_BOOST_LOCKS
    thread_safe_generator_examples();
#else
    std::cout << "\n注意: 线程安全生成器示例需要定义 ATOM_USE_BOOST_LOCKS "
                 "并链接 Boost.Thread 库"
              << std::endl;
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
    lock_free_generator_examples();
#else
    std::cout << "\n注意: 无锁生成器示例需要定义 ATOM_USE_BOOST_LOCKFREE "
                 "并链接 Boost.Lockfree 库"
              << std::endl;
#endif

    return 0;
}