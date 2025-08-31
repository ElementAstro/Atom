#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <bitset>
#include <chrono>
#include <future>
#include <set>
#include <thread>
#include <unordered_set>
#include <vector>
#include <spdlog/spdlog.h>
#include "atom/algorithm/snowflake.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

constexpr uint64_t TEST_EPOCH = 1577836800000;

class SnowflakeTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }

    void extractIdParts(uint64_t id, uint64_t& timestamp,
                        uint64_t& datacenter_id, uint64_t& worker_id,
                        uint64_t& sequence) {
        snowflake_.parseId(id, timestamp, datacenter_id, worker_id, sequence);
    }

    void waitForMillisecond() {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < 2ms) {
            std::this_thread::yield();
        }
    }

    Snowflake<TEST_EPOCH> snowflake_{0, 0};
};

TEST_F(SnowflakeTest, GenerateSingleId) {
    auto ids = snowflake_.nextid<1>();
    auto id = ids[0];
    EXPECT_GT(id, 0);
}

TEST_F(SnowflakeTest, GenerateMultipleIds) {
    constexpr size_t COUNT = 10;
    auto ids = snowflake_.nextid<COUNT>();
    EXPECT_EQ(ids.size(), COUNT);
    std::set<uint64_t> unique_ids(ids.begin(), ids.end());
    EXPECT_EQ(unique_ids.size(), COUNT);
}

TEST_F(SnowflakeTest, IdStructure) {
    auto ids = snowflake_.nextid<1>();
    auto id = ids[0];
    uint64_t timestamp, datacenter_id, worker_id, sequence;
    extractIdParts(id, timestamp, datacenter_id, worker_id, sequence);
    EXPECT_EQ(datacenter_id, 0);
    EXPECT_EQ(worker_id, 0);
    EXPECT_GE(timestamp, TEST_EPOCH);
    EXPECT_LE(timestamp, TEST_EPOCH + 100000);
    EXPECT_LT(sequence, (1ULL << Snowflake<TEST_EPOCH>::SEQUENCE_BITS));
}

TEST_F(SnowflakeTest, IdIncrementsOverTime) {
    auto ids1 = snowflake_.nextid<1>();
    auto id1 = ids1[0];
    waitForMillisecond();
    auto ids2 = snowflake_.nextid<1>();
    auto id2 = ids2[0];
    uint64_t timestamp1, datacenter_id1, worker_id1, sequence1;
    uint64_t timestamp2, datacenter_id2, worker_id2, sequence2;
    extractIdParts(id1, timestamp1, datacenter_id1, worker_id1, sequence1);
    extractIdParts(id2, timestamp2, datacenter_id2, worker_id2, sequence2);
    EXPECT_TRUE(timestamp2 > timestamp1 ||
                (timestamp2 == timestamp1 && sequence2 > sequence1));
}

TEST_F(SnowflakeTest, IdUniqueness) {
    constexpr size_t COUNT = 100000;
    std::unordered_set<uint64_t> unique_ids;
    for (size_t i = 0; i < COUNT; ++i) {
        auto ids = snowflake_.nextid<1>();
        auto id = ids[0];
        EXPECT_TRUE(unique_ids.insert(id).second);
    }
    EXPECT_EQ(unique_ids.size(), COUNT);
}

TEST_F(SnowflakeTest, SequenceIncrements) {
    auto ids1 = snowflake_.nextid<1>();
    auto id1 = ids1[0];
    auto ids2 = snowflake_.nextid<1>();
    auto id2 = ids2[0];
    uint64_t timestamp1, datacenter_id1, worker_id1, sequence1;
    uint64_t timestamp2, datacenter_id2, worker_id2, sequence2;
    extractIdParts(id1, timestamp1, datacenter_id1, worker_id1, sequence1);
    extractIdParts(id2, timestamp2, datacenter_id2, worker_id2, sequence2);
    if (timestamp1 == timestamp2) {
        EXPECT_EQ(sequence2, sequence1 + 1);
    }
}

TEST_F(SnowflakeTest, WorkerAndDatacenterIds) {
    constexpr uint64_t WORKER_ID = 10;
    constexpr uint64_t DATACENTER_ID = 20;
    Snowflake<TEST_EPOCH> custom_snowflake(WORKER_ID, DATACENTER_ID);
    auto ids = custom_snowflake.nextid<1>();
    auto id = ids[0];
    uint64_t timestamp, datacenter_id, worker_id, sequence;
    custom_snowflake.parseId(id, timestamp, datacenter_id, worker_id, sequence);
    EXPECT_EQ(worker_id, WORKER_ID);
    EXPECT_EQ(datacenter_id, DATACENTER_ID);
}

