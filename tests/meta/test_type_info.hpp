/*!
 * \file test_type_info.cpp
 * \brief Unit tests for TypeInfo functionality
 * \author GitHub Copilot
 * \date 2024-10-14
 */

#include <gtest/gtest.h>
#include "atom/meta/type_info.hpp"

#include <array>
#include <list>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace atom::meta::test {

// Test fixture for TypeInfo tests
class TypeInfoTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean registry before each test
        atom::meta::detail::TypeRegistry::getInstance().clear();
    }

    void TearDown() override {
        // Clean registry after each test
        atom::meta::detail::TypeRegistry::getInstance().clear();
    }
};

// Helper classes for testing
class EmptyClass {};

class SimpleClass {
public:
    int value;
    SimpleClass() : value(0) {}
    explicit SimpleClass(int v) : value(v) {}
};

class PolymorphicClass {
public:
    virtual ~PolymorphicClass() = default;
    virtual int getValue() const { return 0; }
};

class DerivedClass : public PolymorphicClass {
public:
    int getValue() const override { return 42; }
};

class FinalClass final {
public:
    int value;
};

class AbstractClass {
public:
    virtual ~AbstractClass() = default;
    virtual int pureVirtual() = 0;
};

enum class ScopedEnum { Value1, Value2 };
enum UnScopedEnum { Value1, Value2 };

// Test basic TypeInfo creation and properties
TEST_F(TypeInfoTest, BasicTypeInfo) {
    // Test fundamental types
    auto intInfo = userType<int>();
    EXPECT_EQ(intInfo.name(), "int");
    EXPECT_TRUE(intInfo.isArithmetic());
    EXPECT_FALSE(intInfo.isPointer());
    EXPECT_FALSE(intInfo.isClass());

    // Test reference types
    auto intRefInfo = userType<int&>();
    EXPECT_TRUE(intRefInfo.isReference());
    EXPECT_EQ(intRefInfo.bareName(), "int");

    // Test const types
    auto constIntInfo = userType<const int>();
    EXPECT_TRUE(constIntInfo.isConst());
    EXPECT_EQ(constIntInfo.bareName(), "int");

    // Test pointer types
    auto intPtrInfo = userType<int*>();
    EXPECT_TRUE(intPtrInfo.isPointer());
    EXPECT_EQ(intPtrInfo.bareName(), "int");

    // Test class types
    auto classInfo = userType<SimpleClass>();
    EXPECT_TRUE(classInfo.isClass());
    EXPECT_TRUE(classInfo.isDefaultConstructible());
    EXPECT_FALSE(classInfo.isPointer());
}

// Test TypeInfo traits for various types
TEST_F(TypeInfoTest, TypeTraits) {
    // Test empty class
    auto emptyInfo = userType<EmptyClass>();
    EXPECT_TRUE(emptyInfo.isEmpty());
    EXPECT_TRUE(emptyInfo.isPod());
    EXPECT_TRUE(emptyInfo.isStandardLayout());
    EXPECT_TRUE(emptyInfo.isTrivial());

    // Test polymorphic class
    auto polyInfo = userType<PolymorphicClass>();
    EXPECT_TRUE(polyInfo.isPolymorphic());
    EXPECT_FALSE(polyInfo.isPod());
    EXPECT_FALSE(polyInfo.isTrivial());
    EXPECT_FALSE(polyInfo.isFinal());

    // Test final class
    auto finalInfo = userType<FinalClass>();
    EXPECT_TRUE(finalInfo.isFinal());

    // Test abstract class
    auto abstractInfo = userType<AbstractClass>();
    EXPECT_TRUE(abstractInfo.isAbstract());
    EXPECT_TRUE(abstractInfo.isPolymorphic());

    // Test enums
    auto scopedEnumInfo = userType<ScopedEnum>();
    EXPECT_TRUE(scopedEnumInfo.isEnum());
    EXPECT_TRUE(scopedEnumInfo.isScopedEnum());

    auto unscopedEnumInfo = userType<UnScopedEnum>();
    EXPECT_TRUE(unscopedEnumInfo.isEnum());
    EXPECT_FALSE(unscopedEnumInfo.isScopedEnum());

    // Test arrays
    auto arrayInfo = userType<int[10]>();
    EXPECT_TRUE(arrayInfo.isArray());
    EXPECT_TRUE(arrayInfo.isBoundedArray());
    EXPECT_FALSE(arrayInfo.isUnboundedArray());

    // C++20 span
    std::array<int, 5> arr = {1, 2, 3, 4, 5};
    std::span<int> spanObj(arr);
    auto spanInfo = userType(spanObj);
    EXPECT_TRUE(spanInfo.isPointer());

    // Test function type
    using FuncType = int(int, int);
    auto funcInfo = userType<FuncType>();
    EXPECT_TRUE(funcInfo.isFunction());
}

