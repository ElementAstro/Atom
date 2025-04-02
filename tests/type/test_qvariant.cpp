#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "atom/type/qvariant.hpp"

namespace atom::type::test {

// Custom struct for testing complex types
struct TestStruct {
    int id;
    std::string name;

    bool operator==(const TestStruct& other) const {
        return id == other.id && name == other.name;
    }

    friend std::ostream& operator<<(std::ostream& os, const TestStruct& obj) {
        os << "TestStruct{id=" << obj.id << ", name=" << obj.name << "}";
        return os;
    }
};

class VariantWrapperTest : public ::testing::Test {
protected:
    using TestVariant =
        VariantWrapper<int, double, std::string, bool, TestStruct>;

    void SetUp() override {
        // Initialize with default values
        intVariant = TestVariant(42);
        doubleVariant = TestVariant(3.14);
        stringVariant = TestVariant(std::string("test"));
        boolVariant = TestVariant(true);
        testStructVariant = TestVariant(TestStruct{1, "test"});
        emptyVariant = TestVariant();  // Default constructor creates monostate
    }

    TestVariant intVariant;
    TestVariant doubleVariant;
    TestVariant stringVariant;
    TestVariant boolVariant;
    TestVariant testStructVariant;
    TestVariant emptyVariant;
};

// Basic Construction Tests
TEST_F(VariantWrapperTest, DefaultConstruction) {
    EXPECT_FALSE(emptyVariant.hasValue());
    EXPECT_EQ(emptyVariant.index(), 0);  // std::monostate is at index 0
}

TEST_F(VariantWrapperTest, ValueConstruction) {
    EXPECT_TRUE(intVariant.hasValue());
    EXPECT_TRUE(intVariant.is<int>());
    EXPECT_EQ(intVariant.get<int>(), 42);

    EXPECT_TRUE(doubleVariant.hasValue());
    EXPECT_TRUE(doubleVariant.is<double>());
    EXPECT_DOUBLE_EQ(doubleVariant.get<double>(), 3.14);

    EXPECT_TRUE(stringVariant.hasValue());
    EXPECT_TRUE(stringVariant.is<std::string>());
    EXPECT_EQ(stringVariant.get<std::string>(), "test");

    EXPECT_TRUE(boolVariant.hasValue());
    EXPECT_TRUE(boolVariant.is<bool>());
    EXPECT_EQ(boolVariant.get<bool>(), true);

    EXPECT_TRUE(testStructVariant.hasValue());
    EXPECT_TRUE(testStructVariant.is<TestStruct>());
    EXPECT_EQ(testStructVariant.get<TestStruct>().id, 1);
    EXPECT_EQ(testStructVariant.get<TestStruct>().name, "test");
}

// Copy and Move Tests
TEST_F(VariantWrapperTest, CopyConstruction) {
    TestVariant copy(intVariant);
    EXPECT_TRUE(copy.is<int>());
    EXPECT_EQ(copy.get<int>(), 42);
}

TEST_F(VariantWrapperTest, MoveConstruction) {
    TestVariant original(123);
    TestVariant moved(std::move(original));

    EXPECT_TRUE(moved.is<int>());
    EXPECT_EQ(moved.get<int>(), 123);

    // Original should be in a valid but unspecified state
    // We don't make specific assertions about it
}

TEST_F(VariantWrapperTest, CopyAssignment) {
    TestVariant copy;
    copy = intVariant;

    EXPECT_TRUE(copy.is<int>());
    EXPECT_EQ(copy.get<int>(), 42);
}

TEST_F(VariantWrapperTest, MoveAssignment) {
    TestVariant original(123);
    TestVariant moved;
    moved = std::move(original);

    EXPECT_TRUE(moved.is<int>());
    EXPECT_EQ(moved.get<int>(), 123);
}

TEST_F(VariantWrapperTest, ValueAssignment) {
    TestVariant variant;
    variant = 42;

    EXPECT_TRUE(variant.is<int>());
    EXPECT_EQ(variant.get<int>(), 42);

    variant = 3.14;
    EXPECT_TRUE(variant.is<double>());
    EXPECT_DOUBLE_EQ(variant.get<double>(), 3.14);

    variant = std::string("test");
    EXPECT_TRUE(variant.is<std::string>());
    EXPECT_EQ(variant.get<std::string>(), "test");

    variant = true;
    EXPECT_TRUE(variant.is<bool>());
    EXPECT_EQ(variant.get<bool>(), true);

    variant = TestStruct{1, "test"};
    EXPECT_TRUE(variant.is<TestStruct>());
    EXPECT_EQ(variant.get<TestStruct>().id, 1);
    EXPECT_EQ(variant.get<TestStruct>().name, "test");
}

// Access Tests
TEST_F(VariantWrapperTest, TypeName) {
    // Type names are implementation-defined, so we just check they're not empty
    EXPECT_FALSE(intVariant.typeName().empty());
    EXPECT_FALSE(doubleVariant.typeName().empty());
    EXPECT_FALSE(stringVariant.typeName().empty());
    EXPECT_FALSE(boolVariant.typeName().empty());
    EXPECT_FALSE(testStructVariant.typeName().empty());
    EXPECT_FALSE(emptyVariant.typeName().empty());
}

TEST_F(VariantWrapperTest, GetWithCorrectType) {
    EXPECT_EQ(intVariant.get<int>(), 42);
    EXPECT_DOUBLE_EQ(doubleVariant.get<double>(), 3.14);
    EXPECT_EQ(stringVariant.get<std::string>(), "test");
    EXPECT_EQ(boolVariant.get<bool>(), true);
    EXPECT_EQ(testStructVariant.get<TestStruct>().id, 1);
    EXPECT_EQ(testStructVariant.get<TestStruct>().name, "test");
}

TEST_F(VariantWrapperTest, GetWithIncorrectType) {
    EXPECT_THROW(intVariant.get<double>(), VariantException);
    EXPECT_THROW(doubleVariant.get<int>(), VariantException);
    EXPECT_THROW(stringVariant.get<bool>(), VariantException);
    EXPECT_THROW(boolVariant.get<std::string>(), VariantException);
    EXPECT_THROW(testStructVariant.get<int>(), VariantException);
}

TEST_F(VariantWrapperTest, IsType) {
    EXPECT_TRUE(intVariant.is<int>());
    EXPECT_FALSE(intVariant.is<double>());

    EXPECT_TRUE(doubleVariant.is<double>());
    EXPECT_FALSE(doubleVariant.is<int>());

    EXPECT_TRUE(stringVariant.is<std::string>());
    EXPECT_FALSE(stringVariant.is<bool>());

    EXPECT_TRUE(boolVariant.is<bool>());
    EXPECT_FALSE(boolVariant.is<std::string>());

    EXPECT_TRUE(testStructVariant.is<TestStruct>());
    EXPECT_FALSE(testStructVariant.is<int>());

    EXPECT_TRUE(emptyVariant.is<std::monostate>());
    EXPECT_FALSE(emptyVariant.is<int>());
}

TEST_F(VariantWrapperTest, TryGet) {
    auto intResult = intVariant.tryGet<int>();
    EXPECT_TRUE(intResult.has_value());
    EXPECT_EQ(intResult.value(), 42);

    auto wrongTypeResult = intVariant.tryGet<double>();
    EXPECT_FALSE(wrongTypeResult.has_value());

    auto emptyResult = emptyVariant.tryGet<int>();
    EXPECT_FALSE(emptyResult.has_value());
}

// Conversion Tests
TEST_F(VariantWrapperTest, ToInt) {
    EXPECT_EQ(intVariant.toInt(), 42);
    EXPECT_EQ(doubleVariant.toInt(), 3);  // Truncated
    EXPECT_FALSE(stringVariant.toInt().has_value());
    EXPECT_EQ(boolVariant.toInt(), 1);  // true -> 1
    EXPECT_FALSE(testStructVariant.toInt().has_value());
    EXPECT_FALSE(emptyVariant.toInt().has_value());

    // Test with numeric string
    TestVariant numericString(std::string("123"));
    EXPECT_EQ(numericString.toInt(), 123);

    // Test with invalid string
    TestVariant invalidString(std::string("abc"));
    EXPECT_FALSE(invalidString.toInt().has_value());

    // Test with mixed string (should fail)
    TestVariant mixedString(std::string("123abc"));
    EXPECT_FALSE(mixedString.toInt().has_value());
}

TEST_F(VariantWrapperTest, ToDouble) {
    EXPECT_DOUBLE_EQ(intVariant.toDouble().value(), 42.0);
    EXPECT_DOUBLE_EQ(doubleVariant.toDouble().value(), 3.14);
    EXPECT_FALSE(stringVariant.toDouble().has_value());
    EXPECT_DOUBLE_EQ(boolVariant.toDouble().value(), 1.0);  // true -> 1.0
    EXPECT_FALSE(testStructVariant.toDouble().has_value());
    EXPECT_FALSE(emptyVariant.toDouble().has_value());

    // Test with numeric string
    TestVariant numericString(std::string("123.45"));
    EXPECT_DOUBLE_EQ(numericString.toDouble().value(), 123.45);

    // Test with invalid string
    TestVariant invalidString(std::string("abc"));
    EXPECT_FALSE(invalidString.toDouble().has_value());

    // Test with mixed string (should fail)
    TestVariant mixedString(std::string("123.45abc"));
    EXPECT_FALSE(mixedString.toDouble().has_value());
}

TEST_F(VariantWrapperTest, ToBool) {
    EXPECT_EQ(intVariant.toBool(), true);     // Non-zero -> true
    EXPECT_EQ(doubleVariant.toBool(), true);  // Non-zero -> true
    EXPECT_FALSE(stringVariant.toBool().has_value());
    EXPECT_EQ(boolVariant.toBool(), true);
    EXPECT_FALSE(testStructVariant.toBool().has_value());
    EXPECT_FALSE(emptyVariant.toBool().has_value());

    // Test with zero int (should be false)
    TestVariant zeroInt(0);
    EXPECT_EQ(zeroInt.toBool(), false);

    // Test with boolean strings
    TestVariant trueString(std::string("true"));
    EXPECT_EQ(trueString.toBool(), true);

    TestVariant falseString(std::string("false"));
    EXPECT_EQ(falseString.toBool(), false);

    TestVariant yesString(std::string("yes"));
    EXPECT_EQ(yesString.toBool(), true);

    TestVariant noString(std::string("no"));
    EXPECT_EQ(noString.toBool(), false);

    TestVariant oneString(std::string("1"));
    EXPECT_EQ(oneString.toBool(), true);

    TestVariant zeroString(std::string("0"));
    EXPECT_EQ(zeroString.toBool(), false);

    // Test with invalid string
    TestVariant invalidString(std::string("invalid"));
    EXPECT_FALSE(invalidString.toBool().has_value());
}

TEST_F(VariantWrapperTest, ToString) {
    EXPECT_EQ(intVariant.toString(), "42");
    // Double conversion might vary by platform, so test more loosely
    EXPECT_TRUE(doubleVariant.toString().find("3.14") != std::string::npos);
    EXPECT_EQ(stringVariant.toString(), "test");
    EXPECT_EQ(boolVariant.toString(), "1");  // bool often converts to 1/0
    // Custom struct should call the overloaded operator<<
    EXPECT_TRUE(testStructVariant.toString().find("TestStruct") !=
                std::string::npos);
    EXPECT_EQ(emptyVariant.toString(), "std::monostate");
}

// Comparison Tests
TEST_F(VariantWrapperTest, EqualityComparison) {
    TestVariant intVariant1(42);
    TestVariant intVariant2(42);
    TestVariant intVariant3(43);

    EXPECT_EQ(intVariant1, intVariant2);
    EXPECT_NE(intVariant1, intVariant3);
    EXPECT_NE(intVariant1, doubleVariant);
    EXPECT_NE(intVariant1, stringVariant);
    EXPECT_NE(intVariant1, emptyVariant);

    // Self equality
    EXPECT_EQ(intVariant1, intVariant1);
}

// Visitor Tests
TEST_F(VariantWrapperTest, VisitWithNonModifyingVisitor) {
    auto result = intVariant.visit([](const auto& value) -> std::string {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
            return "monostate";
        } else if constexpr (std::is_same_v<T, int>) {
            return "int: " + std::to_string(value);
        } else if constexpr (std::is_same_v<T, double>) {
            return "double: " + std::to_string(value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return "string: " + value;
        } else if constexpr (std::is_same_v<T, bool>) {
            return std::string("bool: ") + (value ? "true" : "false");
        } else if constexpr (std::is_same_v<T, TestStruct>) {
            return "struct with id: " + std::to_string(value.id);
        } else {
            return "unknown";
        }
    });

    EXPECT_EQ(result, "int: 42");

    auto emptyResult = emptyVariant.visit([](const auto& value) -> std::string {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
            return "monostate";
        } else {
            return "not monostate";
        }
    });

