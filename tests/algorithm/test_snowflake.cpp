#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bitset>
#include <chrono>
#include <future>
#include <set>
#include <thread>
#include <unordered_set>
#include <vector>

#include "atom/algorithm/snowflake.hpp"
#include "atom/log/loguru.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

// Default epoch for tests (2020-01-01)
constexpr uint64_t TEST_EPOCH = 1577836800000;

// Test fixture for Snowflake tests
class SnowflakeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru for testing
        static bool initialized = false;
        if (!initialized) {
            // loguru::g_stderr_verbosity = loguru::Verbosity_OFF; // Removed
            // loguru
            initialized = true;
        }
    }

    // Helper function to extract parts of a Snowflake ID
    void extractIdParts(uint64_t id, uint64_t& timestamp,
                        uint64_t& datacenter_id, uint64_t& worker_id,
                        uint64_t& sequence) {
        snowflake_.parseId(id, timestamp, datacenter_id, worker_id, sequence);
    }

    // Helper to wait for a specific amount of time to ensure unique timestamps
    void waitForMillisecond() {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < 2ms) {
            std::this_thread::yield();
        }
    }

    Snowflake<TEST_EPOCH> snowflake_{0, 0};
};

// Basic ID generation tests
TEST_F(SnowflakeTest, GenerateSingleId) {
    auto ids = snowflake_.nextid<1>();
    auto id = ids[0];
    EXPECT_GT(id, 0);
}

TEST_F(SnowflakeTest, GenerateMultipleIds) {
    constexpr size_t COUNT = 10;
    auto ids = snowflake_.nextid<COUNT>();

    // Check that we got the right number of IDs
    EXPECT_EQ(ids.size(), COUNT);

    // Check that all IDs are unique
    std::set<uint64_t> unique_ids(ids.begin(), ids.end());
    EXPECT_EQ(unique_ids.size(), COUNT);
}

TEST_F(SnowflakeTest, IdStructure) {
    auto ids = snowflake_.nextid<1>();
    auto id = ids[0];

    uint64_t timestamp, datacenter_id, worker_id, sequence;
    extractIdParts(id, timestamp, datacenter_id, worker_id, sequence);

    // Check that parts are within expected ranges
    EXPECT_EQ(datacenter_id, 0);
    EXPECT_EQ(worker_id, 0);
    EXPECT_GE(timestamp, TEST_EPOCH);
    EXPECT_LE(timestamp, TEST_EPOCH + 100000);  // Reasonable future range
    EXPECT_LT(sequence, (1ULL << Snowflake<TEST_EPOCH>::SEQUENCE_BITS));
}

TEST_F(SnowflakeTest, IdIncrementsOverTime) {
    auto ids1 = snowflake_.nextid<1>();
    auto id1 = ids1[0];

    waitForMillisecond();  // Wait to ensure different timestamp

    auto ids2 = snowflake_.nextid<1>();
    auto id2 = ids2[0];

    uint64_t timestamp1, datacenter_id1, worker_id1, sequence1;
    uint64_t timestamp2, datacenter_id2, worker_id2, sequence2;

    extractIdParts(id1, timestamp1, datacenter_id1, worker_id1, sequence1);
    extractIdParts(id2, timestamp2, datacenter_id2, worker_id2, sequence2);

    // Second ID should have a later timestamp or higher sequence
    EXPECT_TRUE(timestamp2 > timestamp1 ||
                (timestamp2 == timestamp1 && sequence2 > sequence1));
}

// Test uniqueness of generated IDs
TEST_F(SnowflakeTest, IdUniqueness) {
    constexpr size_t COUNT = 100000;
    std::unordered_set<uint64_t> unique_ids;

    for (size_t i = 0; i < COUNT; ++i) {
        auto ids = snowflake_.nextid<1>();
        auto id = ids[0];
        EXPECT_TRUE(unique_ids.insert(id).second)
            << "Duplicate ID generated: " << id;
    }

    EXPECT_EQ(unique_ids.size(), COUNT);
}

// Test sequence number handling within the same millisecond
TEST_F(SnowflakeTest, SequenceIncrements) {
    // Generate two IDs rapidly to ensure they have the same timestamp
    auto ids1 = snowflake_.nextid<1>();
    auto id1 = ids1[0];
    auto ids2 = snowflake_.nextid<1>();
    auto id2 = ids2[0];

    uint64_t timestamp1, datacenter_id1, worker_id1, sequence1;
    uint64_t timestamp2, datacenter_id2, worker_id2, sequence2;

    extractIdParts(id1, timestamp1, datacenter_id1, worker_id1, sequence1);
    extractIdParts(id2, timestamp2, datacenter_id2, worker_id2, sequence2);

    // If timestamps are the same, sequence should increment
    if (timestamp1 == timestamp2) {
        EXPECT_EQ(sequence2, sequence1 + 1);
    }
}

// Test worker and datacenter ID assignment
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

