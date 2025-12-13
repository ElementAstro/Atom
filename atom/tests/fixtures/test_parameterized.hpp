/**
 * @file test_parameterized.hpp
 * @brief Parameterized tests for data-driven testing
 * @details Provides TEST_P and INSTANTIATE_TEST_SUITE_P macros
 *
 * @author Max Qian
 * @copyright GPL3 License
 */

#ifndef ATOM_TEST_FIXTURES_TEST_PARAMETERIZED_HPP
#define ATOM_TEST_FIXTURES_TEST_PARAMETERIZED_HPP

#include <functional>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "atom/tests/core/test.hpp"
#include "atom/tests/fixtures/test_fixture.hpp"

namespace atom::test {

/**
 * @brief Generate test parameter names from values
 */
template <typename T>
auto parameterToString(const T& value) -> std::string {
    if constexpr (std::is_same_v<T, std::string>) {
        return value;
    } else if constexpr (std::is_same_v<T, const char*>) {
        return std::string(value);
    } else if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_arithmetic_v<T>) {
        return std::to_string(value);
    } else {
        std::ostringstream oss;
        oss << &value;  // Use address as fallback
        return oss.str();
    }
}

/**
 * @brief Parameter value container with name generation
 * @tparam T The parameter type
 */
template <typename T>
struct TestParam {
    T value;
    std::string name;

    TestParam(T val) : value(std::move(val)), name(parameterToString(value)) {}
    TestParam(T val, std::string paramName)
        : value(std::move(val)), name(std::move(paramName)) {}
};

/**
 * @brief Base class for parameterized tests
 * @tparam ParamType The type of test parameter
 */
template <typename ParamType>
class ParameterizedTest : public TestFixture {
public:
    using ParamT = ParamType;

    /**
     * @brief Get the current test parameter
     * @return The current parameter value
     */
    [[nodiscard]] auto GetParam() const -> const ParamType& { return param_; }

    /**
     * @brief Set the test parameter (called by test infrastructure)
     * @param param The parameter value
     */
    void SetParam(const ParamType& param) { param_ = param; }

private:
    ParamType param_{};
};

/**
 * @brief Helper to create a vector of test parameters
 * @tparam T The parameter type
 * @param values Initializer list of values
 * @return Vector of TestParam objects
 */
template <typename T>
auto Values(std::initializer_list<T> values) -> std::vector<TestParam<T>> {
    std::vector<TestParam<T>> result;
    result.reserve(values.size());
    for (const auto& v : values) {
        result.emplace_back(v);
    }
    return result;
}

/**
 * @brief Helper to create named test parameters
 * @tparam T The parameter type
 * @param nameValuePairs Pairs of (name, value)
 * @return Vector of TestParam objects
 */
template <typename T>
auto ValuesIn(std::initializer_list<std::pair<std::string, T>> nameValuePairs)
    -> std::vector<TestParam<T>> {
    std::vector<TestParam<T>> result;
    result.reserve(nameValuePairs.size());
    for (const auto& [name, value] : nameValuePairs) {
        result.emplace_back(value, name);
    }
    return result;
}

/**
 * @brief Create parameters from a container
 */
template <typename Container>
auto ValuesIn(const Container& container)
    -> std::vector<TestParam<typename Container::value_type>> {
    using T = typename Container::value_type;
    std::vector<TestParam<T>> result;
    result.reserve(container.size());
    for (const auto& v : container) {
        result.emplace_back(v);
    }
    return result;
}

/**
 * @brief Create a range of integer parameters
 * @param start Start value (inclusive)
 * @param end End value (exclusive)
 * @param step Step size (default: 1)
 * @return Vector of TestParam<int>
 */
inline auto Range(int start, int end, int step = 1)
    -> std::vector<TestParam<int>> {
    std::vector<TestParam<int>> result;
    for (int i = start; i < end; i += step) {
        result.emplace_back(i);
    }
    return result;
}

/**
 * @brief Boolean parameter generator
 * @return Vector with true and false parameters
 */
