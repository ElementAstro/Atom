#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "error_stack.hpp"

using namespace atom::error;
using ::testing::HasSubstr;
using ::testing::Not;

// Mock for LOG_F to capture output
class LogCapture {
public:
    static void captureLog(const std::string& message) {
        capturedLogs.push_back(message);
    }

    static void clearLogs() { capturedLogs.clear(); }

    static const std::vector<std::string>& getLogs() { return capturedLogs; }

private:
    static std::vector<std::string> capturedLogs;
};

inline std::vector<std::string> LogCapture::capturedLogs;

// Mock the LOG_F macro
#define LOG_F(level, ...) LogCapture::captureLog(std::format(__VA_ARGS__))

class ErrorStackTest : public ::testing::Test {
protected:
    void SetUp() override {
        errorStack = ErrorStack::createUnique();
        LogCapture::clearLogs();
    }

    void TearDown() override { errorStack.reset(); }

    std::unique_ptr<ErrorStack> errorStack;
};

TEST_F(ErrorStackTest, PrintFilteredErrorStackWithNoErrors) {
    // Test printing when the stack is empty
    errorStack->printFilteredErrorStack();
    EXPECT_TRUE(LogCapture::getLogs().empty());
}

TEST_F(ErrorStackTest, PrintFilteredErrorStackWithErrors) {
    // Add some errors
    errorStack->insertError("Test error 1", "Module1", "function1", 10,
                            "file1.cpp");
    errorStack->insertError("Test error 2", "Module2", "function2", 20,
                            "file2.cpp");

    // Print and check logs
    errorStack->printFilteredErrorStack();

    auto logs = LogCapture::getLogs();
    EXPECT_EQ(logs.size(), 2);
    EXPECT_THAT(logs[0], HasSubstr("Test error 1"));
    EXPECT_THAT(logs[1], HasSubstr("Test error 2"));
}

TEST_F(ErrorStackTest, PrintFilteredErrorStackWithFilteredModules) {
    // Add some errors
    errorStack->insertError("Test error 1", "Module1", "function1", 10,
                            "file1.cpp");
    errorStack->insertError("Test error 2", "Module2", "function2", 20,
                            "file2.cpp");
    errorStack->insertError("Test error 3", "Module3", "function3", 30,
                            "file3.cpp");

    // Filter Module2
    std::vector<std::string> filteredModules = {"Module2"};
    errorStack->setFilteredModules(filteredModules);

    // Print and check logs
    LogCapture::clearLogs();
    errorStack->printFilteredErrorStack();

    auto logs = LogCapture::getLogs();
    EXPECT_EQ(logs.size(), 2);
    EXPECT_THAT(logs[0], HasSubstr("Test error 1"));
    EXPECT_THAT(logs[1], HasSubstr("Test error 3"));

    // Check that filtered module's error is not printed
    bool hasModule2Error = false;
    for (const auto& log : logs) {
        if (log.find("Test error 2") != std::string::npos) {
            hasModule2Error = true;
            break;
        }
    }
    EXPECT_FALSE(hasModule2Error);
}

TEST_F(ErrorStackTest, PrintFilteredErrorStackAfterClearingFilters) {
    // Add some errors
    errorStack->insertError("Test error 1", "Module1", "function1", 10,
                            "file1.cpp");
    errorStack->insertError("Test error 2", "Module2", "function2", 20,
                            "file2.cpp");

    // Filter Module2
    std::vector<std::string> filteredModules = {"Module2"};
    errorStack->setFilteredModules(filteredModules);

    // Clear filters
    errorStack->clearFilteredModules();

    // Print and check logs
    LogCapture::clearLogs();
    errorStack->printFilteredErrorStack();

    auto logs = LogCapture::getLogs();
    EXPECT_EQ(logs.size(), 2);
    EXPECT_THAT(logs[0], HasSubstr("Test error 1"));
    EXPECT_THAT(logs[1], HasSubstr("Test error 2"));
}

