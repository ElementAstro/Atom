/**
 * @file test_data_factory.cpp
 * @brief Test data factory functionality in the Atom Test Framework
 */

#include "atom/tests/atom_test.hpp"

#include <algorithm>
#include <numeric>

using namespace atom::test;

// ============================================================================
// Test Data Builder Tests
// ============================================================================

struct Person {
    std::string name;
    int age = 0;
    std::string email;
    bool active = false;
};

TEST(DataBuilder, BasicBuilder) {
    auto person = builder<Person>()
                      .set(&Person::name, std::string("John"))
                      .set(&Person::age, 30)
                      .set(&Person::email, std::string("john@example.com"))
                      .set(&Person::active, true)
                      .build();

    expect_eq(person.name, "John");
    expect_eq(person.age, 30);
    expect_eq(person.email, "john@example.com");
    expect_true(person.active);
}

TEST(DataBuilder, BuilderWithLambda) {
    auto person = builder<Person>()
                      .with([](Person& p) {
                          p.name = "Alice";
                          p.age = 25;
                      })
                      .build();

    expect_eq(person.name, "Alice");
    expect_eq(person.age, 25);
}

TEST(DataBuilder, BuildMany) {
    auto people = builder<Person>()
                      .set(&Person::name, std::string("Default"))
                      .set(&Person::age, 20)
                      .buildMany(5);

    expect_size(people, 5);
    for (const auto& person : people) {
        expect_eq(person.name, "Default");
        expect_eq(person.age, 20);
    }
}

TEST(DataBuilder, BuildWithVariations) {
    auto basePerson = builder<Person>()
                          .set(&Person::name, std::string("Base"))
                          .set(&Person::age, 30);

    std::vector<typename TestDataBuilder<Person>::BuilderFunc> variations = {
        [](Person& p) { p.name = "Alice"; p.age = 25; },
        [](Person& p) { p.name = "Bob"; p.age = 35; },
        [](Person& p) { p.name = "Charlie"; p.age = 40; }};

    auto people = basePerson.buildWithVariations(variations);

    expect_size(people, 3);
    expect_eq(people[0].name, "Alice");
    expect_eq(people[1].name, "Bob");
    expect_eq(people[2].name, "Charlie");
}

// ============================================================================
// Test Data Factory Tests
// ============================================================================

TEST(DataFactory, RegisterAndCreate) {
    TestDataFactory<Person> factory;

    factory.registerPreset("admin", []() {
        return Person{"Admin", 40, "admin@example.com", true};
    });

    factory.registerPreset("guest", []() {
        return Person{"Guest", 0, "", false};
    });

    auto admin = factory.create("admin");
    expect_eq(admin.name, "Admin");
    expect_eq(admin.age, 40);
    expect_true(admin.active);

    auto guest = factory.create("guest");
    expect_eq(guest.name, "Guest");
    expect_eq(guest.age, 0);
    expect_false(guest.active);
}

TEST(DataFactory, HasPreset) {
    TestDataFactory<Person> factory;

    factory.registerPreset("user", []() { return Person{}; });

    expect_true(factory.hasPreset("user"));
    expect_false(factory.hasPreset("nonexistent"));
}

TEST(DataFactory, GetPresetNames) {
    TestDataFactory<Person> factory;

    factory.registerPreset("preset1", []() { return Person{}; });
    factory.registerPreset("preset2", []() { return Person{}; });
    factory.registerPreset("preset3", []() { return Person{}; });

    auto names = factory.getPresetNames();
    expect_size(names, 3);
}

TEST(DataFactory, CreateMany) {
    TestDataFactory<Person> factory;

    factory.registerPreset("default", []() {
        return Person{"Default", 25, "default@example.com", true};
    });

    auto people = factory.createMany("default", 3);
    expect_size(people, 3);

    for (const auto& person : people) {
        expect_eq(person.name, "Default");
    }
}

// ============================================================================
// Random Test Data Tests
// ============================================================================

TEST(RandomData, RandomInt) {
    RandomTestData rng(42);  // Fixed seed for reproducibility

    int value = rng.randomInt(0, 100);
    expect_in_range(value, 0, 100);
}

TEST(RandomData, RandomReal) {
    RandomTestData rng(42);

    double value = rng.randomReal(0.0, 1.0);
    expect_in_range(value, 0.0, 1.0);
}

TEST(RandomData, RandomBool) {
    RandomTestData rng(42);

    int trueCount = 0;
    for (int i = 0; i < 100; ++i) {
        if (rng.randomBool(0.5)) {
            trueCount++;
        }
    }

    // Should be roughly 50%, allow some variance
    expect_in_range(trueCount, 30, 70);
}

TEST(RandomData, RandomString) {
    RandomTestData rng(42);

    std::string str = rng.randomString(10);
    expect_size(str, 10);
}

TEST(RandomData, RandomAlphanumeric) {
    RandomTestData rng(42);

    std::string str = rng.randomAlphanumeric(15);
    expect_size(str, 15);

    // Check all characters are alphanumeric
    expect_all_of(str, [](char c) { return std::isalnum(c); });
}

TEST(RandomData, RandomAlpha) {
    RandomTestData rng(42);

    std::string str = rng.randomAlpha(10);
    expect_size(str, 10);

    expect_all_of(str, [](char c) { return std::isalpha(c); });
}

