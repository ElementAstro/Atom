/**
 * @file test_fixture.hpp
 * @brief Test fixtures for setup/teardown lifecycle management
 * @details Provides TestFixture base class and TEST_F macro for fixture-based
 * tests
 *
 * @author Max Qian
 * @copyright GPL3 License
 */

#ifndef ATOM_TEST_FIXTURES_TEST_FIXTURE_HPP
#define ATOM_TEST_FIXTURES_TEST_FIXTURE_HPP

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "atom/tests/core/test.hpp"

namespace atom::test {

/**
 * @brief Base class for test fixtures
 * @details Provides SetUp/TearDown lifecycle methods for test cases.
 *          Derive from this class to create custom test fixtures.
 *
 * Usage:
 * @code
 * class MyFixture : public atom::test::TestFixture {
 * protected:
 *     void SetUp() override {
 *         // Initialize test resources
 *         myResource = std::make_unique<Resource>();
 *     }
 *
 *     void TearDown() override {
 *         // Cleanup test resources
 *         myResource.reset();
 *     }
 *
 *     std::unique_ptr<Resource> myResource;
 * };
 *
 * TEST_F(MyFixture, TestName) {
 *     // Use myResource here
 *     expect_not_null(myResource.get());
 * }
 * @endcode
 */
class TestFixture {
public:
    TestFixture() = default;
    virtual ~TestFixture() = default;

    TestFixture(const TestFixture&) = delete;
    auto operator=(const TestFixture&) -> TestFixture& = delete;
    TestFixture(TestFixture&&) = delete;
    auto operator=(TestFixture&&) -> TestFixture& = delete;

    /**
     * @brief Called before each test method
     * @details Override this to set up test resources
     */
    virtual void SetUp() {}

    /**
     * @brief Called after each test method
     * @details Override this to clean up test resources
     */
    virtual void TearDown() {}

    /**
     * @brief Called once before all tests in the fixture
     * @details Override this for expensive one-time setup
     */
    static void SetUpTestSuite() {}

    /**
     * @brief Called once after all tests in the fixture
     * @details Override this for one-time cleanup
     */
    static void TearDownTestSuite() {}

    /**
     * @brief Check if test has fatal failure
     * @return True if a fatal failure occurred
     */
    [[nodiscard]] bool HasFatalFailure() const { return hasFatalFailure_; }

    /**
     * @brief Check if test has any failure
     * @return True if any failure occurred
     */
    [[nodiscard]] bool HasFailure() const { return hasFailure_; }

    /**
     * @brief Mark test as having a fatal failure
     */
    void RecordFatalFailure() {
        hasFatalFailure_ = true;
        hasFailure_ = true;
    }

    /**
     * @brief Mark test as having a non-fatal failure
     */
    void RecordFailure() { hasFailure_ = true; }

protected:
    /**
     * @brief Record a property for the test result
     * @param key Property key
     * @param value Property value
     */
    void RecordProperty(const std::string& key, const std::string& value) {
        atom::test::recordProperty(key, value);
    }

    void RecordProperty(const std::string& key, int value) {
        RecordProperty(key, std::to_string(value));
    }

private:
    bool hasFatalFailure_{false};
    bool hasFailure_{false};
};

/**
 * @brief Helper class for fixture-based test registration
 * @tparam FixtureType The fixture class type
 */
template <typename FixtureType>
class FixtureTestRegistrar {
    static_assert(std::is_base_of_v<TestFixture, FixtureType>,
                  "FixtureType must derive from TestFixture");

public:
    /**
     * @brief Register a test method with this fixture
     * @param testName Name of the test
     * @param testMethod Test method to run
     * @param suiteName Optional suite name (defaults to fixture class name)
     */
    static void registerTest(std::string testName,
                             void (FixtureType::*testMethod)(),
                             std::string suiteName = "") {
        if (suiteName.empty()) {
            suiteName = getFixtureName();
        }

        auto testFunc = [testMethod]() {
            auto fixture = std::make_unique<FixtureType>();
            fixture->SetUp();
            try {
                (fixture.get()->*testMethod)();
            } catch (...) {
                fixture->TearDown();
                throw;
            }
            fixture->TearDown();
        };

        atom::test::registerTest(suiteName + "." + testName, std::move(testFunc));
    }

