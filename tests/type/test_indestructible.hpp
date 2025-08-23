#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "atom/type/indestructible.hpp"

// Test fixture for Indestructible tests
class IndestructibleTest : public ::testing::Test {
protected:
    // Counter for destructors, copy operations, move operations
    struct Counter {
        static inline int constructor_count = 0;
        static inline int destructor_count = 0;
        static inline int copy_count = 0;
        static inline int move_count = 0;
        static inline int assign_count = 0;
        static inline int move_assign_count = 0;

        static void reset() {
            constructor_count = 0;
            destructor_count = 0;
            copy_count = 0;
            move_count = 0;
            assign_count = 0;
            move_assign_count = 0;
        }
    };

    // Non-trivial test class
    class TestClass : public Counter {
    public:
        int value;
        std::string name;

        TestClass(int v = 0, std::string n = "default")
            : value(v), name(std::move(n)) {
            ++constructor_count;
        }

        ~TestClass() { ++destructor_count; }

        TestClass(const TestClass& other)
            : value(other.value), name(other.name) {
            ++copy_count;
        }

        TestClass(TestClass&& other) noexcept
            : value(other.value), name(std::move(other.name)) {
            ++move_count;
            other.value = 0;
        }

        TestClass& operator=(const TestClass& other) {
            if (this != &other) {
                value = other.value;
                name = other.name;
                ++assign_count;
            }
            return *this;
        }

        TestClass& operator=(TestClass&& other) noexcept {
            if (this != &other) {
                value = other.value;
                name = std::move(other.name);
                other.value = 0;
                ++move_assign_count;
            }
            return *this;
        }

        bool operator==(const TestClass& other) const {
            return value == other.value && name == other.name;
        }
    };

    // Trivial test struct for testing trivial types behavior
    struct TrivialStruct {
        int value;
        double data;
    };

    void SetUp() override { Counter::reset(); }

    void TearDown() override { Counter::reset(); }
};

// Tests for basic construction
TEST_F(IndestructibleTest, BasicConstruction) {
    Indestructible<TestClass> obj(std::in_place, 42, "test");

    EXPECT_EQ(obj.get().value, 42);
    EXPECT_EQ(obj.get().name, "test");
    EXPECT_EQ(Counter::constructor_count, 1);
    EXPECT_EQ(Counter::destructor_count, 0);  // Not destroyed yet
}

TEST_F(IndestructibleTest, CopyConstruction) {
    Indestructible<TestClass> obj1(std::in_place, 42, "test");
    Indestructible<TestClass> obj2(obj1);

    EXPECT_EQ(obj1.get().value, 42);
    EXPECT_EQ(obj1.get().name, "test");
    EXPECT_EQ(obj2.get().value, 42);
    EXPECT_EQ(obj2.get().name, "test");
    EXPECT_EQ(Counter::constructor_count, 1);
    EXPECT_EQ(Counter::copy_count, 1);
}

TEST_F(IndestructibleTest, MoveConstruction) {
    Indestructible<TestClass> obj1(std::in_place, 42, "test");
    Indestructible<TestClass> obj2(std::move(obj1));

    EXPECT_EQ(obj2.get().value, 42);
    EXPECT_EQ(obj2.get().name, "test");
    EXPECT_EQ(obj1.get().value, 0);  // Moved from
    EXPECT_EQ(Counter::constructor_count, 1);
    EXPECT_EQ(Counter::move_count, 1);
}

// Tests for assignment operators
TEST_F(IndestructibleTest, CopyAssignment) {
    Indestructible<TestClass> obj1(std::in_place, 42, "test");
    Indestructible<TestClass> obj2(std::in_place, 10, "other");

    obj2 = obj1;

    EXPECT_EQ(obj1.get().value, 42);
    EXPECT_EQ(obj1.get().name, "test");
    EXPECT_EQ(obj2.get().value, 42);
    EXPECT_EQ(obj2.get().name, "test");
    EXPECT_EQ(Counter::constructor_count, 2);
    EXPECT_EQ(Counter::assign_count, 1);
}

TEST_F(IndestructibleTest, MoveAssignment) {
    Indestructible<TestClass> obj1(std::in_place, 42, "test");
    Indestructible<TestClass> obj2(std::in_place, 10, "other");

    obj2 = std::move(obj1);

    EXPECT_EQ(obj2.get().value, 42);
    EXPECT_EQ(obj2.get().name, "test");
    EXPECT_EQ(obj1.get().value, 0);  // Moved from
    EXPECT_EQ(Counter::constructor_count, 2);
    EXPECT_EQ(Counter::move_assign_count, 1);
}

