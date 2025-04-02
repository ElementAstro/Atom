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

#include "to_byte.hpp"

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
        } else if constexpr (std::is_same_v<T, std::map<int, int>>) {
            deserialized = deserializeMap<int, int>(bytes, offset);
        } else if constexpr (std::is_same_v<T, std::map<std::string, int>>) {
            deserialized = deserializeMap<std::string, int>(bytes, offset);
        } else if constexpr (std::is_same_v<T, std::optional<int>>) {
            deserialized = deserializeOptional<int>(bytes, offset);
        } else if constexpr (std::is_same_v<
                                 T, std::variant<int, float, std::string>>) {
            deserialized =
                deserializeVariant<int, float, std::string>(bytes, offset);
        } else {
            deserialized = deserialize<T>(bytes, offset);
        }

        // Verify
        EXPECT_EQ(deserialized, original);
        EXPECT_EQ(offset, bytes.size());
    }

    std::string tempFilename;
};

// Test serialization of arithmetic types
TEST_F(SerializationTest, SerializeArithmetic) {
    // Integer types
    verifySerializationCycle(int8_t{42});
    verifySerializationCycle(uint8_t{42});
    verifySerializationCycle(int16_t{1234});
    verifySerializationCycle(uint16_t{1234});
    verifySerializationCycle(int32_t{123456});
    verifySerializationCycle(uint32_t{123456});
    verifySerializationCycle(int64_t{1234567890123});
    verifySerializationCycle(uint64_t{1234567890123});

    // Floating point types
    verifySerializationCycle(float{3.14159f});
    verifySerializationCycle(double{3.14159265359});

    // Boolean
    verifySerializationCycle(true);
    verifySerializationCycle(false);

    // Character
    verifySerializationCycle('A');
}

// Test serialization of enum types
enum class TestEnum { Value1, Value2, Value3 };
TEST_F(SerializationTest, SerializeEnum) {
    verifySerializationCycle(TestEnum::Value1);
    verifySerializationCycle(TestEnum::Value2);
    verifySerializationCycle(TestEnum::Value3);
}

// Test serialization of strings
TEST_F(SerializationTest, SerializeString) {
    verifySerializationCycle(std::string(""));
    verifySerializationCycle(std::string("Hello, World!"));
    verifySerializationCycle(
        std::string("Special chars: !@#$%^&*()_+-=[]{}\\|;:'\",.<>/?"));

    // Test with longer string
    std::string longString(10000, 'X');
    verifySerializationCycle(longString);

    // Test with string containing null bytes
    std::string nullString("Hello\0World", 11);
    auto bytes = serialize(nullString);
    size_t offset = 0;
    auto deserialized = deserializeString(bytes, offset);
    EXPECT_EQ(deserialized.size(), nullString.size());
    EXPECT_EQ(
        std::memcmp(deserialized.data(), nullString.data(), nullString.size()),
        0);
}

// Test serialization of vectors
TEST_F(SerializationTest, SerializeVector) {
    verifySerializationCycle(std::vector<int>{});
    verifySerializationCycle(std::vector<int>{1, 2, 3, 4, 5});

    // Test vector of strings
    std::vector<std::string> strVec{"Hello", "World", "!"};
    auto bytes = serialize(strVec);
    size_t offset = 0;
    auto deserialized = deserializeVector<std::string>(bytes, offset);
    EXPECT_EQ(deserialized, strVec);
    EXPECT_EQ(offset, bytes.size());

    // Test nested vectors
    std::vector<std::vector<int>> nestedVec{{1, 2}, {3, 4}, {5, 6}};
    auto nestedBytes = serialize(nestedVec);
    offset = 0;
    auto deserializedNested =
        deserializeVector<std::vector<int>>(nestedBytes, offset);
    EXPECT_EQ(deserializedNested, nestedVec);
    EXPECT_EQ(offset, nestedBytes.size());
}

// Test serialization of lists
TEST_F(SerializationTest, SerializeList) {
    verifySerializationCycle(std::list<int>{});
    verifySerializationCycle(std::list<int>{1, 2, 3, 4, 5});

    // Test list of strings
    std::list<std::string> strList{"Hello", "World", "!"};
    auto bytes = serialize(strList);
    size_t offset = 0;
    auto deserialized = deserializeList<std::string>(bytes, offset);
    EXPECT_EQ(deserialized, strList);
    EXPECT_EQ(offset, bytes.size());

    // Test nested lists
    std::list<std::list<int>> nestedList{{1, 2}, {3, 4}, {5, 6}};
    auto nestedBytes = serialize(nestedList);
    offset = 0;
    auto deserializedNested =
        deserializeList<std::list<int>>(nestedBytes, offset);
    EXPECT_EQ(deserializedNested, nestedList);
    EXPECT_EQ(offset, nestedBytes.size());
}