    /**
     * @brief Register a test with a lambda function
     * @param testName Name of the test
     * @param testFunc Lambda that takes FixtureType& as parameter
     * @param suiteName Optional suite name
     */
    template <typename Func>
    static void registerTestLambda(std::string testName, Func&& testFunc,
                                   std::string suiteName = "") {
        if (suiteName.empty()) {
            suiteName = getFixtureName();
        }

        auto wrappedFunc = [func = std::forward<Func>(testFunc)]() {
            auto fixture = std::make_unique<FixtureType>();
            fixture->SetUp();
            try {
                func(*fixture);
            } catch (...) {
                fixture->TearDown();
                throw;
            }
            fixture->TearDown();
        };

        atom::test::registerTest(suiteName + "." + testName,
                                 std::move(wrappedFunc));
    }

private:
    static auto getFixtureName() -> std::string {
#if defined(__GNUC__) || defined(__clang__)
        std::string name = __PRETTY_FUNCTION__;
        // Extract FixtureType from the function signature
        auto start = name.find("FixtureType = ") + 14;
        auto end = name.find("]", start);
        if (start != std::string::npos && end != std::string::npos) {
            return name.substr(start, end - start);
        }
#elif defined(_MSC_VER)
        std::string name = __FUNCSIG__;
        auto start = name.find("FixtureTestRegistrar<") + 21;
        auto end = name.find(">", start);
        if (start != std::string::npos && end != std::string::npos) {
            return name.substr(start, end - start);
        }
#endif
        return "UnknownFixture";
    }
};

/**
 * @brief Helper for automatic test registration with fixtures
 */
template <typename FixtureType>
class FixtureTestAutoRegistrar {
public:
    FixtureTestAutoRegistrar(std::string testName,
                             void (FixtureType::*testMethod)(),
                             std::string suiteName = "") {
        FixtureTestRegistrar<FixtureType>::registerTest(
            std::move(testName), testMethod, std::move(suiteName));
    }

    template <typename Func>
    FixtureTestAutoRegistrar(std::string testName, Func&& testFunc,
                             std::string suiteName = "") {
        FixtureTestRegistrar<FixtureType>::registerTestLambda(
            std::move(testName), std::forward<Func>(testFunc),
            std::move(suiteName));
    }
};

/**
 * @brief Fixture test suite builder for fluent API
 * @tparam FixtureType The fixture class type
 */
template <typename FixtureType>
class FixtureTestSuiteBuilder {
    static_assert(std::is_base_of_v<TestFixture, FixtureType>,
                  "FixtureType must derive from TestFixture");

public:
    explicit FixtureTestSuiteBuilder(std::string suiteName)
        : suiteName_(std::move(suiteName)) {}

    ~FixtureTestSuiteBuilder() {
        if (!tests_.empty()) {
            FixtureType::SetUpTestSuite();
            for (auto& [name, func] : tests_) {
                atom::test::registerTest(suiteName_ + "." + name,
                                         std::move(func));
            }
        }
    }

    FixtureTestSuiteBuilder(const FixtureTestSuiteBuilder&) = delete;
    auto operator=(const FixtureTestSuiteBuilder&)
        -> FixtureTestSuiteBuilder& = delete;
    FixtureTestSuiteBuilder(FixtureTestSuiteBuilder&&) = delete;
    auto operator=(FixtureTestSuiteBuilder&&)
        -> FixtureTestSuiteBuilder& = delete;

