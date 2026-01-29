#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "atom/async/queue.hpp"

TEST(ThreadSafeQueueTest, PutAndTake) {
    atom::async::ThreadSafeQueue<int> queue;

    // Put elements into the queue
    queue.put(1);
    queue.put(2);
    queue.put(3);

    // Take elements from the queue
    EXPECT_EQ(queue.take(), 1);
    EXPECT_EQ(queue.take(), 2);
    EXPECT_EQ(queue.take(), 3);
    EXPECT_FALSE(queue.take());  // Queue should be empty now
}

TEST(ThreadSafeQueueTest, Destroy) {
    atom::async::ThreadSafeQueue<int> queue;
    queue.put(1);
    queue.put(2);
    queue.put(3);

    auto destroyedQueue = queue.destroy();
    EXPECT_EQ(destroyedQueue.size(), 3);
    EXPECT_TRUE(queue.empty());  // Original queue should be empty now
}

TEST(ThreadSafeQueueTest, Size) {
    atom::async::ThreadSafeQueue<int> queue;
    EXPECT_EQ(queue.size(), 0);

    queue.put(1);
    queue.put(2);
    queue.put(3);

    EXPECT_EQ(queue.size(), 3);
}

TEST(ThreadSafeQueueTest, Empty) {
    atom::async::ThreadSafeQueue<int> queue;
    EXPECT_TRUE(queue.empty());

    queue.put(1);
    EXPECT_FALSE(queue.empty());
}

TEST(ThreadSafeQueueTest, FrontAndBack) {
    atom::async::ThreadSafeQueue<int> queue;
    queue.put(1);
    queue.put(2);
    queue.put(3);

    EXPECT_EQ(queue.front(), 1);
    EXPECT_EQ(queue.back(), 3);
}

TEST(ThreadSafeQueueTest, Emplace) {
    atom::async::ThreadSafeQueue<int> queue;
    queue.emplace(1);
    queue.emplace(2);
    queue.emplace(3);

    EXPECT_EQ(queue.take(), 1);
    EXPECT_EQ(queue.take(), 2);
    EXPECT_EQ(queue.take(), 3);
}

TEST(ThreadSafeQueueTest, WaitAndTake) {
    atom::async::ThreadSafeQueue<int> queue;
    std::thread producer([&queue] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        queue.put(1);
    });

    EXPECT_EQ(queue.waitFor([](int x) { return x == 1; }), 1);
    producer.join();
}

TEST(ThreadSafeQueueTest, WaitUntilEmpty) {
    atom::async::ThreadSafeQueue<int> queue;
    queue.put(1);
    queue.put(2);
    queue.put(3);

    queue.take();
    queue.take();

    queue.waitUntilEmpty();
    EXPECT_TRUE(queue.empty());
}

TEST(ThreadSafeQueueTest, ExtractIf) {
    atom::async::ThreadSafeQueue<int> queue;
    queue.put(1);
    queue.put(2);
    queue.put(3);
    queue.put(4);
    queue.put(5);

    auto extracted = queue.extractIf([](const int& x) { return x % 2 == 0; });

    EXPECT_EQ(extracted.size(), 2);
    EXPECT_TRUE(std::all_of(extracted.begin(), extracted.end(),
                            [](int x) { return x % 2 == 0; }));

    EXPECT_EQ(queue.size(), 3);
    EXPECT_TRUE(std::all_of(queue.toVector().begin(), queue.toVector().end(),
                            [](int x) { return x % 2 != 0; }));
}

TEST(ThreadSafeQueueTest, Sort) {
    atom::async::ThreadSafeQueue<int> queue;
    queue.put(3);
    queue.put(1);
    queue.put(2);

    queue.sort([](int a, int b) { return a < b; });

    EXPECT_EQ(queue.take(), 1);
    EXPECT_EQ(queue.take(), 2);
    EXPECT_EQ(queue.take(), 3);
}

TEST(ThreadSafeQueueTest, Transform) {
    atom::async::ThreadSafeQueue<int> queue;
    queue.put(1);
    queue.put(2);
    queue.put(3);

    auto transformedQueue =
        queue.transform<double>([](int x) -> double { return x * 2; });

    EXPECT_EQ(transformedQueue->take(), 2);
    EXPECT_EQ(transformedQueue->take(), 4);
    EXPECT_EQ(transformedQueue->take(), 6);
}

