// filepath: /home/max/Atom-1/atom/meta/test_vany.hpp
#ifndef ATOM_META_TEST_VANY_HPP
#define ATOM_META_TEST_VANY_HPP

#include <gtest/gtest.h>
#include "atom/meta/vany.hpp"

#include <algorithm>
#include <list>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace atom::meta::test {

// Test fixture for the Any class
class AnyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    // Helper types for testing
    struct SimpleTestType {
        int value = 42;
        SimpleTestType() = default;
        explicit SimpleTestType(int v) : value(v) {}
        bool operator==(const SimpleTestType& other) const {
            return value == other.value;
        }
    };

    struct ComplexTestType {
        std::string name;
        std::vector<int> data;

        ComplexTestType(std::string n, std::vector<int> d)
            : name(std::move(n)), data(std::move(d)) {}

        bool operator==(const ComplexTestType& other) const {
            return name == other.name && data == other.data;
        }

        friend std::ostream& operator<<(std::ostream& os,
                                        const ComplexTestType& obj) {
            os << "ComplexTestType{name=" << obj.name << ", data=[";
            for (size_t i = 0; i < obj.data.size(); ++i) {
                if (i > 0)
                    os << ", ";
                os << obj.data[i];
            }
            os << "]}";
            return os;
        }
    };

    // Non-copyable type to test move semantics
    struct MoveOnlyType {
        std::unique_ptr<int> ptr;

        MoveOnlyType() : ptr(std::make_unique<int>(42)) {}
        explicit MoveOnlyType(int value) : ptr(std::make_unique<int>(value)) {}

        MoveOnlyType(const MoveOnlyType&) = delete;
        MoveOnlyType& operator=(const MoveOnlyType&) = delete;

        MoveOnlyType(MoveOnlyType&&) noexcept = default;
        MoveOnlyType& operator=(MoveOnlyType&&) noexcept = default;

        int getValue() const { return ptr ? *ptr : -1; }
    };

    // Large type that won't fit in the small buffer
    struct LargeType {
        std::array<char, 1024> data;
        int value;

        LargeType() : value(0) { std::fill(data.begin(), data.end(), 'X'); }

        explicit LargeType(int val) : value(val) {
            std::fill(data.begin(), data.end(), 'X');
        }

        bool operator==(const LargeType& other) const {
            return value == other.value;
        }
    };

    // Type with custom hash and equality operator
    struct HashableType {
        int key;
        std::string value;

        HashableType(int k, std::string v) : key(k), value(std::move(v)) {}

        bool operator==(const HashableType& other) const {
            return key == other.key && value == other.value;
        }
    };
};

// Test default constructor
TEST_F(AnyTest, DefaultConstructor) {
    Any any;
    // An empty Any should have a null vtable pointer
    EXPECT_EQ(any.vptr_, nullptr);
    EXPECT_TRUE(any.is_small_);
}

// Test constructing with different types
TEST_F(AnyTest, ConstructWithValue) {
    // Test with a simple int
    {
        Any any(42);
        EXPECT_NE(any.vptr_, nullptr);
        EXPECT_TRUE(any.is_small_);
    }

    // Test with a string
    {
        Any any(std::string("Hello, world!"));
        EXPECT_NE(any.vptr_, nullptr);
        // String is likely to be small enough for small buffer optimization
        EXPECT_TRUE(any.is_small_);
    }

    // Test with a complex type
    {
        ComplexTestType complex("Test", {1, 2, 3});
        Any any(complex);
        EXPECT_NE(any.vptr_, nullptr);
    }

    // Test with a large type
    {
        LargeType large(100);
        Any any(large);
        EXPECT_NE(any.vptr_, nullptr);
        EXPECT_FALSE(
            any.is_small_);  // Should not use small buffer optimization
    }

    // Test with move-only type
    {
        MoveOnlyType moveOnly(123);
        Any any(std::move(moveOnly));
        EXPECT_NE(any.vptr_, nullptr);
    }
}

