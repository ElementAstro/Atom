#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <memory>

// Simple test to verify the test framework is working
TEST(SimpleTest, BasicAssertion) {
    EXPECT_EQ(2 + 2, 4);
    EXPECT_TRUE(true);
    EXPECT_FALSE(false);
}

TEST(SimpleTest, StringOperations) {
    std::string hello = "Hello";
    std::string world = "World";
    std::string combined = hello + " " + world;

    EXPECT_EQ(combined, "Hello World");
    EXPECT_NE(hello, world);
}

TEST(SimpleTest, VectorOperations) {
    std::vector<int> numbers = {1, 2, 3, 4, 5};

    EXPECT_EQ(numbers.size(), 5);
    EXPECT_EQ(numbers[0], 1);
    EXPECT_EQ(numbers[4], 5);

    numbers.push_back(6);
    EXPECT_EQ(numbers.size(), 6);
    EXPECT_EQ(numbers.back(), 6);
}

TEST(SimpleTest, SmartPointers) {
    auto ptr = std::make_unique<int>(42);

    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 42);

    auto shared = std::make_shared<std::string>("test");
    EXPECT_EQ(*shared, "test");
    EXPECT_EQ(shared.use_count(), 1);
}

// Test class for more complex scenarios
class TestClass {
public:
    TestClass(int value) : value_(value) {}

    int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }

    bool isPositive() const { return value_ > 0; }

private:
    int value_;
};

TEST(TestClassTest, BasicFunctionality) {
    TestClass obj(10);

    EXPECT_EQ(obj.getValue(), 10);
    EXPECT_TRUE(obj.isPositive());

    obj.setValue(-5);
    EXPECT_EQ(obj.getValue(), -5);
    EXPECT_FALSE(obj.isPositive());
}

TEST(TestClassTest, ZeroValue) {
    TestClass obj(0);

    EXPECT_EQ(obj.getValue(), 0);
    EXPECT_FALSE(obj.isPositive());
}

// Test fixture example
class CalculatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code here
        result_ = 0;
    }

    void TearDown() override {
        // Cleanup code here
    }

    int add(int a, int b) {
        return a + b;
    }

    int multiply(int a, int b) {
        return a * b;
    }

    int result_;
};

TEST_F(CalculatorTest, Addition) {
    result_ = add(2, 3);
    EXPECT_EQ(result_, 5);

    result_ = add(-1, 1);
    EXPECT_EQ(result_, 0);
}

TEST_F(CalculatorTest, Multiplication) {
    result_ = multiply(3, 4);
    EXPECT_EQ(result_, 12);

    result_ = multiply(-2, 5);
    EXPECT_EQ(result_, -10);
}

TEST_F(CalculatorTest, EdgeCases) {
    result_ = add(0, 0);
    EXPECT_EQ(result_, 0);

    result_ = multiply(0, 100);
    EXPECT_EQ(result_, 0);

    result_ = multiply(1, -1);
    EXPECT_EQ(result_, -1);
}

// Parameterized test example
class ParameterizedTest : public ::testing::TestWithParam<std::pair<int, int>> {
protected:
    int square(int x) {
        return x * x;
    }
};

TEST_P(ParameterizedTest, SquareTest) {
    auto param = GetParam();
    int input = param.first;
    int expected = param.second;

    EXPECT_EQ(square(input), expected);
}

INSTANTIATE_TEST_SUITE_P(
    SquareValues,
    ParameterizedTest,
    ::testing::Values(
        std::make_pair(0, 0),
        std::make_pair(1, 1),
        std::make_pair(2, 4),
        std::make_pair(3, 9),
        std::make_pair(-2, 4),
        std::make_pair(5, 25)
    )
);

// Exception testing
TEST(ExceptionTest, ThrowsException) {
    auto throwingFunction = []() {
        throw std::runtime_error("Test exception");
    };

    EXPECT_THROW(throwingFunction(), std::runtime_error);
}

TEST(ExceptionTest, NoException) {
    auto safeFunction = []() {
        return 42;
    };

    EXPECT_NO_THROW(safeFunction());
    EXPECT_EQ(safeFunction(), 42);
}

// Death test (if supported)
#if !defined(_WIN32) && !defined(NDEBUG)
TEST(DeathTest, Assertion) {
    EXPECT_DEATH({
        assert(false);
    }, "");
}
#endif

// Performance test example
TEST(PerformanceTest, VectorResize) {
    std::vector<int> vec;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        vec.push_back(i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    EXPECT_EQ(vec.size(), 10000);
    EXPECT_LT(duration.count(), 10000); // Should complete in less than 10ms
}

// Test that demonstrates test organization
namespace ComponentTests {

class MockComponent {
public:
    MockComponent(const std::string& name) : name_(name), active_(false) {}

    const std::string& getName() const { return name_; }
    bool isActive() const { return active_; }
    void setActive(bool active) { active_ = active; }

private:
    std::string name_;
    bool active_;
};

TEST(MockComponentTest, BasicOperations) {
    MockComponent component("TestComponent");

    EXPECT_EQ(component.getName(), "TestComponent");
    EXPECT_FALSE(component.isActive());

    component.setActive(true);
    EXPECT_TRUE(component.isActive());
}

TEST(MockComponentTest, MultipleComponents) {
    std::vector<MockComponent> components;

    for (int i = 0; i < 5; ++i) {
        components.emplace_back("Component" + std::to_string(i));
    }

    EXPECT_EQ(components.size(), 5);

    for (size_t i = 0; i < components.size(); ++i) {
        std::string expectedName = "Component" + std::to_string(i);
        EXPECT_EQ(components[i].getName(), expectedName);
        EXPECT_FALSE(components[i].isActive());
    }
}

} // namespace ComponentTests

// Main function is not needed as GTest provides its own main
// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