TEST(ThreadSafeQueueTest, GroupBy) {
    auto intQueue = std::make_shared<atom::async::ThreadSafeQueue<int>>();

    // 添加一些元素
    for (int i = 0; i <= 4; ++i) {
        intQueue->put(i);
    }
    auto groupedQueues = intQueue->groupBy<std::string>(
        [](const int& x) { return (x % 2 == 0) ? "even" : "odd"; });

    EXPECT_EQ(groupedQueues.size(), 4);

    // TODO: Fix this test
    // EXPECT_EQ(groupedQueues[0].get(),
    //          (std::vector{"even", "odd", "even", "odd", "even"}));
}

TEST(ThreadSafeQueueTest, ToVector) {
    atom::async::ThreadSafeQueue<int> queue;
    queue.put(1);
    queue.put(2);
    queue.put(3);

    auto vector = queue.toVector();

    EXPECT_EQ(vector.size(), 3);
    EXPECT_EQ(vector, std::vector<int>({1, 2, 3}));
}

TEST(ThreadSafeQueueTest, ForEach) {
    atom::async::ThreadSafeQueue<int> queue;
    queue.put(1);
    queue.put(2);
    queue.put(3);

    std::vector<int> results;
    queue.forEach([&results](int x) { results.push_back(x * 2); });

    EXPECT_EQ(results.size(), 3);
    EXPECT_EQ(results, std::vector<int>({2, 4, 6}));
}

TEST(ThreadSafeQueueTest, TryTake) {
    atom::async::ThreadSafeQueue<int> queue;
    queue.put(1);

    EXPECT_EQ(queue.tryTake(), 1);
    EXPECT_FALSE(queue.tryTake());  // Queue should be empty now
}

TEST(ThreadSafeQueueTest, TakeFor) {
    atom::async::ThreadSafeQueue<int> queue;

    std::thread producer([&queue] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        queue.put(1);
    });

    EXPECT_EQ(queue.takeFor(std::chrono::milliseconds(200)), 1);
    producer.join();
}

TEST(ThreadSafeQueueTest, TakeUntil) {
    atom::async::ThreadSafeQueue<int> queue;

    std::thread producer([&queue] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        queue.put(1);
    });

    auto timeoutTime =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    EXPECT_EQ(queue.takeUntil(timeoutTime), 1);
    producer.join();
}

// =============================================================================
// Additional ThreadSafeQueue Tests
// =============================================================================

