#include "atom/utils/uuid.hpp"
#include <gtest/gtest.h>
#include <random>

using namespace atom::utils;

class UUIDTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test default constructor
TEST_F(UUIDTest, DefaultConstructor) {
    UUID uuid;
    EXPECT_FALSE(uuid.toString().empty());
}

// Test constructor with 16-byte array
TEST_F(UUIDTest, ConstructorWithArray) {
    std::array<uint8_t, 16> data = {0, 1, 2,  3,  4,  5,  6,  7,
                                    8, 9, 10, 11, 12, 13, 14, 15};
    UUID uuid(data);
    EXPECT_EQ(uuid.getData(), data);
}

// Test toString method
TEST_F(UUIDTest, ToString) {
    UUID uuid;
    std::string uuid_str = uuid.toString();
    EXPECT_EQ(uuid_str.size(), 36);  // UUID string representation length
}

// Test fromString method
TEST_F(UUIDTest, FromString) {
    std::string uuid_str = "123e4567-e89b-12d3-a456-426614174000";
    UUID uuid = UUID::fromString(uuid_str);
    EXPECT_EQ(uuid.toString(), uuid_str);
}

// Test equality operator
TEST_F(UUIDTest, EqualityOperator) {
    UUID uuid1 = UUID::generateV4();
    UUID uuid2 = UUID::generateV4();
    EXPECT_FALSE(uuid1 == uuid2);
    EXPECT_TRUE(uuid1 == uuid1);
}

// Test inequality operator
TEST_F(UUIDTest, InequalityOperator) {
    UUID uuid1 = UUID::generateV4();
    UUID uuid2 = UUID::generateV4();
    EXPECT_TRUE(uuid1 != uuid2);
    EXPECT_FALSE(uuid1 != uuid1);
}

// Test less-than operator
TEST_F(UUIDTest, LessThanOperator) {
    UUID uuid1 = UUID::generateV4();
    UUID uuid2 = UUID::generateV4();
    EXPECT_NE(uuid1 < uuid2, uuid2 < uuid1);
}

// Test stream operators
TEST_F(UUIDTest, StreamOperators) {
    UUID uuid = UUID::generateV4();
    std::stringstream ss;
    ss << uuid;
    UUID uuid2;
    ss >> uuid2;
    EXPECT_EQ(uuid, uuid2);
}

// Test getData method
TEST_F(UUIDTest, GetData) {
    UUID uuid = UUID::generateV4();
    auto data = uuid.getData();
    EXPECT_EQ(data.size(), 16);
}

// Test version method
TEST_F(UUIDTest, Version) {
    UUID uuid = UUID::generateV4();
    EXPECT_EQ(uuid.version(), 4);
}

// Test variant method
TEST_F(UUIDTest, Variant) {
    UUID uuid = UUID::generateV4();
    EXPECT_EQ(uuid.variant(), 2);
}

// Test generateV1 method
TEST_F(UUIDTest, GenerateV1) {
    UUID uuid = UUID::generateV1();
    EXPECT_EQ(uuid.version(), 1);
}

// Test generateV3 method
TEST_F(UUIDTest, GenerateV3) {
    UUID namespace_uuid = UUID::generateV4();
    UUID uuid = UUID::generateV3(namespace_uuid, "test");
    EXPECT_EQ(uuid.version(), 3);
}

// Test generateV4 method
TEST_F(UUIDTest, GenerateV4) {
    UUID uuid = UUID::generateV4();
    EXPECT_EQ(uuid.version(), 4);
}

// Test generateV5 method
TEST_F(UUIDTest, GenerateV5) {
    UUID namespace_uuid = UUID::generateV4();
    UUID uuid = UUID::generateV5(namespace_uuid, "test");
    EXPECT_EQ(uuid.version(), 5);
}

class FastUUIDTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test constructors
TEST_F(FastUUIDTest, Constructors) {
    FastUUID uuid1;
    FastUUID uuid2(uuid1);
    EXPECT_EQ(uuid1, uuid2);
}

// Test fromStrFactory method
TEST_F(FastUUIDTest, FromStrFactory) {
    std::string uuid_str = "123e4567-e89b-12d3-a456-426614174000";
    FastUUID uuid = FastUUID::fromStrFactory(uuid_str);
    EXPECT_EQ(uuid.str(), uuid_str);
}

// Test bytes method
TEST_F(FastUUIDTest, BytesMethod) {
    FastUUID uuid;
    std::string bytes = uuid.bytes();
    EXPECT_EQ(bytes.size(), 16);
}

// Test str method
TEST_F(FastUUIDTest, StrMethod) {
    FastUUID uuid;
    std::string str = uuid.str();
    EXPECT_EQ(str.size(), 36);
}

// Test equality operator
TEST_F(FastUUIDTest, EqualityOperator) {
    FastUUID uuid1;
    FastUUID uuid2;
    EXPECT_FALSE(uuid1 == uuid2);
    EXPECT_TRUE(uuid1 == uuid1);
}

// Test inequality operator
TEST_F(FastUUIDTest, InequalityOperator) {
    FastUUID uuid1;
    FastUUID uuid2;
    EXPECT_TRUE(uuid1 != uuid2);
    EXPECT_FALSE(uuid1 != uuid1);
}

// Test comparison operators
TEST_F(FastUUIDTest, ComparisonOperators) {
    FastUUID uuid1;
    FastUUID uuid2;
    EXPECT_NE(uuid1 < uuid2, uuid2 < uuid1);
    EXPECT_NE(uuid1 > uuid2, uuid2 > uuid1);
    EXPECT_NE(uuid1 <= uuid2, uuid2 <= uuid1);
    EXPECT_NE(uuid1 >= uuid2, uuid2 >= uuid1);
}

// Test stream operators
TEST_F(FastUUIDTest, StreamOperators) {
    FastUUID uuid;
    std::stringstream ss;
    ss << uuid;
    FastUUID uuid2;
    ss >> uuid2;
    EXPECT_EQ(uuid, uuid2);
}

// Test hash method
TEST_F(FastUUIDTest, HashMethod) {
    FastUUID uuid;
    size_t hash = uuid.hash();
    EXPECT_NE(hash, 0);
}

class FastUUIDGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test constructors
TEST_F(FastUUIDGeneratorTest, Constructors) {
    FastUUIDGenerator<std::mt19937> generator1;
    FastUUIDGenerator<std::mt19937> generator2(12345);
    EXPECT_NE(generator1.getUUID(), generator2.getUUID());
}

// Test getUUID method
TEST_F(FastUUIDGeneratorTest, GetUUID) {
    FastUUIDGenerator<std::mt19937> generator;
    FastUUID uuid = generator.getUUID();
    EXPECT_FALSE(uuid.str().empty());
}