// Test serialization of maps
TEST_F(SerializationTest, SerializeMap) {
    // Simple int->int map
    verifySerializationCycle(std::map<int, int>{});
    verifySerializationCycle(std::map<int, int>{{1, 10}, {2, 20}, {3, 30}});

    // String->int map
    std::map<std::string, int> strMap{{"one", 1}, {"two", 2}, {"three", 3}};
    auto bytes = serialize(strMap);
    size_t offset = 0;
    auto deserialized = deserializeMap<std::string, int>(bytes, offset);
    EXPECT_EQ(deserialized, strMap);
    EXPECT_EQ(offset, bytes.size());

    // Int->string map
    std::map<int, std::string> intStrMap{{1, "one"}, {2, "two"}, {3, "three"}};
    auto intStrBytes = serialize(intStrMap);
    offset = 0;
    auto deserializedIntStr =
        deserializeMap<int, std::string>(intStrBytes, offset);
    EXPECT_EQ(deserializedIntStr, intStrMap);
    EXPECT_EQ(offset, intStrBytes.size());

    // Nested map
    std::map<int, std::map<int, int>> nestedMap{{1, {{1, 11}, {2, 12}}},
                                                {2, {{3, 23}, {4, 24}}}};
    auto nestedBytes = serialize(nestedMap);
    offset = 0;
    auto deserializedNested =
        deserializeMap<int, std::map<int, int>>(nestedBytes, offset);
    EXPECT_EQ(deserializedNested, nestedMap);
    EXPECT_EQ(offset, nestedBytes.size());
}

// Test serialization of optionals
TEST_F(SerializationTest, SerializeOptional) {
    // Empty optional
    verifySerializationCycle(std::optional<int>{});

    // Optional with value
    verifySerializationCycle(std::optional<int>{42});

    // Optional with string
    std::optional<std::string> optStr = "Hello, World!";
    auto bytes = serialize(optStr);
    size_t offset = 0;
    auto deserialized = deserializeOptional<std::string>(bytes, offset);
    EXPECT_EQ(deserialized, optStr);
    EXPECT_EQ(offset, bytes.size());

    // Optional with complex type
    std::optional<std::vector<int>> optVec = std::vector<int>{1, 2, 3};
    auto vecBytes = serialize(optVec);
    offset = 0;
    auto deserializedVec =
        deserializeOptional<std::vector<int>>(vecBytes, offset);
    EXPECT_EQ(deserializedVec, optVec);
    EXPECT_EQ(offset, vecBytes.size());
}

// Test serialization of variants
TEST_F(SerializationTest, SerializeVariant) {
    // Test variant with different alternatives
    using TestVariant = std::variant<int, float, std::string>;

    // Int alternative
    TestVariant var1 = 42;
    auto bytes1 = serialize(var1);
    size_t offset = 0;
    auto deserialized1 =
        deserializeVariant<int, float, std::string>(bytes1, offset);
    EXPECT_EQ(deserialized1, var1);
    EXPECT_EQ(offset, bytes1.size());

    // Float alternative
    TestVariant var2 = 3.14f;
    auto bytes2 = serialize(var2);
    offset = 0;
    auto deserialized2 =
        deserializeVariant<int, float, std::string>(bytes2, offset);
    EXPECT_EQ(deserialized2, var2);
    EXPECT_EQ(offset, bytes2.size());

    // String alternative
    TestVariant var3 = std::string("Hello, Variant!");
    auto bytes3 = serialize(var3);
    offset = 0;
    auto deserialized3 =
        deserializeVariant<int, float, std::string>(bytes3, offset);
    EXPECT_EQ(deserialized3, var3);
    EXPECT_EQ(offset, bytes3.size());
}

// Test error handling for deserialize
TEST_F(SerializationTest, DeserializeErrors) {
    // Test with empty data
    std::vector<uint8_t> emptyBytes;
    size_t offset = 0;
    EXPECT_THROW(deserialize<int>(emptyBytes, offset), SerializationException);

    // Test with insufficient data
    std::vector<uint8_t> shortBytes = {0x01, 0x02};  // Not enough for an int
    offset = 0;
    EXPECT_THROW(deserialize<int>(shortBytes, offset), SerializationException);

    // Test with offset beyond the data size
    std::vector<uint8_t> bytes = {0x01, 0x02, 0x03, 0x04};
    offset = bytes.size();
    EXPECT_THROW(deserialize<int>(bytes, offset), SerializationException);

    // Test with negative offset - should not compile or fail at runtime
    // offset = -1;  // This would cause undefined behavior
}