// Test smart pointers
TEST_F(TypeInfoTest, SmartPointers) {
    // Shared pointer
    auto sharedPtrInfo = userType<std::shared_ptr<SimpleClass>>();
    EXPECT_TRUE(sharedPtrInfo.isPointer());
    EXPECT_EQ(sharedPtrInfo.bareName(), "std::shared_ptr<SimpleClass>");

    // Unique pointer
    auto uniquePtrInfo = userType<std::unique_ptr<SimpleClass>>();
    EXPECT_TRUE(uniquePtrInfo.isPointer());

    // Weak pointer
    auto weakPtrInfo = userType<std::weak_ptr<SimpleClass>>();
    EXPECT_TRUE(weakPtrInfo.isPointer());

    // Test with reference to smart pointer
    auto sharedPtrRefInfo = userType<std::shared_ptr<SimpleClass>&>();
    EXPECT_TRUE(sharedPtrRefInfo.isReference());
    EXPECT_TRUE(sharedPtrRefInfo.isPointer());

    // Test with const reference to smart pointer
    auto constSharedPtrRefInfo =
        userType<const std::shared_ptr<SimpleClass>&>();
    EXPECT_TRUE(constSharedPtrRefInfo.isReference());
    EXPECT_TRUE(constSharedPtrRefInfo.isPointer());
    EXPECT_TRUE(constSharedPtrRefInfo.isConst());
}

// Test fromInstance method
TEST_F(TypeInfoTest, FromInstance) {
    SimpleClass obj(42);
    auto info = TypeInfo::fromInstance(obj);
    EXPECT_EQ(info.name(), "SimpleClass");
    EXPECT_TRUE(info.isClass());
    EXPECT_FALSE(info.isPointer());

    SimpleClass* pObj = &obj;
    auto ptrInfo = TypeInfo::fromInstance(pObj);
    EXPECT_TRUE(ptrInfo.isPointer());

    const SimpleClass& refObj = obj;
    auto refInfo = TypeInfo::fromInstance(refObj);
    EXPECT_TRUE(refInfo.isConst());
    EXPECT_TRUE(refInfo.isReference());
}

// Test comparison operators
TEST_F(TypeInfoTest, Comparison) {
    auto intInfo = userType<int>();
    auto anotherIntInfo = userType<int>();
    auto doubleInfo = userType<double>();

    // Test equality
    EXPECT_EQ(intInfo, anotherIntInfo);
    EXPECT_NE(intInfo, doubleInfo);

    // Test less than (needed for ordered containers)
    bool lessThan = intInfo < doubleInfo || doubleInfo < intInfo;
    EXPECT_TRUE(lessThan);            // One must be less than the other
    EXPECT_FALSE(intInfo < intInfo);  // Not less than itself

    // Test bareEqual
    auto intPtrInfo = userType<int*>();
    EXPECT_TRUE(intInfo.bareEqual(intPtrInfo));
    EXPECT_FALSE(intInfo.bareEqual(doubleInfo));

    // Test bareEqualTypeInfo
    EXPECT_TRUE(intInfo.bareEqualTypeInfo(typeid(int)));
    EXPECT_FALSE(intInfo.bareEqualTypeInfo(typeid(double)));
}

