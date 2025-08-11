#include <atomic>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "atom/async/lock.hpp"

// 共享资源
struct SharedCounter {
    int value = 0;
};

// 帮助函数，打印当前线程ID
void print_thread_info(const std::string& msg) {
    std::cout << "[线程 " << std::this_thread::get_id() << "] " << msg
              << std::endl;
}

// 帮助函数，格式化函数执行时间
std::string format_duration(std::chrono::nanoseconds ns) {
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(ns).count();
    if (us < 1000) {
        return std::to_string(us) + " μs";
    } else {
        auto ms = us / 1000.0;
        return std::to_string(ms) + " ms";
    }
}

// =================== 基本用法示例 ===================

void basic_spinlock_example() {
    std::cout << "\n===== 基本的 Spinlock 示例 =====\n";

    atom::async::Spinlock spinlock;
    SharedCounter counter;
    std::vector<std::thread> threads;

    auto increment_function = [&spinlock, &counter](int iterations) {
        for (int i = 0; i < iterations; ++i) {
            // 锁定临界区
            spinlock.lock();
            // 更新共享资源
            counter.value++;
            // 释放锁
            spinlock.unlock();
        }
    };

    // 创建5个线程，每个线程递增计数器1000次
    const int thread_count = 5;
    const int iterations_per_thread = 1000;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(increment_function, iterations_per_thread);
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end_time - start_time);

    std::cout << "预期计数: " << thread_count * iterations_per_thread
              << std::endl;
    std::cout << "实际计数: " << counter.value << std::endl;
    std::cout << "耗时: " << format_duration(duration) << std::endl;
}

// 使用作用域锁(ScopedLock)的示例
void scoped_lock_example() {
    std::cout << "\n===== ScopedLock 示例 =====\n";

    atom::async::Spinlock spinlock;
    SharedCounter counter;
    std::vector<std::thread> threads;

    auto increment_function = [&spinlock, &counter](int iterations) {
        for (int i = 0; i < iterations; ++i) {
            // 使用 ScopedLock - 自动在作用域结束时释放锁
            atom::async::ScopedLock<atom::async::Spinlock> lock(spinlock);
            counter.value++;
            // 锁在作用域结束时自动释放
        }
    };

    const int thread_count = 5;
    const int iterations_per_thread = 1000;

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(increment_function, iterations_per_thread);
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "使用 ScopedLock 的计数: " << counter.value << std::endl;
}

// =================== 高级用法示例 ===================

// TicketSpinlock 示例
void ticket_spinlock_example() {
    std::cout << "\n===== TicketSpinlock 示例 =====\n";

    atom::async::TicketSpinlock ticketLock;
    SharedCounter counter;
    std::vector<std::thread> threads;
    std::atomic<int> waiting_threads{0};

    auto increment_function = [&ticketLock, &counter, &waiting_threads](
                                  int id, int iterations) {
        for (int i = 0; i < iterations; ++i) {
            waiting_threads++;
            // 获取锁并获得票号
            auto ticket = ticketLock.lock();
            waiting_threads--;

            if (i == 0) {  // 只在第一次迭代时打印
                print_thread_info("获得票号: " + std::to_string(ticket));
            }

            // 临界区
            counter.value++;

            // 释放票号对应的锁
            ticketLock.unlock(ticket);
        }
    };

    const int thread_count = 5;
    const int iterations_per_thread = 1000;

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(increment_function, i, iterations_per_thread);
    }

    // 监控等待线程数
    std::thread monitor([&ticketLock, &waiting_threads]() {
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::cout << "等待线程数: " << waiting_threads
                      << ", TicketLock内部等待线程计数: "
                      << ticketLock.waitingThreads() << std::endl;
        }
    });

    for (auto& t : threads) {
        t.join();
    }
    monitor.join();

    std::cout << "使用 TicketSpinlock 的计数: " << counter.value << std::endl;
}

// 使用TicketSpinlock的作用域锁
void scoped_ticket_lock_example() {
    std::cout << "\n===== ScopedTicketLock 示例 =====\n";

    atom::async::TicketSpinlock ticketLock;
    SharedCounter counter;
    std::vector<std::thread> threads;

    auto increment_function = [&ticketLock, &counter](int iterations) {
        for (int i = 0; i < iterations; ++i) {
            // 使用作用域锁，自动处理锁的获取和释放
            atom::async::ScopedTicketLock lock(ticketLock);
            counter.value++;
            // 锁在作用域结束时自动释放
        }
    };

    const int thread_count = 5;
    const int iterations_per_thread = 1000;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(increment_function, iterations_per_thread);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end_time - start_time);

    std::cout << "使用 ScopedTicketLock 的计数: " << counter.value << std::endl;
    std::cout << "耗时: " << format_duration(duration) << std::endl;
}

// =================== 尝试获取锁和超时示例 ===================

