// filepath: /home/max/Atom-1/atom/utils/test_print.hpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "print.hpp"

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

    std::string getOutput() { return capturedOut.str(); }

    std::string getError() { return capturedErr.str(); }

    void clear() {
        capturedOut.str("");
        capturedOut.clear();
        capturedErr.str("");
        capturedErr.clear();
    }

private:
    std::stringstream capturedOut;
    std::stringstream capturedErr;
    std::streambuf* oldCoutBuf;
    std::streambuf* oldCerrBuf;
};

class PrintUtilsTest : public ::testing::Test {
protected:
    void SetUp() override { outputCapture = std::make_unique<OutputCapture>(); }

    void TearDown() override { outputCapture.reset(); }

    // Helper method to remove carriage returns for easier testing
    std::string removeCarriageReturns(const std::string& input) {
        std::string result = input;
        result.erase(std::remove(result.begin(), result.end(), '\r'),
                     result.end());
        return result;
    }

    std::string cleanOutput() {
        return removeCarriageReturns(outputCapture->getOutput());
    }

    std::unique_ptr<OutputCapture> outputCapture;
};

// Tests for printProgressBar
TEST_F(PrintUtilsTest, PrintProgressBarBasicStyle) {
    printProgressBar(0.5, 10, ProgressBarStyle::BASIC);
    std::string output = outputCapture->getOutput();

    EXPECT_TRUE(output.find("[=====") != std::string::npos)
        << "Progress bar should have 5 equals signs";
    EXPECT_TRUE(output.find(">") != std::string::npos)
        << "Progress bar should have a > character";
    EXPECT_TRUE(output.find("50 %") != std::string::npos)
        << "Progress bar should show 50%";
}

TEST_F(PrintUtilsTest, PrintProgressBarBlockStyle) {
    printProgressBar(0.75, 8, ProgressBarStyle::BLOCK);
    std::string output = outputCapture->getOutput();

    EXPECT_TRUE(output.find("█") != std::string::npos)
        << "Block progress bar should use █ character";
    EXPECT_TRUE(output.find("75 %") != std::string::npos)
        << "Progress bar should show 75%";
}

TEST_F(PrintUtilsTest, PrintProgressBarArrowStyle) {
    printProgressBar(0.25, 12, ProgressBarStyle::ARROW);
    std::string output = outputCapture->getOutput();

    EXPECT_TRUE(output.find("→") != std::string::npos)
        << "Arrow progress bar should use → character";
    EXPECT_TRUE(output.find("25 %") != std::string::npos)
        << "Progress bar should show 25%";
}

TEST_F(PrintUtilsTest, PrintProgressBarPercentageStyle) {
    printProgressBar(0.33, 10, ProgressBarStyle::PERCENTAGE);
    std::string output = outputCapture->getOutput();

    EXPECT_TRUE(output.find("33% completed") != std::string::npos)
        << "Percentage style should show '33% completed'";
    EXPECT_FALSE(output.find("[") != std::string::npos)
        << "Percentage style should not contain brackets";
}

TEST_F(PrintUtilsTest, PrintProgressBarInputValidation) {
    // Test negative progress
    printProgressBar(-0.5, 10, ProgressBarStyle::BASIC);
    std::string output = outputCapture->getOutput();
    EXPECT_TRUE(output.find("0 %") != std::string::npos)
        << "Negative progress should be clamped to 0%";
    outputCapture->clear();

    // Test progress > 1
    printProgressBar(1.5, 10, ProgressBarStyle::BASIC);
    output = outputCapture->getOutput();
    EXPECT_TRUE(output.find("100 %") != std::string::npos)
        << "Progress > 1 should be clamped to 100%";
    outputCapture->clear();

    // Test negative bar width
    printProgressBar(0.5, -5, ProgressBarStyle::BASIC);
    output = outputCapture->getOutput();
    EXPECT_TRUE(output.length() > 10)
        << "Negative bar width should use default width";
}

