#include <gtest/gtest.h>
#include "atom/meta/any.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

// Test fixture for BoxedValue tests
class BoxedValueTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Helper classes for testing
struct TestStruct {
    int value;
    std::string name;
    
    TestStruct() : value(0), name("default") {}
    TestStruct(int v, const std::string& n) : value(v), name(n) {}
    
    bool operator==(const TestStruct& other) const {
        return value == other.value && name == other.name;
    }
};

class TestClass {
public:
    TestClass() : data_(42) {}
    explicit TestClass(int data) : data_(data) {}
    
    int getData() const { return data_; }
    void setData(int data) { data_ = data; }
    
private:
    int data_;
};

// Test basic construction and type checking
TEST_F(BoxedValueTest, BasicConstruction) {
    // Default constructor (VoidType)
    atom::meta::BoxedValue voidValue;
    EXPECT_TRUE(voidValue.isVoid());
    EXPECT_TRUE(voidValue.isUndef());
    EXPECT_FALSE(voidValue.isNull());
    
    // Integer construction
    atom::meta::BoxedValue intValue(42);
    EXPECT_FALSE(intValue.isVoid());
    EXPECT_FALSE(intValue.isUndef());
    EXPECT_FALSE(intValue.isNull());
    EXPECT_TRUE(intValue.isType<int>());
    
    // String construction
    atom::meta::BoxedValue stringValue(std::string("test"));
    EXPECT_TRUE(stringValue.isType<std::string>());
    EXPECT_FALSE(stringValue.isType<int>());
    
    // Double construction
    atom::meta::BoxedValue doubleValue(3.14);
    EXPECT_TRUE(doubleValue.isType<double>());
}

// Test copy and move semantics
TEST_F(BoxedValueTest, CopyMoveSemantics) {
    atom::meta::BoxedValue original(42);
    
    // Copy constructor
    atom::meta::BoxedValue copied(original);
    EXPECT_TRUE(copied.isType<int>());
    EXPECT_EQ(copied.cast<int>(), 42);
    
    // Move constructor
    atom::meta::BoxedValue moved(std::move(original));
    EXPECT_TRUE(moved.isType<int>());
    EXPECT_EQ(moved.cast<int>(), 42);
    
    // Copy assignment
    atom::meta::BoxedValue copyAssigned;
    copyAssigned = copied;
    EXPECT_TRUE(copyAssigned.isType<int>());
    EXPECT_EQ(copyAssigned.cast<int>(), 42);
    
    // Move assignment
    atom::meta::BoxedValue moveAssigned;
    moveAssigned = std::move(copied);
    EXPECT_TRUE(moveAssigned.isType<int>());
    EXPECT_EQ(moveAssigned.cast<int>(), 42);
}

// Test type casting functionality
TEST_F(BoxedValueTest, TypeCasting) {
    atom::meta::BoxedValue intValue(42);
    
    // Successful cast
    EXPECT_EQ(intValue.cast<int>(), 42);
    
    // Try cast with correct type
    auto tryResult = intValue.tryCast<int>();
    ASSERT_TRUE(tryResult.has_value());
    EXPECT_EQ(tryResult.value(), 42);
    
    // Try cast with incorrect type
    auto tryResultWrong = intValue.tryCast<std::string>();
    EXPECT_FALSE(tryResultWrong.has_value());
    
    // Cast should throw for wrong type
    EXPECT_THROW(intValue.cast<std::string>(), std::bad_any_cast);
}

// Test const value handling
TEST_F(BoxedValueTest, ConstValues) {
    const int constInt = 100;
    atom::meta::BoxedValue constValue(constInt);
    
    EXPECT_TRUE(constValue.isReadonly());
    EXPECT_TRUE(constValue.isType<int>());
    EXPECT_EQ(constValue.cast<int>(), 100);
}

// Test reference handling
TEST_F(BoxedValueTest, ReferenceHandling) {
    int original = 42;
    atom::meta::BoxedValue refValue(std::ref(original));
    
    EXPECT_TRUE(refValue.isRef());
    EXPECT_TRUE(refValue.isType<int>());
    
    // Modify original and check if reference reflects the change
    original = 100;
    // Note: This behavior depends on implementation details
}

