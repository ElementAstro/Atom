/**
 * @file typed_test.hpp
 * @brief Type-parameterized tests for testing template code
 * @details Provides TYPED_TEST and TYPED_TEST_SUITE macros for GTest-style typed
 * tests
 *
 * @author Max Qian
 * @copyright GPL3 License
 */

#ifndef ATOM_TEST_FIXTURES_TYPED_TEST_HPP
#define ATOM_TEST_FIXTURES_TYPED_TEST_HPP

#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <vector>

#include "atom/tests/core/test.hpp"
#include "atom/tests/fixtures/test_fixture.hpp"

namespace atom::test {

/**
 * @brief Type list for typed tests
 * @tparam Types The types to test with
 */
template <typename... Types>
struct TypeList {
    static constexpr size_t size = sizeof...(Types);
    using types = std::tuple<Types...>;
};

// Alias for GTest compatibility
template <typename... Types>
using Types = TypeList<Types...>;

/**
 * @brief Get human-readable type name
 */
template <typename T>
std::string getTypeName() {
    // Use compiler-specific type name demangling
#if defined(__GNUC__) || defined(__clang__)
    const char* name = typeid(T).name();
    // Basic demangling for common types
    if constexpr (std::is_same_v<T, int>) return "int";
    if constexpr (std::is_same_v<T, long>) return "long";
    if constexpr (std::is_same_v<T, long long>) return "long long";
    if constexpr (std::is_same_v<T, unsigned int>) return "unsigned int";
    if constexpr (std::is_same_v<T, unsigned long>) return "unsigned long";
    if constexpr (std::is_same_v<T, float>) return "float";
    if constexpr (std::is_same_v<T, double>) return "double";
    if constexpr (std::is_same_v<T, char>) return "char";
    if constexpr (std::is_same_v<T, bool>) return "bool";
    if constexpr (std::is_same_v<T, std::string>) return "std::string";
    return name;
#elif defined(_MSC_VER)
    return typeid(T).name();
#else
    return typeid(T).name();
#endif
}

/**
 * @brief Base class for typed test fixtures
 * @tparam T The type being tested
 */
template <typename T>
class TypedTestFixture : public TestFixture {
public:
    using TypeParam = T;

protected:
    /**
     * @brief Get the current type parameter
     * @return Reference to the type (for type traits)
     */
    static constexpr auto GetTypeParam() -> TypeParam* { return nullptr; }
};

/**
 * @brief Helper to extract type at index from type list
 */
template <size_t I, typename TypeList>
struct TypeAt;

template <size_t I, typename... Types>
struct TypeAt<I, TypeList<Types...>> {
    using type = std::tuple_element_t<I, std::tuple<Types...>>;
};

template <size_t I, typename TypeList>
using TypeAt_t = typename TypeAt<I, TypeList>::type;

/**
 * @brief Typed test registrar for automatic test registration
 */
template <typename FixtureTemplate, typename TypeList>
class TypedTestRegistrar;

template <typename FixtureTemplate, typename... Types>
class TypedTestRegistrar<FixtureTemplate, TypeList<Types...>> {
public:
    /**
     * @brief Register a typed test for all types in the list
     * @param suiteName Name of the test suite
     * @param testName Name of the test
     * @param testMethod Test method to run
     */
    template <typename TestMethod>
    static void registerTest(const std::string& suiteName,
                             const std::string& testName, TestMethod testMethod) {
        registerTestImpl<0, Types...>(suiteName, testName, testMethod);
    }

private:
    template <size_t I, typename T, typename... Rest, typename TestMethod>
    static void registerTestImpl(const std::string& suiteName,
                                 const std::string& testName,
                                 TestMethod testMethod) {
        // Register test for type T
        std::string fullName =
            suiteName + "/" + getTypeName<T>() + "." + testName;

        auto testFunc = [testMethod]() {
            using ConcreteFixture = FixtureTemplate<T>;
            ConcreteFixture fixture;
            fixture.SetUp();
            try {
                testMethod(fixture);
            } catch (...) {
                fixture.TearDown();
                throw;
            }
            fixture.TearDown();
        };

        atom::test::registerTest(fullName, std::move(testFunc));

        // Recursively register for remaining types
        if constexpr (sizeof...(Rest) > 0) {
            registerTestImpl<I + 1, Rest...>(suiteName, testName, testMethod);
        }
    }
};

/**
 * @brief Storage for typed test type lists
 */
template <typename TestSuite>
struct TypedTestTypes {
    // Default empty type list - must be specialized
};

/**
 * @brief Typed test suite with parameterized types
 */
template <template <typename> class FixtureTemplate, typename TypeListT>
class TypedTestSuite {
public:
    using Types = TypeListT;