// Tests for printTable
TEST_F(PrintUtilsTest, PrintTableBasic) {
    std::vector<std::vector<std::string>> data = {
        {"Header1", "Header2", "Header3"},
        {"Value1", "Value2", "Value3"},
        {"LongerValue", "Short", "MediumVal"}};

    printTable(data);
    std::string output = cleanOutput();

    EXPECT_TRUE(output.find("| Header1") != std::string::npos)
        << "Table should contain header row";
    EXPECT_TRUE(output.find("+---------") != std::string::npos)
        << "Table should contain separator line";
    EXPECT_TRUE(output.find("| LongerValue") != std::string::npos)
        << "Table should contain longer values";
}

TEST_F(PrintUtilsTest, PrintTableEmpty) {
    std::vector<std::vector<std::string>> emptyData;
    printTable(emptyData);
    EXPECT_TRUE(outputCapture->getOutput().empty())
        << "Empty table should produce no output";
}

TEST_F(PrintUtilsTest, PrintTableInvalidStructure) {
    std::vector<std::vector<std::string>> invalidData = {
        {"Header1", "Header2", "Header3"},
        {"Value1", "Value2"}  // Row with fewer columns
    };

    printTable(invalidData);
    std::string error = outputCapture->getError();

    EXPECT_TRUE(error.find("Error printing table") != std::string::npos)
        << "Invalid table structure should produce an error";
}

// Tests for printJson
TEST_F(PrintUtilsTest, PrintJsonBasic) {
    std::string json = R"({"name":"John","age":30,"city":"New York"})";
    printJson(json);
    std::string output = cleanOutput();

    EXPECT_TRUE(output.find("{\n") != std::string::npos)
        << "JSON should be formatted with newlines";
    EXPECT_TRUE(output.find("\"name\": \"John\"") != std::string::npos)
        << "JSON should be properly spaced";
    EXPECT_TRUE(output.find("\"age\": 30") != std::string::npos)
        << "JSON should be properly spaced";
}

TEST_F(PrintUtilsTest, PrintJsonNested) {
    std::string json =
        R"({"person":{"name":"John","address":{"city":"New York","zip":"10001"}}})";
    printJson(json);
    std::string output = cleanOutput();

    EXPECT_TRUE(output.find("\"person\": {") != std::string::npos)
        << "Nested JSON should be properly formatted";
    EXPECT_TRUE(output.find("\"address\": {") != std::string::npos)
        << "Nested JSON should be properly formatted";
    // Check indentation increases for nested objects
    size_t outer_indent_pos = output.find("\"person\": {");
    size_t inner_indent_pos = output.find("\"address\": {");
    EXPECT_TRUE(inner_indent_pos > outer_indent_pos)
        << "Inner elements should be indented more";
}

TEST_F(PrintUtilsTest, PrintJsonArray) {
    std::string json = R"({"colors":["red","green","blue"]})";
    printJson(json);
    std::string output = cleanOutput();

    EXPECT_TRUE(output.find("\"colors\": [") != std::string::npos)
        << "JSON arrays should be properly formatted";
    EXPECT_TRUE(output.find("\"red\"") != std::string::npos)
        << "JSON array items should be properly formatted";
}

TEST_F(PrintUtilsTest, PrintJsonEmpty) {
    std::string json = "";
    printJson(json);
    std::string output = cleanOutput();

    EXPECT_EQ(output, "{}\n") << "Empty JSON should be printed as '{}'";
}

TEST_F(PrintUtilsTest, PrintJsonInvalidIndent) {
    std::string json = R"({"name":"John"})";
    printJson(json, -3);

    std::string error = outputCapture->getError();
    std::string output = cleanOutput();

    EXPECT_TRUE(error.find("Warning: Negative indent value") !=
                std::string::npos)
        << "Negative indent should produce a warning";
    EXPECT_TRUE(output.find("\"name\": \"John\"") != std::string::npos)
        << "JSON should still be printed with default indent";
}

TEST_F(PrintUtilsTest, PrintJsonWithEscapedQuotes) {
    std::string json = R"({"text":"This is a \"quoted\" string"})";
    printJson(json);
    std::string output = cleanOutput();

    EXPECT_TRUE(output.find("\"text\": \"This is a \\\"quoted\\\" string\"") !=
                std::string::npos)
        << "Escaped quotes should be preserved";
}

