#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <type_traits>
#include <utility>

#include "atom/type/noncopyable.hpp"

// Test fixture for NonCopyable tests
class NonCopyableTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test class that inherits from NonCopyable
class TestNonCopyable : public NonCopyable {
public:
    TestNonCopyable() : value_(0) {}
    explicit TestNonCopyable(int value) : value_(value) {}

    int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }

private:
    int value_;
};

// Test class with virtual destructor
class VirtualTestNonCopyable : public NonCopyable {
public:
    VirtualTestNonCopyable() : value_(0) {}
    explicit VirtualTestNonCopyable(int value) : value_(value) {}
    virtual ~VirtualTestNonCopyable() = default;

    virtual int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }

private:
    int value_;
};

// Derived class for polymorphism tests
class DerivedTestNonCopyable : public VirtualTestNonCopyable {
public:
    DerivedTestNonCopyable() : VirtualTestNonCopyable(100) {}
    explicit DerivedTestNonCopyable(int value) : VirtualTestNonCopyable(value) {}

    int getValue() const override { return VirtualTestNonCopyable::getValue() * 2; }
};

// Basic Construction Tests
TEST_F(NonCopyableTest, DefaultConstruction) {
    TestNonCopyable obj;
    EXPECT_EQ(obj.getValue(), 0);
}

TEST_F(NonCopyableTest, ParameterizedConstruction) {
    TestNonCopyable obj(42);
    EXPECT_EQ(obj.getValue(), 42);
}

TEST_F(NonCopyableTest, VirtualDestructor) {
    // Test that virtual destructor works correctly
    auto obj = std::make_unique<VirtualTestNonCopyable>(123);
    EXPECT_EQ(obj->getValue(), 123);

    // Test polymorphic destruction
    std::unique_ptr<VirtualTestNonCopyable> derived =
        std::make_unique<DerivedTestNonCopyable>(50);
    EXPECT_EQ(derived->getValue(), 100); // 50 * 2
}

// Copy Prevention Tests
TEST_F(NonCopyableTest, CopyConstructorDeleted) {
    // Test that copy constructor is deleted
    static_assert(!std::is_copy_constructible_v<TestNonCopyable>);
    static_assert(!std::is_copy_constructible_v<VirtualTestNonCopyable>);
    static_assert(!std::is_copy_constructible_v<DerivedTestNonCopyable>);
}

TEST_F(NonCopyableTest, CopyAssignmentDeleted) {
    // Test that copy assignment is deleted
    static_assert(!std::is_copy_assignable_v<TestNonCopyable>);
    static_assert(!std::is_copy_assignable_v<VirtualTestNonCopyable>);
    static_assert(!std::is_copy_assignable_v<DerivedTestNonCopyable>);
}

// Move Semantics Tests
TEST_F(NonCopyableTest, MoveConstructible) {
    // Test that move constructor is available for basic NonCopyable
    static_assert(std::is_move_constructible_v<TestNonCopyable>);

    // Note: Classes with virtual destructors may not be move constructible
    // unless explicitly defined, which is expected behavior
    // static_assert(std::is_move_constructible_v<VirtualTestNonCopyable>);
    // static_assert(std::is_move_constructible_v<DerivedTestNonCopyable>);
}

TEST_F(NonCopyableTest, MoveAssignable) {
    // Test that move assignment is available for basic NonCopyable
    static_assert(std::is_move_assignable_v<TestNonCopyable>);

    // Note: Classes with virtual destructors may not be move assignable
    // unless explicitly defined, which is expected behavior
    // static_assert(std::is_move_assignable_v<VirtualTestNonCopyable>);
    // static_assert(std::is_move_assignable_v<DerivedTestNonCopyable>);
}

TEST_F(NonCopyableTest, MoveConstruction) {
    TestNonCopyable original(42);
    TestNonCopyable moved = std::move(original);

    // After move, the moved object should be in a valid state
    // The exact behavior depends on the implementation
    EXPECT_EQ(moved.getValue(), 42);
}

TEST_F(NonCopyableTest, MoveAssignment) {
    TestNonCopyable obj1(10);
    TestNonCopyable obj2(20);

    obj1 = std::move(obj2);

    // After move assignment, obj1 should have obj2's value
    EXPECT_EQ(obj1.getValue(), 20);
}

// Inheritance Tests
TEST_F(NonCopyableTest, InheritanceWorks) {
    // Test that inheritance from NonCopyable works correctly
    DerivedTestNonCopyable derived(25);
    EXPECT_EQ(derived.getValue(), 50); // 25 * 2

    // Test polymorphic behavior
    VirtualTestNonCopyable* base = &derived;
    EXPECT_EQ(base->getValue(), 50);
}

TEST_F(NonCopyableTest, PolymorphicBehavior) {
    std::unique_ptr<VirtualTestNonCopyable> base =
        std::make_unique<DerivedTestNonCopyable>(30);

    EXPECT_EQ(base->getValue(), 60); // 30 * 2

    // Test that we can't copy through base pointer
    static_assert(!std::is_copy_constructible_v<VirtualTestNonCopyable>);
}