TEST_F(ErrorStackTest, PrintFilteredErrorStackWithMultipleFilters) {
    // Add some errors
    errorStack->insertError("Test error 1", "Module1", "function1", 10,
                            "file1.cpp");
    errorStack->insertError("Test error 2", "Module2", "function2", 20,
                            "file2.cpp");
    errorStack->insertError("Test error 3", "Module3", "function3", 30,
                            "file3.cpp");
    errorStack->insertError("Test error 4", "Module4", "function4", 40,
                            "file4.cpp");

    // Filter multiple modules
    std::vector<std::string> filteredModules = {"Module1", "Module3"};
    errorStack->setFilteredModules(filteredModules);

    // Print and check logs
    LogCapture::clearLogs();
    errorStack->printFilteredErrorStack();

    auto logs = LogCapture::getLogs();
    EXPECT_EQ(logs.size(), 2);
    EXPECT_THAT(logs[0], HasSubstr("Test error 2"));
    EXPECT_THAT(logs[1], HasSubstr("Test error 4"));

    // Check that filtered modules' errors are not printed
    for (const auto& log : logs) {
        EXPECT_THAT(log, Not(HasSubstr("Test error 1")));
        EXPECT_THAT(log, Not(HasSubstr("Test error 3")));
    }
}

// Test for inserting duplicate errors
TEST_F(ErrorStackTest, PrintFilteredErrorStackWithDuplicateErrors) {
    // Add duplicate errors
    errorStack->insertError("Duplicate error", "Module1", "function1", 10,
                            "file1.cpp");
    errorStack->insertError("Duplicate error", "Module1", "function1", 10,
                            "file1.cpp");

    // Print and check logs
    LogCapture::clearLogs();
    errorStack->printFilteredErrorStack();

    auto logs = LogCapture::getLogs();
    EXPECT_EQ(logs.size(),
              1);  // Should only print one instance of the duplicate error
    EXPECT_THAT(logs[0], HasSubstr("Duplicate error"));
}

// Test for handling exceptions in printFilteredErrorStack
TEST_F(ErrorStackTest, PrintFilteredErrorStackHandlesExceptions) {
    // This test is more conceptual since we can't easily force an exception in
    // the method We can check that the method is surrounded by try-catch

    // Add an error
    errorStack->insertError("Test error", "Module1", "function1", 10,
                            "file1.cpp");

    // Force clear logs before printing
    LogCapture::clearLogs();

    // Call the method - it should not throw even if there's an internal
    // exception
    EXPECT_NO_THROW(errorStack->printFilteredErrorStack());
}

// Test interaction between printFilteredErrorStack and other error stack
// operations
TEST_F(ErrorStackTest, PrintFilteredErrorStackInteractionWithOtherOperations) {
    // Add some errors
    errorStack->insertError("Error 1", "Module1", "function1", 10, "file1.cpp");
    errorStack->insertError("Error 2", "Module2", "function2", 20, "file2.cpp");

    // Filter Module2
    std::vector<std::string> filteredModules = {"Module2"};
    errorStack->setFilteredModules(filteredModules);

    // Print filtered stack
    LogCapture::clearLogs();
    errorStack->printFilteredErrorStack();

    auto logs = LogCapture::getLogs();
    EXPECT_EQ(logs.size(), 1);
    EXPECT_THAT(logs[0], HasSubstr("Error 1"));

    // Clear the error stack
    errorStack->clear();

    // Print again - should be empty
    LogCapture::clearLogs();
    errorStack->printFilteredErrorStack();
    EXPECT_TRUE(LogCapture::getLogs().empty());

    // Add a new error
    errorStack->insertError("New error", "Module3", "function3", 30,
                            "file3.cpp");

    // Print again - should show the new error
    LogCapture::clearLogs();
    errorStack->printFilteredErrorStack();

    logs = LogCapture::getLogs();
    EXPECT_EQ(logs.size(), 1);
    EXPECT_THAT(logs[0], HasSubstr("New error"));
}

