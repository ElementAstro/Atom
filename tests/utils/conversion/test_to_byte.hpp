// filepath: /home/max/Atom-1/atom/utils/test_to_byte.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <list>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "atom/utils/conversion/to_byte.hpp"

using namespace atom::utils;
using ::testing::ElementsAreArray;

class SerializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary file for testing file operations
        tempFilename = "test_serialization_temp.bin";
    }

    void TearDown() override {
        // Clean up temporary file
        std::remove(tempFilename.c_str());
    }

    // Helper function to convert span to vector for testing
    std::vector<uint8_t> spanToVector(std::span<const uint8_t> span) {
        return std::vector<uint8_t>(span.begin(), span.end());
    }

    // Helper function to verify serialization/deserialization cycle
    template <typename T>
    void verifySerializationCycle(const T& original) {
        // Serialize
        auto bytes = serialize(original);

        // Deserialize
        size_t offset = 0;
        T deserialized;

        if constexpr (std::is_same_v<T, std::string>) {
            deserialized = deserializeString(bytes, offset);
        } else if constexpr (std::is_same_v<T, std::vector<int>>) {
            deserialized = deserializeVector<int>(bytes, offset);
        } else if constexpr (std::is_same_v<T, std::list<int>>) {
            deserialized = deserializeList<int>(bytes, offset);
        } else if constexpr (std::is_same_v<T, std::map<std::string, int>>) {
            deserialized = deserializeMap<std::string, int>(bytes, offset);
        } else {
            deserialized = deserialize<T>(bytes, offset);
        }

        // Verify they match
        EXPECT_EQ(original, deserialized);
    }

    std::string tempFilename;
};

// Test basic numeric type serialization
TEST_F(SerializationTest, BasicNumericTypes) {
    // Test integers
    verifySerializationCycle<int>(42);
    verifySerializationCycle<int>(-42);
    verifySerializationCycle<int>(0);
    verifySerializationCycle<int>(std::numeric_limits<int>::max());
    verifySerializationCycle<int>(std::numeric_limits<int>::min());

    // Test unsigned integers
    verifySerializationCycle<uint32_t>(42u);
    verifySerializationCycle<uint32_t>(0u);
    verifySerializationCycle<uint32_t>(std::numeric_limits<uint32_t>::max());

    // Test floating point
    verifySerializationCycle<float>(3.14159f);
    verifySerializationCycle<float>(-3.14159f);
    verifySerializationCycle<float>(0.0f);
    verifySerializationCycle<float>(std::numeric_limits<float>::infinity());
    verifySerializationCycle<float>(-std::numeric_limits<float>::infinity());

    verifySerializationCycle<double>(3.141592653589793);
    verifySerializationCycle<double>(-3.141592653589793);
    verifySerializationCycle<double>(0.0);

    // Test boolean
    verifySerializationCycle<bool>(true);
    verifySerializationCycle<bool>(false);

    // Test char
    verifySerializationCycle<char>('A');
    verifySerializationCycle<char>('\0');
    verifySerializationCycle<char>('\n');
}

// Test string serialization
TEST_F(SerializationTest, StringSerialization) {
    verifySerializationCycle<std::string>("Hello, World!");
    verifySerializationCycle<std::string>("");
    verifySerializationCycle<std::string>("A");
    verifySerializationCycle<std::string>(
        std::string(1000, 'X'));  // Large string
    verifySerializationCycle<std::string>("String with\nnewlines\tand\ttabs");
    verifySerializationCycle<std::string>(
        "String with special chars: !@#$%^&*()");
}

// Test vector serialization
TEST_F(SerializationTest, VectorSerialization) {
    verifySerializationCycle<std::vector<int>>({});
    verifySerializationCycle<std::vector<int>>({1});
    verifySerializationCycle<std::vector<int>>({1, 2, 3, 4, 5});
    verifySerializationCycle<std::vector<int>>({-1, -2, -3, -4, -5});

    // Large vector
    std::vector<int> largeVec(1000);
    std::iota(largeVec.begin(), largeVec.end(), 0);
    verifySerializationCycle(largeVec);

    // Vector of strings
    verifySerializationCycle<std::vector<std::string>>(
        {"hello", "world", "test"});
    verifySerializationCycle<std::vector<std::string>>({});
    verifySerializationCycle<std::vector<std::string>>({"single"});
}