TEST(RandomData, RandomNumeric) {
    RandomTestData rng(42);

    std::string str = rng.randomNumeric(8);
    expect_size(str, 8);

    expect_all_of(str, [](char c) { return std::isdigit(c); });
}

TEST(RandomData, RandomEmail) {
    RandomTestData rng(42);

    std::string email = rng.randomEmail();
    expect_contains(email, "@");
    expect_ends_with(email, ".com");
}

TEST(RandomData, RandomUuid) {
    RandomTestData rng(42);

    std::string uuid = rng.randomUuid();
    // UUID format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    expect_contains(uuid, "-");
}

TEST(RandomData, RandomElement) {
    RandomTestData rng(42);

    std::vector<int> vec = {1, 2, 3, 4, 5};
    int element = rng.randomElement(vec);

    expect_contains_element(vec, element);
}

TEST(RandomData, RandomChoice) {
    RandomTestData rng(42);

    std::string choice = rng.randomChoice({"apple", "banana", "cherry"});

    expect_any_of(
        std::vector<std::string>{"apple", "banana", "cherry"},
        [&choice](const std::string& s) { return s == choice; });
}

TEST(RandomData, RandomVector) {
    RandomTestData rng(42);

    auto vec = rng.randomVector<int>(10, [&rng]() {
        return rng.randomInt(0, 100);
    });

    expect_size(vec, 10);
    expect_all_of(vec, [](int x) { return x >= 0 && x <= 100; });
}

TEST(RandomData, Shuffle) {
    RandomTestData rng(42);

    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::vector<int> original = vec;

    rng.shuffle(vec);

    // Same elements, possibly different order
    expect_size(vec, original.size());

    std::sort(vec.begin(), vec.end());
    expect_eq(vec, original);
}

// ============================================================================
// Sequence Generator Tests
// ============================================================================

TEST(SequenceGenerator, Integers) {
    auto seq = SequenceGenerator::integers(0, 5);

    expect_eq(seq, std::vector<int>({0, 1, 2, 3, 4}));
}

TEST(SequenceGenerator, IntegersWithStep) {
    auto seq = SequenceGenerator::integers(0, 10, 2);

    expect_eq(seq, std::vector<int>({0, 2, 4, 6, 8}));
}

TEST(SequenceGenerator, Strings) {
    auto seq = SequenceGenerator::strings("item_{}", 3);

    expect_size(seq, 3);
    expect_eq(seq[0], "item_0");
    expect_eq(seq[1], "item_1");
    expect_eq(seq[2], "item_2");
}

TEST(SequenceGenerator, Fibonacci) {
    auto seq = SequenceGenerator::fibonacci(8);

    expect_eq(seq, std::vector<uint64_t>({0, 1, 1, 2, 3, 5, 8, 13}));
}

TEST(SequenceGenerator, PowersOf2) {
    auto seq = SequenceGenerator::powersOf2(5);

    expect_eq(seq, std::vector<uint64_t>({1, 2, 4, 8, 16}));
}

TEST(SequenceGenerator, Repeat) {
    auto seq = SequenceGenerator::repeat(42, 5);

    expect_size(seq, 5);
    expect_all_of(seq, [](int x) { return x == 42; });
}

TEST(SequenceGenerator, Cycle) {
    auto seq = SequenceGenerator::cycle(std::vector<int>{1, 2, 3}, 7);

    expect_eq(seq, std::vector<int>({1, 2, 3, 1, 2, 3, 1}));
}

// ============================================================================
// Boundary Values Tests
// ============================================================================

TEST(BoundaryValues, ForInteger) {
    auto bounds = BoundaryValues::forInteger<int>();

    expect_not_empty(bounds);
    expect_contains_element(bounds, 0);
    expect_contains_element(bounds, 1);
    expect_contains_element(bounds, -1);
    expect_contains_element(bounds, std::numeric_limits<int>::min());
    expect_contains_element(bounds, std::numeric_limits<int>::max());
}

TEST(BoundaryValues, ForUnsigned) {
    auto bounds = BoundaryValues::forUnsigned<unsigned int>();

    expect_not_empty(bounds);
    expect_contains_element(bounds, 0u);
    expect_contains_element(bounds, 1u);
    expect_contains_element(bounds, std::numeric_limits<unsigned int>::max());
}

TEST(BoundaryValues, ForFloat) {
    auto bounds = BoundaryValues::forFloat<double>();

    expect_not_empty(bounds);
    expect_contains_element(bounds, 0.0);
    expect_contains_element(bounds, 1.0);
    expect_contains_element(bounds, -1.0);
}

TEST(BoundaryValues, ForStringLength) {
    auto bounds = BoundaryValues::forStringLength(100);

    expect_contains_element(bounds, size_t(0));
    expect_contains_element(bounds, size_t(1));
    expect_contains_element(bounds, size_t(50));
    expect_contains_element(bounds, size_t(99));
    expect_contains_element(bounds, size_t(100));
}

TEST(BoundaryValues, ForArrayIndex) {
    auto bounds = BoundaryValues::forArrayIndex(10);

    expect_contains_element(bounds, size_t(0));
    expect_contains_element(bounds, size_t(1));
    expect_contains_element(bounds, size_t(5));
    expect_contains_element(bounds, size_t(9));
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    return runAllTests(argc, argv);
}
