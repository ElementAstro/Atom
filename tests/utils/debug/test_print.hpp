// filepath: /home/max/Atom-1/atom/utils/test_print.hpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "atom/utils/debug/print.hpp"

namespace atom::utils::tests {

// Custom output redirection for testing console output
class OutputCapture {
public:
    OutputCapture() {
        // Save original cout buffer
        oldCoutBuf = std::cout.rdbuf();
        oldCerrBuf = std::cerr.rdbuf();

        // Redirect cout to our stringstream
        std::cout.rdbuf(capturedOut.rdbuf());
        std::cerr.rdbuf(capturedErr.rdbuf());
    }

    ~OutputCapture() {
        // Restore original buffers
        std::cout.rdbuf(oldCoutBuf);
        std::cerr.rdbuf(oldCerrBuf);
    }

    std::string getCout() const { return capturedOut.str(); }
    std::string getCerr() const { return capturedErr.str(); }

    void clear() {
        capturedOut.str("");
        capturedOut.clear();
        capturedErr.str("");
        capturedErr.clear();
    }

private:
    std::streambuf* oldCoutBuf;
    std::streambuf* oldCerrBuf;
    std::ostringstream capturedOut;
    std::ostringstream capturedErr;
};

class PrintUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test data
        testVector = {1, 2, 3, 4, 5};
        testMap = {{"key1", "value1"}, {"key2", "value2"}};
        testString = "Hello, World!";
        testInt = 42;
        testFloat = 3.14159f;
        testBool = true;
    }

    std::vector<int> testVector;
    std::map<std::string, std::string> testMap;
    std::string testString;
    int testInt;
    float testFloat;
    bool testBool;
};

// Test basic print functionality
TEST_F(PrintUtilsTest, BasicPrint) {
    OutputCapture capture;

    print("Hello, World!");
    EXPECT_EQ(capture.getCout(), "Hello, World!\n");

    capture.clear();
    print(testInt);
    EXPECT_EQ(capture.getCout(), "42\n");

    capture.clear();
    print(testFloat);
    EXPECT_TRUE(capture.getCout().find("3.14159") != std::string::npos);

    capture.clear();
    print(testBool);
    EXPECT_EQ(capture.getCout(), "true\n");
}

// Test print with multiple arguments
TEST_F(PrintUtilsTest, PrintMultipleArgs) {
    OutputCapture capture;

    print("Number:", testInt, "String:", testString);
    std::string output = capture.getCout();
    EXPECT_TRUE(output.find("Number:") != std::string::npos);
    EXPECT_TRUE(output.find("42") != std::string::npos);
    EXPECT_TRUE(output.find("String:") != std::string::npos);
    EXPECT_TRUE(output.find("Hello, World!") != std::string::npos);
}

// Test container printing
TEST_F(PrintUtilsTest, ContainerPrinting) {
    OutputCapture capture;

    print(testVector);
    std::string output = capture.getCout();
    EXPECT_TRUE(output.find("[") != std::string::npos);
    EXPECT_TRUE(output.find("1") != std::string::npos);
    EXPECT_TRUE(output.find("2") != std::string::npos);
    EXPECT_TRUE(output.find("5") != std::string::npos);
    EXPECT_TRUE(output.find("]") != std::string::npos);

    capture.clear();
    print(testMap);
    output = capture.getCout();
    EXPECT_TRUE(output.find("key1") != std::string::npos);
    EXPECT_TRUE(output.find("value1") != std::string::npos);
}

// Test formatted print
TEST_F(PrintUtilsTest, FormattedPrint) {
    OutputCapture capture;

    printf("Number: {}, String: {}", testInt, testString);
    std::string output = capture.getCout();
    EXPECT_TRUE(output.find("Number: 42") != std::string::npos);
    EXPECT_TRUE(output.find("String: Hello, World!") != std::string::npos);
}

// Test error printing
TEST_F(PrintUtilsTest, ErrorPrinting) {
    OutputCapture capture;

    printError("This is an error message");
    EXPECT_TRUE(capture.getCerr().find("This is an error message") != std::string::npos);

    capture.clear();
    printWarning("This is a warning message");
    EXPECT_TRUE(capture.getCout().find("This is a warning message") != std::string::npos);
}