// Test copy constructor
TEST_F(AnyTest, CopyConstructor) {
    // Create an Any with a simple value
    Any original(42);

    // Copy it
    Any copy(original);

    // Check that both contain the same value
    EXPECT_NE(copy.vptr_, nullptr);
    EXPECT_TRUE(copy.is_small_);
    EXPECT_EQ(*copy.as<int>(), 42);

    // Test copying large objects
    {
        LargeType large(200);
        Any originalLarge(large);
        Any copyLarge(originalLarge);

        EXPECT_FALSE(copyLarge.is_small_);
        EXPECT_EQ(copyLarge.as<LargeType>()->value, 200);
    }

    // Test copying complex objects
    {
        ComplexTestType complex("Complex", {4, 5, 6});
        Any originalComplex(complex);
        Any copyComplex(originalComplex);

        EXPECT_EQ(copyComplex.as<ComplexTestType>()->name, "Complex");
        EXPECT_EQ(copyComplex.as<ComplexTestType>()->data,
                  std::vector<int>({4, 5, 6}));
    }
}

// Test move constructor
TEST_F(AnyTest, MoveConstructor) {
    // Test with small object
    {
        Any original(42);
        Any moved(std::move(original));

        EXPECT_NE(moved.vptr_, nullptr);
        EXPECT_TRUE(moved.is_small_);
        EXPECT_EQ(*moved.as<int>(), 42);

        // Original should be reset
        EXPECT_EQ(original.vptr_, nullptr);
    }

    // Test with large object
    {
        LargeType large(300);
        Any originalLarge(large);
        void* originalPtr = originalLarge.ptr;  // Store original pointer

        Any movedLarge(std::move(originalLarge));

        EXPECT_FALSE(movedLarge.is_small_);
        EXPECT_EQ(movedLarge.ptr, originalPtr);  // Pointer should be moved
        EXPECT_EQ(movedLarge.as<LargeType>()->value, 300);

        // Original should be reset
        EXPECT_EQ(originalLarge.vptr_, nullptr);
        EXPECT_EQ(originalLarge.ptr, nullptr);
    }

    // Test with move-only type
    {
        MoveOnlyType moveOnly(456);
        Any original(std::move(moveOnly));
        Any moved(std::move(original));

        EXPECT_NE(moved.vptr_, nullptr);
        EXPECT_EQ(moved.as<MoveOnlyType>()->getValue(), 456);

        // Original should be reset
        EXPECT_EQ(original.vptr_, nullptr);
    }
}

// Test copy assignment operator
TEST_F(AnyTest, CopyAssignment) {
    // Create an Any with a simple value
    Any original(42);

    // Empty Any assigned from original
    Any empty;
    empty = original;

    EXPECT_NE(empty.vptr_, nullptr);
    EXPECT_TRUE(empty.is_small_);
    EXPECT_EQ(*empty.as<int>(), 42);

    // Assign over existing value
    Any string(std::string("Hello"));
    string = original;

    EXPECT_NE(string.vptr_, nullptr);
    EXPECT_TRUE(string.is_small_);
    EXPECT_EQ(*string.as<int>(), 42);

    // Self-assignment
    original = original;
    EXPECT_EQ(*original.as<int>(), 42);

    // Complex to complex assignment
    ComplexTestType complex1("First", {1, 2, 3});
    ComplexTestType complex2("Second", {4, 5, 6});

    Any anyComplex1(complex1);
    Any anyComplex2(complex2);

    anyComplex1 = anyComplex2;
    EXPECT_EQ(anyComplex1.as<ComplexTestType>()->name, "Second");
}

