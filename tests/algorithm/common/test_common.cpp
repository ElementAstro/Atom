#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <array>
#include <vector>

#include "atom/algorithm/common/concepts.hpp"
#include "atom/algorithm/common/endian.hpp"
#include "atom/algorithm/common/hex.hpp"
#include "atom/algorithm/common/parallel.hpp"

using namespace atom::algorithm;

// ============================================================================
// Endian Tests
// ============================================================================

class EndianTest : public ::testing::Test {};

TEST_F(EndianTest, ByteSwap16) {
    EXPECT_EQ(endian::byteSwap16(0x1234), 0x3412);
    EXPECT_EQ(endian::byteSwap16(0x0000), 0x0000);
    EXPECT_EQ(endian::byteSwap16(0xFFFF), 0xFFFF);
    EXPECT_EQ(endian::byteSwap16(0x00FF), 0xFF00);
}

TEST_F(EndianTest, ByteSwap32) {
    EXPECT_EQ(endian::byteSwap32(0x12345678), 0x78563412);
    EXPECT_EQ(endian::byteSwap32(0x00000000), 0x00000000);
    EXPECT_EQ(endian::byteSwap32(0xFFFFFFFF), 0xFFFFFFFF);
    EXPECT_EQ(endian::byteSwap32(0x000000FF), 0xFF000000);
}

TEST_F(EndianTest, ByteSwap64) {
    EXPECT_EQ(endian::byteSwap64(0x123456789ABCDEF0ULL), 0xF0DEBC9A78563412ULL);
    EXPECT_EQ(endian::byteSwap64(0x0000000000000000ULL), 0x0000000000000000ULL);
    EXPECT_EQ(endian::byteSwap64(0xFFFFFFFFFFFFFFFFULL), 0xFFFFFFFFFFFFFFFFULL);
}

TEST_F(EndianTest, ReadBig32) {
    std::array<u8, 4> bytes = {0x12, 0x34, 0x56, 0x78};
    EXPECT_EQ(endian::readBig32(bytes), 0x12345678);
}

TEST_F(EndianTest, ReadLittle32) {
    std::array<u8, 4> bytes = {0x78, 0x56, 0x34, 0x12};
    EXPECT_EQ(endian::readLittle32(bytes), 0x12345678);
}

TEST_F(EndianTest, WriteBig32) {
    std::array<u8, 4> bytes{};
    endian::writeBig32(0x12345678, bytes);
    EXPECT_EQ(bytes[0], 0x12);
    EXPECT_EQ(bytes[1], 0x34);
    EXPECT_EQ(bytes[2], 0x56);
    EXPECT_EQ(bytes[3], 0x78);
}

TEST_F(EndianTest, WriteLittle32) {
    std::array<u8, 4> bytes{};
    endian::writeLittle32(0x12345678, bytes);
    EXPECT_EQ(bytes[0], 0x78);
    EXPECT_EQ(bytes[1], 0x56);
    EXPECT_EQ(bytes[2], 0x34);
    EXPECT_EQ(bytes[3], 0x12);
}

// ============================================================================
// Hex Tests
// ============================================================================

class HexTest : public ::testing::Test {};

TEST_F(HexTest, ToHexStringFromSpan) {
    std::vector<u8> bytes = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    EXPECT_EQ(hex::toHexString(bytes), "0123456789abcdef");
    EXPECT_EQ(hex::toHexString(bytes, true), "0123456789ABCDEF");
}

TEST_F(HexTest, ToHexStringFromArray) {
    std::array<u8, 4> bytes = {0xDE, 0xAD, 0xBE, 0xEF};
    EXPECT_EQ(hex::toHexString(bytes), "deadbeef");
    EXPECT_EQ(hex::toHexString(bytes, true), "DEADBEEF");
}

TEST_F(HexTest, ToHexStringEmpty) {
    std::vector<u8> empty;
    EXPECT_EQ(hex::toHexString(empty), "");
}

TEST_F(HexTest, WordsToHexString) {
    std::vector<u32> words = {0x78563412, 0xF0DEBC9A};
    // Little-endian byte order within each word
    EXPECT_EQ(hex::wordsToHexString(words), "123456789abcdef0");
}

TEST_F(HexTest, HexCharToValue) {
    EXPECT_EQ(hex::hexCharToValue('0'), 0);
    EXPECT_EQ(hex::hexCharToValue('9'), 9);
    EXPECT_EQ(hex::hexCharToValue('a'), 10);
    EXPECT_EQ(hex::hexCharToValue('f'), 15);
    EXPECT_EQ(hex::hexCharToValue('A'), 10);
    EXPECT_EQ(hex::hexCharToValue('F'), 15);
    EXPECT_EQ(hex::hexCharToValue('g'), -1);
    EXPECT_EQ(hex::hexCharToValue('z'), -1);
}

TEST_F(HexTest, FromHexString) {
    auto result = hex::fromHexString("0123456789abcdef");
    std::vector<u8> expected = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    EXPECT_EQ(result, expected);
}

