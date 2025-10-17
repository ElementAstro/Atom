#include "atom/components/type_conversion.hpp"
#include "atom/components/scripting_api.hpp"

#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>

using namespace atom::components::scripting;

// Test fixture for TypeConverter tests
class TypeConverterTest : public ::testing::Test {
protected:
    void SetUp() override { converter_ = std::make_unique<TypeConverter>(); }

    std::unique_ptr<TypeConverter> converter_;
};

// Test fixture for type traits tests
class TypeTraitsTest : public ::testing::Test {
protected:
    // Test types for trait testing
    using TestVector = std::vector<int>;
    using TestMap = std::map<std::string, int>;
    using TestUnorderedMap = std::unordered_map<std::string, int>;
    using TestSet = std::set<int>;
    using TestOptional = std::optional<int>;
    using TestUniquePtr = std::unique_ptr<int>;
    using TestSharedPtr = std::shared_ptr<int>;
    using TestTuple = std::tuple<int, std::string, double>;
};

// ============================================================================
// Type Traits Tests
// ============================================================================

TEST_F(TypeTraitsTest, ContainerTraits) {
    using namespace type_traits;

    // Test container detection
    EXPECT_TRUE(is_container<TestVector>::value);
    EXPECT_TRUE(is_container<std::deque<int>>::value);
    EXPECT_TRUE(is_container<std::list<int>>::value);
    EXPECT_TRUE(is_container<TestSet>::value);

    // Test non-containers
    EXPECT_FALSE(is_container<int>::value);
    EXPECT_FALSE(is_container<std::string>::value);
    EXPECT_FALSE(is_container<TestOptional>::value);
}

TEST_F(TypeTraitsTest, AssociativeTraits) {
    using namespace type_traits;

    // Test associative container detection
    EXPECT_TRUE(is_associative<TestMap>::value);
    EXPECT_TRUE(is_associative<TestUnorderedMap>::value);

    // Test non-associative containers
    EXPECT_FALSE(is_associative<TestVector>::value);
    EXPECT_FALSE(is_associative<TestSet>::value);
    EXPECT_FALSE(is_associative<int>::value);
}

TEST_F(TypeTraitsTest, OptionalTraits) {
    using namespace type_traits;

    // Test optional detection
    EXPECT_TRUE(is_optional<TestOptional>::value);
    EXPECT_TRUE(is_optional<std::optional<std::string>>::value);

    // Test non-optionals
    EXPECT_FALSE(is_optional<int>::value);
    EXPECT_FALSE(is_optional<TestVector>::value);
    EXPECT_FALSE(is_optional<TestUniquePtr>::value);
}

TEST_F(TypeTraitsTest, SmartPointerTraits) {
    using namespace type_traits;

    // Test smart pointer detection
    EXPECT_TRUE(is_smart_pointer<TestUniquePtr>::value);
    EXPECT_TRUE(is_smart_pointer<TestSharedPtr>::value);
    EXPECT_TRUE(is_smart_pointer<std::weak_ptr<int>>::value);

    // Test non-smart pointers
    EXPECT_FALSE(is_smart_pointer<int*>::value);
    EXPECT_FALSE(is_smart_pointer<int>::value);
    EXPECT_FALSE(is_smart_pointer<TestOptional>::value);
}

TEST_F(TypeTraitsTest, TupleTraits) {
    using namespace type_traits;

    // Test tuple detection
    EXPECT_TRUE(is_tuple<TestTuple>::value);
    EXPECT_TRUE(is_tuple<std::tuple<int>>::value);
    EXPECT_TRUE(is_tuple<std::tuple<>>::value);

    // Test non-tuples
    EXPECT_FALSE(is_tuple<int>::value);
    EXPECT_FALSE(is_tuple<TestVector>::value);
    EXPECT_FALSE(is_tuple<std::pair<int, int>>::value);
}

// ============================================================================
// TypeConverter Tests
// ============================================================================

TEST_F(TypeConverterTest, BasicTypeConversion) {
    // Test basic type conversions
    ScriptValue intValue(42);
    ScriptValue doubleValue(3.14);
    ScriptValue stringValue("hello");
    ScriptValue boolValue(true);

    // Convert to C++ types
    auto intResult = converter_->toNative<int>(intValue);
    auto doubleResult = converter_->toNative<double>(doubleValue);
    auto stringResult = converter_->toNative<std::string>(stringValue);
    auto boolResult = converter_->toNative<bool>(boolValue);

    EXPECT_EQ(intResult, 42);
    EXPECT_DOUBLE_EQ(doubleResult, 3.14);
    EXPECT_EQ(stringResult, "hello");
    EXPECT_EQ(boolResult, true);
}

