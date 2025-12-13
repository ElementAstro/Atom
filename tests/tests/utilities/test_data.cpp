/*
 * test_data.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for test data utilities in atom/tests/utilities/test_data.hpp

**************************************************/

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "atom/tests/utilities/test_data.hpp"

namespace atom::test::utilities::tests {

// ============================================================================
// TestDataBuilder Tests
// ============================================================================

struct Person {
    std::string name;
    int age = 0;
    bool active = false;
};

class TestDataBuilderTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestDataBuilderTest, BuildSimpleObject) {
    auto person = TestDataBuilder<Person>()
                      .set(&Person::name, std::string("Alice"))
                      .set(&Person::age, 30)
                      .set(&Person::active, true)
                      .build();

    EXPECT_EQ(person.name, "Alice");
    EXPECT_EQ(person.age, 30);
    EXPECT_TRUE(person.active);
}

TEST_F(TestDataBuilderTest, BuildWithDefaults) {
    auto person = TestDataBuilder<Person>().build();

    EXPECT_TRUE(person.name.empty());
    EXPECT_EQ(person.age, 0);
    EXPECT_FALSE(person.active);
}

TEST_F(TestDataBuilderTest, BuildMany) {
    auto people = TestDataBuilder<Person>()
                      .set(&Person::active, true)
                      .buildMany(5);

    EXPECT_EQ(people.size(), 5);
    for (const auto& person : people) {
        EXPECT_TRUE(person.active);
    }
}

TEST_F(TestDataBuilderTest, BuildWithVariations) {
    std::vector<std::function<void(Person&)>> variations = {
        [](Person& p) { p.name = "Alice"; },
        [](Person& p) { p.name = "Bob"; },
        [](Person& p) { p.name = "Charlie"; }};

    auto people =
        TestDataBuilder<Person>().set(&Person::age, 25).buildWithVariations(
            variations);

    EXPECT_EQ(people.size(), 3);
    EXPECT_EQ(people[0].name, "Alice");
    EXPECT_EQ(people[1].name, "Bob");
    EXPECT_EQ(people[2].name, "Charlie");

    for (const auto& person : people) {
        EXPECT_EQ(person.age, 25);
    }
}

// ============================================================================
// TestDataFactory Tests
// ============================================================================

class TestDataFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        factory.registerPreset("default", [](Person& p) {
            p.name = "Default";
            p.age = 0;
            p.active = false;
        });

        factory.registerPreset("admin", [](Person& p) {
            p.name = "Admin";
            p.age = 35;
            p.active = true;
        });
    }
    void TearDown() override {}

    TestDataFactory<Person> factory;
};

TEST_F(TestDataFactoryTest, CreateDefault) {
    auto person = factory.create("default");

    EXPECT_EQ(person.name, "Default");
    EXPECT_EQ(person.age, 0);
    EXPECT_FALSE(person.active);
}

TEST_F(TestDataFactoryTest, CreateWithPreset) {
    auto admin = factory.create("admin");

    EXPECT_EQ(admin.name, "Admin");
    EXPECT_EQ(admin.age, 35);
    EXPECT_TRUE(admin.active);
}

TEST_F(TestDataFactoryTest, CreateWithOverride) {
    auto person = factory.create("default", [](Person& p) { p.age = 50; });

    EXPECT_EQ(person.name, "Default");
    EXPECT_EQ(person.age, 50);
}

TEST_F(TestDataFactoryTest, CreateMany) {
    auto people = factory.createMany("default", 3);

    EXPECT_EQ(people.size(), 3);
    for (const auto& person : people) {
        EXPECT_EQ(person.name, "Default");
    }
}

// ============================================================================
// RandomTestData Tests
// ============================================================================

class RandomTestDataTest : public ::testing::Test {
protected:
    void SetUp() override { rng = std::make_unique<RandomTestData>(42); }
    void TearDown() override { rng.reset(); }

    std::unique_ptr<RandomTestData> rng;
};

TEST_F(RandomTestDataTest, RandomInt) {
    int value = rng->randomInt(0, 100);
    EXPECT_GE(value, 0);
    EXPECT_LE(value, 100);
}