// Tests for accessor methods
TEST_F(IndestructibleTest, GetMethod) {
    Indestructible<TestClass> obj(std::in_place, 42, "test");

    // Test const& access
    const Indestructible<TestClass>& constRef = obj;
    EXPECT_EQ(constRef.get().value, 42);
    EXPECT_EQ(constRef.get().name, "test");

    // Test modifying via get()
    obj.get().value = 100;
    EXPECT_EQ(obj.get().value, 100);

    // Test rvalue reference access (can't easily test directly, but we can
    // verify it compiles)
    auto getValue = [](Indestructible<TestClass>&& obj) -> int {
        return std::move(obj).get().value;
    };

    EXPECT_EQ(getValue(std::move(obj)), 100);
}

TEST_F(IndestructibleTest, ArrowOperator) {
    Indestructible<TestClass> obj(std::in_place, 42, "test");

    // Test non-const arrow operator
    EXPECT_EQ(obj->value, 42);
    EXPECT_EQ(obj->name, "test");

    // Test const arrow operator
    const Indestructible<TestClass>& constRef = obj;
    EXPECT_EQ(constRef->value, 42);
    EXPECT_EQ(constRef->name, "test");

    // Test modifying via arrow operator
    obj->value = 100;
    EXPECT_EQ(obj->value, 100);
}

TEST_F(IndestructibleTest, ConversionOperators) {
    Indestructible<TestClass> obj(std::in_place, 42, "test");

    // Test T& conversion
    TestClass& ref = obj;
    EXPECT_EQ(ref.value, 42);
    EXPECT_EQ(ref.name, "test");

    // Test const T& conversion
    const Indestructible<TestClass>& constObj = obj;
    const TestClass& constRef = constObj;
    EXPECT_EQ(constRef.value, 42);
    EXPECT_EQ(constRef.name, "test");

    // Test modifying via converted reference
    ref.value = 100;
    EXPECT_EQ(obj.get().value, 100);

    // Test rvalue reference conversion (can't easily test directly, but we can
    // verify it compiles)
    auto getValue = [](TestClass&& obj) -> int { return obj.value; };

    EXPECT_EQ(getValue(std::move(obj)), 100);
}

// Tests for reset and emplace
TEST_F(IndestructibleTest, ResetMethod) {
    Indestructible<TestClass> obj(std::in_place, 42, "test");

    EXPECT_EQ(obj.get().value, 42);
    EXPECT_EQ(obj.get().name, "test");

    obj.reset(100, "reset");

    EXPECT_EQ(obj.get().value, 100);
    EXPECT_EQ(obj.get().name, "reset");
    EXPECT_EQ(Counter::constructor_count, 2);
    EXPECT_EQ(Counter::destructor_count, 1);  // The first object was destroyed
}

TEST_F(IndestructibleTest, EmplaceMethod) {
    Indestructible<TestClass> obj(std::in_place, 42, "test");

    EXPECT_EQ(obj.get().value, 42);
    EXPECT_EQ(obj.get().name, "test");

    obj.emplace(100, "emplaced");

    EXPECT_EQ(obj.get().value, 100);
    EXPECT_EQ(obj.get().name, "emplaced");
    EXPECT_EQ(Counter::constructor_count, 2);
    EXPECT_EQ(Counter::destructor_count, 1);  // The first object was destroyed
}

// Test destructors
TEST_F(IndestructibleTest, DestructorBehavior) {
    {
        Indestructible<TestClass> obj(std::in_place, 42, "test");
        EXPECT_EQ(Counter::destructor_count, 0);
    }
    // After scope exit, destructor should be called
    EXPECT_EQ(Counter::destructor_count, 1);
}

// Tests with trivial types
TEST_F(IndestructibleTest, TrivialTypeConstruction) {
    Indestructible<TrivialStruct> obj(std::in_place, TrivialStruct{42, 3.14});

    EXPECT_EQ(obj.get().value, 42);
    EXPECT_DOUBLE_EQ(obj.get().data, 3.14);
}

TEST_F(IndestructibleTest, TrivialTypeCopyAndMove) {
    Indestructible<TrivialStruct> obj1(std::in_place, TrivialStruct{42, 3.14});
    Indestructible<TrivialStruct> obj2(obj1);

    EXPECT_EQ(obj2.get().value, 42);
    EXPECT_DOUBLE_EQ(obj2.get().data, 3.14);

    Indestructible<TrivialStruct> obj3(std::in_place, TrivialStruct{10, 2.71});
    obj3 = std::move(obj1);

    EXPECT_EQ(obj3.get().value, 42);
    EXPECT_DOUBLE_EQ(obj3.get().data, 3.14);
}