TEST(ThreadSafeQueueTest, ConcurrentPutTake) {
    atom::async::ThreadSafeQueue<int> queue;
    std::atomic<int> putCount{0};
    std::atomic<int> takeCount{0};

    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int opsPerThread = 100;

    // Put threads
    for (int t = 0; t < numThreads / 2; ++t) {
        threads.emplace_back([&queue, &putCount, opsPerThread]() {
            for (int i = 0; i < opsPerThread; ++i) {
                queue.put(i);
                putCount.fetch_add(1);
            }
        });
    }

    // Take threads
    for (int t = 0; t < numThreads / 2; ++t) {
        threads.emplace_back([&queue, &takeCount, opsPerThread]() {
            for (int i = 0; i < opsPerThread; ++i) {
                if (queue.tryTake().has_value()) {
                    takeCount.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Drain remaining
    while (queue.tryTake().has_value()) {
        takeCount.fetch_add(1);
    }

    EXPECT_EQ(putCount.load(), takeCount.load());
}

TEST(ThreadSafeQueueTest, ComplexType) {
    struct ComplexData {
        int id;
        std::string name;
        std::vector<int> values;
    };

    atom::async::ThreadSafeQueue<ComplexData> queue;

    queue.put({1, "first", {1, 2, 3}});
    queue.put({2, "second", {4, 5, 6}});

    auto first = queue.take();
    EXPECT_TRUE(first.has_value());
    EXPECT_EQ(first->id, 1);
    EXPECT_EQ(first->name, "first");
    EXPECT_EQ(first->values.size(), 3u);
}

TEST(ThreadSafeQueueTest, Clear) {
    atom::async::ThreadSafeQueue<int> queue;
    queue.put(1);
    queue.put(2);
    queue.put(3);

    EXPECT_EQ(queue.size(), 3);

    queue.clear();

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
}

TEST(ThreadSafeQueueTest, MoveOnlyType) {
    atom::async::ThreadSafeQueue<std::unique_ptr<int>> queue;

    queue.put(std::make_unique<int>(42));
    queue.put(std::make_unique<int>(100));

    auto first = queue.take();
    EXPECT_TRUE(first.has_value());
    EXPECT_EQ(**first, 42);

    auto second = queue.take();
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(**second, 100);
}

TEST(ThreadSafeQueueTest, LargeQueue) {
    atom::async::ThreadSafeQueue<int> queue;
    const int numElements = 10000;

    for (int i = 0; i < numElements; ++i) {
        queue.put(i);
    }

    EXPECT_EQ(queue.size(), numElements);

    int count = 0;
    while (queue.take().has_value()) {
        count++;
    }

    EXPECT_EQ(count, numElements);
}

TEST(ThreadSafeQueueTest, FilterAndExtract) {
    atom::async::ThreadSafeQueue<int> queue;
    for (int i = 1; i <= 10; ++i) {
        queue.put(i);
    }

    // Extract multiples of 3
    auto extracted = queue.extractIf([](const int& x) { return x % 3 == 0; });

    EXPECT_EQ(extracted.size(), 3);  // 3, 6, 9
    EXPECT_EQ(queue.size(), 7);
}

TEST(ThreadSafeQueueTest, TransformToString) {
    atom::async::ThreadSafeQueue<int> queue;
    queue.put(1);
    queue.put(2);
    queue.put(3);

    auto stringQueue =
        queue.transform<std::string>([](int x) { return std::to_string(x); });

    EXPECT_EQ(stringQueue->take().value(), "1");
    EXPECT_EQ(stringQueue->take().value(), "2");
    EXPECT_EQ(stringQueue->take().value(), "3");
}

TEST(ThreadSafeQueueTest, ProducerConsumerPattern) {
    atom::async::ThreadSafeQueue<int> queue;
    std::atomic<bool> producerDone{false};
    std::atomic<int> consumedCount{0};

    // Producer
    std::thread producer([&queue, &producerDone]() {
        for (int i = 0; i < 100; ++i) {
            queue.put(i);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        producerDone = true;
    });

    // Consumer
    std::thread consumer([&queue, &producerDone, &consumedCount]() {
        while (!producerDone.load() || !queue.empty()) {
            if (queue.tryTake().has_value()) {
                consumedCount.fetch_add(1);
            }
            std::this_thread::yield();
        }
    });

    producer.join();
    consumer.join();

    // Drain any remaining
    while (queue.tryTake().has_value()) {
        consumedCount.fetch_add(1);
    }

    EXPECT_EQ(consumedCount.load(), 100);
}

TEST(ThreadSafeQueueTest, MultipleProducersOneConsumer) {
    atom::async::ThreadSafeQueue<int> queue;
    std::atomic<int> producedCount{0};
    std::atomic<int> consumedCount{0};
    std::atomic<bool> done{false};

    const int numProducers = 5;
    const int itemsPerProducer = 100;

    std::vector<std::thread> producers;
    for (int p = 0; p < numProducers; ++p) {
        producers.emplace_back([&queue, &producedCount, p, itemsPerProducer]() {
            for (int i = 0; i < itemsPerProducer; ++i) {
                queue.put(p * 1000 + i);
                producedCount.fetch_add(1);
            }
        });
    }

    std::thread consumer([&queue, &consumedCount, &done]() {
        while (!done.load() || !queue.empty()) {
            if (queue.tryTake().has_value()) {
                consumedCount.fetch_add(1);
            }
            std::this_thread::yield();
        }
    });

    for (auto& p : producers) {
        p.join();
    }

    done = true;
    consumer.join();

    // Drain remaining
    while (queue.tryTake().has_value()) {
        consumedCount.fetch_add(1);
    }

    EXPECT_EQ(producedCount.load(), numProducers * itemsPerProducer);
    EXPECT_EQ(consumedCount.load(), numProducers * itemsPerProducer);
}

TEST(ThreadSafeQueueTest, TakeForTimeout) {
    atom::async::ThreadSafeQueue<int> queue;

    auto start = std::chrono::steady_clock::now();
    auto result = queue.takeFor(std::chrono::milliseconds(50));
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(result.has_value());
    EXPECT_GE(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(),
        45);
}

TEST(ThreadSafeQueueTest, Reduce) {
    atom::async::ThreadSafeQueue<int> queue;
    queue.put(1);
    queue.put(2);
    queue.put(3);
    queue.put(4);
    queue.put(5);

    int sum = 0;
    queue.forEach([&sum](int x) { sum += x; });

    EXPECT_EQ(sum, 15);
}