    EXPECT_EQ(emptyResult, "monostate");
}

// State Modification Tests
TEST_F(VariantWrapperTest, Reset) {
    EXPECT_TRUE(intVariant.hasValue());

    intVariant.reset();

    EXPECT_FALSE(intVariant.hasValue());
    EXPECT_TRUE(intVariant.is<std::monostate>());
    EXPECT_EQ(intVariant.index(), 0);
}

// Thread Safety Tests
TEST_F(VariantWrapperTest, ThreadSafety) {
    TestVariant sharedVariant(0);
    constexpr int numThreads = 10;
    constexpr int iterationsPerThread = 1000;

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(
            [&sharedVariant, &successCount, i, iterationsPerThread]() {
                for (int j = 0; j < iterationsPerThread; ++j) {
                    try {
                        // Every other thread writes
                        if (i % 2 == 0) {
                            sharedVariant = i * 1000 + j;
                        }
                        // Other threads read
                        else {
                            auto value = sharedVariant.tryGet<int>();
                            if (value.has_value()) {
                                successCount++;
                            }
                        }
                    } catch (...) {
                        // Ignore any exceptions
                    }
                }
            });
    }

    for (auto& t : threads) {
        t.join();
    }

    // We don't assert on exact counts, just that we had some successful reads
    // and no crashes occurred
    EXPECT_GT(successCount, 0);
}

