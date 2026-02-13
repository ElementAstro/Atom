/**
 * @file test_main.cpp
 * @brief Comprehensive test suite for the Atom Test Framework
 *
 * This file demonstrates all features of the Atom Test Framework including:
 * - Basic assertions
 * - Test fixtures
 * - Parameterized tests
 * - Mock objects
 * - Skip and timeout functionality
 * - Data factories
 * - Reporters
 * - Benchmarks
 */

#include "atom/tests/atom_test.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>
#include <vector>

using namespace atom::test;

// ============================================================================
// SECTION 1: Basic Assertions
// ============================================================================

TEST(BasicAssertions, EqualityAssertions) {
    expect_eq(1, 1);
    expect_eq(std::string("hello"), std::string("hello"));
    expect_ne(1, 2);
    expect_ne(std::string("hello"), std::string("world"));
}

TEST(BasicAssertions, ComparisonAssertions) {
    expect_gt(5, 3);
    expect_lt(3, 5);
    expect_ge(5, 5);
    expect_le(5, 5);
}

TEST(BasicAssertions, BooleanAssertions) {
    expect_true(true);
    expect_true(1 == 1);
    expect_false(false);
    expect_false(1 == 2);
}

TEST(BasicAssertions, PointerAssertions) {
    int* nullPtr = nullptr;
    int value = 42;
    int* validPtr = &value;

    expect_null(nullPtr);
    expect_not_null(validPtr);
}

TEST(BasicAssertions, FloatingPointAssertions) {
    expect_approx(3.14159, 3.14160, 0.001);
    expect_near(1.0, 1.001, 0.01);
    expect_in_range(5, 1, 10);
}

TEST(BasicAssertions, StringAssertions) {
    expect_contains("hello world", "world");
    expect_starts_with("hello world", "hello");
    expect_ends_with("hello world", "world");
    expect_matches("test123", "[a-z]+[0-9]+");
}

TEST(BasicAssertions, ContainerAssertions) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::vector<int> emptyVec;

    expect_not_empty(vec);
    expect_empty(emptyVec);
    expect_size(vec, 5);
    expect_contains_element(vec, 3);
    expect_sorted(vec);
    expect_unique(vec);
}

TEST(BasicAssertions, PredicateAssertions) {
    std::vector<int> evens = {2, 4, 6, 8};
    std::vector<int> mixed = {1, 2, 3, 4};
    std::vector<int> odds = {1, 3, 5, 7};

    expect_all_of(evens, [](int x) { return x % 2 == 0; });
    expect_any_of(mixed, [](int x) { return x % 2 == 0; });
    expect_none_of(odds, [](int x) { return x % 2 == 0; });
}

TEST(BasicAssertions, ExceptionAssertions) {
    expect_throws([]() { throw std::runtime_error("error"); });
    expect_throws_with_message(
        []() { throw std::runtime_error("specific error"); }, "specific");
    expect_no_throw([]() {
        int x = 1 + 1;
        (void)x;
    });
}

// ============================================================================
// SECTION 2: Test Fixtures
// ============================================================================

class VectorFixture : public TestFixture {
protected:
    void SetUp() override { vec = {1, 2, 3, 4, 5}; }

    void TearDown() override { vec.clear(); }

    std::vector<int> vec;
};

TEST_F(VectorFixture, VectorIsInitialized) {
    expect_size(vec, 5);
    expect_not_empty(vec);
}

TEST_F(VectorFixture, CanModifyVector) {
    vec.push_back(6);
    expect_size(vec, 6);
    expect_contains_element(vec, 6);
}

TEST_F(VectorFixture, VectorIsSorted) { expect_sorted(vec); }

class ResourceFixture : public TestFixture {
protected:
    void SetUp() override {
        resource = std::make_unique<std::string>("Test Resource");
    }

    void TearDown() override { resource.reset(); }

    std::unique_ptr<std::string> resource;
};

TEST_F(ResourceFixture, ResourceIsCreated) {
    expect_not_null(resource.get());
    expect_eq(*resource, "Test Resource");
}

// ============================================================================
// SECTION 3: Parameterized Tests
// ============================================================================

class SquareTest : public ParameterizedTest<std::pair<int, int>> {};

INSTANTIATE_TEST_SUITE_P(
    Squares, SquareTest,
    Values<std::pair<int, int>>({{1, 1}, {2, 4}, {3, 9}, {4, 16}, {5, 25}}));

TEST_P(SquareTest, SquareIsCorrect) {
    const auto& [input, expected] = GetParam();
    expect_eq(input * input, expected);
}

class StringLengthTest
    : public ParameterizedTest<std::pair<std::string, size_t>> {};

INSTANTIATE_TEST_SUITE_P(StringLengths, StringLengthTest,
                         Values<std::pair<std::string, size_t>>(
                             {{"", 0}, {"a", 1}, {"hello", 5}, {"world!", 6}}));

TEST_P(StringLengthTest, LengthIsCorrect) {
    const auto& [str, expectedLen] = GetParam();
    expect_eq(str.length(), expectedLen);
}

// ============================================================================
// SECTION 4: Mock Objects
// ============================================================================

TEST(MockTests, BasicMock) {
    MockFunction<int(int, int)> mockAdd;
    mockAdd.expect().willReturn(10);

    int result = mockAdd(3, 7);
    expect_eq(result, 10);
    expect_eq(mockAdd.callCount(), 1);
}

TEST(MockTests, MockWithInvoke) {
    MockFunction<int(int)> mockDouble;
    mockDouble.expect().willInvoke([](int x) { return x * 2; });

    expect_eq(mockDouble(5), 10);
    expect_eq(mockDouble(3), 6);
}