// Test debug printing
TEST_F(PrintUtilsTest, DebugPrinting) {
    OutputCapture capture;

    printDebug("Debug message");
    std::string output = capture.getCout();
    EXPECT_TRUE(output.find("Debug message") != std::string::npos);

    capture.clear();
    printInfo("Info message");
    output = capture.getCout();
    EXPECT_TRUE(output.find("Info message") != std::string::npos);
}

// Test colored printing
TEST_F(PrintUtilsTest, ColoredPrinting) {
    OutputCapture capture;

    printColored("Red text", Color::RED);
    std::string output = capture.getCout();
    EXPECT_TRUE(output.find("Red text") != std::string::npos);
    // Should contain ANSI color codes
    EXPECT_TRUE(output.find("\033[") != std::string::npos);

    capture.clear();
    printColored("Green text", Color::GREEN);
    output = capture.getCout();
    EXPECT_TRUE(output.find("Green text") != std::string::npos);
}

// Test styled printing
TEST_F(PrintUtilsTest, StyledPrinting) {
    OutputCapture capture;

    printStyled("Bold text", TextStyle::BOLD);
    std::string output = capture.getCout();
    EXPECT_TRUE(output.find("Bold text") != std::string::npos);
    // Should contain ANSI style codes
    EXPECT_TRUE(output.find("\033[") != std::string::npos);

    capture.clear();
    printStyled("Underlined text", TextStyle::UNDERLINE);
    output = capture.getCout();
    EXPECT_TRUE(output.find("Underlined text") != std::string::npos);
}

// Test progress bar
TEST_F(PrintUtilsTest, ProgressBar) {
    OutputCapture capture;

    ProgressBar progressBar(100, ProgressBarStyle::BASIC);
    progressBar.update(50);
    progressBar.display();

    std::string output = capture.getCout();
    EXPECT_TRUE(output.find("[") != std::string::npos);
    EXPECT_TRUE(output.find("]") != std::string::npos);
    EXPECT_TRUE(output.find("50%") != std::string::npos);

    capture.clear();
    progressBar.update(100);
    progressBar.display();
    output = capture.getCout();
    EXPECT_TRUE(output.find("100%") != std::string::npos);
}

// Test table printing
TEST_F(PrintUtilsTest, TablePrinting) {
    OutputCapture capture;

    std::vector<std::vector<std::string>> tableData = {
        {"Name", "Age", "City"},
        {"Alice", "25", "New York"},
        {"Bob", "30", "London"},
        {"Charlie", "35", "Tokyo"}
    };

    printTable(tableData);
    std::string output = capture.getCout();
    
    EXPECT_TRUE(output.find("Name") != std::string::npos);
    EXPECT_TRUE(output.find("Alice") != std::string::npos);
    EXPECT_TRUE(output.find("25") != std::string::npos);
    EXPECT_TRUE(output.find("Tokyo") != std::string::npos);
    // Should contain table borders
    EXPECT_TRUE(output.find("|") != std::string::npos);
    EXPECT_TRUE(output.find("-") != std::string::npos);
}

// Test logging functionality
TEST_F(PrintUtilsTest, LoggingFunctionality) {
    OutputCapture capture;

    log(LogLevel::INFO_LEVEL, "Information message");
    std::string output = capture.getCout();
    EXPECT_TRUE(output.find("Information message") != std::string::npos);
    EXPECT_TRUE(output.find("INFO") != std::string::npos);

    capture.clear();
    log(LogLevel::ERROR_LEVEL, "Error message");
    output = capture.getCerr();
    EXPECT_TRUE(output.find("Error message") != std::string::npos);
    EXPECT_TRUE(output.find("ERROR") != std::string::npos);

    capture.clear();
    log(LogLevel::WARNING_LEVEL, "Warning message");
    output = capture.getCout();
    EXPECT_TRUE(output.find("Warning message") != std::string::npos);
    EXPECT_TRUE(output.find("WARNING") != std::string::npos);

    capture.clear();
    log(LogLevel::DEBUG_LEVEL, "Debug message");
    output = capture.getCout();
    EXPECT_TRUE(output.find("Debug message") != std::string::npos);
    EXPECT_TRUE(output.find("DEBUG") != std::string::npos);
}