// Test JSON serialization
TEST_F(TypeInfoTest, ToJson) {
    auto intInfo = userType<int>();
    std::string json = intInfo.toJson();

    // Check basic JSON structure
    EXPECT_TRUE(json.find("\"typeName\": \"int\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"bareTypeName\": \"int\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"traits\"") != std::string::npos);

    // Check specific traits
    EXPECT_TRUE(json.find("\"isArithmetic\": true") != std::string::npos);
    EXPECT_TRUE(json.find("\"isPointer\": false") != std::string::npos);

    // Test complex type
    auto classInfo = userType<SimpleClass>();
    std::string classJson = classInfo.toJson();
    EXPECT_TRUE(classJson.find("\"typeName\": \"SimpleClass\"") !=
                std::string::npos);
    EXPECT_TRUE(classJson.find("\"isClass\": true") != std::string::npos);
}

// Test type registry basic functionality
TEST_F(TypeInfoTest, TypeRegistry) {
    // Register types
    registerType<int>("Integer");
    registerType<SimpleClass>("Simple");

    // Check registration
    EXPECT_TRUE(isTypeRegistered("Integer"));
    EXPECT_TRUE(isTypeRegistered("Simple"));
    EXPECT_FALSE(isTypeRegistered("NotRegistered"));

    // Get registered type info
    auto intInfoOpt = getTypeInfo("Integer");
    EXPECT_TRUE(intInfoOpt.has_value());
    EXPECT_EQ(intInfoOpt->name(), "int");

    // Get registered type names
    auto names = getRegisteredTypeNames();
    EXPECT_EQ(names.size(), 2);
    EXPECT_TRUE(std::find(names.begin(), names.end(), "Integer") !=
                names.end());
    EXPECT_TRUE(std::find(names.begin(), names.end(), "Simple") != names.end());
}

