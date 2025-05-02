#include <atomic>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// 假设下面的头文件已经包含了ThreadLocal类定义
#include "atom/async/threadlocal.hpp"

// 用于同步输出的互斥锁
std::mutex g_outputMutex;

// 辅助宏，用于格式化输出
#define SECTION(name) std::cout << "\n=== " << name << " ===\n"
#define LOG(msg)                                                              \
    {                                                                         \
        std::lock_guard<std::mutex> lock(g_outputMutex);                      \
        std::cout << "[Thread " << std::setw(5) << std::this_thread::get_id() \
                  << "] " << msg << std::endl;                                \
    }

// 简单数据结构，用于ThreadLocal示例
struct Counter {
    int value = 0;
    std::string name = "未命名";

    Counter() = default;
    Counter(int val, std::string n) : value(val), name(std::move(n)) {}

    void increment() { value++; }
    void reset() { value = 0; }

    std::string toString() const {
        return "Counter{name='" + name + "', value=" + std::to_string(value) +
               "}";
    }
};

// 实现简单的不可复制类
class NonCopyable {
protected:
    NonCopyable() = default;
    ~NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

// 用于测试的复杂对象类型
class Resource : public NonCopyable {
public:
    Resource() : id_(next_id_++) {
        LOG("创建Resource #" + std::to_string(id_));
    }

    explicit Resource(int value) : id_(next_id_++), value_(value) {
        LOG("创建Resource #" + std::to_string(id_) +
            " 值=" + std::to_string(value_));
    }

    ~Resource() { LOG("销毁Resource #" + std::to_string(id_)); }

    // 移动构造函数
    Resource(Resource&& other) noexcept : id_(other.id_), value_(other.value_) {
        other.id_ = -1;
        LOG("移动Resource #" + std::to_string(id_));
    }

    // 移动赋值运算符
    Resource& operator=(Resource&& other) noexcept {
        if (this != &other) {
            id_ = other.id_;
            value_ = other.value_;
            other.id_ = -1;
            LOG("移动赋值Resource #" + std::to_string(id_));
        }
        return *this;
    }

    int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }
    int getId() const { return id_; }

    std::string toString() const {
        return "Resource{id=" + std::to_string(id_) +
               ", value=" + std::to_string(value_) + "}";
    }

private:
    int id_;
    int value_ = 0;
    static std::atomic<int> next_id_;
};

std::atomic<int> Resource::next_id_(1);