// Test list serialization
TEST_F(SerializationTest, ListSerialization) {
    verifySerializationCycle<std::list<int>>({});
    verifySerializationCycle<std::list<int>>({1});
    verifySerializationCycle<std::list<int>>({1, 2, 3, 4, 5});
    verifySerializationCycle<std::list<int>>({-1, -2, -3, -4, -5});
}

// Test map serialization
TEST_F(SerializationTest, MapSerialization) {
    verifySerializationCycle<std::map<std::string, int>>({});
    verifySerializationCycle<std::map<std::string, int>>({{"key1", 1}});
    verifySerializationCycle<std::map<std::string, int>>(
        {{"key1", 1}, {"key2", 2}, {"key3", 3}});

    // Map with complex values
    verifySerializationCycle<std::map<int, std::string>>(
        {{1, "one"}, {2, "two"}, {3, "three"}});
}

// Test optional serialization
TEST_F(SerializationTest, OptionalSerialization) {
    verifySerializationCycle<std::optional<int>>(std::nullopt);
    verifySerializationCycle<std::optional<int>>(42);
    verifySerializationCycle<std::optional<int>>(0);
    verifySerializationCycle<std::optional<int>>(-42);

    verifySerializationCycle<std::optional<std::string>>(std::nullopt);
    verifySerializationCycle<std::optional<std::string>>("hello");
    verifySerializationCycle<std::optional<std::string>>("");
}

// Test variant serialization
TEST_F(SerializationTest, VariantSerialization) {
    using TestVariant = std::variant<int, std::string, double>;

    verifySerializationCycle<TestVariant>(42);
    verifySerializationCycle<TestVariant>(std::string("hello"));
    verifySerializationCycle<TestVariant>(3.14159);

    // Test with different variant alternatives
    verifySerializationCycle<TestVariant>(0);
    verifySerializationCycle<TestVariant>(std::string(""));
    verifySerializationCycle<TestVariant>(0.0);
}

// Test tuple serialization
TEST_F(SerializationTest, TupleSerialization) {
    verifySerializationCycle<std::tuple<int, std::string, double>>(
        std::make_tuple(42, "hello", 3.14159));

    verifySerializationCycle<std::tuple<int>>(std::make_tuple(42));

    verifySerializationCycle<std::tuple<>>(std::make_tuple());

    verifySerializationCycle<std::tuple<int, int, int>>(
        std::make_tuple(1, 2, 3));
}

// Test pair serialization
TEST_F(SerializationTest, PairSerialization) {
    verifySerializationCycle<std::pair<int, std::string>>(
        std::make_pair(42, "hello"));

    verifySerializationCycle<std::pair<std::string, std::string>>(
        std::make_pair("key", "value"));

    verifySerializationCycle<std::pair<int, int>>(std::make_pair(1, 2));
}

// Test enum serialization
enum class TestEnum : int { VALUE1 = 1, VALUE2 = 2, VALUE3 = 100 };

TEST_F(SerializationTest, EnumSerialization) {
    verifySerializationCycle<TestEnum>(TestEnum::VALUE1);
    verifySerializationCycle<TestEnum>(TestEnum::VALUE2);
    verifySerializationCycle<TestEnum>(TestEnum::VALUE3);
}

