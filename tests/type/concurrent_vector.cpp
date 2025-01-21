#include "atom/type/concurrent_vector.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

using namespace atom::type;

class ConcurrentVectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        vector = std::make_unique<concurrent_vector<int>>();
    }

    void TearDown() override { vector.reset(); }

    std::unique_ptr<concurrent_vector<int>> vector;
};

// Basic Operations Tests
TEST_F(ConcurrentVectorTest, InitialSizeIsZero) {
    EXPECT_EQ(vector->get_size(), 0);
}

TEST_F(ConcurrentVectorTest, PushBackIncreasesSize) {
    vector->push_back(1);
    EXPECT_EQ(vector->get_size(), 1);
}

TEST_F(ConcurrentVectorTest, PopBackDecreasesSize) {
    vector->push_back(1);
    vector->pop_back();
    EXPECT_EQ(vector->get_size(), 0);
}

TEST_F(ConcurrentVectorTest, ElementAccess) {
    vector->push_back(42);
    EXPECT_EQ((*vector)[0], 42);
}

// Concurrent Operation Tests
TEST_F(ConcurrentVectorTest, ConcurrentPushBack) {
    const int num_threads = 4;
    const int items_per_thread = 1000;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < items_per_thread; ++j) {
                vector->push_back(i * items_per_thread + j);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(vector->get_size(), num_threads * items_per_thread);
}

TEST_F(ConcurrentVectorTest, ParallelForEach) {
    for (int i = 0; i < 100; ++i) {
        vector->push_back(i);
    }

    std::atomic<int> sum{0};
    vector->parallel_for_each([&](int& value) { sum += value; });

    EXPECT_EQ(sum, 4950);  // Sum of numbers 0-99
}

TEST_F(ConcurrentVectorTest, BatchInsert) {
    std::vector<int> values{1, 2, 3, 4, 5};
    vector->batch_insert(values);
    EXPECT_EQ(vector->get_size(), 5);
    EXPECT_EQ((*vector)[0], 1);
    EXPECT_EQ((*vector)[4], 5);
}

TEST_F(ConcurrentVectorTest, ParallelBatchInsert) {
    std::vector<int> values(1000);
    for (int i = 0; i < 1000; ++i) {
        values[i] = i;
    }
    vector->parallel_batch_insert(values);
    EXPECT_EQ(vector->get_size(), 1000);
}

// Thread Pool Tests
TEST_F(ConcurrentVectorTest, ThreadPoolTask) {
    std::atomic<bool> task_completed{false};
    vector->submit_task([&]() { task_completed = true; });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(task_completed);
}

// Edge Cases Tests
TEST_F(ConcurrentVectorTest, ClearOperation) {
    for (int i = 0; i < 10; ++i) {
        vector->push_back(i);
    }
    vector->clear();
    EXPECT_EQ(vector->get_size(), 0);
}

TEST_F(ConcurrentVectorTest, ClearRangeOperation) {
    for (int i = 0; i < 10; ++i) {
        vector->push_back(i);
    }
    vector->clear_range(2, 5);
    EXPECT_EQ((*vector)[2], 0);  // Should be default-constructed
}

TEST_F(ConcurrentVectorTest, ParallelFind) {
    for (int i = 0; i < 1000; ++i) {
        vector->push_back(i);
    }
    EXPECT_TRUE(vector->parallel_find(500));
    EXPECT_FALSE(vector->parallel_find(1001));
}

TEST_F(ConcurrentVectorTest, MoveSemantics) {
    vector->push_back(std::move(42));
    EXPECT_EQ((*vector)[0], 42);
}

TEST_F(ConcurrentVectorTest, GetConstData) {
    vector->push_back(1);
    vector->push_back(2);
    const auto& data = vector->get_data();
    EXPECT_EQ(data[0], 1);
    EXPECT_EQ(data[1], 2);
}

TEST_F(ConcurrentVectorTest, OutOfRangeAccess) {
    vector->push_back(1);
    EXPECT_DEATH((*vector)[1], ".*");
}