TEST_F(HexTest, FromHexStringUppercase) {
    auto result = hex::fromHexString("DEADBEEF");
    std::vector<u8> expected = {0xDE, 0xAD, 0xBE, 0xEF};
    EXPECT_EQ(result, expected);
}

TEST_F(HexTest, FromHexStringInvalid) {
    // Odd length
    EXPECT_TRUE(hex::fromHexString("123").empty());
    // Invalid characters
    EXPECT_TRUE(hex::fromHexString("gg").empty());
}

TEST_F(HexTest, RoundTrip) {
    std::vector<u8> original = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    auto hexStr = hex::toHexString(original);
    auto result = hex::fromHexString(hexStr);
    EXPECT_EQ(original, result);
}

// ============================================================================
// Parallel Tests
// ============================================================================

class ParallelTest : public ::testing::Test {};

TEST_F(ParallelTest, GetDefaultThreadCount) {
    auto count = parallel::getDefaultThreadCount();
    EXPECT_GT(count, 0);
}

TEST_F(ParallelTest, CalculateChunkSize) {
    // Basic calculation
    EXPECT_EQ(parallel::calculateChunkSize(100, 4, 10), 25);

    // Respects minimum chunk size
    EXPECT_EQ(parallel::calculateChunkSize(100, 100, 50), 50);

    // Edge cases
    EXPECT_EQ(parallel::calculateChunkSize(0, 4, 10), 0);
    EXPECT_EQ(parallel::calculateChunkSize(100, 0, 10), 100);
}

TEST_F(ParallelTest, ParallelForEachSmallData) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    std::atomic<int> sum{0};

    parallel::parallelForEach<int>(
        std::span<int>(data),
        [&sum](std::span<int> chunk, usize) {
            for (int val : chunk) {
                sum += val;
            }
        },
        2, 2);

    EXPECT_EQ(sum.load(), 15);
}

TEST_F(ParallelTest, ParallelForEachLargeData) {
    std::vector<int> data(10000);
    std::iota(data.begin(), data.end(), 0);
    std::atomic<long long> sum{0};

    parallel::parallelForEach<int>(
        std::span<int>(data),
        [&sum](std::span<int> chunk, usize) {
            for (int val : chunk) {
                sum += val;
            }
        },
        4, 100);

    // Sum of 0..9999 = 9999 * 10000 / 2 = 49995000
    EXPECT_EQ(sum.load(), 49995000LL);
}

TEST_F(ParallelTest, ParallelMapBasic) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8};

    auto results = parallel::parallelMap<int, int>(
        std::span<const int>(data),
        [](std::span<const int> chunk, usize) {
            int sum = 0;
            for (int val : chunk) {
                sum += val;
            }
            return sum;
        },
        2, 2);

    int total = 0;
    for (int r : results) {
        total += r;
    }
    EXPECT_EQ(total, 36);  // 1+2+3+4+5+6+7+8 = 36
}

TEST_F(ParallelTest, ParallelMapReduceBasic) {
    std::vector<int> data(1000);
    std::iota(data.begin(), data.end(), 1);

    auto result = parallel::parallelMapReduce<int, long long>(
        std::span<const int>(data),
        [](std::span<const int> chunk, usize) -> long long {
            long long sum = 0;
            for (int val : chunk) {
                sum += val;
            }
            return sum;
        },
        [](long long a, long long b) { return a + b; },
        0LL,
        4);

    // Sum of 1..1000 = 1000 * 1001 / 2 = 500500
    EXPECT_EQ(result, 500500LL);
}

TEST_F(ParallelTest, ParallelForEachEmpty) {
    std::vector<int> empty;
    bool called = false;

    parallel::parallelForEach<int>(
        std::span<int>(empty),
        [&called](std::span<int>, usize) {
            called = true;
        });

    EXPECT_FALSE(called);
}

// ============================================================================
// Concepts Tests (compile-time checks)
// ============================================================================

// These are compile-time checks - if they compile, the concepts work
static_assert(StringLike<std::string>);
static_assert(StringLike<std::string_view>);
static_assert(!StringLike<int>);

static_assert(ByteLike<std::byte>);
static_assert(ByteLike<char>);
static_assert(ByteLike<unsigned char>);
static_assert(ByteLike<u8>);
static_assert(!ByteLike<int>);

static_assert(ByteContainer<std::vector<u8>>);
static_assert(ByteContainer<std::array<u8, 16>>);
static_assert(!ByteContainer<std::vector<int>>);

static_assert(UInt32Container<std::vector<u32>>);
static_assert(!UInt32Container<std::vector<u64>>);

static_assert(Numeric<int>);
static_assert(Numeric<float>);
static_assert(!Numeric<std::string>);

static_assert(Integral<int>);
static_assert(Integral<u64>);
static_assert(!Integral<float>);

static_assert(FloatingPoint<float>);
static_assert(FloatingPoint<double>);
static_assert(!FloatingPoint<int>);

TEST(ConceptsTest, CompileTimeChecks) {
    // This test exists to ensure the static_asserts above are evaluated
    SUCCEED();
}
