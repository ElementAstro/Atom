/**
 * @file test_fixtures.cpp
 * @brief Test fixture functionality in the Atom Test Framework
 */

#include "atom/tests/atom_test.hpp"

#include <memory>
#include <vector>

using namespace atom::test;

// ============================================================================
// Simple Test Fixture
// ============================================================================

class SimpleFixture : public TestFixture {
protected:
    void SetUp() override {
        value = 42;
        message = "Hello, Test!";
    }

    void TearDown() override {
        value = 0;
        message.clear();
    }

    int value = 0;
    std::string message;
};

TEST_F(SimpleFixture, ValueIsInitialized) {
    expect_eq(value, 42);
}

TEST_F(SimpleFixture, MessageIsInitialized) {
    expect_eq(message, "Hello, Test!");
}

TEST_F(SimpleFixture, CanModifyValue) {
    value = 100;
    expect_eq(value, 100);
}

// ============================================================================
// Resource Management Fixture
// ============================================================================

class ResourceFixture : public TestFixture {
protected:
    void SetUp() override {
        resource = std::make_unique<std::vector<int>>();
        resource->push_back(1);
        resource->push_back(2);
        resource->push_back(3);
    }

    void TearDown() override {
        resource.reset();
    }

    std::unique_ptr<std::vector<int>> resource;
};

TEST_F(ResourceFixture, ResourceIsCreated) {
    expect_not_null(resource.get());
}

TEST_F(ResourceFixture, ResourceHasCorrectSize) {
    expect_size(*resource, 3);
}

TEST_F(ResourceFixture, ResourceContainsExpectedValues) {
    expect_contains_element(*resource, 1);
    expect_contains_element(*resource, 2);
    expect_contains_element(*resource, 3);
}

// ============================================================================
// Counter Fixture (Tests isolation)
// ============================================================================

class CounterFixture : public TestFixture {
protected:
    void SetUp() override {
        counter = 0;
    }

    void TearDown() override {
        // Verify counter was modified
    }

    int counter = -1;
};

TEST_F(CounterFixture, CounterStartsAtZero) {
    expect_eq(counter, 0);
}

TEST_F(CounterFixture, CanIncrementCounter) {
    counter++;
    expect_eq(counter, 1);
}

TEST_F(CounterFixture, CanIncrementMultipleTimes) {
    counter++;
    counter++;
    counter++;
    expect_eq(counter, 3);
}

// ============================================================================
// Database Mock Fixture
// ============================================================================

class DatabaseFixture : public TestFixture {
protected:
    void SetUp() override {
        connected = true;
        records.clear();
        records["user1"] = "Alice";
        records["user2"] = "Bob";
    }

    void TearDown() override {
        connected = false;
        records.clear();
    }

    bool connected = false;
    std::map<std::string, std::string> records;
};

TEST_F(DatabaseFixture, IsConnected) {
    expect_true(connected);
}

TEST_F(DatabaseFixture, HasRecords) {
    expect_not_empty(records);
    expect_size(records, 2);
}

TEST_F(DatabaseFixture, CanFindUser) {
    auto it = records.find("user1");
    expect_true(it != records.end());
    expect_eq(it->second, "Alice");
}

TEST_F(DatabaseFixture, CanAddRecord) {
    records["user3"] = "Charlie";
    expect_size(records, 3);
}

// ============================================================================
// Simple TEST macro (without fixture)
// ============================================================================

TEST(NoFixture, SimpleTest) {
    int x = 1 + 1;
    expect_eq(x, 2);
}

TEST(NoFixture, AnotherSimpleTest) {
    std::string s = "hello";
    expect_eq(s.length(), 5);
}

// ============================================================================
// Disabled Tests
// ============================================================================

TEST_DISABLED(DisabledTests, ThisTestIsDisabled) {
    // This test should not run
    FAIL("This should not execute");
}

TEST_F_DISABLED(SimpleFixture, DisabledFixtureTest) {
    // This fixture test should not run
    FAIL("This should not execute");
}

// ============================================================================
// Tagged Tests
// ============================================================================

TEST_TAGGED(TaggedTests, FastTest, "fast", "unit") {
    expect_true(true);
}

TEST_TAGGED(TaggedTests, SlowTest, "slow", "integration") {
    expect_true(true);
}

TEST_TAGGED(TaggedTests, DatabaseTest, "database", "integration") {
    expect_true(true);
}

// ============================================================================
// Timeout Tests
// ============================================================================

TEST_TIMEOUT(TimeoutTests, FastOperation, 1000) {
    // This should complete well within 1 second
    int sum = 0;
    for (int i = 0; i < 100; ++i) {
        sum += i;
    }
    expect_gt(sum, 0);
}

// ============================================================================
// Fixture with Timeout
// ============================================================================

class TimedFixture : public TestFixture {
protected:
    void SetUp() override {
        startTime = std::chrono::steady_clock::now();
    }

    void TearDown() override {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        // Could log duration here
        (void)duration;
    }

    std::chrono::steady_clock::time_point startTime;
};

TEST_F_TIMEOUT(TimedFixture, TimedFixtureTest, 500) {
    // Quick operation
    expect_true(true);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    return runAllTests(argc, argv);
}