// Test thread safety of type registry
TEST_F(TypeInfoTest, ThreadSafeRegistry) {
    constexpr int NUM_THREADS = 10;
    constexpr int TYPES_PER_THREAD = 100;

    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([t]() {
            for (int i = 0; i < TYPES_PER_THREAD; ++i) {
                std::string typeName =
                    "Type_" + std::to_string(t) + "_" + std::to_string(i);
                registerType<int>(typeName);

                // Do some reads as well
                [[maybe_unused]] auto exists = isTypeRegistered(typeName);
                [[maybe_unused]] auto typeInfo = getTypeInfo(typeName);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all types were registered
    int count = 0;
    for (int t = 0; t < NUM_THREADS; ++t) {
        for (int i = 0; i < TYPES_PER_THREAD; ++i) {
            std::string typeName =
                "Type_" + std::to_string(t) + "_" + std::to_string(i);
            if (isTypeRegistered(typeName)) {
                count++;
            }
        }
    }

    EXPECT_EQ(count, NUM_THREADS * TYPES_PER_THREAD);
}

// Test type compatibility checking
TEST_F(TypeInfoTest, TypeCompatibility) {
    // Same types are compatible
    EXPECT_TRUE((areTypesCompatible<int, int>()));

    // Convertible types are compatible
    EXPECT_TRUE((areTypesCompatible<int, double>()));
    EXPECT_TRUE((areTypesCompatible<double, int>()));

    // Inheritance relationship
    EXPECT_TRUE((areTypesCompatible<DerivedClass, PolymorphicClass>()));
    EXPECT_TRUE((areTypesCompatible<PolymorphicClass, DerivedClass>()));

    // Non-compatible types
    EXPECT_FALSE((areTypesCompatible<SimpleClass, int>()));

    // Pointer and reference compatibility
    EXPECT_TRUE((areTypesCompatible<int, int&>()));
    EXPECT_TRUE((areTypesCompatible<int*, int*>()));
    EXPECT_TRUE((areTypesCompatible<int*, const int*>()));
    EXPECT_FALSE((areTypesCompatible<int*, double*>()));
}

// Test type factory
TEST_F(TypeInfoTest, TypeFactory) {
    // Register a factory for SimpleClass
    TypeFactory::registerFactory<SimpleClass, SimpleClass>("Simple");

    // Create an instance
    auto instance = TypeFactory::createInstance<SimpleClass>("Simple");
    EXPECT_NE(instance, nullptr);
    EXPECT_EQ(instance->value, 0);  // Default constructed

    // Try to create an unregistered type
    auto nullInstance = TypeFactory::createInstance<SimpleClass>("Nonexistent");
    EXPECT_EQ(nullInstance, nullptr);

    // Register a derived class with base type
    TypeFactory::registerFactory<DerivedClass, PolymorphicClass>("Derived");

    // Create as base type
    auto baseInstance =
        TypeFactory::createInstance<PolymorphicClass>("Derived");
    EXPECT_NE(baseInstance, nullptr);
    EXPECT_EQ(baseInstance->getValue(),
              42);  // Should call derived implementation
}

// Test exception handling
TEST_F(TypeInfoTest, ExceptionHandling) {
    // Test TypeInfoException
    try {
        throw TypeInfoException("Test exception");
        FAIL() << "Should have thrown an exception";
    } catch (const TypeInfoException& e) {
        std::string message(e.what());
        EXPECT_TRUE(message.find("Test exception") != std::string::npos);
        EXPECT_TRUE(message.find("at ") !=
                    std::string::npos);  // Should contain location info
    }
}

// Test hash specialization
TEST_F(TypeInfoTest, HashFunction) {
    std::hash<TypeInfo> hasher;

    auto intInfo = userType<int>();
    auto doubleInfo = userType<double>();
    auto intPtrInfo = userType<int*>();

    // Different types should have different hashes
    EXPECT_NE(hasher(intInfo), hasher(doubleInfo));

    // Same types should have same hash
    auto anotherIntInfo = userType<int>();
    EXPECT_EQ(hasher(intInfo), hasher(anotherIntInfo));

    // Test with undefined TypeInfo
    TypeInfo undefInfo;
    EXPECT_EQ(hasher(undefInfo), 0);
}

// Test stream operator
TEST_F(TypeInfoTest, StreamOperator) {
    std::ostringstream ss;

    auto intInfo = userType<int>();
    ss << intInfo;
    EXPECT_EQ(ss.str(), "int");

    ss.str("");
    auto classInfo = userType<SimpleClass>();
    ss << classInfo;
    EXPECT_EQ(ss.str(), "SimpleClass");
}

// Test with span (C++20 feature)
TEST_F(TypeInfoTest, SpanSupport) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::span<int> dynamicSpan(vec);

    auto spanInfo = userType(dynamicSpan);
    EXPECT_TRUE(spanInfo.isPointer());

    // Test with fixed extent span
    std::span<int, 5> fixedSpan(vec);
    auto fixedSpanInfo = userType(fixedSpan);
    EXPECT_TRUE(fixedSpanInfo.isPointer());

    // Verify different extents create different types
    EXPECT_NE((userType<std::span<int, 5>>()),
              (userType<std::span<int, 10>>()));
}

// Test with complex types
TEST_F(TypeInfoTest, ComplexTypes) {
    // Test with container of containers
    using ComplexType = std::vector<std::list<std::string>>;

    auto complexInfo = userType<ComplexType>();
    EXPECT_TRUE(complexInfo.isClass());
    EXPECT_FALSE(complexInfo.isPod());

    // Test with function pointers
    using FuncPtr = int (*)(int, int);
    auto funcPtrInfo = userType<FuncPtr>();
    EXPECT_TRUE(funcPtrInfo.isPointer());

    // Test with function references
    using FuncRef = int (&)(int, int);
    auto funcRefInfo = userType<FuncRef>();
    EXPECT_TRUE(funcRefInfo.isReference());
}

// Test registering with custom TypeInfo
TEST_F(TypeInfoTest, RegisterCustomTypeInfo) {
    // Create a TypeInfo for int
    auto intInfo = userType<int>();

    // Register with a custom name
    registerType("CustomInt", intInfo);

    // Verify registration
    EXPECT_TRUE(isTypeRegistered("CustomInt"));

    auto retrievedInfo = getTypeInfo("CustomInt");
    EXPECT_TRUE(retrievedInfo.has_value());
    EXPECT_EQ(*retrievedInfo, intInfo);
}

}  // namespace atom::meta::test

// Main function to run the tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}