TEST_F(TypeConverterTest, VectorConversion) {
    // Create a vector of ScriptValues
    std::vector<ScriptValue> scriptArray = {ScriptValue(1), ScriptValue(2),
                                            ScriptValue(3), ScriptValue(4)};
    ScriptValue arrayValue(scriptArray);

    // Convert to C++ vector
    auto cppVector = converter_->toNative<std::vector<int>>(arrayValue);

    EXPECT_EQ(cppVector.size(), 4);
    EXPECT_EQ(cppVector[0], 1);
    EXPECT_EQ(cppVector[1], 2);
    EXPECT_EQ(cppVector[2], 3);
    EXPECT_EQ(cppVector[3], 4);
}

TEST_F(TypeConverterTest, MapConversion) {
    // Create a map of ScriptValues
    std::unordered_map<std::string, ScriptValue> scriptObject = {
        {"key1", ScriptValue(10)},
        {"key2", ScriptValue(20)},
        {"key3", ScriptValue(30)}};
    ScriptValue objectValue(scriptObject);

    // Convert to C++ map
    auto cppMap = converter_->toNative<std::map<std::string, int>>(objectValue);

    EXPECT_EQ(cppMap.size(), 3);
    EXPECT_EQ(cppMap["key1"], 10);
    EXPECT_EQ(cppMap["key2"], 20);
    EXPECT_EQ(cppMap["key3"], 30);
}

TEST_F(TypeConverterTest, OptionalConversion) {
    // Test optional with value
    ScriptValue valuePresent(42);
    auto optionalWithValue =
        converter_->toNative<std::optional<int>>(valuePresent);

    EXPECT_TRUE(optionalWithValue.has_value());
    EXPECT_EQ(optionalWithValue.value(), 42);

    // Test optional without value (null)
    ScriptValue nullValue;
    auto optionalEmpty = converter_->toNative<std::optional<int>>(nullValue);

    EXPECT_FALSE(optionalEmpty.has_value());
}

TEST_F(TypeConverterTest, TupleConversion) {
    // Create array for tuple conversion
    std::vector<ScriptValue> tupleArray = {
        ScriptValue(42), ScriptValue("hello"), ScriptValue(3.14)};
    ScriptValue tupleValue(tupleArray);

    // Convert to C++ tuple
    auto cppTuple =
        converter_->toNative<std::tuple<int, std::string, double>>(tupleValue);

    EXPECT_EQ(std::get<0>(cppTuple), 42);
    EXPECT_EQ(std::get<1>(cppTuple), "hello");
    EXPECT_DOUBLE_EQ(std::get<2>(cppTuple), 3.14);
}

TEST_F(TypeConverterTest, ReverseConversion) {
    // Test converting C++ types back to ScriptValue

    // Basic types
    auto intScript = converter_->fromNative(42);
    auto doubleScript = converter_->fromNative(3.14);
    auto stringScript = converter_->fromNative(std::string("hello"));
    auto boolScript = converter_->fromNative(true);

    EXPECT_EQ(intScript.get<int64_t>(), 42);
    EXPECT_DOUBLE_EQ(doubleScript.get<double>(), 3.14);
    EXPECT_EQ(stringScript.get<std::string>(), "hello");
    EXPECT_EQ(boolScript.get<bool>(), true);
}

TEST_F(TypeConverterTest, VectorReverseConversion) {
    std::vector<int> cppVector = {1, 2, 3, 4, 5};

    auto scriptValue = converter_->fromNative(cppVector);

    EXPECT_TRUE(scriptValue.holds<std::vector<ScriptValue>>());

    const auto& scriptArray = scriptValue.get<std::vector<ScriptValue>>();
    EXPECT_EQ(scriptArray.size(), 5);
    EXPECT_EQ(scriptArray[0].get<int64_t>(), 1);
    EXPECT_EQ(scriptArray[4].get<int64_t>(), 5);
}

TEST_F(TypeConverterTest, MapReverseConversion) {
    std::map<std::string, int> cppMap = {
        {"alpha", 1}, {"beta", 2}, {"gamma", 3}};

    auto scriptValue = converter_->fromNative(cppMap);

    EXPECT_TRUE(
        scriptValue.holds<std::unordered_map<std::string, ScriptValue>>());

    const auto& scriptObject =
        scriptValue.get<std::unordered_map<std::string, ScriptValue>>();
    EXPECT_EQ(scriptObject.size(), 3);
    EXPECT_EQ(scriptObject.at("alpha").get<int64_t>(), 1);
    EXPECT_EQ(scriptObject.at("beta").get<int64_t>(), 2);
    EXPECT_EQ(scriptObject.at("gamma").get<int64_t>(), 3);
}

// ============================================================================
// Advanced Conversion Tests
// ============================================================================