// Test move assignment operator
TEST_F(AnyTest, MoveAssignment) {
    // Test with small object
    {
        Any original(42);
        Any target;
        target = std::move(original);

        EXPECT_NE(target.vptr_, nullptr);
        EXPECT_TRUE(target.is_small_);
        EXPECT_EQ(*target.as<int>(), 42);

        // Original should be reset
        EXPECT_EQ(original.vptr_, nullptr);
    }

    // Test with large object
    {
        LargeType large(300);
        Any originalLarge(large);
        void* originalPtr = originalLarge.ptr;

        Any targetLarge;
        targetLarge = std::move(originalLarge);

        EXPECT_FALSE(targetLarge.is_small_);
        EXPECT_EQ(targetLarge.ptr, originalPtr);
        EXPECT_EQ(targetLarge.as<LargeType>()->value, 300);

        // Original should be reset
        EXPECT_EQ(originalLarge.vptr_, nullptr);
        EXPECT_EQ(originalLarge.ptr, nullptr);
    }

    // Test assigning to an Any that already has a value
    {
        Any source(123);
        Any target(std::string("Target"));

        target = std::move(source);
        EXPECT_NE(target.vptr_, nullptr);
        EXPECT_EQ(*target.as<int>(), 123);

        EXPECT_EQ(source.vptr_, nullptr);
    }
}

// Test direct value assignment
TEST_F(AnyTest, ValueAssignment) {
    Any any;

    // Assign int
    any = 42;
    EXPECT_NE(any.vptr_, nullptr);
    EXPECT_TRUE(any.is_small_);
    EXPECT_EQ(*any.as<int>(), 42);

    // Reassign to different type
    any = std::string("Hello");
    EXPECT_NE(any.vptr_, nullptr);
    EXPECT_EQ(*any.as<std::string>(), "Hello");

    // Assign complex type
    ComplexTestType complex("Complex", {7, 8, 9});
    any = complex;
    EXPECT_EQ(any.as<ComplexTestType>()->name, "Complex");

    // Assign large type
    LargeType large(400);
    any = large;
    EXPECT_FALSE(any.is_small_);
    EXPECT_EQ(any.as<LargeType>()->value, 400);

    // Assign move-only type
    MoveOnlyType moveOnly(789);
    any = std::move(moveOnly);
    EXPECT_EQ(any.as<MoveOnlyType>()->getValue(), 789);
}

// Test swap operation
TEST_F(AnyTest, Swap) {
    // Swap small objects
    {
        Any a1(42);
        Any a2(std::string("Hello"));

        a1.swap(a2);

        EXPECT_EQ(*a1.as<std::string>(), "Hello");
        EXPECT_EQ(*a2.as<int>(), 42);
    }

    // Swap large and small objects
    {
        LargeType large(500);
        Any a1(large);
        Any a2(123);

        a1.swap(a2);

        EXPECT_TRUE(a1.is_small_);
        EXPECT_EQ(*a1.as<int>(), 123);

        EXPECT_FALSE(a2.is_small_);
        EXPECT_EQ(a2.as<LargeType>()->value, 500);
    }

    // Swap with empty Any
    {
        Any a1(42);
        Any empty;

        a1.swap(empty);

        EXPECT_EQ(a1.vptr_, nullptr);  // a1 is now empty
        EXPECT_NE(empty.vptr_, nullptr);
        EXPECT_EQ(*empty.as<int>(), 42);

        // Swap back
        empty.swap(a1);
        EXPECT_EQ(empty.vptr_, nullptr);
        EXPECT_EQ(*a1.as<int>(), 42);
    }

    // Self swap
    {
        Any any(42);
        any.swap(any);
        EXPECT_EQ(*any.as<int>(), 42);  // Should remain the same
    }
}