// Test attribute system
TEST_F(BoxedValueTest, AttributeSystem) {
    atom::meta::BoxedValue value(42);
    
    // Set attributes
    value.setAttr("description", atom::meta::BoxedValue(std::string("test integer")));
    value.setAttr("category", atom::meta::BoxedValue(std::string("number")));
    
    // Check if attributes exist
    EXPECT_TRUE(value.hasAttr("description"));
    EXPECT_TRUE(value.hasAttr("category"));
    EXPECT_FALSE(value.hasAttr("nonexistent"));
    
    // Get attributes
    auto descAttr = value.getAttr("description");
    EXPECT_TRUE(descAttr.isType<std::string>());
    EXPECT_EQ(descAttr.cast<std::string>(), "test integer");
    
    // Get non-existent attribute should return void
    auto nonExistentAttr = value.getAttr("nonexistent");
    EXPECT_TRUE(nonExistentAttr.isVoid());
}

// Test complex types
TEST_F(BoxedValueTest, ComplexTypes) {
    TestStruct testStruct(42, "test");
    atom::meta::BoxedValue structValue(testStruct);
    
    EXPECT_TRUE(structValue.isType<TestStruct>());
    
    auto retrievedStruct = structValue.cast<TestStruct>();
    EXPECT_EQ(retrievedStruct.value, 42);
    EXPECT_EQ(retrievedStruct.name, "test");
}

// Test container types
TEST_F(BoxedValueTest, ContainerTypes) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    atom::meta::BoxedValue vecValue(vec);
    
    EXPECT_TRUE(vecValue.isType<std::vector<int>>());
    
    auto retrievedVec = vecValue.cast<std::vector<int>>();
    EXPECT_EQ(retrievedVec.size(), 5);
    EXPECT_EQ(retrievedVec[0], 1);
    EXPECT_EQ(retrievedVec[4], 5);
}

// Test smart pointers
TEST_F(BoxedValueTest, SmartPointers) {
    auto sharedPtr = std::make_shared<TestClass>(100);
    atom::meta::BoxedValue ptrValue(sharedPtr);
    
    EXPECT_TRUE(ptrValue.isType<std::shared_ptr<TestClass>>());
    
    auto retrievedPtr = ptrValue.cast<std::shared_ptr<TestClass>>();
    EXPECT_EQ(retrievedPtr->getData(), 100);
}

// Test swap functionality
TEST_F(BoxedValueTest, SwapFunctionality) {
    atom::meta::BoxedValue value1(42);
    atom::meta::BoxedValue value2(std::string("test"));
    
    value1.swap(value2);
    
    EXPECT_TRUE(value1.isType<std::string>());
    EXPECT_TRUE(value2.isType<int>());
    EXPECT_EQ(value1.cast<std::string>(), "test");
    EXPECT_EQ(value2.cast<int>(), 42);
}

// Test debug string functionality
TEST_F(BoxedValueTest, DebugString) {
    atom::meta::BoxedValue intValue(42);
    std::string debugStr = intValue.debugString();
    EXPECT_FALSE(debugStr.empty());
    EXPECT_TRUE(debugStr.find("42") != std::string::npos);
    
    atom::meta::BoxedValue stringValue(std::string("test"));
    std::string stringDebugStr = stringValue.debugString();
    EXPECT_FALSE(stringDebugStr.empty());
    EXPECT_TRUE(stringDebugStr.find("test") != std::string::npos);
}

// Test visitor pattern
TEST_F(BoxedValueTest, VisitorPattern) {
    atom::meta::BoxedValue intValue(42);

    // Test const visitor
    auto result = intValue.visit([](const auto& value) -> int {
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, int>) {
            return value * 2;
        }
        return 0;
    });
    EXPECT_EQ(result, 84);

    // Test non-const visitor (modifying)
    atom::meta::BoxedValue mutableValue(10);
    mutableValue.visit([](auto& value) {
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, int>) {
            value *= 3;
        }
    });
    EXPECT_EQ(mutableValue.cast<int>(), 30);
}

// Test thread safety
TEST_F(BoxedValueTest, ThreadSafety) {
    atom::meta::BoxedValue sharedValue(0);
    constexpr int numThreads = 10;
    constexpr int incrementsPerThread = 100;

    std::vector<std::thread> threads;
    std::atomic<int> completedThreads(0);

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&sharedValue, &completedThreads, incrementsPerThread]() {
            for (int j = 0; j < incrementsPerThread; ++j) {
                sharedValue.visit([](auto& value) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(value)>, int>) {
                        ++value;
                    }
                });
            }
            completedThreads.fetch_add(1);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(completedThreads.load(), numThreads);
    EXPECT_EQ(sharedValue.cast<int>(), numThreads * incrementsPerThread);
}