// Test initialization with invalid IDs
TEST_F(SnowflakeTest, InvalidInitialization) {
    // Worker ID too large
    EXPECT_THROW(
        {
            Snowflake<TEST_EPOCH> invalid(
                Snowflake<TEST_EPOCH>::MAX_WORKER_ID + 1, 0);
        },
        InvalidWorkerIdException);

    // Datacenter ID too large
    EXPECT_THROW(
        {
            Snowflake<TEST_EPOCH> invalid(
                0, Snowflake<TEST_EPOCH>::MAX_DATACENTER_ID + 1);
        },
        InvalidDatacenterIdException);
}

// Test the init method
TEST_F(SnowflakeTest, InitMethod) {
    Snowflake<TEST_EPOCH> snowflake;

    // Test valid initialization
    EXPECT_NO_THROW(snowflake.init(15, 20));
    EXPECT_EQ(snowflake.getWorkerId(), 15);
    EXPECT_EQ(snowflake.getDatacenterId(), 20);

    // Test invalid worker ID
    EXPECT_THROW(
        { snowflake.init(Snowflake<TEST_EPOCH>::MAX_WORKER_ID + 1, 0); },
        InvalidWorkerIdException);

    // Test invalid datacenter ID
    EXPECT_THROW(
        { snowflake.init(0, Snowflake<TEST_EPOCH>::MAX_DATACENTER_ID + 1); },
        InvalidDatacenterIdException);
}

// Test ID validation
TEST_F(SnowflakeTest, IdValidation) {
    Snowflake<TEST_EPOCH> snowflake(5, 10);
    auto ids = snowflake.nextid<1>();
    auto id = ids[0];

    // ID should be valid for the instance that created it
    EXPECT_TRUE(snowflake.validateId(id));

    // ID should be invalid for an instance with different worker/datacenter IDs
    Snowflake<TEST_EPOCH> other_snowflake(6, 10);
    EXPECT_FALSE(other_snowflake.validateId(id));

    // Create a fake ID with future timestamp
    uint64_t fake_timestamp = snowflake.extractTimestamp(id) + 10000;
    uint64_t fake_id = ((fake_timestamp - TEST_EPOCH)
                        << Snowflake<TEST_EPOCH>::TIMESTAMP_LEFT_SHIFT) |
                       (10 << Snowflake<TEST_EPOCH>::DATACENTER_ID_SHIFT) |
                       (5 << Snowflake<TEST_EPOCH>::WORKER_ID_SHIFT) | 123;

    // Future timestamp IDs should be invalid
    EXPECT_FALSE(snowflake.validateId(fake_id));
}

// Test timestamp extraction
TEST_F(SnowflakeTest, TimestampExtraction) {
    auto ids = snowflake_.nextid<1>();
    auto id = ids[0];
    uint64_t timestamp = snowflake_.extractTimestamp(id);

    // Timestamp should be recent
    uint64_t current_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

    // Allow for some clock drift (±10 seconds)
    EXPECT_NEAR(timestamp, current_time, 10000);
}

// Test ID parsing
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

// Test the reset functionality
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

    // After reset, sequence should be 0
    EXPECT_EQ(sequence2, 0);
}

// Test statistics
TEST_F(SnowflakeTest, Statistics) {
    Snowflake<TEST_EPOCH> snowflake;
    auto stats_before = snowflake.getStatistics();

    // Generate 100 IDs
    for (int i = 0; i < 100; ++i) {
        auto ids = snowflake.nextid<1>();
        (void)ids;  // Discard the result to avoid the warning
    }

    auto stats_after = snowflake.getStatistics();

    // Statistics should be updated
    EXPECT_GE(stats_after.timestamp_wait_count,
              stats_before.timestamp_wait_count);
}

// Test serialization and deserialization
TEST_F(SnowflakeTest, Serialization) {
    Snowflake<TEST_EPOCH> original(12, 24);

    // Generate some IDs to advance sequence
    for (int i = 0; i < 10; ++i) {
        auto ids = original.nextid<1>();
        (void)ids;  // Discard the result to avoid the warning
    }

    std::string serialized = original.serialize();

    // Verify we got a non-empty string with expected format
    EXPECT_FALSE(serialized.empty());
    EXPECT_TRUE(serialized.find(':') != std::string::npos);

    // Create a new instance and deserialize
    Snowflake<TEST_EPOCH> restored;
    restored.deserialize(serialized);

    // Verify properties match
    EXPECT_EQ(restored.getWorkerId(), 12);
    EXPECT_EQ(restored.getDatacenterId(), 24);

    // Generate IDs from both and verify they validate against the respective
    // generator
    auto ids_original = original.nextid<1>();
    auto original_id = ids_original[0];
    auto ids_restored = restored.nextid<1>();
    auto restored_id = ids_restored[0];

    EXPECT_TRUE(original.validateId(original_id));
    EXPECT_TRUE(restored.validateId(restored_id));
    EXPECT_FALSE(original.validateId(
        restored_id));  // Shouldn't validate against the other generator
    EXPECT_FALSE(restored.validateId(original_id));
}

// Test invalid deserialization
TEST_F(SnowflakeTest, InvalidDeserialization) {
    Snowflake<TEST_EPOCH> snowflake;

    // Test invalid format
    EXPECT_THROW(
        { snowflake.deserialize("not:enough:parts"); }, SnowflakeException);

    // Test invalid data
    EXPECT_THROW(
        { snowflake.deserialize("invalid:data:not:a:number"); },
        std::exception);
}