// Tests for printBarChart
TEST_F(PrintUtilsTest, PrintBarChartBasic) {
    std::map<std::string, int> data = {
        {"Item1", 10}, {"Item2", 20}, {"Item3", 5}};

    printBarChart(data, 10);
    std::string output = cleanOutput();

    EXPECT_TRUE(output.find("Item1") != std::string::npos)
        << "Bar chart should contain labels";
    EXPECT_TRUE(output.find("Item2") != std::string::npos)
        << "Bar chart should contain labels";
    EXPECT_TRUE(output.find("Item3") != std::string::npos)
        << "Bar chart should contain labels";
    // Check for bar characters
    EXPECT_TRUE(output.find("######") != std::string::npos)
        << "Chart should contain bars";
}

TEST_F(PrintUtilsTest, PrintBarChartEmpty) {
    std::map<std::string, int> emptyData;
    printBarChart(emptyData);
    std::string output = cleanOutput();

    EXPECT_TRUE(output.find("No data to display") != std::string::npos)
        << "Empty bar chart should indicate no data";
}

TEST_F(PrintUtilsTest, PrintBarChartZeroValues) {
    std::map<std::string, int> zeroData = {{"Item1", 0}, {"Item2", 0}};

    printBarChart(zeroData);
    std::string output = cleanOutput();

    EXPECT_TRUE(output.find("All values are zero or negative") !=
                std::string::npos)
        << "Zero values should be indicated";
    EXPECT_TRUE(output.find("Item1") != std::string::npos)
        << "Labels should still be displayed";
}

TEST_F(PrintUtilsTest, PrintBarChartNegativeValues) {
    std::map<std::string, int> negData = {{"Item1", -10}, {"Item2", -5}};

    printBarChart(negData);
    std::string output = cleanOutput();

    EXPECT_TRUE(output.find("All values are zero or negative") !=
                std::string::npos)
        << "Negative values should be indicated";
}

TEST_F(PrintUtilsTest, PrintBarChartInvalidWidth) {
    std::map<std::string, int> data = {{"Item1", 10}, {"Item2", 20}};

    printBarChart(data, -5);
    std::string output = cleanOutput();

    EXPECT_TRUE(output.find("Item1") != std::string::npos)
        << "Chart should be displayed with default width for negative input";
}

TEST_F(PrintUtilsTest, PrintBarChartLongLabels) {
    std::map<std::string, int> data = {{"VeryVeryVeryLongItemName", 10},
                                       {"Item2", 20}};

    printBarChart(data, 15);
    std::string output = cleanOutput();

    EXPECT_TRUE(output.find("...") != std::string::npos)
        << "Very long labels should be truncated with ellipsis";
}

// Tests for the Logger class
TEST_F(PrintUtilsTest, LoggerSingleton) {
    auto& logger1 = Logger::getInstance();
    auto& logger2 = Logger::getInstance();

    // Verify we get the same instance
    EXPECT_EQ(&logger1, &logger2)
        << "Logger singleton should return the same instance";
}

TEST_F(PrintUtilsTest, LoggerOpenAndClose) {
    auto& logger = Logger::getInstance();

    // Create a temporary filename
    std::string testLogFile =
        "test_log_" + std::to_string(time(nullptr)) + ".log";

    // Test opening the file
    bool opened = logger.openLogFile(testLogFile);
    EXPECT_TRUE(opened) << "Logger should successfully open a log file";

    // Close the file explicitly
    logger.close();

    // Clean up
    std::remove(testLogFile.c_str());
}

// Tests for formatting utils
TEST_F(PrintUtilsTest, FormatLiteral) {
    auto formatter = "Hello, {}!"_fmt;
    std::string result = formatter("world");

    EXPECT_EQ(result, "Hello, world!")
        << "Format literal should format strings correctly";
}

TEST_F(PrintUtilsTest, FormatLiteralMultipleArgs) {
    auto formatter = "Value: {}, Status: {}, Success: {}"_fmt;
    std::string result = formatter(42, "active", true);

    EXPECT_EQ(result, "Value: 42, Status: active, Success: true")
        << "Format literal should handle multiple arguments";
}

TEST_F(PrintUtilsTest, FormatLiteralWithInvalidFormat) {
    auto formatter = "Missing closing brace: {"_fmt;
    std::string result = formatter("test");

    EXPECT_TRUE(result.find("Format error") != std::string::npos)
        << "Format literal should handle invalid formats gracefully";
}