TEST_F(SnowflakeTest, InvalidInitialization) {
    EXPECT_THROW({
        Snowflake<TEST_EPOCH> invalid(Snowflake<TEST_EPOCH>::MAX_WORKER_ID + 1, 0);
    }, InvalidWorkerIdException);
    EXPECT_THROW({
        Snowflake<TEST_EPOCH> invalid(0, Snowflake<TEST_EPOCH>::MAX_DATACENTER_ID + 1);
    }, InvalidDatacenterIdException);
}

TEST_F(SnowflakeTest, InitMethod) {
    Snowflake<TEST_EPOCH> snowflake;
    EXPECT_NO_THROW(snowflake.init(15, 20));
    EXPECT_EQ(snowflake.getWorkerId(), 15);
    EXPECT_EQ(snowflake.getDatacenterId(), 20);
    EXPECT_THROW({ snowflake.init(Snowflake<TEST_EPOCH>::MAX_WORKER_ID + 1, 0); }, InvalidWorkerIdException);
    EXPECT_THROW({ snowflake.init(0, Snowflake<TEST_EPOCH>::MAX_DATACENTER_ID + 1); }, InvalidDatacenterIdException);
}

TEST_F(SnowflakeTest, IdValidation) {
    Snowflake<TEST_EPOCH> snowflake(5, 10);
    auto ids = snowflake.nextid<1>();
    auto id = ids[0];
    EXPECT_TRUE(snowflake.validateId(id));
    Snowflake<TEST_EPOCH> other_snowflake(6, 10);
    EXPECT_FALSE(other_snowflake.validateId(id));
    uint64_t fake_timestamp = snowflake.extractTimestamp(id) + 10000;
    uint64_t fake_id = ((fake_timestamp - TEST_EPOCH)
                        << Snowflake<TEST_EPOCH>::TIMESTAMP_LEFT_SHIFT) |
                       (10 << Snowflake<TEST_EPOCH>::DATACENTER_ID_SHIFT) |
                       (5 << Snowflake<TEST_EPOCH>::WORKER_ID_SHIFT) | 123;
    EXPECT_FALSE(snowflake.validateId(fake_id));
}

TEST_F(SnowflakeTest, TimestampExtraction) {
    auto ids = snowflake_.nextid<1>();
    auto id = ids[0];
    uint64_t timestamp = snowflake_.extractTimestamp(id);
    uint64_t current_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    EXPECT_NEAR(timestamp, current_time, 10000);
}

TEST_F(SnowflakeTest, IdParsing) {
    Snowflake<TEST_EPOCH> snowflake(15, 20);
    auto ids = snowflake.nextid<1>();
    auto id = ids[0];
    uint64_t timestamp, datacenter_id, worker_id, sequence;
    snowflake.parseId(id, timestamp, datacenter_id, worker_id, sequence);
    EXPECT_EQ(datacenter_id, 20);
    EXPECT_EQ(worker_id, 15);
    EXPECT_GE(timestamp, TEST_EPOCH);
}

TEST_F(SnowflakeTest, Reset) {
    auto ids1 = snowflake_.nextid<1>();
    auto id1 = ids1[0];
    snowflake_.reset();
    auto ids2 = snowflake_.nextid<1>();
    auto id2 = ids2[0];
    uint64_t timestamp1, datacenter_id1, worker_id1, sequence1;
    uint64_t timestamp2, datacenter_id2, worker_id2, sequence2;
    extractIdParts(id1, timestamp1, datacenter_id1, worker_id1, sequence1);
    extractIdParts(id2, timestamp2, datacenter_id2, worker_id2, sequence2);
    EXPECT_EQ(sequence2, 0);
}

TEST_F(SnowflakeTest, Statistics) {
    Snowflake<TEST_EPOCH> snowflake;
    auto stats_before = snowflake.getStatistics();
    for (int i = 0; i < 100; ++i) {
        auto ids = snowflake.nextid<1>();
        (void)ids;
    }
    auto stats_after = snowflake.getStatistics();
    EXPECT_GE(stats_after.timestamp_wait_count, stats_before.timestamp_wait_count);
}

TEST_F(SnowflakeTest, Serialization) {
    Snowflake<TEST_EPOCH> original(12, 24);
    for (int i = 0; i < 10; ++i) {
        auto ids = original.nextid<1>();
        (void)ids;
    }
    std::string serialized = original.serialize();
    EXPECT_FALSE(serialized.empty());
    EXPECT_TRUE(serialized.find(':') != std::string::npos);
    Snowflake<TEST_EPOCH> restored;
    restored.deserialize(serialized);
    EXPECT_EQ(restored.getWorkerId(), 12);
    EXPECT_EQ(restored.getDatacenterId(), 24);
    auto ids_original = original.nextid<1>();
    auto original_id = ids_original[0];
    auto ids_restored = restored.nextid<1>();
    auto restored_id = ids_restored[0];
    EXPECT_TRUE(original.validateId(original_id));
    EXPECT_TRUE(restored.validateId(restored_id));
    EXPECT_FALSE(original.validateId(restored_id));
    EXPECT_FALSE(restored.validateId(original_id));
}