// Test small buffer optimization
TEST_F(AnyTest, SmallBufferOptimization) {
    // Test types that should use small buffer
    EXPECT_TRUE((Any::kIsSmallObject<int>));
    EXPECT_TRUE((Any::kIsSmallObject<double>));
    EXPECT_TRUE((Any::kIsSmallObject<SimpleTestType>));

    // String might use small buffer depending on implementation
    // This demonstrates dependency on platform and implementation
    bool stringIsSmall = (sizeof(std::string) <= Any::kSmallObjectSize);
    Any anyString(std::string("Test"));
    EXPECT_EQ(anyString.is_small_, stringIsSmall);

    // Test types that should not use small buffer
    EXPECT_FALSE((Any::kIsSmallObject<LargeType>));

    // Verify large type storage
    Any anyLarge(LargeType(600));
    EXPECT_FALSE(anyLarge.is_small_);

    // Verify small type storage
    Any anySmall(SimpleTestType(777));
    EXPECT_TRUE(anySmall.is_small_);
}

// Test memory management
TEST_F(AnyTest, MemoryManagement) {
    // Track construction/destruction
    static int constructCount = 0;
    static int destructCount = 0;

    struct TrackingType {
        TrackingType() { constructCount++; }
        TrackingType(const TrackingType&) { constructCount++; }
        TrackingType(TrackingType&&) noexcept { constructCount++; }
        ~TrackingType() { destructCount++; }
    };

    constructCount = 0;
    destructCount = 0;

    {
        // Create and destroy Any with tracking type
        Any any(TrackingType{});
        EXPECT_EQ(constructCount, 2);  // Original + copy into Any
        EXPECT_EQ(destructCount, 1);   // Original destroyed
    }
    // After Any goes out of scope
    EXPECT_EQ(destructCount, 2);  // Both original and Any's copy destroyed

    // Test with multiple Any objects
    constructCount = 0;
    destructCount = 0;

    {
        Any any1(TrackingType{});
        EXPECT_EQ(constructCount, 2);
        EXPECT_EQ(destructCount, 1);

        Any any2(any1);  // Copy
        EXPECT_EQ(constructCount, 3);
        EXPECT_EQ(destructCount, 1);

        Any any3(std::move(any1));  // Move
        EXPECT_EQ(constructCount,
                  4);  // Move constructor still creates a new object
        EXPECT_EQ(destructCount, 1);
    }
    // All destroyed
    EXPECT_EQ(destructCount, 4);
}

// Test with edge cases
TEST_F(AnyTest, EdgeCases) {
    // Empty Any
    Any empty;
    EXPECT_EQ(empty.vptr_, nullptr);

    // Copy empty Any
    Any copyEmpty(empty);
    EXPECT_EQ(copyEmpty.vptr_, nullptr);

    // Move empty Any
    Any moveEmpty(std::move(empty));
    EXPECT_EQ(moveEmpty.vptr_, nullptr);

    // Assign from empty
    Any any(42);
    any = empty;
    EXPECT_EQ(any.vptr_, nullptr);

    // Swap with empty
    Any value(43);
    value.swap(empty);
    EXPECT_EQ(value.vptr_, nullptr);
    EXPECT_NE(empty.vptr_, nullptr);
    EXPECT_EQ(*empty.as<int>(), 43);
}

// Test with complex scenarios
TEST_F(AnyTest, ComplexScenarios) {
    // Create vector of Any objects
    std::vector<Any> anyVector;
    anyVector.emplace_back(42);
    anyVector.emplace_back(std::string("Hello"));
    anyVector.emplace_back(ComplexTestType("Vector", {10, 11, 12}));
    anyVector.emplace_back();  // Empty Any

    LargeType large(700);
    anyVector.emplace_back(large);

    // Verify each element
    EXPECT_EQ(*anyVector[0].as<int>(), 42);
    EXPECT_EQ(*anyVector[1].as<std::string>(), "Hello");
    EXPECT_EQ(anyVector[2].as<ComplexTestType>()->name, "Vector");
    EXPECT_EQ(anyVector[3].vptr_, nullptr);  // Empty
    EXPECT_EQ(anyVector[4].as<LargeType>()->value, 700);

    // Test nested Any objects
    Any outer;
    {
        Any inner(std::string("Nested"));
        outer = inner;
    }
    // Inner is destroyed, but outer should still have a valid copy
    EXPECT_EQ(*outer.as<std::string>(), "Nested");
}