TEST_F(RandomTestDataTest, RandomIntRange) {
    for (int i = 0; i < 100; ++i) {
        int value = rng->randomInt(10, 20);
        EXPECT_GE(value, 10);
        EXPECT_LE(value, 20);
    }
}

TEST_F(RandomTestDataTest, RandomReal) {
    double value = rng->randomReal(0.0, 1.0);
    EXPECT_GE(value, 0.0);
    EXPECT_LE(value, 1.0);
}

TEST_F(RandomTestDataTest, RandomBool) {
    int trueCount = 0;
    int falseCount = 0;

    for (int i = 0; i < 100; ++i) {
        if (rng->randomBool()) {
            trueCount++;
        } else {
            falseCount++;
        }
    }

    // Both should have some occurrences with fair probability
    EXPECT_GT(trueCount, 0);
    EXPECT_GT(falseCount, 0);
}

TEST_F(RandomTestDataTest, RandomAlphanumeric) {
    std::string str = rng->randomAlphanumeric(10);
    EXPECT_EQ(str.length(), 10);

    for (char c : str) {
        EXPECT_TRUE(std::isalnum(c));
    }
}

TEST_F(RandomTestDataTest, RandomEmail) {
    std::string email = rng->randomEmail();
    EXPECT_NE(email.find('@'), std::string::npos);
    EXPECT_NE(email.find('.'), std::string::npos);
}

TEST_F(RandomTestDataTest, RandomUuid) {
    std::string uuid = rng->randomUuid();
    EXPECT_EQ(uuid.length(), 36);
    EXPECT_EQ(uuid[8], '-');
    EXPECT_EQ(uuid[13], '-');
    EXPECT_EQ(uuid[18], '-');
    EXPECT_EQ(uuid[23], '-');
}

TEST_F(RandomTestDataTest, RandomElement) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    int element = rng->randomElement(vec);

    auto it = std::find(vec.begin(), vec.end(), element);
    EXPECT_NE(it, vec.end());
}

TEST_F(RandomTestDataTest, RandomChoice) {
    auto choice = rng->randomChoice({1, 2, 3, 4, 5});
    EXPECT_GE(choice, 1);
    EXPECT_LE(choice, 5);
}

TEST_F(RandomTestDataTest, ShuffleVector) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::vector<int> original = vec;

    rng->shuffle(vec);

    // Same elements but possibly different order
    std::sort(vec.begin(), vec.end());
    EXPECT_EQ(vec, original);
}

// ============================================================================
// SequenceGenerator Tests
// ============================================================================

class SequenceGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SequenceGeneratorTest, IntegerSequence) {
    auto seq = SequenceGenerator::integers(0, 5);
    std::vector<int> expected = {0, 1, 2, 3, 4};
    EXPECT_EQ(seq, expected);
}

TEST_F(SequenceGeneratorTest, IntegerSequenceWithStep) {
    auto seq = SequenceGenerator::integers(0, 10, 2);
    std::vector<int> expected = {0, 2, 4, 6, 8};
    EXPECT_EQ(seq, expected);
}

TEST_F(SequenceGeneratorTest, StringSequence) {
    auto seq = SequenceGenerator::strings("item_", 3);
    std::vector<std::string> expected = {"item_0", "item_1", "item_2"};
    EXPECT_EQ(seq, expected);
}

TEST_F(SequenceGeneratorTest, FibonacciSequence) {
    auto seq = SequenceGenerator::fibonacci(8);
    std::vector<uint64_t> expected = {0, 1, 1, 2, 3, 5, 8, 13};
    EXPECT_EQ(seq, expected);
}

TEST_F(SequenceGeneratorTest, PowersOf2) {
    auto seq = SequenceGenerator::powersOf2(5);
    std::vector<uint64_t> expected = {1, 2, 4, 8, 16};
    EXPECT_EQ(seq, expected);
}