inline auto Bool() -> std::vector<TestParam<bool>> {
    return {TestParam<bool>(false, "false"), TestParam<bool>(true, "true")};
}

/**
 * @brief Combine multiple parameter sets (Cartesian product) - 2 dimensions
 */
template <typename T1, typename T2>
auto Combine(const std::vector<TestParam<T1>>& params1,
             const std::vector<TestParam<T2>>& params2)
    -> std::vector<TestParam<std::tuple<T1, T2>>> {
    std::vector<TestParam<std::tuple<T1, T2>>> result;
    result.reserve(params1.size() * params2.size());

    for (const auto& p1 : params1) {
        for (const auto& p2 : params2) {
            std::string combinedName = p1.name + "_" + p2.name;
            result.emplace_back(std::make_tuple(p1.value, p2.value),
                                combinedName);
        }
    }

    return result;
}

/**
 * @brief Combine 3 dimensions
 */
template <typename T1, typename T2, typename T3>
auto Combine(const std::vector<TestParam<T1>>& params1,
             const std::vector<TestParam<T2>>& params2,
             const std::vector<TestParam<T3>>& params3)
    -> std::vector<TestParam<std::tuple<T1, T2, T3>>> {
    std::vector<TestParam<std::tuple<T1, T2, T3>>> result;
    result.reserve(params1.size() * params2.size() * params3.size());

    for (const auto& p1 : params1) {
        for (const auto& p2 : params2) {
            for (const auto& p3 : params3) {
                std::string combinedName =
                    p1.name + "_" + p2.name + "_" + p3.name;
                result.emplace_back(std::make_tuple(p1.value, p2.value, p3.value),
                                    combinedName);
            }
        }
    }

    return result;
}

/**
 * @brief Parameterized test registrar
 * @tparam TestClass The parameterized test class
 */
template <typename TestClass>
class ParameterizedTestRegistrar {
public:
    using ParamType = typename TestClass::ParamT;

    /**
     * @brief Register parameterized tests with the given parameters
     * @param suiteName Name of the test suite
     * @param testName Base name for the test
     * @param testMethod Test method to run
     * @param params Vector of test parameters
     */
    static void registerTests(const std::string& suiteName,
                              const std::string& testName,
                              void (TestClass::*testMethod)(),
                              const std::vector<TestParam<ParamType>>& params) {
        for (size_t i = 0; i < params.size(); ++i) {
            const auto& param = params[i];
            std::string fullName =
                suiteName + "." + testName + "/" + param.name;

            auto testFunc = [testMethod, paramValue = param.value]() {
                TestClass fixture;
                fixture.SetParam(paramValue);
                fixture.SetUp();
                try {
                    (fixture.*testMethod)();
                } catch (...) {
                    fixture.TearDown();
                    throw;
                }
                fixture.TearDown();
            };

            atom::test::registerTest(fullName, std::move(testFunc));
        }
    }

    /**
     * @brief Register parameterized tests with lambda
     */
    template <typename Func>
    static void registerTestsLambda(
        const std::string& suiteName, const std::string& testName, Func&& func,
        const std::vector<TestParam<ParamType>>& params) {
        for (size_t i = 0; i < params.size(); ++i) {
            const auto& param = params[i];
            std::string fullName =
                suiteName + "." + testName + "/" + param.name;

            auto testFunc = [f = std::forward<Func>(func),
                             paramValue = param.value]() {
                TestClass fixture;
                fixture.SetParam(paramValue);
                fixture.SetUp();
                try {
                    f(fixture);
                } catch (...) {
                    fixture.TearDown();
                    throw;
                }
                fixture.TearDown();
            };

            atom::test::registerTest(fullName, std::move(testFunc));
        }
    }
};

/**
 * @brief Storage for parameterized test parameters
 */
template <typename TestClass>
struct ParamStorage {
    using ParamType = typename TestClass::ParamT;
    static inline std::vector<TestParam<ParamType>> params;
};

/**
 * @brief Parameter name generator interface
 */