    /**
     * @brief Add a test to this fixture suite
     * @param testName Name of the test
     * @param testFunc Test function that takes FixtureType& as parameter
     * @return Reference for method chaining
     */
    template <typename Func>
    auto addTest(std::string testName, Func&& testFunc)
        -> FixtureTestSuiteBuilder& {
        auto wrappedFunc = [func = std::forward<Func>(testFunc)]() {
            auto fixture = std::make_unique<FixtureType>();
            fixture->SetUp();
            try {
                func(*fixture);
            } catch (...) {
                fixture->TearDown();
                throw;
            }
            fixture->TearDown();
        };

        tests_.emplace_back(std::move(testName), std::move(wrappedFunc));
        return *this;
    }

private:
    std::string suiteName_;
    std::vector<std::pair<std::string, std::function<void()>>> tests_;
};

/**
 * @brief RAII resource guard for test fixtures
 */
template <typename Resource>
class TestResource {
public:
    template <typename CreateFunc, typename DestroyFunc>
    TestResource(CreateFunc create, DestroyFunc destroy)
        : resource_(create()), destroy_(std::move(destroy)) {}

    ~TestResource() {
        if (destroy_) {
            destroy_(resource_);
        }
    }

    TestResource(const TestResource&) = delete;
    TestResource& operator=(const TestResource&) = delete;

    TestResource(TestResource&& other) noexcept
        : resource_(std::move(other.resource_)),
          destroy_(std::move(other.destroy_)) {
        other.destroy_ = nullptr;
    }

    TestResource& operator=(TestResource&& other) noexcept {
        if (this != &other) {
            if (destroy_) {
                destroy_(resource_);
            }
            resource_ = std::move(other.resource_);
            destroy_ = std::move(other.destroy_);
            other.destroy_ = nullptr;
        }
        return *this;
    }

    Resource& get() { return resource_; }
    const Resource& get() const { return resource_; }

    Resource* operator->() { return &resource_; }
    const Resource* operator->() const { return &resource_; }

    Resource& operator*() { return resource_; }
    const Resource& operator*() const { return resource_; }

private:
    Resource resource_;
    std::function<void(Resource&)> destroy_;
};

/**
 * @brief Shared fixture for expensive resources
 */
template <typename FixtureType>
class SharedFixture : public TestFixture {
public:
    static FixtureType& GetSharedInstance() {
        static FixtureType instance;
        return instance;
    }

    static void SetUpTestSuite() { GetSharedInstance().SetUp(); }

    static void TearDownTestSuite() { GetSharedInstance().TearDown(); }
};

}  // namespace atom::test

/**
 * @brief Define a test case using a fixture
 * @param fixture_class The fixture class name
 * @param test_name The test case name
 */
#define TEST_F(fixture_class, test_name)                                      \
    class fixture_class##_##test_name##_Test : public fixture_class {         \
    public:                                                                   \
        void TestBody();                                                      \
    };                                                                        \
    static struct fixture_class##_##test_name##_Registrar {                   \
        fixture_class##_##test_name##_Registrar() {                           \
            atom::test::registerTest(                                         \
                #fixture_class "." #test_name, []() {                         \
                    fixture_class##_##test_name##_Test fixture;               \
                    fixture.SetUp();                                          \
                    try {                                                     \
                        fixture.TestBody();                                   \
                    } catch (...) {                                           \
                        fixture.TearDown();                                   \
                        throw;                                                \
                    }                                                         \
                    fixture.TearDown();                                       \
                });                                                           \
        }                                                                     \
    } fixture_class##_##test_name##_registrar_instance;                       \
    void fixture_class##_##test_name##_Test::TestBody()

/**
 * @brief Define a simple test case without a fixture
 * @param suite_name The test suite name
 * @param test_name The test case name
 */