TEST_F(SequenceGeneratorTest, RepeatSequence) {
    auto seq = SequenceGenerator::repeat(42, 5);
    std::vector<int> expected = {42, 42, 42, 42, 42};
    EXPECT_EQ(seq, expected);
}

TEST_F(SequenceGeneratorTest, CycleSequence) {
    std::vector<int> values = {1, 2, 3};
    auto seq = SequenceGenerator::cycle(values, 7);
    std::vector<int> expected = {1, 2, 3, 1, 2, 3, 1};
    EXPECT_EQ(seq, expected);
}

// ============================================================================
// BoundaryValues Tests
// ============================================================================

class BoundaryValuesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BoundaryValuesTest, IntegerBoundaries) {
    auto bounds = BoundaryValues::forInteger<int>();

    EXPECT_FALSE(bounds.empty());
    EXPECT_NE(std::find(bounds.begin(), bounds.end(), 0), bounds.end());
    EXPECT_NE(std::find(bounds.begin(), bounds.end(),
                        std::numeric_limits<int>::min()),
              bounds.end());
    EXPECT_NE(std::find(bounds.begin(), bounds.end(),
                        std::numeric_limits<int>::max()),
              bounds.end());
}

TEST_F(BoundaryValuesTest, UnsignedBoundaries) {
    auto bounds = BoundaryValues::forUnsigned<unsigned int>();

    EXPECT_FALSE(bounds.empty());
    EXPECT_NE(std::find(bounds.begin(), bounds.end(), 0u), bounds.end());
    EXPECT_NE(std::find(bounds.begin(), bounds.end(),
                        std::numeric_limits<unsigned int>::max()),
              bounds.end());
}

TEST_F(BoundaryValuesTest, FloatBoundaries) {
    auto bounds = BoundaryValues::forFloat<double>();

    EXPECT_FALSE(bounds.empty());
    EXPECT_NE(std::find(bounds.begin(), bounds.end(), 0.0), bounds.end());
}

TEST_F(BoundaryValuesTest, StringLengthBoundaries) {
    auto bounds = BoundaryValues::forStringLength(100);

    EXPECT_FALSE(bounds.empty());
    EXPECT_NE(std::find(bounds.begin(), bounds.end(), 0ul), bounds.end());
    EXPECT_NE(std::find(bounds.begin(), bounds.end(), 100ul), bounds.end());
}

TEST_F(BoundaryValuesTest, ArrayIndexBoundaries) {
    auto bounds = BoundaryValues::forArrayIndex(10);

    EXPECT_FALSE(bounds.empty());
    EXPECT_NE(std::find(bounds.begin(), bounds.end(), 0ul), bounds.end());
    EXPECT_NE(std::find(bounds.begin(), bounds.end(), 9ul), bounds.end());
}

// ============================================================================
// TestDataFixture Tests
// ============================================================================

class TestDataFixtureTest : public TestDataFixture {
protected:
    void SetUp() override { TestDataFixture::SetUp(); }
    void TearDown() override { TestDataFixture::TearDown(); }
};

TEST_F(TestDataFixtureTest, RandomDataAvailable) {
    EXPECT_NE(random(), nullptr);
}

TEST_F(TestDataFixtureTest, UseRandomData) {
    int value = random()->randomInt(0, 100);
    EXPECT_GE(value, 0);
    EXPECT_LE(value, 100);
}

// ============================================================================
// Builder Pattern Edge Cases
// ============================================================================

class BuilderEdgeCasesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BuilderEdgeCasesTest, EmptyBuilder) {
    auto person = TestDataBuilder<Person>().build();
    EXPECT_TRUE(person.name.empty());
}

TEST_F(BuilderEdgeCasesTest, BuildManyZero) {
    auto people = TestDataBuilder<Person>().buildMany(0);
    EXPECT_TRUE(people.empty());
}

TEST_F(BuilderEdgeCasesTest, ChainedSets) {
    auto person = TestDataBuilder<Person>()
                      .set(&Person::name, std::string("First"))
                      .set(&Person::name, std::string("Second"))
                      .build();

    EXPECT_EQ(person.name, "Second");
}

}  // namespace atom::test::utilities::tests