TEST_F(SnowflakeTest, InvalidDeserialization) {
    Snowflake<TEST_EPOCH> snowflake;
    EXPECT_THROW({ snowflake.deserialize("not:enough:parts"); }, SnowflakeException);
    EXPECT_THROW({ snowflake.deserialize("invalid:data:not:a:number"); }, std::exception);
}

TEST_F(SnowflakeTest, ThreadSafety) {
    Snowflake<TEST_EPOCH, std::mutex> thread_safe_snowflake;
    constexpr int NUM_THREADS = 10;
    constexpr int IDS_PER_THREAD = 1000;
    std::vector<std::future<std::vector<uint64_t>>> futures;
    std::mutex result_mutex;
    std::set<uint64_t> all_ids;
    for (int i = 0; i < NUM_THREADS; ++i) {
        futures.push_back(
            std::async(std::launch::async, [&thread_safe_snowflake]() {
                std::vector<uint64_t> ids;
                for (int j = 0; j < IDS_PER_THREAD; ++j) {
                    auto next_ids = thread_safe_snowflake.nextid<1>();
                    ids.push_back(next_ids[0]);
                }
                return ids;
            }));
    }
    for (auto& future : futures) {
        auto thread_ids = future.get();
        std::lock_guard<std::mutex> lock(result_mutex);
        for (auto id : thread_ids) {
            EXPECT_TRUE(all_ids.insert(id).second);
        }
    }
    EXPECT_EQ(all_ids.size(), NUM_THREADS * IDS_PER_THREAD);
}

TEST_F(SnowflakeTest, BatchEfficiency) {
    constexpr size_t BATCH_SIZE = 1000;
    auto start_individual = std::chrono::high_resolution_clock::now();
    std::vector<uint64_t> individual_ids;
    for (size_t i = 0; i < BATCH_SIZE; ++i) {
        auto next_ids = snowflake_.nextid<1>();
        individual_ids.push_back(next_ids[0]);
    }
    auto end_individual = std::chrono::high_resolution_clock::now();
    auto individual_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end_individual - start_individual).count();
    auto start_batch = std::chrono::high_resolution_clock::now();
    auto batch_ids = snowflake_.nextid<BATCH_SIZE>();
    auto end_batch = std::chrono::high_resolution_clock::now();
    auto batch_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_batch - start_batch).count();
    std::set<uint64_t> unique_ids(batch_ids.begin(), batch_ids.end());
    EXPECT_EQ(unique_ids.size(), BATCH_SIZE);
    spdlog::info("Individual generation of {} IDs took {} microseconds", BATCH_SIZE, individual_duration);
    spdlog::info("Batch generation of {} IDs took {} microseconds", BATCH_SIZE, batch_duration);
    spdlog::info("Batch generation is {}x faster", static_cast<double>(individual_duration) / batch_duration);
}

TEST_F(SnowflakeTest, LockTypes) {
    Snowflake<TEST_EPOCH, SnowflakeNonLock> non_lock_snowflake;
    auto ids1 = non_lock_snowflake.nextid<1>();
    (void)ids1;
    Snowflake<TEST_EPOCH, std::mutex> mutex_snowflake;
    auto ids2 = mutex_snowflake.nextid<1>();
    (void)ids2;
#ifdef ATOM_USE_BOOST
    Snowflake<TEST_EPOCH, boost::mutex> boost_mutex_snowflake;
    auto ids3 = boost_mutex_snowflake.nextid<1>();
    (void)ids3;
#endif
}

TEST_F(SnowflakeTest, IdBitStructure) {
    Snowflake<TEST_EPOCH> snowflake(31, 31);
    auto ids = snowflake.nextid<1>();
    auto id = ids[0];
    std::bitset<64> bits(id);
    std::string bit_string = bits.to_string();
    std::string datacenter_bits = bit_string.substr(
        64 - snowflake.DATACENTER_ID_SHIFT - snowflake.DATACENTER_ID_BITS, snowflake.DATACENTER_ID_BITS);
    std::string worker_bits = bit_string.substr(
        64 - snowflake.WORKER_ID_SHIFT - snowflake.WORKER_ID_BITS, snowflake.WORKER_ID_BITS);
    std::string sequence_bits = bit_string.substr(
        64 - snowflake.SEQUENCE_BITS, snowflake.SEQUENCE_BITS);
    std::bitset<5> datacenter_bitset(datacenter_bits);
    std::bitset<5> worker_bitset(worker_bits);
    std::bitset<12> sequence_bitset(sequence_bits);
    uint64_t timestamp, datacenter_id, worker_id, sequence;
    snowflake.parseId(id, timestamp, datacenter_id, worker_id, sequence);
    EXPECT_EQ(datacenter_id, 31);
    EXPECT_EQ(worker_id, 31);
}