#define TEST(suite_name, test_name)                                           \
    static void suite_name##_##test_name##_TestBody();                        \
    static struct suite_name##_##test_name##_Registrar {                      \
        suite_name##_##test_name##_Registrar() {                              \
            atom::test::registerTest(#suite_name "." #test_name,              \
                                     suite_name##_##test_name##_TestBody);    \
        }                                                                     \
    } suite_name##_##test_name##_registrar_instance;                          \
    static void suite_name##_##test_name##_TestBody()

/**
 * @brief Define a disabled test case
 * @param suite_name The test suite name
 * @param test_name The test case name
 */
#define TEST_DISABLED(suite_name, test_name)                                  \
    static void suite_name##_##test_name##_TestBody();                        \
    static struct suite_name##_##test_name##_Registrar {                      \
        suite_name##_##test_name##_Registrar() {                              \
            atom::test::registerTest(                                         \
                #suite_name "." #test_name,                                   \
                suite_name##_##test_name##_TestBody, false, 0.0, true);       \
        }                                                                     \
    } suite_name##_##test_name##_registrar_instance;                          \
    static void suite_name##_##test_name##_TestBody()

/**
 * @brief Define a disabled fixture test case
 * @param fixture_class The fixture class name
 * @param test_name The test case name
 */
#define TEST_F_DISABLED(fixture_class, test_name)                             \
    class fixture_class##_##test_name##_Test : public fixture_class {         \
    public:                                                                   \
        void TestBody();                                                      \
    };                                                                        \
    static struct fixture_class##_##test_name##_Registrar {                   \
        fixture_class##_##test_name##_Registrar() {                           \
            atom::test::registerTest(                                         \
                #fixture_class "." #test_name, []() {                         \
                    fixture_class##_##test_name##_Test fixture;               \
                    fixture.SetUp();                                          \
                    try {                                                     \
                        fixture.TestBody();                                   \
                    } catch (...) {                                           \
                        fixture.TearDown();                                   \
                        throw;                                                \
                    }                                                         \
                    fixture.TearDown();                                       \
                },                                                            \
                false, 0.0, true);                                            \
        }                                                                     \
    } fixture_class##_##test_name##_registrar_instance;                       \
    void fixture_class##_##test_name##_Test::TestBody()

/**
 * @brief Define a test with timeout
 * @param suite_name The test suite name
 * @param test_name The test case name
 * @param timeout_ms Timeout in milliseconds
 */
#define TEST_TIMEOUT(suite_name, test_name, timeout_ms)                       \
    static void suite_name##_##test_name##_TestBody();                        \
    static struct suite_name##_##test_name##_Registrar {                      \
        suite_name##_##test_name##_Registrar() {                              \
            atom::test::registerTest(#suite_name "." #test_name,              \
                                     suite_name##_##test_name##_TestBody,     \
                                     true, static_cast<double>(timeout_ms));  \
        }                                                                     \
    } suite_name##_##test_name##_registrar_instance;                          \
    static void suite_name##_##test_name##_TestBody()

/**
 * @brief Define a fixture test with timeout
 * @param fixture_class The fixture class name
 * @param test_name The test case name
 * @param timeout_ms Timeout in milliseconds
 */
#define TEST_F_TIMEOUT(fixture_class, test_name, timeout_ms)                  \
    class fixture_class##_##test_name##_Test : public fixture_class {         \
    public:                                                                   \
        void TestBody();                                                      \
    };                                                                        \
    static struct fixture_class##_##test_name##_Registrar {                   \
        fixture_class##_##test_name##_Registrar() {                           \
            atom::test::registerTest(                                         \
                #fixture_class "." #test_name, []() {                         \
                    fixture_class##_##test_name##_Test fixture;               \
                    fixture.SetUp();                                          \
                    try {                                                     \
                        fixture.TestBody();                                   \
                    } catch (...) {                                           \
                        fixture.TearDown();                                   \
                        throw;                                                \
                    }                                                         \
                    fixture.TearDown();                                       \
                },                                                            \
                true, static_cast<double>(timeout_ms));                       \
        }                                                                     \
    } fixture_class##_##test_name##_registrar_instance;                       \
    void fixture_class##_##test_name##_Test::TestBody()