// Test error handling for deserializeString
TEST_F(SerializationTest, DeserializeStringErrors) {
    // Test with empty data
    std::vector<uint8_t> emptyBytes;
    size_t offset = 0;
    EXPECT_THROW(deserializeString(emptyBytes, offset), SerializationException);

    // Test with insufficient data for size
    std::vector<uint8_t> shortBytes = {0x01, 0x02};  // Not enough for size_t
    offset = 0;
    EXPECT_THROW(deserializeString(shortBytes, offset), SerializationException);

    // Test with size but insufficient data for string
    std::vector<uint8_t> sizeBytes = serialize(size_t{10});  // Size = 10
    std::vector<uint8_t> insufficientBytes(sizeBytes);
    insufficientBytes.insert(insufficientBytes.end(),
                             {'H', 'e', 'l', 'l', 'o'});  // Only 5 chars
    offset = 0;
    EXPECT_THROW(deserializeString(insufficientBytes, offset),
                 SerializationException);
}

// Test error handling for deserializeVariant
TEST_F(SerializationTest, DeserializeVariantErrors) {
    // Test with empty data
    std::vector<uint8_t> emptyBytes;
    size_t offset = 0;
    EXPECT_THROW(
        (deserializeVariant<int, float, std::string>(emptyBytes, offset)),
        SerializationException);

    // Test with invalid index
    std::vector<uint8_t> invalidIndexBytes = serialize(size_t{
        3});  // Index 3 is out of range for variant<int, float, std::string>
    offset = 0;
    EXPECT_THROW((deserializeVariant<int, float, std::string>(invalidIndexBytes,
                                                              offset)),
                 SerializationException);

    // Test with insufficient data after index
    std::vector<uint8_t> insufficientBytes =
        serialize(size_t{0});  // Index 0 (int)
    offset = 0;
    EXPECT_THROW((deserializeVariant<int, float, std::string>(insufficientBytes,
                                                              offset)),
                 SerializationException);
}

// Test file operations
TEST_F(SerializationTest, FileOperations) {
    // Test data
    std::vector<int> testData = {1, 2, 3, 4, 5};
    auto serialized = serialize(testData);

    // Save to file
    EXPECT_NO_THROW(saveToFile(serialized, tempFilename));

    // Load from file
    std::vector<uint8_t> loaded;
    EXPECT_NO_THROW(loaded = loadFromFile(tempFilename));

    // Verify loaded data
    EXPECT_EQ(loaded, serialized);

    // Deserialize loaded data
    size_t offset = 0;
    auto deserialized = deserializeVector<int>(loaded, offset);
    EXPECT_EQ(deserialized, testData);

    // Test error handling with non-existent file
    EXPECT_THROW(loadFromFile("non_existent_file.bin"), SerializationException);

// Test error handling with non-writable file
#ifndef _WIN32
    // This test may not work on Windows due to permissions
    EXPECT_THROW(saveToFile(serialized, "/root/test_file.bin"),
                 SerializationException);
#endif
}

// Test with large data to ensure it handles memory correctly
TEST_F(SerializationTest, LargeData) {
    // Large vector of integers
    std::vector<int> largeVec(10000);
    std::ranges::iota(largeVec.begin(), largeVec.end(),
                      0);  // Fill with 0...9999

    auto bytes = serialize(largeVec);
    size_t offset = 0;
    auto deserialized = deserializeVector<int>(bytes, offset);

    EXPECT_EQ(deserialized, largeVec);
    EXPECT_EQ(offset, bytes.size());

    // Large string
    std::string largeString(100000, 'X');
    auto stringBytes = serialize(largeString);
    offset = 0;
    auto deserializedString = deserializeString(stringBytes, offset);

    EXPECT_EQ(deserializedString, largeString);
    EXPECT_EQ(offset, stringBytes.size());
}

// Custom serializable type for testing
struct CustomType {
    int x;
    float y;
    std::string z;