// Tests for Timer class
TEST_F(PrintUtilsTest, TimerBasic) {
    Timer timer;
    double elapsed = timer.elapsed();

    EXPECT_GE(elapsed, 0.0)
        << "Timer should return a non-negative elapsed time";
}

TEST_F(PrintUtilsTest, TimerReset) {
    Timer timer;
    // Introduce a delay
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    double elapsed1 = timer.elapsed();

    timer.reset();
    double elapsed2 = timer.elapsed();

    EXPECT_GT(elapsed1, elapsed2) << "Timer reset should restart the timer";
}

TEST_F(PrintUtilsTest, TimerMeasureFunction) {
    auto result = Timer::measure("Test operation", []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return 42;
    });

    EXPECT_EQ(result, 42) << "Timer::measure should return function result";
    std::string output = cleanOutput();
    EXPECT_TRUE(output.find("Test operation completed in") != std::string::npos)
        << "Timer::measure should print operation name and time";
}

TEST_F(PrintUtilsTest, TimerMeasureVoidFunction) {
    outputCapture->clear();
    Timer::measureVoid("Void operation", []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });

    std::string output = cleanOutput();
    EXPECT_TRUE(output.find("Void operation completed in") != std::string::npos)
        << "Timer::measureVoid should print operation name and time";
}

// Tests for CodeBlock class
TEST_F(PrintUtilsTest, CodeBlockIndentation) {
    CodeBlock codeBlock;

    codeBlock.println("Level 0");
    codeBlock.increaseIndent();
    codeBlock.println("Level 1");
    codeBlock.increaseIndent();
    codeBlock.println("Level 2");
    codeBlock.decreaseIndent();
    codeBlock.println("Level 1 again");

    std::string output = cleanOutput();
    std::vector<std::string> lines;
    std::stringstream ss(output);
    std::string line;
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }

    ASSERT_GE(lines.size(), 4) << "Output should have at least 4 lines";
    EXPECT_EQ(lines[0], "Level 0") << "First line should have no indent";
    EXPECT_EQ(lines[1], "    Level 1")
        << "Second line should have one indent level";
    EXPECT_EQ(lines[2], "        Level 2")
        << "Third line should have two indent levels";
    EXPECT_EQ(lines[3], "    Level 1 again")
        << "Fourth line should have one indent level";
}

TEST_F(PrintUtilsTest, CodeBlockScopedIndent) {
    CodeBlock codeBlock;

    codeBlock.println("Level 0");
    {
        auto indent = codeBlock.indent();
        codeBlock.println("Level 1");
        {
            auto indent2 = codeBlock.indent();
            codeBlock.println("Level 2");
        }  // indent automatically decreases here
        codeBlock.println("Level 1 again");
    }  // indent automatically decreases here
    codeBlock.println("Level 0 again");

    std::string output = cleanOutput();
    std::vector<std::string> lines;
    std::stringstream ss(output);
    std::string line;
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }

    ASSERT_GE(lines.size(), 5) << "Output should have at least 5 lines";
    EXPECT_EQ(lines[0], "Level 0") << "First line should have no indent";
    EXPECT_EQ(lines[1], "    Level 1")
        << "Second line should have one indent level";
    EXPECT_EQ(lines[2], "        Level 2")
        << "Third line should have two indent levels";
    EXPECT_EQ(lines[3], "    Level 1 again")
        << "Fourth line should have one indent level";
    EXPECT_EQ(lines[4], "Level 0 again") << "Fifth line should have no indent";
}

// Tests for MathStats class
TEST_F(PrintUtilsTest, MathStatsMean) {
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0};
    double mean = MathStats::mean(data);

    EXPECT_DOUBLE_EQ(mean, 3.0) << "Mean calculation should be accurate";
}

TEST_F(PrintUtilsTest, MathStatsEmptyMean) {
    std::vector<double> emptyData;
    EXPECT_THROW(MathStats::mean(emptyData), std::invalid_argument)
        << "Mean of empty container should throw exception";
}