/**
 * @brief Define a test with tags for filtering
 * @param suite_name The test suite name
 * @param test_name The test case name
 * @param ... Tags (comma-separated strings)
 */
#define TEST_TAGGED(suite_name, test_name, ...)                               \
    static void suite_name##_##test_name##_TestBody();                        \
    static struct suite_name##_##test_name##_Registrar {                      \
        suite_name##_##test_name##_Registrar() {                              \
            atom::test::registerTest(#suite_name "." #test_name,              \
                                     suite_name##_##test_name##_TestBody,     \
                                     false, 0.0, false, {},                   \
                                     std::vector<std::string>{__VA_ARGS__});  \
        }                                                                     \
    } suite_name##_##test_name##_registrar_instance;                          \
    static void suite_name##_##test_name##_TestBody()

/**
 * @brief GTest-style ASSERT macros (fatal - stops test on failure)
 */
#define ASSERT_TRUE(expr)                                                     \
    do {                                                                      \
        if (!(expr)) {                                                        \
            throw std::runtime_error(std::string(__FILE__) + ":" +            \
                                     std::to_string(__LINE__) +               \
                                     ": ASSERT_TRUE failed: " #expr);         \
        }                                                                     \
    } while (0)

#define ASSERT_FALSE(expr)                                                    \
    do {                                                                      \
        if (expr) {                                                           \
            throw std::runtime_error(std::string(__FILE__) + ":" +            \
                                     std::to_string(__LINE__) +               \
                                     ": ASSERT_FALSE failed: " #expr);        \
        }                                                                     \
    } while (0)

#define ASSERT_EQ(lhs, rhs)                                                   \
    do {                                                                      \
        if (!((lhs) == (rhs))) {                                              \
            throw std::runtime_error(std::string(__FILE__) + ":" +            \
                                     std::to_string(__LINE__) +               \
                                     ": ASSERT_EQ failed: " #lhs " != " #rhs);\
        }                                                                     \
    } while (0)

#define ASSERT_NE(lhs, rhs)                                                   \
    do {                                                                      \
        if ((lhs) == (rhs)) {                                                 \
            throw std::runtime_error(std::string(__FILE__) + ":" +            \
                                     std::to_string(__LINE__) +               \
                                     ": ASSERT_NE failed: " #lhs " == " #rhs);\
        }                                                                     \
    } while (0)

#define ASSERT_LT(lhs, rhs)                                                   \
    do {                                                                      \
        if (!((lhs) < (rhs))) {                                               \
            throw std::runtime_error(std::string(__FILE__) + ":" +            \
                                     std::to_string(__LINE__) +               \
                                     ": ASSERT_LT failed: " #lhs " >= " #rhs);\
        }                                                                     \
    } while (0)

#define ASSERT_LE(lhs, rhs)                                                   \
    do {                                                                      \
        if (!((lhs) <= (rhs))) {                                              \
            throw std::runtime_error(std::string(__FILE__) + ":" +            \
                                     std::to_string(__LINE__) +               \
                                     ": ASSERT_LE failed: " #lhs " > " #rhs); \
        }                                                                     \
    } while (0)

#define ASSERT_GT(lhs, rhs)                                                   \
    do {                                                                      \
        if (!((lhs) > (rhs))) {                                               \
            throw std::runtime_error(std::string(__FILE__) + ":" +            \
                                     std::to_string(__LINE__) +               \
                                     ": ASSERT_GT failed: " #lhs " <= " #rhs);\
        }                                                                     \
    } while (0)

#define ASSERT_GE(lhs, rhs)                                                   \
    do {                                                                      \
        if (!((lhs) >= (rhs))) {                                              \
            throw std::runtime_error(std::string(__FILE__) + ":" +            \
                                     std::to_string(__LINE__) +               \
                                     ": ASSERT_GE failed: " #lhs " < " #rhs); \
        }                                                                     \
    } while (0)

#define ASSERT_STREQ(lhs, rhs)                                                \
    do {                                                                      \
        if (std::string(lhs) != std::string(rhs)) {                           \
            throw std::runtime_error(                                         \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                ": ASSERT_STREQ failed: \"" + std::string(lhs) +              \
                "\" != \"" + std::string(rhs) + "\"");                        \
        }                                                                     \
    } while (0)

#define ASSERT_STRNE(lhs, rhs)                                                \
    do {                                                                      \
        if (std::string(lhs) == std::string(rhs)) {                           \
            throw std::runtime_error(                                         \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                ": ASSERT_STRNE failed: \"" + std::string(lhs) +              \
                "\" == \"" + std::string(rhs) + "\"");                        \
        }                                                                     \
    } while (0)

#define ASSERT_NEAR(val1, val2, abs_error)                                    \
    do {                                                                      \
        if (std::abs((val1) - (val2)) > (abs_error)) {                        \
            throw std::runtime_error(std::string(__FILE__) + ":" +            \
                                     std::to_string(__LINE__) +               \
                                     ": ASSERT_NEAR failed");                 \
        }                                                                     \
    } while (0)

#define ASSERT_THROW(statement, exception_type)                               \
    do {                                                                      \
        bool caught = false;                                                  \
        try {                                                                 \
            statement;                                                        \
        } catch (const exception_type&) {                                     \
            caught = true;                                                    \
        } catch (...) {                                                       \
        }                                                                     \
        if (!caught) {                                                        \
            throw std::runtime_error(                                         \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                ": ASSERT_THROW failed: expected " #exception_type);          \
        }                                                                     \
    } while (0)

#define ASSERT_NO_THROW(statement)                                            \
    do {                                                                      \
        try {                                                                 \
            statement;                                                        \
        } catch (...) {                                                       \
            throw std::runtime_error(std::string(__FILE__) + ":" +            \
                                     std::to_string(__LINE__) +               \
                                     ": ASSERT_NO_THROW failed");             \
        }                                                                     \
    } while (0)

/**
 * @brief GTest-style EXPECT macros (non-fatal - continues test on failure)
 */
#define EXPECT_TRUE(expr) expect_true(expr)
#define EXPECT_FALSE(expr) expect_false(expr)
#define EXPECT_EQ(lhs, rhs) expect_eq(lhs, rhs)
#define EXPECT_NE(lhs, rhs) expect_ne(lhs, rhs)
#define EXPECT_LT(lhs, rhs) expect_lt(lhs, rhs)
#define EXPECT_LE(lhs, rhs) expect_le(lhs, rhs)
#define EXPECT_GT(lhs, rhs) expect_gt(lhs, rhs)
#define EXPECT_GE(lhs, rhs) expect_ge(lhs, rhs)
#define EXPECT_STREQ(lhs, rhs) expect_eq(std::string(lhs), std::string(rhs))
#define EXPECT_STRNE(lhs, rhs) expect_ne(std::string(lhs), std::string(rhs))
#define EXPECT_NEAR(val1, val2, abs_error) expect_near(val1, val2, abs_error)
#define EXPECT_THROW(statement, exception_type) \
    expect_throws_as([&]() { statement; }, exception_type)
#define EXPECT_NO_THROW(statement) expect_no_throw([&]() { statement; })

/**
 * @brief Skip current test
 */
#define GTEST_SKIP() \
    throw std::runtime_error("SKIPPED: Test skipped via GTEST_SKIP()")

#define GTEST_SKIP_MESSAGE(msg) \
    throw std::runtime_error(std::string("SKIPPED: ") + (msg))

#endif  // ATOM_TEST_FIXTURES_TEST_FIXTURE_HPP