// Test to_string behavior
TEST_F(AnyTest, ToString) {
    // Set up test objects
    Any anyInt(42);
    Any anyString(std::string("Test String"));
    Any anyComplex(ComplexTestType("ToString", {1, 2, 3}));

    // Test using defaultToString via vtable
    EXPECT_EQ(anyInt.vptr_->toString(anyInt.getPtr()), "42");
    EXPECT_EQ(anyString.vptr_->toString(anyString.getPtr()), "Test String");

    // Complex should use operator<< implementation
    std::string complexStr = anyComplex.vptr_->toString(anyComplex.getPtr());
    EXPECT_TRUE(complexStr.find("ToString") != std::string::npos);
    EXPECT_TRUE(complexStr.find("1, 2, 3") != std::string::npos);
}

// Test equality comparison
TEST_F(AnyTest, EqualsAndHash) {
    // Set up test objects
    HashableType hash1(1, "One");
    HashableType hash2(1, "One");  // Same values
    HashableType hash3(2, "Two");  // Different values

    Any any1(hash1);
    Any any2(hash2);
    Any any3(hash3);

    // Test equals via vtable
    EXPECT_TRUE(any1.vptr_->equals(any1.getPtr(), any2.getPtr()));
    EXPECT_FALSE(any1.vptr_->equals(any1.getPtr(), any3.getPtr()));

    // Test hash via vtable
    EXPECT_EQ(any1.vptr_->hash(any1.getPtr()), any2.vptr_->hash(any2.getPtr()));
    EXPECT_NE(any1.vptr_->hash(any1.getPtr()), any3.vptr_->hash(any3.getPtr()));
}

// Test foreach implementation
TEST_F(AnyTest, Foreach) {
    // Create a collection
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    Any anyVector(numbers);

    std::vector<int> collected;
    std::function<void(const Any&)> collector = [&collected](const Any& item) {
        collected.push_back(*item.as<int>());
    };

    // Use foreach via vtable
    anyVector.vptr_->foreach (anyVector.getPtr(), collector);

    EXPECT_EQ(collected, numbers);

    // Test with non-iterable type
    Any anyInt(42);
    EXPECT_THROW(anyInt.vptr_->foreach (anyInt.getPtr(), collector),
                 atom::error::InvalidArgument);
}

// Test invoke implementation
TEST_F(AnyTest, Invoke) {
    // Test basic invoke functionality
    Any anyInt(42);
    bool invoked = false;
    int invokedValue = 0;

    std::function<void(const void*)> invoker =
        [&invoked, &invokedValue](const void* ptr) {
            invoked = true;
            invokedValue = *static_cast<const int*>(ptr);
        };

    // Use invoke via vtable
    anyInt.vptr_->invoke(anyInt.getPtr(), invoker);

    EXPECT_TRUE(invoked);
    EXPECT_EQ(invokedValue, 42);
}

// Test type info access
TEST_F(AnyTest, TypeInfo) {
    // Test type info for various types
    Any anyInt(42);
    Any anyString(std::string("Type"));
    Any anyComplex(ComplexTestType("Type", {1}));

    // Access type info via vtable
    EXPECT_EQ(anyInt.vptr_->type(), typeid(int));
    EXPECT_EQ(anyString.vptr_->type(), typeid(std::string));
    EXPECT_EQ(anyComplex.vptr_->type(), typeid(ComplexTestType));

    // Check size via vtable
    EXPECT_EQ(anyInt.vptr_->size(), sizeof(int));
    EXPECT_EQ(anyString.vptr_->size(), sizeof(std::string));
    EXPECT_EQ(anyComplex.vptr_->size(), sizeof(ComplexTestType));
}

}  // namespace atom::meta::test

#endif  // ATOM_META_TEST_VANY_HPP
