#include "../atom/async/safetype.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <thread>
#include <vector>

// 辅助函数：打印分隔线
void printSeparator(const std::string& title) {
    std::cout << "\n===== " << title << " =====\n" << std::endl;
}

// 辅助函数：生成随机整数
int getRandomInt(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(min, max);
    return dist(gen);
}

// 辅助函数：创建多个线程执行指定函数
template <typename Func>
void runWithThreads(int threadCount, Func func) {
    std::vector<std::thread> threads;
    threads.reserve(threadCount);

    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back(func, i);
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

// 辅助函数：测量执行时间
template <typename Func>
auto measureTime(Func func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start)
               .count() /
           1000.0;
}

// 1. LockFreeStack 示例
void lockFreeStackExample() {
    printSeparator("LockFreeStack 基本示例");

    // 创建一个存储整数的无锁栈
    atom::async::LockFreeStack<int> intStack;

    // 基本操作
    std::cout << "基本push/pop操作:" << std::endl;
    intStack.push(10);
    intStack.push(20);
    intStack.push(30);

    std::cout << "栈大小: " << intStack.size() << std::endl;
    std::cout << "栈顶元素: " << intStack.top().value_or(-1) << std::endl;

    while (auto value = intStack.pop()) {
        std::cout << "弹出: " << *value << std::endl;
    }

    std::cout << "栈是否为空: " << (intStack.empty() ? "是" : "否")
              << std::endl;

    // 边界情况
    std::cout << "\n边界情况:" << std::endl;
    auto emptyTop = intStack.top();
    std::cout << "空栈的top()返回: " << (emptyTop.has_value() ? "有值" : "无值")
              << std::endl;

    auto emptyPop = intStack.pop();
    std::cout << "空栈的pop()返回: " << (emptyPop.has_value() ? "有值" : "无值")
              << std::endl;

    // 多线程测试
    printSeparator("LockFreeStack 多线程测试");
    atom::async::LockFreeStack<int> sharedStack;
    std::atomic<int> pushCount(0);
    std::atomic<int> popCount(0);

    // 运行10个线程，每个线程推入和弹出1000个元素
    const int threadCount = 10;
    const int operationsPerThread = 1000;

    auto time = measureTime([&]() {
        runWithThreads(threadCount, [&](int threadId) {
            // 每个线程推入一些值
            for (int i = 0; i < operationsPerThread; ++i) {
                sharedStack.push(threadId * 10000 + i);
                pushCount.fetch_add(1, std::memory_order_relaxed);
            }

            // 然后尝试弹出一些值
            for (int i = 0; i < operationsPerThread; ++i) {
                if (auto val = sharedStack.pop()) {
                    popCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    });

    std::cout << "完成 " << threadCount << " 个线程的并发操作，每个线程执行 "
              << operationsPerThread << " 次push和pop" << std::endl;
    std::cout << "总共push次数: " << pushCount << std::endl;
    std::cout << "总共pop成功次数: " << popCount << std::endl;
    std::cout << "最终栈大小: " << sharedStack.size() << std::endl;
    std::cout << "执行时间: " << time << " 毫秒" << std::endl;

    // 移动语义测试
    printSeparator("LockFreeStack 移动语义测试");
    atom::async::LockFreeStack<std::string> stringStack;

    // 使用移动语义推入字符串
    std::string largeString(1000, 'X');
    std::cout << "推入前字符串长度: " << largeString.size() << std::endl;

    stringStack.push(std::move(largeString));
    std::cout << "推入后原字符串长度: " << largeString.size() << std::endl;

    // 弹出并检查
    auto poppedString = stringStack.pop();
    if (poppedString) {
        std::cout << "弹出的字符串长度: " << poppedString->size() << std::endl;
    }
}

// 2. LockFreeHashTable 示例
void lockFreeHashTableExample() {
    printSeparator("LockFreeHashTable 基本示例");

    // 创建一个存储 string->int 映射的无锁哈希表
    atom::async::LockFreeHashTable<std::string, int> userScores;

    // 基本操作
    std::cout << "基本插入和查找:" << std::endl;
    userScores.insert("Alice", 95);
    userScores.insert("Bob", 87);
    userScores.insert("Charlie", 92);

    auto aliceScore = userScores.find("Alice");
    if (aliceScore) {
        std::cout << "Alice的分数: " << aliceScore->get() << std::endl;
    }

    auto unknownScore = userScores.find("Unknown");
    std::cout << "未知用户是否存在: "
              << (unknownScore.has_value() ? "是" : "否") << std::endl;

    // 测试下标操作符 (会创建缺失的条目)
    std::cout << "\n测试下标操作符:" << std::endl;
    std::cout << "使用下标操作符前Dave存在: "
              << (userScores.find("Dave").has_value() ? "是" : "否")
              << std::endl;

    userScores["Dave"] = 75;  // 创建新条目
    std::cout << "使用下标操作符后Dave分数: " << userScores["Dave"]
              << std::endl;

    userScores["Alice"] = 100;  // 更新已有条目
    std::cout << "更新后Alice分数: " << userScores["Alice"] << std::endl;

    // 使用擦除功能
    bool erased = userScores.erase("Bob");
    std::cout << "\n擦除Bob: " << (erased ? "成功" : "失败") << std::endl;
    std::cout << "擦除后Bob存在: "
              << (userScores.find("Bob").has_value() ? "是" : "否")
              << std::endl;

    // 清空表格
    userScores.clear();
    std::cout << "\n清空后大小: " << userScores.size() << std::endl;

    // 使用范围构造器 (C++20)
    std::cout << "\n使用范围构造器:" << std::endl;
    std::map<std::string, int> initialMap = {
        {"Player1", 100}, {"Player2", 200}, {"Player3", 300}};

    atom::async::LockFreeHashTable<std::string, int> gameScores(initialMap);
    std::cout << "从map构造的哈希表大小: " << gameScores.size() << std::endl;

    // 迭代哈希表
    std::cout << "哈希表内容:" << std::endl;
    for (const auto& [key, value] : gameScores) {
        std::cout << key << ": " << value << std::endl;
    }

    // 多线程测试
    printSeparator("LockFreeHashTable 多线程测试");
    atom::async::LockFreeHashTable<int, int> sharedTable(
        128);  // 使用较大的桶数量提高并发性能
    std::atomic<int> insertCount(0);
    std::atomic<int> findCount(0);
    std::atomic<int> findSuccessCount(0);

    auto time = measureTime([&]() {
        runWithThreads(8, [&](int threadId) {
            const int operationsPerThread = 10000;
            const int keyRange = 1000;  // 键的范围，使一些线程操作相同的键

            // 执行插入
            for (int i = 0; i < operationsPerThread; ++i) {
                int key = getRandomInt(0, keyRange);
                sharedTable.insert(key, threadId * operationsPerThread + i);
                insertCount.fetch_add(1, std::memory_order_relaxed);
            }

            // 执行查找
            for (int i = 0; i < operationsPerThread; ++i) {
                int key = getRandomInt(0, keyRange);
                auto result = sharedTable.find(key);
                findCount.fetch_add(1, std::memory_order_relaxed);
                if (result) {
                    findSuccessCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    });

    std::cout << "完成多线程操作:" << std::endl;
    std::cout << "插入次数: " << insertCount << std::endl;
    std::cout << "查找次数: " << findCount << std::endl;
    std::cout << "查找成功次数: " << findSuccessCount << std::endl;
    std::cout << "最终哈希表大小: " << sharedTable.size() << std::endl;
    std::cout << "执行时间: " << time << " 毫秒" << std::endl;
}

// 3. ThreadSafeVector 示例
void threadSafeVectorExample() {
    printSeparator("ThreadSafeVector 基本示例");

    // 创建具有初始容量的线程安全向量
    atom::async::ThreadSafeVector<int> safeVec(10);

    // 基本操作
    std::cout << "基本操作:" << std::endl;
    safeVec.pushBack(10);
    safeVec.pushBack(20);
    safeVec.pushBack(30);

    std::cout << "向量大小: " << safeVec.getSize() << std::endl;
    std::cout << "向量容量: " << safeVec.getCapacity() << std::endl;

    // 访问元素
    std::cout << "\n访问元素:" << std::endl;
    try {
        std::cout << "索引0元素: " << safeVec.at(0) << std::endl;
        std::cout << "索引1元素: " << safeVec[1] << std::endl;  // 使用[]操作符
        std::cout << "索引2元素: " << safeVec.at(2) << std::endl;

        // 超出范围的访问 - 应抛出异常
        std::cout << "索引3元素(超出范围): " << safeVec.at(3) << std::endl;
    } catch (const std::exception& e) {
        std::cout << "捕获异常: " << e.what() << std::endl;
    }

    // 安全访问函数
    std::cout << "\n安全访问函数:" << std::endl;
    auto elem0 = safeVec.try_at(0);
    std::cout << "try_at(0): "
              << (elem0.has_value() ? std::to_string(*elem0) : "无值")
              << std::endl;

    auto elem3 = safeVec.try_at(3);
    std::cout << "try_at(3): "
              << (elem3.has_value() ? std::to_string(*elem3) : "无值")
              << std::endl;

    // 前端和后端访问
    std::cout << "\n前端和后端访问:" << std::endl;
    std::cout << "front(): " << safeVec.front() << std::endl;
    std::cout << "back(): " << safeVec.back() << std::endl;

    // 弹出元素
    auto popped = safeVec.popBack();
    std::cout << "\n弹出后端元素: "
              << (popped.has_value() ? std::to_string(*popped) : "无值")
              << std::endl;
    std::cout << "弹出后向量大小: " << safeVec.getSize() << std::endl;

    // 清空向量
    safeVec.clear();
    std::cout << "\n清空后大小: " << safeVec.getSize() << std::endl;
    std::cout << "清空后容量: " << safeVec.getCapacity() << std::endl;

    // 空向量的边界情况
    std::cout << "\n空向量边界情况:" << std::endl;
    try {
        std::cout << "空向量的front(): " << safeVec.front() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "front()捕获异常: " << e.what() << std::endl;
    }

    auto emptyBack = safeVec.try_back();
    std::cout << "空向量的try_back(): "
              << (emptyBack.has_value() ? std::to_string(*emptyBack) : "无值")
              << std::endl;

    // 使用范围构造器 (C++20)
    std::cout << "\n使用范围构造器:" << std::endl;
    std::vector<int> initialValues = {100, 200, 300, 400, 500};
    atom::async::ThreadSafeVector<int> rangeVec(initialValues);

    std::cout << "从vector构造的向量大小: " << rangeVec.getSize() << std::endl;
    for (size_t i = 0; i < rangeVec.getSize(); ++i) {
        std::cout << "rangeVec[" << i << "] = " << rangeVec[i] << std::endl;
    }

    // 测试收缩容量
    std::cout << "\n测试收缩容量:" << std::endl;
    std::cout << "收缩前容量: " << rangeVec.getCapacity() << std::endl;
    rangeVec.shrinkToFit();
    std::cout << "收缩后容量: " << rangeVec.getCapacity() << std::endl;

    // 多线程测试
    printSeparator("ThreadSafeVector 多线程测试");
    atom::async::ThreadSafeVector<int> sharedVec(1000);  // 预分配一些容量
    std::atomic<int> pushCount(0);
    std::atomic<int> popCount(0);
    std::atomic<int> readCount(0);

    auto time = measureTime([&]() {
        runWithThreads(10, [&](int threadId) {
            // 一半的线程主要推入，一半的线程主要弹出
            if (threadId % 2 == 0) {
                // 推入线程
                for (int i = 0; i < 10000; ++i) {
                    sharedVec.pushBack(threadId * 10000 + i);
                    pushCount.fetch_add(1, std::memory_order_relaxed);

                    // 有时也读取元素
                    if (i % 10 == 0) {
                        auto optElem = sharedVec.try_at(getRandomInt(
                            0, static_cast<int>(sharedVec.getSize()) - 1));
                        if (optElem.has_value()) {
                            readCount.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                }
            } else {
                // 弹出和读取线程
                for (int i = 0; i < 10000; ++i) {
                    // 75%的概率尝试弹出
                    if (i % 4 != 0) {
                        auto popped = sharedVec.popBack();
                        if (popped.has_value()) {
                            popCount.fetch_add(1, std::memory_order_relaxed);
                        }
                    }

                    // 25%的概率尝试读取
                    else {
                        size_t size = sharedVec.getSize();
                        if (size > 0) {
                            auto optElem = sharedVec.try_at(
                                getRandomInt(0, static_cast<int>(size) - 1));
                            if (optElem.has_value()) {
                                readCount.fetch_add(1,
                                                    std::memory_order_relaxed);
                            }
                        }
                    }
                }
            }
        });
    });

    std::cout << "完成多线程操作:" << std::endl;
    std::cout << "推入次数: " << pushCount << std::endl;
    std::cout << "弹出成功次数: " << popCount << std::endl;
    std::cout << "读取成功次数: " << readCount << std::endl;
    std::cout << "最终向量大小: " << sharedVec.getSize() << std::endl;
    std::cout << "最终向量容量: " << sharedVec.getCapacity() << std::endl;
    std::cout << "执行时间: " << time << " 毫秒" << std::endl;
}

// 4. LockFreeList 示例
void lockFreeListExample() {
    printSeparator("LockFreeList 基本示例");

    // 创建无锁链表
    atom::async::LockFreeList<std::string> nameList;

    // 基本操作
    std::cout << "基本操作:" << std::endl;
    nameList.pushFront("Charlie");
    nameList.pushFront("Bob");
    nameList.pushFront("Alice");

    std::cout << "链表大小: " << nameList.size() << std::endl;

    // 访问头部元素
    auto frontName = nameList.front();
    std::cout << "头部元素: " << (frontName.has_value() ? *frontName : "无值")
              << std::endl;

    // 使用迭代器遍历
    std::cout << "\n迭代器遍历:" << std::endl;
    for (const auto& name : nameList) {
        std::cout << "- " << name << std::endl;
    }

    // 弹出元素
    std::cout << "\n弹出元素:" << std::endl;
    while (auto name = nameList.popFront()) {
        std::cout << "弹出: " << *name << std::endl;
    }

    std::cout << "弹出后链表大小: " << nameList.size() << std::endl;
    std::cout << "链表是否为空: " << (nameList.empty() ? "是" : "否")
              << std::endl;

    // 空链表的边界情况
    std::cout << "\n空链表边界情况:" << std::endl;
    auto emptyFront = nameList.front();
    std::cout << "空链表的front()返回: "
              << (emptyFront.has_value() ? "有值" : "无值") << std::endl;

    auto emptyPop = nameList.popFront();
    std::cout << "空链表的popFront()返回: "
              << (emptyPop.has_value() ? "有值" : "无值") << std::endl;

    // 移动语义测试
    printSeparator("LockFreeList 移动语义测试");
    atom::async::LockFreeList<std::vector<int>> vectorList;

    // 创建一个大向量并用移动语义推入
    std::vector<int> largeVector(1000, 42);
    std::cout << "推入前向量大小: " << largeVector.size() << std::endl;

    vectorList.pushFront(std::move(largeVector));
    std::cout << "推入后原向量大小: " << largeVector.size() << std::endl;

    // 弹出并检查
    auto poppedVector = vectorList.popFront();
    if (poppedVector) {
        std::cout << "弹出的向量大小: " << poppedVector->size() << std::endl;
    }

    // 多线程测试
    printSeparator("LockFreeList 多线程测试");
    atom::async::LockFreeList<int> sharedList;
    std::atomic<int> pushCount(0);
    std::atomic<int> popCount(0);

    auto time = measureTime([&]() {
        runWithThreads(8, [&](int threadId) {
            // 生产者线程
            if (threadId < 4) {
                for (int i = 0; i < 5000; ++i) {
                    sharedList.pushFront(threadId * 10000 + i);
                    pushCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
            // 消费者线程
            else {
                for (int i = 0; i < 5000; ++i) {
                    if (auto val = sharedList.popFront()) {
                        popCount.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        });
    });

    std::cout << "完成多线程操作:" << std::endl;
    std::cout << "推入次数: " << pushCount << std::endl;
    std::cout << "弹出成功次数: " << popCount << std::endl;
    std::cout << "最终链表大小: " << sharedList.size() << std::endl;
    std::cout << "执行时间: " << time << " 毫秒" << std::endl;

    // 演示移动操作
    atom::async::LockFreeList<int> list1;
    list1.pushFront(10);
    list1.pushFront(20);

    std::cout << "\n移动前list1大小: " << list1.size() << std::endl;

    // 移动构造
    atom::async::LockFreeList<int> list2(std::move(list1));
    std::cout << "移动后list1大小: " << list1.size() << std::endl;
    std::cout << "移动后list2大小: " << list2.size() << std::endl;

    // 再次填充list1
    list1.pushFront(30);
    list1.pushFront(40);
    std::cout << "\n填充后list1大小: " << list1.size() << std::endl;

    // 移动赋值
    list2 = std::move(list1);
    std::cout << "移动赋值后list1大小: " << list1.size() << std::endl;
    std::cout << "移动赋值后list2大小: " << list2.size() << std::endl;
}

// 综合性能测试：比较不同数据结构在高并发下的性能
void comparativePerformanceTest() {
    printSeparator("并发数据结构性能比较");

    const int OPERATIONS = 100000;  // 总操作数
    const int THREADS = 8;          // 线程数
    const int OPS_PER_THREAD = OPERATIONS / THREADS;

    // 无锁栈性能测试
    std::cout << "1. LockFreeStack 性能测试:" << std::endl;
    atom::async::LockFreeStack<int> stack;
    auto stackTime = measureTime([&]() {
        runWithThreads(THREADS, [&](int threadId) {
            // 一半推入，一半弹出
            if (threadId < THREADS / 2) {
                for (int i = 0; i < OPS_PER_THREAD; ++i) {
                    stack.push(i);
                }
            } else {
                for (int i = 0; i < OPS_PER_THREAD; ++i) {
                    stack.pop();
                }
            }
        });
    });

    // 无锁哈希表性能测试
    std::cout << "2. LockFreeHashTable 性能测试:" << std::endl;
    atom::async::LockFreeHashTable<int, int> hashTable(1024);
    auto hashTableTime = measureTime([&]() {
        runWithThreads(THREADS, [&](int threadId) {
            const int keyRange = OPERATIONS / 10;  // 使用有限范围的键以产生冲突

            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                int key = getRandomInt(0, keyRange);
                if (i % 3 == 0) {  // 33% 查找
                    hashTable.find(key);
                } else if (i % 3 == 1) {  // 33% 插入
                    hashTable.insert(key, threadId * OPS_PER_THREAD + i);
                } else {  // 33% 删除
                    hashTable.erase(key);
                }
            }
        });
    });

    // 线程安全向量性能测试
    std::cout << "3. ThreadSafeVector 性能测试:" << std::endl;
    atom::async::ThreadSafeVector<int> vector(OPERATIONS /
                                              2);  // 预分配一半容量
    auto vectorTime = measureTime([&]() {
        runWithThreads(THREADS, [&](int threadId) {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                if (i % 4 == 0) {  // 25% 读取
                    auto size = vector.getSize();
                    if (size > 0) {
                        vector.try_at(
                            getRandomInt(0, static_cast<int>(size) - 1));
                    }
                } else if (i % 4 == 1) {  // 25% 弹出
                    vector.popBack();
                } else {  // 50% 推入
                    vector.pushBack(threadId * OPS_PER_THREAD + i);
                }
            }
        });
    });

    // 无锁链表性能测试
    std::cout << "4. LockFreeList 性能测试:" << std::endl;
    atom::async::LockFreeList<int> list;
    auto listTime = measureTime([&]() {
        runWithThreads(THREADS, [&](int threadId) {
            // 交替推入和弹出
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                if (i % 2 == 0) {
                    list.pushFront(threadId * OPS_PER_THREAD + i);
                } else {
                    list.popFront();
                }
            }
        });
    });

    // 结果汇总
    std::cout << "\n性能比较结果:" << std::endl;
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "LockFreeStack:    " << stackTime << " 毫秒" << std::endl;
    std::cout << "LockFreeHashTable: " << hashTableTime << " 毫秒" << std::endl;
    std::cout << "ThreadSafeVector: " << vectorTime << " 毫秒" << std::endl;
    std::cout << "LockFreeList:     " << listTime << " 毫秒" << std::endl;
}

int main() {
    std::cout << "==== atom::async 线程安全数据结构示例 ====" << std::endl;

    try {
        // 运行各个数据结构的示例
        lockFreeStackExample();
        lockFreeHashTableExample();
        threadSafeVectorExample();
        lockFreeListExample();

        // 运行性能比较测试
        comparativePerformanceTest();

    } catch (const std::exception& e) {
        std::cerr << "未捕获的异常: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n所有示例已成功完成!" << std::endl;
    return 0;
}