// Container Tests
TEST_F(NonCopyableTest, VectorOfNonCopyable) {
    // Test that we can store NonCopyable objects in containers using move semantics
    std::vector<TestNonCopyable> vec;

    // Should be able to emplace_back
    vec.emplace_back(10);
    vec.emplace_back(20);
    vec.emplace_back(30);

    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0].getValue(), 10);
    EXPECT_EQ(vec[1].getValue(), 20);
    EXPECT_EQ(vec[2].getValue(), 30);

    // Should be able to push_back with move
    TestNonCopyable obj(40);
    vec.push_back(std::move(obj));

    EXPECT_EQ(vec.size(), 4);
    EXPECT_EQ(vec[3].getValue(), 40);
}

TEST_F(NonCopyableTest, UniquePtrOfNonCopyable) {
    // Test that NonCopyable works well with unique_ptr
    auto ptr = std::make_unique<TestNonCopyable>(100);
    EXPECT_EQ(ptr->getValue(), 100);

    // Test move of unique_ptr
    auto moved_ptr = std::move(ptr);
    EXPECT_EQ(moved_ptr->getValue(), 100);
    EXPECT_EQ(ptr, nullptr);
}

// Factory Function Tests
TEST_F(NonCopyableTest, FactoryFunction) {
    // Test factory function that returns NonCopyable by value
    auto factory = [](int value) -> TestNonCopyable {
        return TestNonCopyable(value);
    };

    auto obj = factory(123);
    EXPECT_EQ(obj.getValue(), 123);
}

TEST_F(NonCopyableTest, ReturnByValue) {
    // Test function that returns NonCopyable by value
    auto create_object = []() -> TestNonCopyable {
        TestNonCopyable obj(456);
        return obj; // Should use move semantics
    };

    auto result = create_object();
    EXPECT_EQ(result.getValue(), 456);
}

// RAII Tests
TEST_F(NonCopyableTest, RAIIPattern) {
    // Test RAII pattern with NonCopyable
    class RAIIResource : public NonCopyable {
    public:
        RAIIResource() : acquired_(true) {}
        ~RAIIResource() { release(); }

        void release() { acquired_ = false; }
        bool isAcquired() const { return acquired_; }

    private:
        bool acquired_;
    };

    {
        RAIIResource resource;
        EXPECT_TRUE(resource.isAcquired());
    } // resource should be automatically released here

    // Test with unique_ptr
    auto resource_ptr = std::make_unique<RAIIResource>();
    EXPECT_TRUE(resource_ptr->isAcquired());
    resource_ptr.reset(); // Explicit cleanup
}

// Thread Safety Tests (conceptual)
TEST_F(NonCopyableTest, ThreadSafetyConsiderations) {
    // NonCopyable itself doesn't provide thread safety,
    // but it prevents accidental copying in multithreaded contexts

    class ThreadSafeCounter : public NonCopyable {
    public:
        ThreadSafeCounter() : count_(0) {}

        void increment() { ++count_; }
        int getCount() const { return count_; }

    private:
        std::atomic<int> count_;
    };

    ThreadSafeCounter counter;
    counter.increment();
    counter.increment();

    EXPECT_EQ(counter.getCount(), 2);

    // Verify that we can't accidentally copy it
    static_assert(!std::is_copy_constructible_v<ThreadSafeCounter>);
    static_assert(!std::is_copy_assignable_v<ThreadSafeCounter>);
}

// Boost Compatibility Tests
TEST_F(NonCopyableTest, BoostCompatibility) {
    // Test that our NonCopyable behaves similarly whether using Boost or not

#ifdef ATOM_USE_BOOST
    // When using Boost, NonCopyable inherits from boost::noncopyable
    static_assert(std::is_base_of_v<boost::noncopyable, NonCopyable>);
#endif

    // Regardless of implementation, the behavior should be the same
    static_assert(!std::is_copy_constructible_v<NonCopyable>);
    static_assert(!std::is_copy_assignable_v<NonCopyable>);
    static_assert(std::is_move_constructible_v<NonCopyable>);
    static_assert(std::is_move_assignable_v<NonCopyable>);
}

// Edge Cases Tests
TEST_F(NonCopyableTest, EmptyDerivedClass) {
    // Test that even empty derived classes work correctly
    class EmptyNonCopyable : public NonCopyable {};

    static_assert(!std::is_copy_constructible_v<EmptyNonCopyable>);
    static_assert(!std::is_copy_assignable_v<EmptyNonCopyable>);
    static_assert(std::is_move_constructible_v<EmptyNonCopyable>);
    static_assert(std::is_move_assignable_v<EmptyNonCopyable>);

    EmptyNonCopyable obj;
    EmptyNonCopyable moved = std::move(obj);

    // Should compile and work without issues
    SUCCEED();
}

TEST_F(NonCopyableTest, MultipleInheritance) {
    // Test multiple inheritance scenarios
    class OtherBase {
    public:
        virtual ~OtherBase() = default;
        virtual int getOtherValue() const { return 999; }
    };

    class MultipleInheritance : public NonCopyable, public OtherBase {
    public:
        MultipleInheritance() : value_(42) {}
        int getValue() const { return value_; }

    private:
        int value_;
    };

    MultipleInheritance obj;
    EXPECT_EQ(obj.getValue(), 42);
    EXPECT_EQ(obj.getOtherValue(), 999);

    // Should still be non-copyable
    static_assert(!std::is_copy_constructible_v<MultipleInheritance>);
    static_assert(!std::is_copy_assignable_v<MultipleInheritance>);
}