// Additional test for thread safety of printFilteredErrorStack
TEST_F(ErrorStackTest, PrintFilteredErrorStackIsThreadSafe) {
    // Add some errors
    for (int i = 0; i < 100; i++) {
        std::string errorMsg = "Error " + std::to_string(i);
        std::string moduleName = "Module" + std::to_string(i % 5);
        errorStack->insertError(errorMsg, moduleName, "function", i,
                                "file.cpp");
    }

    // Filter some modules
    std::vector<std::string> filteredModules = {"Module0", "Module2",
                                                "Module4"};
    errorStack->setFilteredModules(filteredModules);

    // Create multiple threads that print the filtered stack
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back(
            [this]() { errorStack->printFilteredErrorStack(); });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // We can't easily check the output, but the test ensures that
    // concurrent access doesn't crash the program
    SUCCEED()
        << "Multiple threads successfully accessed printFilteredErrorStack";
}

// Test for getFilteredErrorsByModule method which relates to filtered errors
TEST_F(ErrorStackTest, GetFilteredErrorsByModule) {
    // Add some errors
    errorStack->insertError("Error 1", "Module1", "function1", 10, "file1.cpp");
    errorStack->insertError("Error 2", "Module1", "function2", 20, "file1.cpp");
    errorStack->insertError("Error 3", "Module2", "function3", 30, "file2.cpp");

    // Get errors by module
    auto module1Errors = errorStack->getFilteredErrorsByModule("Module1");
    EXPECT_EQ(module1Errors.size(), 2);
    EXPECT_EQ(module1Errors[0].errorMessage, "Error 1");
    EXPECT_EQ(module1Errors[1].errorMessage, "Error 2");

    auto module2Errors = errorStack->getFilteredErrorsByModule("Module2");
    EXPECT_EQ(module2Errors.size(), 1);
    EXPECT_EQ(module2Errors[0].errorMessage, "Error 3");

    auto module3Errors = errorStack->getFilteredErrorsByModule("Module3");
    EXPECT_TRUE(module3Errors.empty());

    // Filter Module1
    std::vector<std::string> filteredModules = {"Module1"};
    errorStack->setFilteredModules(filteredModules);

    // Now Module1 errors should be filtered
    module1Errors = errorStack->getFilteredErrorsByModule("Module1");
    EXPECT_TRUE(module1Errors.empty());

    // Module2 errors should still be available
    module2Errors = errorStack->getFilteredErrorsByModule("Module2");
    EXPECT_EQ(module2Errors.size(), 1);
}

// Test for comprehensive error stack behavior
TEST_F(ErrorStackTest, ComprehensiveErrorStackBehavior) {
    // Start with empty stack
    EXPECT_TRUE(errorStack->isEmpty());
    EXPECT_EQ(errorStack->size(), 0);

    // Add some errors
    errorStack->insertError("Error 1", "Module1", "function1", 10, "file1.cpp");
    errorStack->insertError("Error 2", "Module2", "function2", 20, "file2.cpp");

    // Check stack state
    EXPECT_FALSE(errorStack->isEmpty());
    EXPECT_EQ(errorStack->size(), 2);

    // Get latest error
    auto latestError = errorStack->getLatestError();
    ASSERT_TRUE(latestError.has_value());
    EXPECT_EQ(latestError->errorMessage,
              "Error 2");  // Assuming Error 2 was added last

    // Filter Module1 and print
    std::vector<std::string> filteredModules = {"Module1"};
    errorStack->setFilteredModules(filteredModules);

    LogCapture::clearLogs();
    errorStack->printFilteredErrorStack();

    auto logs = LogCapture::getLogs();
    EXPECT_EQ(logs.size(), 1);
    EXPECT_THAT(logs[0], HasSubstr("Error 2"));

    // Clear filter and print again
    errorStack->clearFilteredModules();

    LogCapture::clearLogs();
    errorStack->printFilteredErrorStack();

    logs = LogCapture::getLogs();
    EXPECT_EQ(logs.size(), 2);

    // Clear the error stack
    errorStack->clear();
    EXPECT_TRUE(errorStack->isEmpty());

    // Printing should now produce no output
    LogCapture::clearLogs();
    errorStack->printFilteredErrorStack();
    EXPECT_TRUE(LogCapture::getLogs().empty());
}