// Test endianness handling
TEST_F(SerializationTest, EndiannessHandling) {
    uint32_t value = 0x12345678;
    auto bytes = serialize(value);

    // Verify the bytes are in the expected order based on system endianness
    EXPECT_EQ(bytes.size(), sizeof(uint32_t));

    // Deserialize and verify
    size_t offset = 0;
    uint32_t deserialized = deserialize<uint32_t>(bytes, offset);
    EXPECT_EQ(value, deserialized);

    // Test with different endianness functions
    auto bigEndianBytes = serializeBigEndian(value);
    auto littleEndianBytes = serializeLittleEndian(value);

    EXPECT_EQ(bigEndianBytes.size(), sizeof(uint32_t));
    EXPECT_EQ(littleEndianBytes.size(), sizeof(uint32_t));

// On little-endian systems, they should be different
#ifdef ATOM_LITTLE_ENDIAN
    EXPECT_NE(bigEndianBytes, littleEndianBytes);
#endif
}

// Test file serialization
TEST_F(SerializationTest, FileSerialization) {
    std::vector<int> testData = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Serialize to file
    EXPECT_TRUE(serializeToFile(testData, tempFilename));

    // Deserialize from file
    auto deserializedData = deserializeFromFile<std::vector<int>>(tempFilename);
    EXPECT_TRUE(deserializedData.has_value());
    EXPECT_EQ(testData, deserializedData.value());

    // Test with non-existent file
    auto nonExistentResult =
        deserializeFromFile<std::vector<int>>("non_existent_file.bin");
    EXPECT_FALSE(nonExistentResult.has_value());
}

// Test error handling
TEST_F(SerializationTest, ErrorHandling) {
    // Test deserialization with insufficient data
    std::vector<uint8_t> insufficientData = {0x01, 0x02};
    size_t offset = 0;

    EXPECT_THROW(deserialize<uint64_t>(insufficientData, offset),
                 std::runtime_error);

    // Test offset out of bounds
    std::vector<uint8_t> validData = {0x01, 0x02, 0x03, 0x04};
    offset = 10;  // Out of bounds
    EXPECT_THROW(deserialize<uint32_t>(validData, offset), std::out_of_range);

    // Test string deserialization with invalid length
    std::vector<uint8_t> invalidStringData = {0xFF, 0xFF, 0xFF,
                                              0xFF};  // Very large length
    offset = 0;
    EXPECT_THROW(deserializeString(invalidStringData, offset),
                 std::runtime_error);
}

// Test performance with large data
TEST_F(SerializationTest, PerformanceTest) {
    // Create large test data
    std::vector<int> largeData(100000);
    std::iota(largeData.begin(), largeData.end(), 0);

    auto start = std::chrono::high_resolution_clock::now();

    // Serialize
    auto bytes = serialize(largeData);

    // Deserialize
    size_t offset = 0;
    auto deserialized = deserializeVector<int>(bytes, offset);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time (adjust threshold as needed)
    EXPECT_LT(duration.count(), 1000);  // 1 second max

    // Verify correctness
    EXPECT_EQ(largeData, deserialized);
}

// Test thread safety
TEST_F(SerializationTest, ThreadSafety) {
    const int numThreads = 4;
    const int operationsPerThread = 100;
    std::vector<std::future<bool>> futures;

    for (int t = 0; t < numThreads; ++t) {
        futures.push_back(std::async(std::launch::async, [operationsPerThread,
                                                          t]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                // Test different data types in each thread
                std::vector<int> testData = {t, i, t + i, t * i};

                try {
                    auto bytes = serialize(testData);
                    size_t offset = 0;
                    auto deserialized = deserializeVector<int>(bytes, offset);

                    if (testData != deserialized) {
                        return false;
                    }
                } catch (...) {
                    return false;
                }
            }
            return true;
        }));
    }

    // Wait for all threads and check results
    for (auto& future : futures) {
        EXPECT_TRUE(future.get());
    }
}

// Test memory alignment
TEST_F(SerializationTest, MemoryAlignment) {
    // Test that serialized data maintains proper alignment for different types
    struct AlignedStruct {
        char c;
        int i;
        double d;

        bool operator==(const AlignedStruct& other) const {
            return c == other.c && i == other.i && d == other.d;
        }
    };

    AlignedStruct original{'A', 42, 3.14159};

    auto bytes = serialize(original);
    size_t offset = 0;
    auto deserialized = deserialize<AlignedStruct>(bytes, offset);

    EXPECT_EQ(original, deserialized);
}