    bool operator==(const CustomType& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

// Provide serialize function for CustomType
auto serialize(const CustomType& obj) -> std::vector<uint8_t> {
    std::vector<uint8_t> bytes;

    auto xBytes = serialize(obj.x);
    auto yBytes = serialize(obj.y);
    auto zBytes = serialize(obj.z);

    bytes.insert(bytes.end(), xBytes.begin(), xBytes.end());
    bytes.insert(bytes.end(), yBytes.begin(), yBytes.end());
    bytes.insert(bytes.end(), zBytes.begin(), zBytes.end());

    return bytes;
}

// Provide deserialize function for CustomType
auto deserialize(const std::span<const uint8_t>& bytes,
                 size_t& offset) -> CustomType {
    CustomType obj;
    obj.x = deserialize<int>(bytes, offset);
    obj.y = deserialize<float>(bytes, offset);
    obj.z = deserializeString(bytes, offset);
    return obj;
}

// Test with custom serializable type
TEST_F(SerializationTest, CustomType) {
    CustomType original{42, 3.14f, "Hello, Custom Type!"};

    auto bytes = serialize(original);
    size_t offset = 0;
    auto deserialized = deserialize<CustomType>(bytes, offset);

    EXPECT_EQ(deserialized, original);
    EXPECT_EQ(offset, bytes.size());

    // Test in a container
    std::vector<CustomType> vec{
        {1, 1.1f, "One"}, {2, 2.2f, "Two"}, {3, 3.3f, "Three"}};

    auto vecBytes = serialize(vec);
    offset = 0;
    auto deserializedVec = deserializeVector<CustomType>(vecBytes, offset);

    EXPECT_EQ(deserializedVec, vec);
    EXPECT_EQ(offset, vecBytes.size());
}

// Test endianness handling
TEST_F(SerializationTest, EndiannessHandling) {
    // Test serialization of multi-byte values
    uint16_t value16 = 0x1234;
    uint32_t value32 = 0x12345678;
    uint64_t value64 = 0x1234567890ABCDEF;

    // Serialize
    auto bytes16 = serialize(value16);
    auto bytes32 = serialize(value32);
    auto bytes64 = serialize(value64);

    // Check sizes
    EXPECT_EQ(bytes16.size(), sizeof(uint16_t));
    EXPECT_EQ(bytes32.size(), sizeof(uint32_t));
    EXPECT_EQ(bytes64.size(), sizeof(uint64_t));

    // Deserialize
    size_t offset = 0;
    auto deserialized16 = deserialize<uint16_t>(bytes16, offset);
    offset = 0;
    auto deserialized32 = deserialize<uint32_t>(bytes32, offset);
    offset = 0;
    auto deserialized64 = deserialize<uint64_t>(bytes64, offset);

    // Check values are preserved
    EXPECT_EQ(deserialized16, value16);
    EXPECT_EQ(deserialized32, value32);
    EXPECT_EQ(deserialized64, value64);

// If boost is enabled, we could add additional tests for explicit endian
// conversion
#ifdef ATOM_USE_BOOST
// These tests would go here
#endif
}

// Test with nested containers
TEST_F(SerializationTest, NestedContainers) {
    // Map of string -> vector<int>
    std::map<std::string, std::vector<int>> complexMap{
        {"one", {1, 2, 3}}, {"two", {4, 5, 6}}, {"three", {7, 8, 9}}};

    auto bytes = serialize(complexMap);
    size_t offset = 0;
    auto deserialized =
        deserializeMap<std::string, std::vector<int>>(bytes, offset);

    EXPECT_EQ(deserialized, complexMap);
    EXPECT_EQ(offset, bytes.size());

    // Vector of optionals
    std::vector<std::optional<int>> optVec{
        std::optional<int>{1}, std::optional<int>{}, std::optional<int>{3}};

    auto optVecBytes = serialize(optVec);
    offset = 0;
    auto deserializedOptVec =
        deserializeVector<std::optional<int>>(optVecBytes, offset);

    EXPECT_EQ(deserializedOptVec, optVec);
    EXPECT_EQ(offset, optVecBytes.size());

    // Optional containing a map
    std::optional<std::map<int, std::string>> optMap =
        std::map<int, std::string>{{1, "one"}, {2, "two"}, {3, "three"}};

    auto optMapBytes = serialize(optMap);
    offset = 0;
    auto deserializedOptMap =
        deserializeOptional<std::map<int, std::string>>(optMapBytes, offset);

    EXPECT_EQ(deserializedOptMap, optMap);
    EXPECT_EQ(offset, optMapBytes.size());
}

// Test partial deserialization
TEST_F(SerializationTest, PartialDeserialization) {
    // Create a serialized buffer with multiple items
    int intValue = 42;
    float floatValue = 3.14f;
    std::string strValue = "Hello, World!";

    std::vector<uint8_t> bytes;
    auto intBytes = serialize(intValue);
    auto floatBytes = serialize(floatValue);
    auto strBytes = serialize(strValue);

    bytes.insert(bytes.end(), intBytes.begin(), intBytes.end());
    bytes.insert(bytes.end(), floatBytes.begin(), floatBytes.end());
    bytes.insert(bytes.end(), strBytes.begin(), strBytes.end());

    // Deserialize each item in sequence
    size_t offset = 0;
    auto deserializedInt = deserialize<int>(bytes, offset);
    auto deserializedFloat = deserialize<float>(bytes, offset);
    auto deserializedStr = deserializeString(bytes, offset);

    EXPECT_EQ(deserializedInt, intValue);
    EXPECT_EQ(deserializedFloat, floatValue);
    EXPECT_EQ(deserializedStr, strValue);
    EXPECT_EQ(offset, bytes.size());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}