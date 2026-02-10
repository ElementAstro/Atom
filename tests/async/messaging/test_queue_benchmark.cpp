/*
 * test_queue_benchmark.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-26

Description: Benchmark tests for messaging queue implementations

**************************************************/

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "atom/async/messaging/queue.hpp"

namespace atom::async::test {

/**
 * @brief Queue performance benchmark utility class
 * @tparam Q Queue type
 * @tparam T Element type
 */
template <template <typename> class Q, typename T>
class QueueBenchmark {
public:
    /**
     * @brief Test put/take performance with elements of different sizes
     * @param numOperations Number of operations to perform
     * @param elementSize Size of each element in bytes
     */
    static void benchmarkPutTake(size_t numOperations,
                                 size_t elementSize = sizeof(T)) {
        Q<std::vector<char>> queue;

        // Fill element to reach specified size
        std::vector<char> element(elementSize, 'X');

        auto start = std::chrono::high_resolution_clock::now();

        // Put operations
        for (size_t i = 0; i < numOperations; ++i) {
            queue.put(element);
        }

        // Take operations
        for (size_t i = 0; i < numOperations; ++i) {
            auto result = queue.take();
            if (!result) break;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Queue type: " << typeid(Q<T>).name() << "\n";
        std::cout << "Element size: " << elementSize << " bytes\n";
        std::cout << "Operations: " << numOperations << " puts + "
                  << numOperations << " takes\n";
        std::cout << "Total time: " << duration.count() << " µs\n";
        std::cout << "Average time per operation: "
                  << duration.count() / (numOperations * 2.0) << " µs\n";
        std::cout << "----------------------------------------\n";
    }

    /**
     * @brief Test multi-producer, multi-consumer performance
     * @param numProducers Number of producer threads
     * @param numConsumers Number of consumer threads
     * @param itemsPerProducer Number of items each producer will produce
     */
    static void benchmarkMultiThreaded(size_t numProducers, size_t numConsumers,
                                       size_t itemsPerProducer) {
        Q<size_t> queue;
        std::atomic<size_t> producedCount{0};
        std::atomic<size_t> consumedCount{0};

        auto start = std::chrono::high_resolution_clock::now();

        // Create producer threads
        std::vector<std::thread> producers;
        for (size_t p = 0; p < numProducers; ++p) {
            producers.emplace_back(
                [&queue, &producedCount, itemsPerProducer, p]() {
                    for (size_t i = 0; i < itemsPerProducer; ++i) {
                        queue.put(p * itemsPerProducer + i);
                        producedCount.fetch_add(1, std::memory_order_relaxed);
                    }
                });
        }

        // Create consumer threads
        std::vector<std::thread> consumers;
        const size_t totalItems = numProducers * itemsPerProducer;
        for (size_t c = 0; c < numConsumers; ++c) {
            consumers.emplace_back([&queue, &consumedCount, totalItems]() {
                while (consumedCount.load(std::memory_order_relaxed) <
                       totalItems) {
                    auto item = queue.tryTake();
                    if (item) {
                        consumedCount.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        std::this_thread::yield();
                    }
                }
            });
        }

        // Wait for all threads to complete
        for (auto& p : producers) p.join();
        for (auto& c : consumers) c.join();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Queue type: " << typeid(Q<T>).name() << "\n";
        std::cout << "Threads: " << numProducers << " producers, "
                  << numConsumers << " consumers\n";
        std::cout << "Total items: " << totalItems << "\n";
        std::cout << "Total time: " << duration.count() << " µs\n";
        std::cout << "Throughput: "
                  << (totalItems * 1000000.0) / duration.count()
                  << " ops/sec\n";
        std::cout << "----------------------------------------\n";
    }
};

// Test fixture for queue benchmarks
class QueueBenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(QueueBenchmarkTest, ThreadSafeQueuePutTake) {
    std::cout << "\n=== ThreadSafeQueue Put/Take Benchmark ===\n";
    QueueBenchmark<ThreadSafeQueue, int>::benchmarkPutTake(10000, 64);
}

TEST_F(QueueBenchmarkTest, ThreadSafeQueueMultiThreaded) {
    std::cout << "\n=== ThreadSafeQueue Multi-Threaded Benchmark ===\n";
    QueueBenchmark<ThreadSafeQueue, int>::benchmarkMultiThreaded(4, 4, 1000);
}

#ifdef ATOM_USE_LOCKFREE_QUEUE
TEST_F(QueueBenchmarkTest, LockFreeQueuePutTake) {
    std::cout << "\n=== LockFreeQueue Put/Take Benchmark ===\n";
    QueueBenchmark<LockFreeQueue, int>::benchmarkPutTake(10000, 64);
}

TEST_F(QueueBenchmarkTest, SPSCQueuePutTake) {
    std::cout << "\n=== SPSCQueue Put/Take Benchmark ===\n";
    QueueBenchmark<SPSCQueue, int>::benchmarkPutTake(10000, 64);
}
#endif

}  // namespace atom::async::test