TEST_F(TypeConverterTest, NestedContainerConversion) {
    // Test nested vector conversion
    std::vector<std::vector<int>> nestedVector = {{1, 2}, {3, 4}, {5, 6}};

    auto scriptValue = converter_->fromNative(nestedVector);
    auto convertedBack =
        converter_->toNative<std::vector<std::vector<int>>>(scriptValue);

    EXPECT_EQ(convertedBack.size(), 3);
    EXPECT_EQ(convertedBack[0].size(), 2);
    EXPECT_EQ(convertedBack[0][0], 1);
    EXPECT_EQ(convertedBack[2][1], 6);
}

TEST_F(TypeConverterTest, ComplexMapConversion) {
    // Test map with vector values
    std::map<std::string, std::vector<int>> complexMap = {
        {"numbers", {1, 2, 3}}, {"more_numbers", {4, 5, 6}}};

    auto scriptValue = converter_->fromNative(complexMap);
    auto convertedBack =
        converter_->toNative<std::map<std::string, std::vector<int>>>(
            scriptValue);

    EXPECT_EQ(convertedBack.size(), 2);
    EXPECT_EQ(convertedBack["numbers"].size(), 3);
    EXPECT_EQ(convertedBack["numbers"][0], 1);
    EXPECT_EQ(convertedBack["more_numbers"][2], 6);
}

TEST_F(TypeConverterTest, SharedPtrConversion) {
    auto sharedPtr = std::make_shared<int>(42);

    auto scriptValue = converter_->fromNative(sharedPtr);
    auto convertedBack =
        converter_->toNative<std::shared_ptr<int>>(scriptValue);

    EXPECT_NE(convertedBack, nullptr);
    EXPECT_EQ(*convertedBack, 42);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(TypeConverterTest, InvalidTypeConversion) {
    ScriptValue stringValue("not a number");

    // Should handle invalid conversions gracefully
    EXPECT_THROW(converter_->toNative<int>(stringValue),
                 std::bad_variant_access);
}

TEST_F(TypeConverterTest, EmptyContainerConversion) {
    std::vector<int> emptyVector;

    auto scriptValue = converter_->fromNative(emptyVector);
    auto convertedBack = converter_->toNative<std::vector<int>>(scriptValue);

    EXPECT_TRUE(convertedBack.empty());
}

TEST_F(TypeConverterTest, NullPointerConversion) {
    std::shared_ptr<int> nullPtr;

    auto scriptValue = converter_->fromNative(nullPtr);

    // Should convert to null/monostate
    EXPECT_TRUE(scriptValue.holds<std::monostate>());
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(TypeConverterTest, LargeVectorConversion) {
    // Test conversion of large vector
    std::vector<int> largeVector;
    for (int i = 0; i < 10000; ++i) {
        largeVector.push_back(i);
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto scriptValue = converter_->fromNative(largeVector);
    auto convertedBack = converter_->toNative<std::vector<int>>(scriptValue);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(convertedBack.size(), 10000);
    EXPECT_EQ(convertedBack[0], 0);
    EXPECT_EQ(convertedBack[9999], 9999);

    // Should complete in reasonable time (less than 1 second)
    EXPECT_LT(duration.count(), 1000);
}

// ============================================================================
// Type Registration Tests
// ============================================================================

TEST_F(TypeConverterTest, CustomTypeRegistration) {
    // Test registering custom type converters
    struct CustomType {
        int value;
        std::string name;
    };

    // Register custom converter
    converter_->registerConverter<CustomType>(
        [](const CustomType& obj) -> ScriptValue {
            std::unordered_map<std::string, ScriptValue> map;
            map["value"] = ScriptValue(obj.value);
            map["name"] = ScriptValue(obj.name);
            return ScriptValue(map);
        },
        [](const ScriptValue& script) -> CustomType {
            const auto& map =
                script.get<std::unordered_map<std::string, ScriptValue>>();
            CustomType obj;
            obj.value = map.at("value").get<int64_t>();
            obj.name = map.at("name").get<std::string>();
            return obj;
        });

    // Test custom type conversion
    CustomType original{42, "test"};
    auto scriptValue = converter_->fromNative(original);
    auto converted = converter_->toNative<CustomType>(scriptValue);

    EXPECT_EQ(converted.value, 42);
    EXPECT_EQ(converted.name, "test");
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(TypeConverterTest, ConcurrentConversion) {
    const int numThreads = 4;
    const int conversionsPerThread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, &successCount]() {
            for (int i = 0; i < conversionsPerThread; ++i) {
                try {
                    std::vector<int> testVector = {i, i + 1, i + 2};
                    auto scriptValue = converter_->fromNative(testVector);
                    auto convertedBack =
                        converter_->toNative<std::vector<int>>(scriptValue);

                    if (convertedBack.size() == 3 && convertedBack[0] == i) {
                        successCount++;
                    }
                } catch (...) {
                    // Handle any exceptions
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), numThreads * conversionsPerThread);
}