TEST(MockTests, SpyFunction) {
    auto realFunc = [](int x) { return x * x; };
    Spy<int(int)> spy(realFunc);

    expect_eq(spy(4), 16);
    expect_eq(spy(5), 25);
    expect_eq(spy.callCount(), 2);
}

// ============================================================================
// SECTION 5: Skip and Timeout
// ============================================================================

TEST(SkipTests, ConditionalSkip) {
    bool condition = false;
    SKIP_IF(condition, "Skipping due to condition");
    expect_true(true);
}

TEST_TIMEOUT(TimeoutTests, FastOperation, 1000) {
    int sum = 0;
    for (int i = 0; i < 100; ++i) {
        sum += i;
    }
    expect_eq(sum, 4950);
}

// ============================================================================
// SECTION 6: Data Factories
// ============================================================================

struct User {
    std::string name;
    int age = 0;
    bool active = false;
};

TEST(DataFactoryTests, BuilderPattern) {
    auto user = builder<User>()
                    .set(&User::name, std::string("Alice"))
                    .set(&User::age, 30)
                    .set(&User::active, true)
                    .build();

    expect_eq(user.name, "Alice");
    expect_eq(user.age, 30);
    expect_true(user.active);
}

TEST(DataFactoryTests, RandomData) {
    RandomTestData rng(42);

    int randInt = rng.randomInt(0, 100);
    expect_in_range(randInt, 0, 100);

    std::string randStr = rng.randomAlphanumeric(10);
    expect_size(randStr, 10);

    std::string email = rng.randomEmail();
    expect_contains(email, "@");
}

TEST(DataFactoryTests, SequenceGenerator) {
    auto seq = SequenceGenerator::integers(0, 5);
    expect_eq(seq, std::vector<int>({0, 1, 2, 3, 4}));

    auto fib = SequenceGenerator::fibonacci(6);
    expect_eq(fib, std::vector<uint64_t>({0, 1, 1, 2, 3, 5}));
}

TEST(DataFactoryTests, BoundaryValues) {
    auto intBounds = BoundaryValues::forInteger<int>();
    expect_not_empty(intBounds);
    expect_contains_element(intBounds, 0);
    expect_contains_element(intBounds, std::numeric_limits<int>::max());
}

// ============================================================================
// SECTION 7: Reporters
// ============================================================================

TEST(ReporterTests, CreateReporters) {
    auto console = createReporter("console");
    auto json = createReporter("json");
    auto xml = createReporter("xml");
    auto html = createReporter("html");
    auto markdown = createReporter("markdown");

    expect_not_null(console.get());
    expect_not_null(json.get());
    expect_not_null(xml.get());
    expect_not_null(html.get());
    expect_not_null(markdown.get());
}

TEST(ReporterTests, GenerateReport) {
    auto reporter = createReporter("json");

    TestStats stats;
    stats.totalTests = 3;
    stats.passedAsserts = 2;
    stats.failedAsserts = 1;
    stats.results.push_back({"Test1", true, false, "", 10.0, false});
    stats.results.push_back({"Test2", true, false, "", 15.0, false});
    stats.results.push_back({"Test3", false, false, "Error", 5.0, false});

    auto outDir = std::filesystem::temp_directory_path() / "atom_test_reports";
    std::filesystem::create_directories(outDir);

    reporter->onTestRunStart(stats.totalTests);
    for (const auto& result : stats.results) {
        reporter->onTestEnd(result);
    }
    reporter->onTestRunEnd(stats);
    reporter->generateReport(stats, outDir.string());

    auto outFile = outDir / "test_report.json";
    expect_true(std::filesystem::exists(outFile));

    std::ifstream file(outFile);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string report = buffer.str();
    expect_not_empty(report);
    expect_contains(report, "total_tests");
}

// ============================================================================
// SECTION 8: Test Registry
// ============================================================================

TEST(RegistryTests, GetTestInfo) {
    auto names = getAllTestNames();
    expect_not_empty(names);

    auto [total, enabled, disabled] = getTestCountByStatus();
    expect_gt(total, 0);
}

// ============================================================================
// SECTION 9: Tagged Tests
// ============================================================================

TEST_TAGGED(TaggedTests, UnitTest, "unit", "fast") { expect_true(true); }

TEST_TAGGED(TaggedTests, IntegrationTest, "integration", "slow") {
    expect_true(true);
}

// ============================================================================
// SECTION 10: Disabled Tests
// ============================================================================

TEST_DISABLED(DisabledTests, ThisWontRun) { FAIL("This should not execute"); }

// ============================================================================
// SECTION 11: Custom Assertions
// ============================================================================

TEST(CustomAssertions, CustomPredicate) {
    int value = 42;
    expect_that(
        value, [](int x) { return x > 0 && x < 100; },
        "Value should be positive and less than 100");
}

TEST(CustomAssertions, SetEquality) {
    std::set<int> set1 = {1, 2, 3};
    std::set<int> set2 = {3, 2, 1};
    expect_set_eq(set1, set2);
}

// ============================================================================
// SECTION 12: RAII Guards
// ============================================================================

TEST(GuardTests, TestGuard) {
    bool cleanedUp = false;

    {
        auto guard = makeGuard([&cleanedUp]() { cleanedUp = true; });
        expect_false(cleanedUp);
    }

    expect_true(cleanedUp);
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    std::cout << "=================================================\n";
    std::cout << "  Atom Test Framework - Comprehensive Test Suite\n";
    std::cout << "=================================================\n\n";

    // List all tests if requested
    if (argc > 1 && std::string(argv[1]) == "--list") {
        listTests();
        return 0;
    }

    return runAllTests(argc, argv);
}