// Test file output
TEST_F(PrintUtilsTest, FileOutput) {
    const std::string filename = "test_output.txt";
    
    // Clean up any existing file
    std::remove(filename.c_str());

    printToFile(filename, "Test message to file");
    
    // Read the file and verify content
    std::ifstream file(filename);
    ASSERT_TRUE(file.is_open());
    
    std::string content;
    std::getline(file, content);
    file.close();
    
    EXPECT_EQ(content, "Test message to file");
    
    // Clean up
    std::remove(filename.c_str());
}

// Test timestamp functionality
TEST_F(PrintUtilsTest, TimestampFunctionality) {
    OutputCapture capture;

    printWithTimestamp("Message with timestamp");
    std::string output = capture.getCout();
    
    EXPECT_TRUE(output.find("Message with timestamp") != std::string::npos);
    // Should contain timestamp format (basic check)
    EXPECT_TRUE(output.find(":") != std::string::npos);
}

// Test memory usage printing
TEST_F(PrintUtilsTest, MemoryUsagePrinting) {
    OutputCapture capture;

    printMemoryUsage();
    std::string output = capture.getCout();
    
    // Should contain memory-related keywords
    EXPECT_TRUE(output.find("Memory") != std::string::npos || 
                output.find("memory") != std::string::npos ||
                output.find("MB") != std::string::npos ||
                output.find("KB") != std::string::npos);
}

// Test thread-safe printing
TEST_F(PrintUtilsTest, ThreadSafePrinting) {
    const int numThreads = 4;
    const int messagesPerThread = 10;
    std::vector<std::future<void>> futures;

    OutputCapture capture;

    for (int t = 0; t < numThreads; ++t) {
        futures.push_back(std::async(std::launch::async, [t, messagesPerThread]() {
            for (int i = 0; i < messagesPerThread; ++i) {
                printThreadSafe("Thread", t, "Message", i);
            }
        }));
    }

    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }

    std::string output = capture.getCout();
    
    // Should contain messages from all threads
    for (int t = 0; t < numThreads; ++t) {
        EXPECT_TRUE(output.find("Thread " + std::to_string(t)) != std::string::npos);
    }
}