// Test custom serialization for user-defined types
struct CustomType {
    int value1;
    std::string value2;

    bool operator==(const CustomType& other) const {
        return value1 == other.value1 && value2 == other.value2;
    }
};

// Specialize serialization for CustomType
template <>
std::vector<uint8_t> serialize<CustomType>(const CustomType& obj) {
    auto bytes1 = serialize(obj.value1);
    auto bytes2 = serialize(obj.value2);

    std::vector<uint8_t> result;
    result.insert(result.end(), bytes1.begin(), bytes1.end());
    result.insert(result.end(), bytes2.begin(), bytes2.end());

    return result;
}

template <>
CustomType deserialize<CustomType>(std::span<const uint8_t> data,
                                   size_t& offset) {
    CustomType result;
    result.value1 = deserialize<int>(data, offset);
    result.value2 = deserializeString(data, offset);
    return result;
}

TEST_F(SerializationTest, CustomTypeSerialization) {
    CustomType original{42, "hello"};
    verifySerializationCycle(original);

    CustomType original2{-100, ""};
    verifySerializationCycle(original2);

    CustomType original3{0, "very long string with lots of content to test"};
    verifySerializationCycle(original3);
}

// Test boundary conditions
TEST_F(SerializationTest, BoundaryConditions) {
    // Test maximum values
    verifySerializationCycle<uint8_t>(std::numeric_limits<uint8_t>::max());
    verifySerializationCycle<uint16_t>(std::numeric_limits<uint16_t>::max());
    verifySerializationCycle<uint32_t>(std::numeric_limits<uint32_t>::max());
    verifySerializationCycle<uint64_t>(std::numeric_limits<uint64_t>::max());

    // Test minimum values
    verifySerializationCycle<int8_t>(std::numeric_limits<int8_t>::min());
    verifySerializationCycle<int16_t>(std::numeric_limits<int16_t>::min());
    verifySerializationCycle<int32_t>(std::numeric_limits<int32_t>::min());
    verifySerializationCycle<int64_t>(std::numeric_limits<int64_t>::min());

    // Test special floating point values
    verifySerializationCycle<float>(std::numeric_limits<float>::quiet_NaN());
    verifySerializationCycle<double>(std::numeric_limits<double>::quiet_NaN());
    verifySerializationCycle<float>(
        std::numeric_limits<float>::signaling_NaN());
    verifySerializationCycle<double>(
        std::numeric_limits<double>::signaling_NaN());
}

// Test compression integration (if available)
TEST_F(SerializationTest, CompressionIntegration) {
    // Create highly compressible data
    std::vector<int> repetitiveData(10000, 42);

    auto normalBytes = serialize(repetitiveData);
    auto compressedBytes = serializeCompressed(repetitiveData);

    // Compressed should be smaller
    EXPECT_LT(compressedBytes.size(), normalBytes.size());

    // Decompress and verify
    size_t offset = 0;
    auto decompressed =
        deserializeCompressed<std::vector<int>>(compressedBytes, offset);
    EXPECT_EQ(repetitiveData, decompressed);
}

// Test versioning support
TEST_F(SerializationTest, VersioningSupport) {
    struct VersionedData {
        uint32_t version;
        std::string data;

        bool operator==(const VersionedData& other) const {
            return version == other.version && data == other.data;
        }
    };

    VersionedData v1{1, "version 1 data"};
    VersionedData v2{2, "version 2 data"};

    auto bytes1 = serializeWithVersion(v1, 1);
    auto bytes2 = serializeWithVersion(v2, 2);

    size_t offset = 0;
    uint32_t version;
    auto deserialized1 =
        deserializeWithVersion<VersionedData>(bytes1, offset, version);
    EXPECT_EQ(version, 1u);
    EXPECT_EQ(v1, deserialized1);

    offset = 0;
    auto deserialized2 =
        deserializeWithVersion<VersionedData>(bytes2, offset, version);
    EXPECT_EQ(version, 2u);
    EXPECT_EQ(v2, deserialized2);
}

}  // namespace