template <typename ParamType>
class ParamNameGenerator {
public:
    virtual ~ParamNameGenerator() = default;
    virtual std::string operator()(const ParamType& param, size_t index) const {
        (void)param;
        return std::to_string(index);
    }
};

/**
 * @brief Default parameter name generator using parameterToString
 */
template <typename ParamType>
class DefaultParamNameGenerator : public ParamNameGenerator<ParamType> {
public:
    std::string operator()(const ParamType& param,
                           [[maybe_unused]] size_t index) const override {
        return parameterToString(param);
    }
};

/**
 * @brief Custom name generator using a function
 */
template <typename ParamType>
class FunctionParamNameGenerator : public ParamNameGenerator<ParamType> {
public:
    using NameFunc = std::function<std::string(const ParamType&)>;

    explicit FunctionParamNameGenerator(NameFunc func) : func_(std::move(func)) {}

    std::string operator()(const ParamType& param,
                           [[maybe_unused]] size_t index) const override {
        return func_(param);
    }

private:
    NameFunc func_;
};

/**
 * @brief Helper to create a custom name generator
 */
template <typename ParamType>
auto WithParamName(std::function<std::string(const ParamType&)> func) {
    return FunctionParamNameGenerator<ParamType>(std::move(func));
}

/**
 * @brief Printing info for test parameters
 */
template <typename ParamType>
struct PrintToStringParamName {
    std::string operator()(const ParamType& param) const {
        return parameterToString(param);
    }
};

}  // namespace atom::test

/**
 * @brief Define a parameterized test case
 * @param test_suite The test suite/fixture class (must derive from
 * ParameterizedTest)
 * @param test_name The test case name
 */
#define TEST_P(test_suite, test_name)                                         \
    class test_suite##_##test_name##_Test : public test_suite {               \
    public:                                                                   \
        void TestBody();                                                      \
    };                                                                        \
    static struct test_suite##_##test_name##_Registrar {                      \
        test_suite##_##test_name##_Registrar() {                              \
            const auto& params =                                              \
                atom::test::ParamStorage<test_suite>::params;                 \
            for (size_t i = 0; i < params.size(); ++i) {                      \
                const auto& param = params[i];                                \
                std::string fullName =                                        \
                    std::string(#test_suite) + "." + #test_name + "/" +       \
                    param.name;                                               \
                auto testFunc = [paramValue = param.value]() {                \
                    test_suite##_##test_name##_Test fixture;                  \
                    fixture.SetParam(paramValue);                             \
                    fixture.SetUp();                                          \
                    try {                                                     \
                        fixture.TestBody();                                   \
                    } catch (...) {                                           \
                        fixture.TearDown();                                   \
                        throw;                                                \
                    }                                                         \
                    fixture.TearDown();                                       \
                };                                                            \
                atom::test::registerTest(fullName, std::move(testFunc));      \
            }                                                                 \
        }                                                                     \
    } test_suite##_##test_name##_registrar_instance;                          \
    void test_suite##_##test_name##_Test::TestBody()

/**
 * @brief Instantiate parameterized tests with specific parameters
 * @param prefix Unique prefix for this instantiation
 * @param test_suite The parameterized test suite class
 * @param params_generator Expression that generates test parameters
 */
#define INSTANTIATE_TEST_SUITE_P(prefix, test_suite, params_generator)        \
    static struct prefix##_##test_suite##_ParamInit {                         \
        prefix##_##test_suite##_ParamInit() {                                 \
            atom::test::ParamStorage<test_suite>::params = params_generator;  \
        }                                                                     \
    } prefix##_##test_suite##_param_init_instance

// GTest compatibility alias
#define INSTANTIATE_TEST_CASE_P(prefix, test_suite, params_generator) \
    INSTANTIATE_TEST_SUITE_P(prefix, test_suite, params_generator)

/**
 * @brief Helper macro to get the current parameter in a TEST_P body
 */
#define GetParam() this->GetParam()

#endif  // ATOM_TEST_FIXTURES_TEST_PARAMETERIZED_HPP