// Test performance with large output
TEST_F(PrintUtilsTest, PerformanceTest) {
    OutputCapture capture;

    auto start = std::chrono::high_resolution_clock::now();

    // Print a large amount of data
    for (int i = 0; i < 1000; ++i) {
        print("Performance test message", i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 1000); // 1 second max

    std::string output = capture.getCout();
    EXPECT_TRUE(output.find("Performance test message 0") != std::string::npos);
    EXPECT_TRUE(output.find("Performance test message 999") != std::string::npos);
}

// Test edge cases and error handling
TEST_F(PrintUtilsTest, EdgeCasesAndErrorHandling) {
    OutputCapture capture;

    // Test empty string
    print("");
    EXPECT_EQ(capture.getCout(), "\n");

    capture.clear();

    // Test null pointer (if applicable)
    const char* nullPtr = nullptr;
    EXPECT_NO_THROW(print(nullPtr));

    capture.clear();

    // Test very long string
    std::string longString(10000, 'A');
    EXPECT_NO_THROW(print(longString));

    capture.clear();

    // Test special characters
    print("Special chars: \n\t\r\\\"\'");
    std::string output = capture.getCout();
    EXPECT_TRUE(output.find("Special chars:") != std::string::npos);
}

// Test custom formatting
TEST_F(PrintUtilsTest, CustomFormatting) {
    OutputCapture capture;

    // Test precision formatting for floats
    printFormatted("Float with precision: {:.2f}", 3.14159);
    std::string output = capture.getCout();
    EXPECT_TRUE(output.find("3.14") != std::string::npos);

    capture.clear();

    // Test width formatting
    printFormatted("Padded number: {:5d}", 42);
    output = capture.getCout();
    EXPECT_TRUE(output.find("   42") != std::string::npos ||
                output.find("42   ") != std::string::npos);

    capture.clear();

    // Test hex formatting
    printFormatted("Hex value: {:x}", 255);
    output = capture.getCout();
    EXPECT_TRUE(output.find("ff") != std::string::npos);
}

// Test different progress bar styles
TEST_F(PrintUtilsTest, ProgressBarStyles) {
    OutputCapture capture;

    // Test BLOCK style
    ProgressBar blockBar(100, ProgressBarStyle::BLOCK);
    blockBar.update(50);
    blockBar.display();
    std::string output = capture.getCout();
    EXPECT_TRUE(output.find("50%") != std::string::npos);

    capture.clear();

    // Test ARROW style
    ProgressBar arrowBar(100, ProgressBarStyle::ARROW);
    arrowBar.update(75);
    arrowBar.display();
    output = capture.getCout();
    EXPECT_TRUE(output.find("75%") != std::string::npos);

    capture.clear();

    // Test PERCENTAGE style
    ProgressBar percentBar(100, ProgressBarStyle::PERCENTAGE);
    percentBar.update(100);
    percentBar.display();
    output = capture.getCout();
    EXPECT_TRUE(output.find("100%") != std::string::npos);
}

// Test complex data structures
TEST_F(PrintUtilsTest, ComplexDataStructures) {
    OutputCapture capture;

    // Test nested containers
    std::vector<std::vector<int>> nestedVector = {{1, 2}, {3, 4}, {5, 6}};
    print(nestedVector);
    std::string output = capture.getCout();
    EXPECT_TRUE(output.find("1") != std::string::npos);
    EXPECT_TRUE(output.find("6") != std::string::npos);

    capture.clear();

    // Test map with complex values
    std::map<std::string, std::vector<int>> complexMap = {
        {"first", {1, 2, 3}},
        {"second", {4, 5, 6}}
    };
    print(complexMap);
    output = capture.getCout();
    EXPECT_TRUE(output.find("first") != std::string::npos);
    EXPECT_TRUE(output.find("second") != std::string::npos);
}

// Test conditional printing
TEST_F(PrintUtilsTest, ConditionalPrinting) {
    OutputCapture capture;

    // Test debug level printing
    setLogLevel(LogLevel::DEBUG_LEVEL);
    printDebug("Debug message should appear");
    std::string output = capture.getCout();
    EXPECT_TRUE(output.find("Debug message should appear") != std::string::npos);

    capture.clear();

    // Test with higher log level
    setLogLevel(LogLevel::ERROR_LEVEL);
    printDebug("Debug message should not appear");
    output = capture.getCout();
    EXPECT_TRUE(output.empty() || output.find("Debug message should not appear") == std::string::npos);

    capture.clear();

    // Error should still appear
    printError("Error message should appear");
    output = capture.getCerr();
    EXPECT_TRUE(output.find("Error message should appear") != std::string::npos);
}

// Test output redirection
TEST_F(PrintUtilsTest, OutputRedirection) {
    const std::string filename = "test_redirect.txt";

    // Clean up any existing file
    std::remove(filename.c_str());

    // Redirect output to file
    redirectOutputToFile(filename);

    print("Redirected message");

    // Restore normal output
    restoreOutput();

    // Read the file and verify content
    std::ifstream file(filename);
    ASSERT_TRUE(file.is_open());

    std::string content;
    std::getline(file, content);
    file.close();

    EXPECT_TRUE(content.find("Redirected message") != std::string::npos);

    // Clean up
    std::remove(filename.c_str());
}

// Test buffer management
TEST_F(PrintUtilsTest, BufferManagement) {
    OutputCapture capture;

    // Test buffer flushing
    print("Message 1");
    flushOutput();
    print("Message 2");

    std::string output = capture.getCout();
    EXPECT_TRUE(output.find("Message 1") != std::string::npos);
    EXPECT_TRUE(output.find("Message 2") != std::string::npos);

    capture.clear();

    // Test buffer clearing
    clearBuffer();
    print("After clear");
    output = capture.getCout();
    EXPECT_TRUE(output.find("After clear") != std::string::npos);
}

}  // namespace atom::utils::tests