// Tests for destruction_guard
TEST_F(IndestructibleTest, DestructionGuard) {
    TestClass* ptr = new TestClass(42, "guard-test");
    EXPECT_EQ(Counter::constructor_count, 1);
    EXPECT_EQ(Counter::destructor_count, 0);

    {
        destruction_guard<TestClass> guard(ptr);
    }
    // After scope exit, guard should call destroy_at
    EXPECT_EQ(Counter::destructor_count, 1);

    // Since the object is already destroyed, we need to manually deallocate the
    // memory to avoid memory leaks. In real code, you would typically use
    // placement new with properly aligned storage.
    ::operator delete(ptr);
}

// Test with standard containers
TEST_F(IndestructibleTest, StdContainerCompat) {
    std::vector<Indestructible<int>> vec;
    vec.emplace_back(std::in_place, 1);
    vec.emplace_back(std::in_place, 2);
    vec.emplace_back(std::in_place, 3);

    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0].get(), 1);
    EXPECT_EQ(vec[1].get(), 2);
    EXPECT_EQ(vec[2].get(), 3);
}

// Test with string type
TEST_F(IndestructibleTest, StringType) {
    Indestructible<std::string> str(std::in_place, "Hello, world!");

    EXPECT_EQ(str.get(), "Hello, world!");

    str.get() += " More text.";
    EXPECT_EQ(str.get(), "Hello, world! More text.");

    str.reset("Reset string");
    EXPECT_EQ(str.get(), "Reset string");
}

// Test with unique_ptr
TEST_F(IndestructibleTest, UniquePtr) {
    Counter::reset();

    Indestructible<std::unique_ptr<TestClass>> ptr(
        std::in_place, std::make_unique<TestClass>(42, "unique"));

    EXPECT_EQ(Counter::constructor_count, 1);
    EXPECT_EQ(ptr.get()->value, 42);
    EXPECT_EQ(ptr.get()->name, "unique");

    ptr.reset(std::make_unique<TestClass>(100, "reset"));

    EXPECT_EQ(Counter::constructor_count, 2);
    EXPECT_EQ(Counter::destructor_count,
              1);  // First unique_ptr's TestClass was destroyed
    EXPECT_EQ(ptr.get()->value, 100);
    EXPECT_EQ(ptr.get()->name, "reset");
}

// Test with multiple emplace calls
TEST_F(IndestructibleTest, MultipleEmplace) {
    Indestructible<TestClass> obj(std::in_place, 0, "initial");

    EXPECT_EQ(Counter::constructor_count, 1);
    EXPECT_EQ(Counter::destructor_count, 0);

    for (int i = 1; i <= 5; i++) {
        obj.emplace(i, "emplace-" + std::to_string(i));
    }

    EXPECT_EQ(Counter::constructor_count, 6);  // Initial + 5 emplace calls
    EXPECT_EQ(Counter::destructor_count,
              5);  // 5 objects were destroyed by emplace
    EXPECT_EQ(obj.get().value, 5);
    EXPECT_EQ(obj.get().name, "emplace-5");
}

// Test comparison with contained type
TEST_F(IndestructibleTest, ComparisonWithContainedType) {
    Indestructible<TestClass> obj(std::in_place, 42, "test");
    TestClass raw(42, "test");

    EXPECT_TRUE(obj.get() == raw);

    raw.value = 100;
    EXPECT_FALSE(obj.get() == raw);

    obj.get().value = 100;
    EXPECT_TRUE(obj.get() == raw);
}

// Test is_destructible trait behavior
TEST_F(IndestructibleTest, IsDestructible) {
    // The class is named "Indestructible" but it's still technically
    // destructible from a type trait perspective (it has a destructor)
    EXPECT_TRUE(std::is_destructible_v<Indestructible<TestClass>>);
}

// Test with void* pointer type (edge case)
TEST_F(IndestructibleTest, VoidPointerType) {
    int value = 42;
    Indestructible<void*> ptr(std::in_place, &value);

    EXPECT_EQ(*(static_cast<int*>(ptr.get())), 42);

    value = 100;
    EXPECT_EQ(*(static_cast<int*>(ptr.get())), 100);
}

// Test with function pointers
TEST_F(IndestructibleTest, FunctionPointer) {
    using FuncType = int (*)(int);

    Indestructible<FuncType> fn(
        std::in_place, +[](int x) -> int { return x * 2; });

    EXPECT_EQ(fn.get()(21), 42);
}

// Test with direct struct initialization
struct Point {
    int x, y;
};

TEST_F(IndestructibleTest, DirectStructInit) {
    Indestructible<Point> point(std::in_place, Point{10, 20});

    EXPECT_EQ(point->x, 10);
    EXPECT_EQ(point->y, 20);

    point->x = 30;
    point->y = 40;

    EXPECT_EQ(point->x, 30);
    EXPECT_EQ(point->y, 40);
}