TEST_F(PrintUtilsTest, MathStatsMedian) {
    std::vector<double> oddData = {5.0, 1.0, 3.0, 2.0, 4.0};
    double oddMedian = MathStats::median(oddData);
    EXPECT_DOUBLE_EQ(oddMedian, 3.0)
        << "Median of odd-sized sorted data should be middle element";

    std::vector<double> evenData = {1.0, 3.0, 5.0, 7.0};
    double evenMedian = MathStats::median(evenData);
    EXPECT_DOUBLE_EQ(evenMedian, 4.0) << "Median of even-sized sorted data "
                                         "should be average of middle elements";
}

TEST_F(PrintUtilsTest, MathStatsStandardDeviation) {
    std::vector<double> data = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    double stddev = MathStats::standardDeviation(data);

    // Expected standard deviation: 2.0
    EXPECT_NEAR(stddev, 2.0, 0.0001)
        << "Standard deviation calculation should be accurate";
}

// Tests for text styling functions
TEST_F(PrintUtilsTest, PrintStyled) {
    printStyled(TextStyle::BOLD, "Bold text");
    std::string output = outputCapture->getOutput();

    EXPECT_TRUE(output.find("\033[1mBold text\033[0m") != std::string::npos)
        << "Bold text should have appropriate ANSI codes";
    outputCapture->clear();

    printStyled(TextStyle::UNDERLINE, "Underlined text");
    output = outputCapture->getOutput();
    EXPECT_TRUE(output.find("\033[4mUnderlined text\033[0m") !=
                std::string::npos)
        << "Underlined text should have appropriate ANSI codes";
}

TEST_F(PrintUtilsTest, PrintColored) {
    printColored(Color::RED, "Red text");
    std::string output = outputCapture->getOutput();

    EXPECT_TRUE(output.find("\033[31mRed text\033[0m") != std::string::npos)
        << "Red text should have appropriate ANSI codes";
    outputCapture->clear();

    printColored(Color::BLUE, "Blue text");
    output = outputCapture->getOutput();
    EXPECT_TRUE(output.find("\033[34mBlue text\033[0m") != std::string::npos)
        << "Blue text should have appropriate ANSI codes";
}

// Tests for thread-safe logging
TEST_F(PrintUtilsTest, ThreadSafeLogging) {
    std::string testLogFile = "thread_safe_log_test.log";
    std::ofstream file(testLogFile, std::ios::trunc);
    file.close();

    auto logOperation = [&testLogFile]() {
        std::ofstream logFile(testLogFile, std::ios::app);
        log(logFile, LogLevel::INFO, "Test log message from thread {}",
            std::this_thread::get_id());
    };

    // Create multiple threads to test thread safety
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back(logOperation);
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify log file has entries
    std::ifstream logFile(testLogFile);
    std::string content((std::istreambuf_iterator<char>(logFile)),
                        std::istreambuf_iterator<char>());

    EXPECT_GE(std::count(content.begin(), content.end(), '\n'), 5)
        << "Log file should contain at least 5 entries";

    // Clean up
    std::remove(testLogFile.c_str());
}

// Test example for MemoryTracker
TEST_F(PrintUtilsTest, MemoryTracker) {
    MemoryTracker tracker;

    tracker.allocate("Buffer1", 1024);
    tracker.allocate("Buffer2", 2048);

    outputCapture->clear();
    tracker.printUsage();
    std::string output = cleanOutput();

    EXPECT_TRUE(output.find("Buffer1: 1024 bytes") != std::string::npos)
        << "Memory tracker should report Buffer1";
    EXPECT_TRUE(output.find("Buffer2: 2048 bytes") != std::string::npos)
        << "Memory tracker should report Buffer2";
    EXPECT_TRUE(output.find("Total memory usage:") != std::string::npos)
        << "Memory tracker should report total usage";

    // Test deallocation
    tracker.deallocate("Buffer1");

    outputCapture->clear();
    tracker.printUsage();
    output = cleanOutput();

    EXPECT_FALSE(output.find("Buffer1") != std::string::npos)
        << "Deallocated buffer should not appear in report";
    EXPECT_TRUE(output.find("Buffer2: 2048 bytes") != std::string::npos)
        << "Buffer2 should still be reported";
}

}  // namespace atom::utils::tests