int main() {
    std::cout << "===== atom::async::ThreadLocal 使用示例 =====\n\n";

    //==============================================================
    // 1. 基本用法
    //==============================================================
    SECTION("1. 基本用法");
    {
        // 创建一个ThreadLocal实例，存储int类型
        atom::async::ThreadLocal<int> threadLocalInt;

        // 为当前线程设置值
        threadLocalInt.reset(42);
        LOG("当前线程的threadLocalInt值: " + std::to_string(*threadLocalInt));

        // 修改当前线程的值
        *threadLocalInt = 100;
        LOG("修改后的threadLocalInt值: " +
            std::to_string(threadLocalInt.get()));

        // 创建新线程，每个线程有自己的值
        std::vector<std::thread> threads;
        for (int i = 1; i <= 3; ++i) {
            threads.emplace_back([i, &threadLocalInt]() {
                // 设置此线程的值
                threadLocalInt.reset(i * 10);
                LOG("设置threadLocalInt为: " + std::to_string(*threadLocalInt));

                // 模拟一些操作
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                // 再次读取值（应该与之前设置的相同）
                LOG("再次读取threadLocalInt: " +
                    std::to_string(*threadLocalInt));
            });
        }

        // 等待所有线程完成
        for (auto& t : threads) {
            t.join();
        }

        // 主线程的值应该保持不变
        LOG("所有线程完成后，主线程的threadLocalInt值: " +
            std::to_string(*threadLocalInt));
    }

    //==============================================================
    // 2. 使用初始化函数
    //==============================================================
    SECTION("2. 使用初始化函数");
    {
        // 创建一个带初始化函数的ThreadLocal实例
        atom::async::ThreadLocal<Counter> threadLocalCounter([]() {
            // 这个函数为每个线程初始化Counter
            return Counter(0,
                           "线程" + std::to_string(std::hash<std::thread::id>{}(
                                        std::this_thread::get_id())));
        });

        // 检查当前线程的值
        LOG("当前线程的Counter: " + threadLocalCounter->toString());

        // 增加计数
        threadLocalCounter->increment();
        LOG("增加计数后: " + threadLocalCounter->toString());

        // 在多个线程中使用
        std::vector<std::thread> threads;
        for (int i = 0; i < 3; ++i) {
            threads.emplace_back([&threadLocalCounter]() {
                // 访问这个线程的Counter（会自动初始化）
                LOG("初始Counter: " + threadLocalCounter->toString());

                // 增加几次计数
                for (int j = 0; j < 3; ++j) {
                    threadLocalCounter->increment();
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    LOG("增加后: " + threadLocalCounter->toString());
                }
            });
        }

        // 等待所有线程完成
        for (auto& t : threads) {
            t.join();
        }

        // 检查每个线程的最终值（使用forEach方法）
        LOG("使用forEach方法检查所有线程的最终Counter值:");
        std::atomic<int> count = 0;
        threadLocalCounter.forEach([&count](Counter& counter) {
            LOG("发现Counter: " + counter.toString());
            count++;
        });
        LOG("总共找到 " + std::to_string(count) + " 个线程本地值");

        // 清除所有线程的值
        threadLocalCounter.clear();
        LOG("清除后，线程本地存储大小: " +
            std::to_string(threadLocalCounter.size()));
    }

    //==============================================================
    // 3. 异常处理
    //==============================================================
    SECTION("3. 异常处理");
    {
        // 创建一个可能抛出异常的初始化函数的ThreadLocal实例
        atom::async::ThreadLocal<int> throwingThreadLocal([]() -> int {
            if (std::hash<std::thread::id>{}(std::this_thread::get_id()) % 2 ==
                0) {
                // 对于哈希值为偶数的线程ID，抛出异常
                throw std::runtime_error("初始化失败 - 线程ID哈希为偶数");
            }
            return 42;
        });

        // 尝试访问当前线程的值
        try {
            int value = throwingThreadLocal.get();
            LOG("成功获取值: " + std::to_string(value));
        } catch (const std::exception& e) {
            LOG("捕获异常: " + std::string(e.what()));
        }

        // 创建没有初始化函数的ThreadLocal
        atom::async::ThreadLocal<std::string> noInitializer;

        // 尝试访问未初始化的值
        try {
            std::string value = noInitializer.get();
            LOG("不应该到达这里");
        } catch (const std::exception& e) {
            LOG("预期的异常: " + std::string(e.what()));
        }

        // 使用reset可以避免这个问题
        noInitializer.reset("已初始化");
        LOG("reset后的值: " + noInitializer.get());
    }

    //==============================================================
    // 4. 使用各种运算符
    //==============================================================
    SECTION("4. 使用各种运算符");
    {
        // 创建一个Counter的ThreadLocal实例
        atom::async::ThreadLocal<Counter> counterTL(
            []() { return Counter(0, "操作符测试"); });

        // 使用箭头运算符
        counterTL->increment();
        LOG("使用箭头运算符后: value = " + std::to_string(counterTL->value));

        // 使用解引用运算符
        (*counterTL).increment();
        LOG("使用解引用运算符后: value = " +
            std::to_string((*counterTL).value));

        // 检查hasValue和getPointer方法
        LOG("hasValue结果: " +
            std::string(counterTL.hasValue() ? "true" : "false"));

        if (Counter* ptr = counterTL.getPointer()) {
            LOG("getPointer返回的值: " + ptr->toString());
        } else {
            LOG("getPointer返回nullptr");
        }

        // 清除当前线程的值
        counterTL.clearCurrentThread();
        LOG("clearCurrentThread后，hasValue结果: " +
            std::string(counterTL.hasValue() ? "true" : "false"));

        // getPointer应该返回nullptr
        std::string result = counterTL.getPointer() ? "非nullptr" : "nullptr";
        LOG("clearCurrentThread后，getPointer返回: " + result);
    }

    //==============================================================
    // 5. 复杂对象类型
    //==============================================================
    SECTION("5. 复杂对象类型");
    {
        // 为可移动但不可复制的类型创建ThreadLocal
        // 使用默认构造函数的方式
        atom::async::ThreadLocal<Resource> resourceTL(
            []() { return Resource(100); });

        LOG("主线程的Resource: " + resourceTL->toString());

        // 测试线程本地值
        std::vector<std::thread> threads;
        for (int i = 1; i <= 2; ++i) {
            threads.emplace_back([i, &resourceTL]() {
                // 访问这个线程的Resource
                LOG("初始Resource: " + resourceTL->toString());

                // 修改Resource的值
                resourceTL->setValue(i * 200);
                LOG("修改后: " + resourceTL->toString());

                // 使用reset提供新的Resource
                resourceTL.reset(Resource(i * 300));
                LOG("reset后: " + resourceTL->toString());
            });
        }

        // 等待所有线程完成
        for (auto& t : threads) {
            t.join();
        }

        // 检查各线程的Resource数量
        LOG("线程本地存储中的Resource数量: " +
            std::to_string(resourceTL.size()));

        // 在所有Resource实例上执行操作
        resourceTL.forEach(
            [](Resource& r) { LOG("发现Resource: " + r.toString()); });
    }

    //==============================================================
    // 6. 边界情况和特殊场景
    //==============================================================
    SECTION("6. 边界情况和特殊场景");
    {
        // 创建一个ThreadLocal<int>实例
        atom::async::ThreadLocal<int> emptyTL;

        // 检查未初始化的线程本地值
        LOG("hasValue结果: " +
            std::string(emptyTL.hasValue() ? "true" : "false"));

        std::string ptrResult = emptyTL.getPointer() ? "非nullptr" : "nullptr";
        LOG("getPointer返回: " + ptrResult);

        // 测试forEach在空状态下的行为
        int count = 0;
        emptyTL.forEach([&count](int& /* value */) {
            LOG("不应该执行到这里！");
            count++;
        });
        LOG("forEach调用计数: " + std::to_string(count));

        // 测试size方法
        LOG("空状态下size值: " + std::to_string(emptyTL.size()));

        // 测试ThreadLocal的移动操作
        atom::async::ThreadLocal<int> sourceTL;
        sourceTL.reset(999);

        // 移动构造 - 使用std::move而不是直接初始化
        atom::async::ThreadLocal<int> movedTL;
        movedTL = std::move(sourceTL);
        LOG("移动赋值后，实例的值: " +
            std::to_string(movedTL.hasValue() ? *movedTL : -1));

        // 移动另一个实例 - 分开创建和赋值操作
        atom::async::ThreadLocal<int> assignedTL;
        assignedTL = std::move(movedTL);
        LOG("二次移动后，新实例的值: " +
            std::to_string(assignedTL.hasValue() ? *assignedTL : -1));
    }

    //==============================================================
    // 7. 实际应用场景：线程本地数据库连接
    //==============================================================
    SECTION("7. 实际应用场景：线程本地数据库连接");
    {
        // 创建一个原子计数器用于连接ID
        std::atomic<int> conn_id_counter(1);

        // 模拟数据库连接类
        class DBConnection {
        public:
            DBConnection(std::atomic<int>& id_source) : id_(id_source++) {
                LOG("创建数据库连接 #" + std::to_string(id_));
                // 模拟连接建立延迟
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            ~DBConnection() { LOG("关闭数据库连接 #" + std::to_string(id_)); }

            bool executeQuery(const std::string& query) {
                LOG("在连接 #" + std::to_string(id_) + " 上执行查询: " + query);
                // 模拟查询执行
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                return true;
            }

        private:
            int id_;
        };

        // 创建线程本地数据库连接池
        atom::async::ThreadLocal<DBConnection> dbConnectionTL(
            [&conn_id_counter]() {
                // 使用外部的atomic变量
                return DBConnection(conn_id_counter);
            });

        // 模拟并发查询
        std::vector<std::thread> threads;
        for (int i = 0; i < 3; ++i) {
            threads.emplace_back([&dbConnectionTL, i]() {
                // 模拟多个查询
                for (int j = 1; j <= 3; ++j) {
                    // 每个线程使用自己的连接
                    std::string query =
                        "SELECT * FROM table" + std::to_string(j) +
                        " WHERE thread_id = " + std::to_string(i);
                    dbConnectionTL->executeQuery(query);

                    // 模拟一些处理时间
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            });
        }

        // 等待所有线程完成
        for (auto& t : threads) {
            t.join();
        }

        // 检查连接数量
        LOG("线程本地存储中的连接数量: " +
            std::to_string(dbConnectionTL.size()));

        // 清除所有连接（将触发析构函数）
        dbConnectionTL.clear();
        LOG("清除连接后，存储大小: " + std::to_string(dbConnectionTL.size()));
    }

    //==============================================================
    // 8. 性能比较：ThreadLocal vs 普通对象 + 互斥锁
    //==============================================================
    SECTION("8. 性能比较：ThreadLocal vs 普通对象 + 互斥锁");
    {
        constexpr int NUM_THREADS = 4;
        constexpr int OPERATIONS_PER_THREAD = 10000;

        // 使用互斥锁保护的计数器
        struct ProtectedCounter {
            std::mutex mutex;
            int value = 0;

            void increment() {
                std::lock_guard<std::mutex> guard(mutex);
                value++;
            }

            int get() {
                std::lock_guard<std::mutex> guard(mutex);
                return value;
            }
        } sharedCounter;

        // 线程本地计数器
        atom::async::ThreadLocal<Counter> threadLocalCounter(
            []() { return Counter(); });

        // 比较性能
        auto testSharedCounter = [&]() {
            auto start = std::chrono::high_resolution_clock::now();

            std::vector<std::thread> threads;
            for (int i = 0; i < NUM_THREADS; ++i) {
                threads.emplace_back([&sharedCounter]() {
                    for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                        sharedCounter.increment();
                    }
                });
            }

            for (auto& t : threads) {
                t.join();
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                      start)
                    .count();

            LOG("共享计数器最终值: " + std::to_string(sharedCounter.get()));
            LOG("共享计数器耗时: " + std::to_string(duration) + " ms");
        };

        auto testThreadLocalCounter = [&]() {
            auto start = std::chrono::high_resolution_clock::now();

            std::vector<std::thread> threads;
            for (int i = 0; i < NUM_THREADS; ++i) {
                threads.emplace_back([&threadLocalCounter]() {
                    for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                        threadLocalCounter->increment();
                    }
                });
            }

            for (auto& t : threads) {
                t.join();
            }

            int totalCount = 0;
            threadLocalCounter.forEach([&totalCount](Counter& counter) {
                totalCount += counter.value;
            });

            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                      start)
                    .count();

            LOG("线程本地计数器最终合计值: " + std::to_string(totalCount));
            LOG("线程本地计数器耗时: " + std::to_string(duration) + " ms");
        };

        LOG("开始性能比较...");
        testSharedCounter();
        testThreadLocalCounter();
    }

    std::cout << "\n===== 示例完成 =====\n";
    return 0;
}