// Test error handling
TEST_F(BoxedValueTest, ErrorHandling) {
    atom::meta::BoxedValue voidValue;

    // Casting void should throw
    EXPECT_THROW(voidValue.cast<int>(), std::bad_any_cast);

    // Try cast on void should return nullopt
    auto tryResult = voidValue.tryCast<int>();
    EXPECT_FALSE(tryResult.has_value());

    // Visiting void with fallback
    auto result = voidValue.visit([](const auto& value) -> int {
        return 42;
    });
    // Result depends on implementation - might be default constructed or throw
}

// Test assignment operators
TEST_F(BoxedValueTest, AssignmentOperators) {
    atom::meta::BoxedValue value;

    // Assign different types
    value = 42;
    EXPECT_TRUE(value.isType<int>());
    EXPECT_EQ(value.cast<int>(), 42);

    value = std::string("test");
    EXPECT_TRUE(value.isType<std::string>());
    EXPECT_EQ(value.cast<std::string>(), "test");

    value = 3.14;
    EXPECT_TRUE(value.isType<double>());
    EXPECT_EQ(value.cast<double>(), 3.14);
}

// Test helper functions
TEST_F(BoxedValueTest, HelperFunctions) {
    // Test var() helper
    auto varValue = atom::meta::var(42);
    EXPECT_TRUE(varValue.isType<int>());
    EXPECT_EQ(varValue.cast<int>(), 42);

    // Test constVar() helper
    int constInt = 100;
    auto constVarValue = atom::meta::constVar(constInt);
    EXPECT_TRUE(constVarValue.isReadonly());

    // Test voidVar() helper
    auto voidVarValue = atom::meta::voidVar();
    EXPECT_TRUE(voidVarValue.isVoid());

    // Test varWithDesc() helper
    auto descValue = atom::meta::varWithDesc(42, "test integer");
    EXPECT_TRUE(descValue.isType<int>());
    EXPECT_TRUE(descValue.hasAttr("description"));

    // Test makeBoxedValue() helper
    auto madeValue = atom::meta::makeBoxedValue(42, false, true);
    EXPECT_TRUE(madeValue.isType<int>());
    EXPECT_TRUE(madeValue.isReadonly());
}

// Test edge cases
TEST_F(BoxedValueTest, EdgeCases) {
    // Test with nullptr
    atom::meta::BoxedValue nullptrValue(nullptr);
    EXPECT_TRUE(nullptrValue.isType<std::nullptr_t>());

    // Test with function pointer
    auto funcPtr = [](int x) { return x * 2; };
    atom::meta::BoxedValue funcValue(funcPtr);
    EXPECT_TRUE(funcValue.isType<decltype(funcPtr)>());

    // Test with array
    int arr[5] = {1, 2, 3, 4, 5};
    atom::meta::BoxedValue arrValue(arr);
    // Array decays to pointer
    EXPECT_TRUE(arrValue.isType<int*>());

    // Test with enum
    enum class TestEnum { Value1, Value2, Value3 };
    atom::meta::BoxedValue enumValue(TestEnum::Value2);
    EXPECT_TRUE(enumValue.isType<TestEnum>());
}

// Test memory management
TEST_F(BoxedValueTest, MemoryManagement) {
    // Test with large object
    std::vector<int> largeVec(10000, 42);
    atom::meta::BoxedValue largeValue(largeVec);
    EXPECT_TRUE(largeValue.isType<std::vector<int>>());

    // Test copy doesn't share memory
    atom::meta::BoxedValue copied(largeValue);
    auto& originalVec = largeValue.cast<std::vector<int>>();
    auto& copiedVec = copied.cast<std::vector<int>>();

    // Modify original
    originalVec[0] = 100;
    EXPECT_EQ(originalVec[0], 100);
    EXPECT_EQ(copiedVec[0], 42);  // Copy should be unchanged
}

// Test type information
TEST_F(BoxedValueTest, TypeInformation) {
    atom::meta::BoxedValue intValue(42);

    // Test type checking
    EXPECT_TRUE(intValue.isType<int>());
    EXPECT_FALSE(intValue.isType<double>());
    EXPECT_FALSE(intValue.isType<std::string>());

    // Test with const types
    const double constDouble = 3.14;
    atom::meta::BoxedValue constValue(constDouble);
    EXPECT_TRUE(constValue.isType<double>());
    EXPECT_TRUE(constValue.isReadonly());
}

}  // namespace