    template <typename TestMethod>
    static void addTest(const std::string& suiteName,
                        const std::string& testName, TestMethod testMethod) {
        TypedTestRegistrar<FixtureTemplate, TypeListT>::registerTest(
            suiteName, testName, testMethod);
    }
};

/**
 * @brief Type-parameterized test case (P suffix for parameterized)
 */
template <typename TypeList>
class TypeParameterizedTestCase;

template <typename... Types>
class TypeParameterizedTestCase<TypeList<Types...>> {
public:
    template <typename TestMethod>
    static void run(const std::string& prefix, const std::string& testName,
                    TestMethod testMethod) {
        runImpl<Types...>(prefix, testName, testMethod);
    }

private:
    template <typename T, typename... Rest, typename TestMethod>
    static void runImpl(const std::string& prefix, const std::string& testName,
                        TestMethod testMethod) {
        std::string fullName = prefix + "/" + getTypeName<T>() + "." + testName;

        auto testFunc = [testMethod]() {
            T typeInstance{};
            testMethod(typeInstance);
        };

        atom::test::registerTest(fullName, std::move(testFunc));

        if constexpr (sizeof...(Rest) > 0) {
            runImpl<Rest...>(prefix, testName, testMethod);
        }
    }
};

/**
 * @brief Helper for type-parameterized test without fixture
 */
template <typename TypeListT>
class TypedTestWithoutFixture {
public:
    template <typename TestFunc>
    static void registerTests(const std::string& suiteName,
                              const std::string& testName, TestFunc func) {
        registerImpl<TypeListT>(suiteName, testName, func,
                                std::make_index_sequence<TypeListT::size>{});
    }

private:
    template <typename TL, typename TestFunc, size_t... Is>
    static void registerImpl(const std::string& suiteName,
                             const std::string& testName, TestFunc func,
                             std::index_sequence<Is...>) {
        (registerSingle<TypeAt_t<Is, TL>>(suiteName, testName, func), ...);
    }

    template <typename T, typename TestFunc>
    static void registerSingle(const std::string& suiteName,
                               const std::string& testName, TestFunc func) {
        std::string fullName =
            suiteName + "/" + getTypeName<T>() + "." + testName;
        atom::test::registerTest(fullName, [func]() { func.template operator()<T>(); });
    }
};

// Common type lists for convenience
using IntegerTypes = Types<short, int, long, long long>;
using UnsignedTypes = Types<unsigned short, unsigned int, unsigned long,
                            unsigned long long>;
using FloatingTypes = Types<float, double, long double>;
using NumericTypes = Types<short, int, long, long long, float, double>;
using CharTypes = Types<char, signed char, unsigned char, wchar_t, char16_t,
                        char32_t>;
using PodTypes = Types<char, short, int, long, float, double>;

}  // namespace atom::test

/**
 * @brief Define a typed test suite with a fixture and type list
 * @param fixture_name The fixture template name
 * @param types_list The type list to use
 */
#define TYPED_TEST_SUITE(fixture_name, types_list)                             \
    template <>                                                                \
    struct atom::test::TypedTestTypes<fixture_name> {                          \
        using Types = types_list;                                              \
    }

// GTest compatibility alias
#define TYPED_TEST_CASE(fixture_name, types_list) \
    TYPED_TEST_SUITE(fixture_name, types_list)

/**
 * @brief Define a typed test case
 * @param fixture_name The fixture template name
 * @param test_name The test case name
 */
#define TYPED_TEST(fixture_name, test_name)                                    \
    template <typename TypeParam>                                              \
    class fixture_name##_##test_name##_Test : public fixture_name<TypeParam> { \
    public:                                                                    \
        void TestBody();                                                       \
    };                                                                         \
    template <typename TypeParam>                                              \
    struct fixture_name##_##test_name##_Registrar {                            \
        fixture_name##_##test_name##_Registrar() {                             \
            using Types =                                                      \
                typename atom::test::TypedTestTypes<fixture_name>::Types;      \
            registerAll<Types>(std::make_index_sequence<Types::size>{});       \
        }                                                                      \
        template <typename TL, size_t... Is>                                   \
        void registerAll(std::index_sequence<Is...>) {                         \
            (registerSingle<atom::test::TypeAt_t<Is, TL>>(), ...);             \
        }                                                                      \
        template <typename T>                                                  \
        void registerSingle() {                                                \
            std::string fullName = std::string(#fixture_name) + "/" +          \
                                   atom::test::getTypeName<T>() + "." +        \
                                   #test_name;                                 \
            atom::test::registerTest(fullName, []() {                          \
                fixture_name##_##test_name##_Test<T> fixture;                  \
                fixture.SetUp();                                               \
                try {                                                          \
                    fixture.TestBody();                                        \
                } catch (...) {                                                \
                    fixture.TearDown();                                        \
                    throw;                                                     \
                }                                                              \
                fixture.TearDown();                                            \
            });                                                                \
        }                                                                      \
    };                                                                         \
    static fixture_name##_##test_name##_Registrar<void>                        \
        fixture_name##_##test_name##_registrar_instance;                       \
    template <typename TypeParam>                                              \
    void fixture_name##_##test_name##_Test<TypeParam>::TestBody()

/**
 * @brief Type-parameterized test suite (for more advanced use)
 */
#define TYPED_TEST_SUITE_P(suite_name)                                         \
    template <typename TypeParam>                                              \
    class suite_name : public atom::test::TypedTestFixture<TypeParam>

/**
 * @brief Register a type-parameterized test
 */
#define REGISTER_TYPED_TEST_SUITE_P(suite_name, ...)                           \
    template <typename TypeParam>                                              \
    struct suite_name##_TestNames {                                            \
        static constexpr const char* names[] = {__VA_ARGS__};                  \
    }

/**
 * @brief Instantiate typed tests for specific types
 */
#define INSTANTIATE_TYPED_TEST_SUITE_P(prefix, suite_name, types_list)         \
    static struct prefix##_##suite_name##_Instantiator {                       \
        prefix##_##suite_name##_Instantiator() {                               \
            /* Instantiation logic */                                          \
        }                                                                      \
    } prefix##_##suite_name##_instantiator_instance

/**
 * @brief Helper macro to get TypeParam in TYPED_TEST body
 */
#define TypeParam TypeParam

#endif  // ATOM_TEST_FIXTURES_TYPED_TEST_HPP