// Additional tests for edge cases
TEST_F(VariantWrapperTest, EmptyState) {
    EXPECT_FALSE(emptyVariant.hasValue());
    EXPECT_TRUE(emptyVariant.is<std::monostate>());
    EXPECT_EQ(emptyVariant.index(), 0);

    // Getting monostate should work
    EXPECT_NO_THROW(emptyVariant.get<std::monostate>());

    // Getting any other type should throw
    EXPECT_THROW(emptyVariant.get<int>(), VariantException);
}

// Test for variant with different wrapper type
TEST_F(VariantWrapperTest, ConstructFromDifferentVariantWrapper) {
    // Create a variant with only int and string
    using OtherVariant = VariantWrapper<int, std::string>;
    OtherVariant source(123);

    // Construct our test variant from it
    TestVariant target(source);

    EXPECT_TRUE(target.is<int>());
    EXPECT_EQ(target.get<int>(), 123);

    // Test with string
    OtherVariant stringSource(std::string("hello"));
    TestVariant stringTarget(stringSource);

    EXPECT_TRUE(stringTarget.is<std::string>());
    EXPECT_EQ(stringTarget.get<std::string>(), "hello");
}

// Stream operator test
TEST_F(VariantWrapperTest, StreamOperator) {
    std::ostringstream oss;

    oss << intVariant;
    EXPECT_EQ(oss.str(), "42");

    oss.str("");
    oss << emptyVariant;
    EXPECT_EQ(oss.str(), "std::monostate");
}