// Test multi-threading safety
TEST_F(SnowflakeTest, ThreadSafety) {
    Snowflake<TEST_EPOCH, std::mutex> thread_safe_snowflake;
    constexpr int NUM_THREADS = 10;
    constexpr int IDS_PER_THREAD = 1000;

    std::vector<std::future<std::vector<uint64_t>>> futures;
    std::mutex result_mutex;
    std::set<uint64_t> all_ids;

    // Launch multiple threads to generate IDs simultaneously
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

    // Collect results and check for duplicates
    for (auto& future : futures) {
        auto thread_ids = future.get();
        std::lock_guard<std::mutex> lock(result_mutex);
        for (auto id : thread_ids) {
            EXPECT_TRUE(all_ids.insert(id).second)
                << "Duplicate ID detected: " << id;
        }
    }

    // Verify we got the expected number of unique IDs
    EXPECT_EQ(all_ids.size(), NUM_THREADS * IDS_PER_THREAD);
}

// Test batch generation efficiency
TEST_F(SnowflakeTest, BatchEfficiency) {
    constexpr size_t BATCH_SIZE = 1000;

    // Measure time for individual generation
    auto start_individual = std::chrono::high_resolution_clock::now();
    std::vector<uint64_t> individual_ids;
    for (size_t i = 0; i < BATCH_SIZE; ++i) {
        auto next_ids = snowflake_.nextid<1>();
        individual_ids.push_back(next_ids[0]);
    }
    auto end_individual = std::chrono::high_resolution_clock::now();
    auto individual_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end_individual -
                                                              start_individual)
            .count();

    // Measure time for batch generation
    auto start_batch = std::chrono::high_resolution_clock::now();
    auto batch_ids = snowflake_.nextid<BATCH_SIZE>();
    auto end_batch = std::chrono::high_resolution_clock::now();
    auto batch_duration = std::chrono::duration_cast<std::chrono::microseconds>(
                              end_batch - start_batch)
                              .count();

    // Verify all IDs are unique
    std::set<uint64_t> unique_ids(batch_ids.begin(), batch_ids.end());
    EXPECT_EQ(unique_ids.size(), BATCH_SIZE);

    // Output performance information
    std::cout << "Individual generation of " << BATCH_SIZE << " IDs took "
              << individual_duration << " microseconds" << std::endl;
    std::cout << "Batch generation of " << BATCH_SIZE << " IDs took "
              << batch_duration << " microseconds" << std::endl;
    std::cout << "Batch generation is "
              << static_cast<double>(individual_duration) / batch_duration
              << "x faster" << std::endl;

    // Batch generation is expected to be faster, but we don't enforce it as a
    // test as performance can vary by platform
}

// Test different lock types
TEST_F(SnowflakeTest, LockTypes) {
    // Test with no lock (default)
    Snowflake<TEST_EPOCH, SnowflakeNonLock> non_lock_snowflake;
    auto ids1 = non_lock_snowflake.nextid<1>();
    (void)ids1;

    // Test with std::mutex
    Snowflake<TEST_EPOCH, std::mutex> mutex_snowflake;
    auto ids2 = mutex_snowflake.nextid<1>();
    (void)ids2;

#ifdef ATOM_USE_BOOST
    // Test with boost::mutex if available
    Snowflake<TEST_EPOCH, boost::mutex> boost_mutex_snowflake;
    auto ids3 = boost_mutex_snowflake.nextid<1>();
    (void)ids3;
#endif
}

// Test ID bit structure
TEST_F(SnowflakeTest, IdBitStructure) {
    Snowflake<TEST_EPOCH> snowflake(
        31, 31);  // Use max values for worker and datacenter
    auto ids = snowflake.nextid<1>();
    auto id = ids[0];

    // Convert to binary for bit-level inspection
    std::bitset<64> bits(id);
    std::string bit_string = bits.to_string();

    // Extract the key parts from the binary representation
    std::string datacenter_bits = bit_string.substr(
        64 - snowflake.TIMESTAMP_LEFT_SHIFT, snowflake.DATACENTER_ID_BITS);

    std::string worker_bits = bit_string.substr(
        64 - snowflake.DATACENTER_ID_SHIFT, snowflake.WORKER_ID_BITS);

    std::string sequence_bits = bit_string.substr(
        64 - snowflake.WORKER_ID_SHIFT, snowflake.SEQUENCE_BITS);

    // Convert back to integers
    std::bitset<5> datacenter_bitset(datacenter_bits);
    std::bitset<5> worker_bitset(worker_bits);
    std::bitset<12> sequence_bitset(sequence_bits);

    // Now use parseId to get the official values
    uint64_t timestamp, datacenter_id, worker_id, sequence;
    snowflake.parseId(id, timestamp, datacenter_id, worker_id, sequence);

    // Validate against known values
    EXPECT_EQ(datacenter_id, 31);
    EXPECT_EQ(worker_id, 31);
}