void trylock_example() {
    std::cout << "\n===== tryLock 示例 =====\n";

    atom::async::Spinlock spinlock;
    SharedCounter counter;
    std::atomic<int> failed_attempts{0};

    auto work_function = [&spinlock, &counter, &failed_attempts](int id,
                                                                 int attempts) {
        for (int i = 0; i < attempts; ++i) {
            // 尝试获取锁，但不阻塞
            if (spinlock.tryLock()) {
                // 成功获取锁
                counter.value++;
                std::this_thread::sleep_for(
                    std::chrono::microseconds(id * 100));  // 引入一些竞争
                spinlock.unlock();
            } else {
                // 未能获取锁
                failed_attempts++;
            }

            // 小暂停减少竞争
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    };

    std::vector<std::thread> threads;
    const int thread_count = 5;
    const int attempts_per_thread = 100;

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(work_function, i, attempts_per_thread);
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "总尝试次数: " << thread_count * attempts_per_thread
              << std::endl;
    std::cout << "成功获取锁次数: " << counter.value << std::endl;
    std::cout << "失败尝试次数: " << failed_attempts.load() << std::endl;
}

// 带超时的尝试获取锁示例
void trylock_timeout_example() {
    std::cout << "\n===== 带超时的 tryLock 示例 =====\n";

    atom::async::Spinlock spinlock;
    SharedCounter counter;
    std::atomic<int> timeout_count{0};

    // 先让一个线程长时间持有锁
    std::thread holding_thread([&spinlock]() {
        print_thread_info("获取锁并持有500ms");
        spinlock.lock();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        spinlock.unlock();
        print_thread_info("释放锁");
    });

    // 稍等片刻确保第一个线程已获得锁
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 多个线程尝试使用超时获取锁
    std::vector<std::thread> threads;
    for (int i = 1; i <= 3; ++i) {
        threads.emplace_back([&spinlock, &counter, &timeout_count, i]() {
            print_thread_info("尝试获取锁，超时 " + std::to_string(i * 100) +
                              "ms");

            auto timeout = std::chrono::milliseconds(i * 100);
            bool acquired = spinlock.tryLock(timeout);

            if (acquired) {
                print_thread_info("成功获取锁");
                counter.value++;
                spinlock.unlock();
            } else {
                print_thread_info("获取锁超时");
                timeout_count++;
            }
        });

        // 稍微错开线程启动时间
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    holding_thread.join();
    for (auto& t : threads) {
        t.join();
    }

    std::cout << "成功获取锁次数: " << counter.value << std::endl;
    std::cout << "超时次数: " << timeout_count.load() << std::endl;
}

// =================== 不同锁类型比较 ===================

void compare_lock_types() {
    std::cout << "\n===== 不同锁类型性能比较 =====\n";

    const int iterations = 100000;
    const int threads_count = 4;

    // 测试特定锁类型的性能
    auto test_lock = [iterations, threads_count](auto& lock,
                                                 const std::string& name) {
        SharedCounter counter;
        std::vector<std::thread> threads;

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < threads_count; ++i) {
            threads.emplace_back([&lock, &counter, iterations]() {
                for (int j = 0; j < iterations; ++j) {
                    auto ticket = lock.lock();
                    counter.value++;
                    lock.unlock(ticket);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << std::setw(20) << name << ": " << duration.count()
                  << " ms, 计数值: " << counter.value << std::endl;
    };

    // 测试不同类型的锁
    {
        atom::async::Spinlock lock;
        test_lock(lock, "Spinlock");
    }

    {
        atom::async::TicketSpinlock lock;
        test_lock(lock, "TicketSpinlock");
    }

    {
        atom::async::UnfairSpinlock lock;
        test_lock(lock, "UnfairSpinlock");
    }

    {
        atom::async::AdaptiveSpinlock lock;
        test_lock(lock, "AdaptiveSpinlock");
    }

    {
        std::mutex lock;
        test_lock(lock, "std::mutex");
    }

#ifdef ATOM_PLATFORM_WINDOWS
    {
        atom::async::WindowsSpinlock lock;
        test_lock(lock, "WindowsSpinlock");
    }
#endif

#ifdef ATOM_HAS_ATOMIC_WAIT
    {
        atom::async::AtomicWaitLock lock;
        test_lock(lock, "AtomicWaitLock");
    }
#endif
}

// =================== 错误处理示例 ===================

void error_handling_example() {
    std::cout << "\n===== 错误处理示例 =====\n";

#ifdef ATOM_DEBUG
    // 在DEBUG模式下，Spinlock会检测重入锁定（同一线程多次锁定）
    atom::async::Spinlock spinlock;

    try {
        std::cout << "尝试获取锁..." << std::endl;
        spinlock.lock();
        std::cout << "成功获取锁" << std::endl;

        std::cout << "尝试再次获取相同的锁（应当抛出异常）..." << std::endl;
        spinlock.lock();  // 在DEBUG模式下，这应该抛出异常
        std::cout << "错误：未检测到重入锁定" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "捕获到异常：" << e.what() << std::endl;
    }

    // 确保释放锁
    try {
        spinlock.unlock();
    } catch (...) {
        std::cout << "解锁时发生异常" << std::endl;
    }
#else
    std::cout << "非DEBUG模式下，未启用死锁检测" << std::endl;
#endif

    // TicketSpinlock错误示例：释放错误的票号
    atom::async::TicketSpinlock ticketLock;

    try {
        auto ticket = ticketLock.lock();
        std::cout << "已获取票号: " << ticket << std::endl;

        // 尝试释放错误的票号
        uint64_t wrong_ticket = ticket + 1;
        std::cout << "尝试释放错误的票号: " << wrong_ticket
                  << " (应当抛出异常)..." << std::endl;
        ticketLock.unlock(wrong_ticket);
        std::cout << "错误：未检测到无效票号" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "捕获到异常：" << e.what() << std::endl;
    }

    // 确保释放正确的票号
    try {
        ticketLock.unlock(0);  // 释放正确的票号(0)
        std::cout << "成功释放正确的票号" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "释放票号时发生异常：" << e.what() << std::endl;
    }
}

// =================== LockFactory 使用示例 ===================

void lock_factory_example() {
    std::cout << "\n===== LockFactory 使用示例 =====\n";

    try {
        // 创建自动优化的锁
        auto optimized_lock = atom::async::LockFactory::createOptimizedLock();
        std::cout << "成功创建自动优化锁" << std::endl;

        // 创建特定类型的锁
        auto spinlock = atom::async::LockFactory::createLock(
            atom::async::LockFactory::LockType::SPINLOCK);
        std::cout << "成功创建Spinlock" << std::endl;

        auto ticket_lock = atom::async::LockFactory::createLock(
            atom::async::LockFactory::LockType::TICKET_SPINLOCK);
        std::cout << "成功创建TicketSpinlock" << std::endl;

        // 尝试创建无效的锁类型
        try {
            auto invalid_lock = atom::async::LockFactory::createLock(
                static_cast<atom::async::LockFactory::LockType>(999));
            std::cout << "错误：成功创建了无效的锁类型" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "预期的异常：" << e.what() << std::endl;
        }

    } catch (const std::exception& e) {
        std::cout << "创建锁时发生异常：" << e.what() << std::endl;
    }
}

// =================== 计数信号量使用示例 ===================

void counting_semaphore_example() {
    std::cout << "\n===== CountingSemaphore 示例 =====\n";

    // 创建一个初始值为2的信号量（最多允许2个线程同时访问）
    atom::async::CountingSemaphore<10> semaphore(2);
    SharedCounter counter;
    std::mutex cout_mutex;  // 用于保护控制台输出

    auto worker = [&](int id, int iterations) {
        for (int i = 0; i < iterations; ++i) {
            // 获取信号量
            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "线程 " << id << " 尝试获取信号量..." << std::endl;
            }

            semaphore.acquire();

            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "线程 " << id << " 获取了信号量，开始工作"
                          << std::endl;
            }

            // 模拟工作
            counter.value++;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // 释放信号量
            semaphore.release();

            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "线程 " << id << " 释放了信号量" << std::endl;
            }

            // 在迭代之间暂停一下
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    };

    std::vector<std::thread> threads;
    const int thread_count = 5;  // 5个线程竞争2个信号量
    const int iterations_per_thread = 3;

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(worker, i, iterations_per_thread);
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "计数器最终值: " << counter.value << std::endl;
}

// =================== 二元信号量使用示例 ===================

void binary_semaphore_example() {
    std::cout << "\n===== BinarySemaphore 示例 =====\n";

    // 创建一个初始值为0的二元信号量（初始状态为不可用）
    atom::async::BinarySemaphore semaphore(0);

    std::mutex cout_mutex;
    std::string shared_message;

    // 消费者线程：等待信号量，然后处理消息
    std::thread consumer([&]() {
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "消费者: 等待消息..." << std::endl;
        }

        // 等待信号量变为可用
        semaphore.acquire();

        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "消费者: 收到消息: " << shared_message << std::endl;
        }
    });

    // 等待一小段时间，确保消费者线程已经开始等待
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 生产者：设置消息并释放信号量
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "生产者: 准备消息并通知消费者" << std::endl;
    }

    shared_message = "Hello from producer!";
    semaphore.release();

    consumer.join();
}

// =================== 主函数 ===================

int main() {
    std::cout << "========= atom::async 锁机制示例 =========\n";

    // 基本示例
    basic_spinlock_example();
    scoped_lock_example();

    // 高级用法
    ticket_spinlock_example();
    scoped_ticket_lock_example();

    // 尝试获取锁示例
    trylock_example();
    trylock_timeout_example();

    // 不同锁类型比较
    compare_lock_types();

    // 错误处理
    error_handling_example();

    // 工厂模式
    lock_factory_example();

    // 信号量示例
    counting_semaphore_example();
    binary_semaphore_example();

    std::cout << "\n========= 示例完成 =========\n";
    return 0;
}
