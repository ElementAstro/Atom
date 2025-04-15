#ifndef ATOM_TEST_REGISTRY_HPP
#define ATOM_TEST_REGISTRY_HPP

#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "atom/tests/test.hpp"

namespace atom::test {

/**
 * @brief Central test registry class
 * @details Manages registration and access to all test suites and test cases
 */
class TestRegistry {
public:
    /**
     * @brief Get the global test registry instance
     * @return Reference to the global TestRegistry object
     */
    static auto instance() -> TestRegistry& {
        static TestRegistry registry;
        return registry;
    }

    /**
     * @brief Register a test suite
     * @param suite The test suite to register
     */
    void registerSuite(TestSuite suite) {
        std::lock_guard<std::mutex> lock(mutex_);
        suites_.push_back(std::move(suite));
    }

    /**
     * @brief Register an individual test case
     * @param testCase The test case to register
     * @param suiteName Optional suite name (added to default suite if empty)
     */
    void registerTest(TestCase testCase, const std::string& suiteName = "") {
        std::lock_guard<std::mutex> lock(mutex_);

        // If a suite name is specified, add the test to that suite
        if (!suiteName.empty()) {
            auto it = std::find_if(suites_.begin(), suites_.end(),
                                   [&suiteName](const TestSuite& s) {
                                       return s.name == suiteName;
                                   });
            if (it != suites_.end()) {
                it->testCases.push_back(std::move(testCase));
                return;
            }

            // Create new suite if it doesn't exist
            suites_.push_back({suiteName, {std::move(testCase)}});
        } else {
            // Add to default suite (empty name)
            auto it =
                std::find_if(suites_.begin(), suites_.end(),
                             [](const TestSuite& s) { return s.name.empty(); });
            if (it != suites_.end()) {
                it->testCases.push_back(std::move(testCase));
            } else {
                suites_.push_back({"", {std::move(testCase)}});
            }
        }
    }

    /**
     * @brief Get all registered test suites
     * @return Const reference to all test suites
     */
    [[nodiscard]] auto getSuites() const -> const std::vector<TestSuite>& {
        return suites_;
    }

    /**
     * @brief Get mutable reference to all registered test suites
     * @return Mutable reference to all test suites
     */
    auto getSuites() -> std::vector<TestSuite>& { 
        return suites_; 
    }

    /**
     * @brief Clear all registered tests
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        suites_.clear();
    }

    /**
     * @brief Find a test case by name
     * @param name Test case name
     * @return Pointer to the found test case, nullptr if not found
     */
    [[nodiscard]] auto findTestByName(std::string_view name) const
        -> const TestCase* {
        for (const auto& suite : suites_) {
            for (const auto& test : suite.testCases) {
                if (test.name == name) {
                    return &test;
                }
            }
        }
        return nullptr;
    }

    /**
     * @brief Find a test suite by name
     * @param name Test suite name
     * @return Pointer to the found test suite, nullptr if not found
     */
    [[nodiscard]] auto findSuiteByName(std::string_view name) const
        -> const TestSuite* {
        for (const auto& suite : suites_) {
            if (suite.name == name) {
                return &suite;
            }
        }
        return nullptr;
    }

    /**
     * @brief Filter tests by tag
     * @param tag Tag to filter by
     * @return Vector of test cases that include the specified tag
     */
    [[nodiscard]] auto findTestsByTag(std::string_view tag) const
        -> std::vector<const TestCase*> {
        std::vector<const TestCase*> result;
        
        for (const auto& suite : suites_) {
            for (const auto& test : suite.testCases) {
                // Check if the tag exists in this test's tags
                if (std::find(test.tags.begin(), test.tags.end(), tag) != test.tags.end()) {
                    result.push_back(&test);
                }
            }
        }
        
        return result;
    }
    
    /**
     * @brief Get all available test tags in the registry
     * @return Vector of unique tag strings
     */
    [[nodiscard]] auto getAllTags() const -> std::vector<std::string> {
        std::vector<std::string> allTags;
        
        for (const auto& suite : suites_) {
            for (const auto& test : suite.testCases) {
                allTags.insert(allTags.end(), test.tags.begin(), test.tags.end());
            }
        }
        
        // Remove duplicates
        std::sort(allTags.begin(), allTags.end());
        allTags.erase(std::unique(allTags.begin(), allTags.end()), allTags.end());
        
        return allTags;
    }
    
    /**
     * @brief Count the total number of registered test cases
     * @return Total test count
     */
    [[nodiscard]] auto getTotalTestCount() const -> size_t {
        size_t count = 0;
        for (const auto& suite : suites_) {
            count += suite.testCases.size();
        }
        return count;
    }

    /**
     * @brief Run all tests in a specific suite
     * @param suiteName Name of the suite to run
     * @return True if all tests passed, false otherwise
     */
    bool runSuite(std::string_view suiteName) {
        const TestSuite* suite = findSuiteByName(suiteName);
        if (!suite) {
            return false;
        }

        bool allPassed = true;
        for (const auto& test : suite->testCases) {
            allPassed &= test.testFunction();
        }
        
        return allPassed;
    }

private:
    std::vector<TestSuite> suites_;  ///< Collection of registered test suites
    std::mutex mutex_;               ///< Mutex to protect test registration

    // Private constructor ensures singleton pattern
    TestRegistry() = default;
};

}  // namespace atom::test

#endif  // ATOM_TEST_REGISTRY_HPP