// Print test (just verify it doesn't crash)
TEST_F(VariantWrapperTest, Print) {
    EXPECT_NO_THROW(intVariant.print());
    EXPECT_NO_THROW(doubleVariant.print());
    EXPECT_NO_THROW(stringVariant.print());
    EXPECT_NO_THROW(boolVariant.print());
    EXPECT_NO_THROW(testStructVariant.print());
    EXPECT_NO_THROW(emptyVariant.print());
}

// Template type validation
TEST_F(VariantWrapperTest, CompileTimeTypeValidation) {
    // This test is more for compile-time validation
    // If this compiles, the type validation is working

    TestVariant v1(42);                     // Valid int
    TestVariant v2(3.14);                   // Valid double
    TestVariant v3(std::string("test"));    // Valid string
    TestVariant v4(true);                   // Valid bool
    TestVariant v5(TestStruct{1, "test"});  // Valid TestStruct

    // These would fail at compile time with a static_assert:
    // TestVariant v6(std::vector<int>{});  // Invalid type
    // TestVariant v7(1.0f);                // Invalid type (float, not double)
}

// WithThreadSafety method test
TEST_F(VariantWrapperTest, WithThreadSafety) {
    int result = intVariant.withThreadSafety([]() { return 123; });
    EXPECT_EQ(result, 123);

    std::string strResult = stringVariant.withThreadSafety(
        []() { return std::string("lambda result"); });
    EXPECT_EQ(strResult, "lambda result");
}

}  // namespace atom::type::test
