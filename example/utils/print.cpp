/*
 * print_utilities_example.cpp
 *
 * This example demonstrates the usage of the print utilities provided by
 * the atom::utils namespace.
 *
 * Copyright (C) 2024 Example User
 */

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "atom/utils/print.hpp"

// A sample class to demonstrate printing custom objects
class Point {
private:
    double x, y;

public:
    Point(double x, double y) : x(x), y(y) {}

    friend std::ostream& operator<<(std::ostream& os, const Point& p) {
        os << "Point(" << p.x << ", " << p.y << ")";
        return os;
    }
};

// Helper function to generate random data
std::map<std::string, int> generateRandomData(int count) {
    std::map<std::string, int> data;
    std::vector<std::string> categories = {
        "Apples",  "Oranges", "Bananas",    "Grapes",      "Strawberries",
        "Peaches", "Pears",   "Pineapples", "Watermelons", "Cherries"};

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10, 200);

    for (int i = 0; i < count && i < static_cast<int>(categories.size()); ++i) {
        data[categories[i]] = dis(gen);
    }

    return data;
}

// Example JSON string for formatting
const std::string SAMPLE_JSON = R"({
     "name": "John Doe",
     "age": 30,
     "address": {
         "street": "123 Main St",
         "city": "Springfield",
         "state": "IL",
         "zip": "62701"
     },
     "phoneNumbers": [
         {"type": "home", "number": "555-1234"},
         {"type": "work", "number": "555-5678"}
     ],
     "isActive": true,
     "balance": 123.45
 })";

// Helper function to demonstrate the timer
void performHeavyComputation() {
    std::vector<int> data(10000000);
    std::iota(data.begin(), data.end(), 1);

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(data.begin(), data.end(), g);

    std::sort(data.begin(), data.end());
}

// Helper function to demonstrate the memory tracker
class BigObject {
private:
    std::vector<double> data;

public:
    explicit BigObject(size_t size) : data(size, 0.0) {
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<double>(i);
        }
    }
};

int main() {
    std::cout << "========================================================="
              << std::endl;
    std::cout << "        ATOM PRINT UTILITIES COMPREHENSIVE EXAMPLE        "
              << std::endl;
    std::cout << "========================================================="
              << std::endl;

    // ==========================================
    // 1. Basic Printing Functions
    // ==========================================
    std::cout << "\n=== 1. Basic Printing Functions ===" << std::endl;

    // Basic print and println
    atom::utils::print("Hello, {}! The answer is {}.\n", "World", 42);
    atom::utils::println("This is a complete line with value: {}", 3.14159);

    // Printing to custom streams
    std::ostringstream oss;
    atom::utils::printToStream(oss, "This goes to a string stream: {}",
                               "custom text");
    std::cout << "Stream content: " << oss.str() << std::endl;

    atom::utils::printlnToStream(std::cout,
                                 "This is printed with a newline: {}", 100);

    // File output (comment out if you don't want file creation)
    atom::utils::printToFile("print_example_output.txt",
                             "This text is written to a file: {}",
                             "Hello from the print utilities!");
    std::cout << "Text written to 'print_example_output.txt'" << std::endl;

    // ==========================================
    // 2. Colored and Styled Text
    // ==========================================
    std::cout << "\n=== 2. Colored and Styled Text ===" << std::endl;

    // Colored output
    std::cout << "Different colored text examples: ";
    atom::utils::printColored(atom::utils::Color::RED, "Red Text ");
    atom::utils::printColored(atom::utils::Color::GREEN, "Green Text ");
    atom::utils::printColored(atom::utils::Color::BLUE, "Blue Text ");
    atom::utils::printColored(atom::utils::Color::YELLOW, "Yellow Text");
    std::cout << std::endl;

    // Text styling
    std::cout << "Different text styles: ";
    atom::utils::printStyled(atom::utils::TextStyle::BOLD, "Bold ");
    atom::utils::printStyled(atom::utils::TextStyle::UNDERLINE, "Underlined ");
    atom::utils::printStyled(atom::utils::TextStyle::BLINK, "Blinking ");
    atom::utils::printStyled(atom::utils::TextStyle::REVERSE, "Reversed");
    std::cout << std::endl;

    // ==========================================
    // 3. Progress Bars
    // ==========================================
    std::cout << "\n=== 3. Progress Bars ===" << std::endl;

    // Demonstrate different progress bar styles
    std::cout << "Basic Progress Bar:" << std::endl;
    for (int i = 0; i <= 10; ++i) {
        atom::utils::printProgressBar(i / 10.0f, 40,
                                      atom::utils::ProgressBarStyle::BASIC);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << std::endl;

    std::cout << "Block Progress Bar:" << std::endl;
    for (int i = 0; i <= 10; ++i) {
        atom::utils::printProgressBar(i / 10.0f, 40,
                                      atom::utils::ProgressBarStyle::BLOCK);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << std::endl;

    std::cout << "Arrow Progress Bar:" << std::endl;
    for (int i = 0; i <= 10; ++i) {
        atom::utils::printProgressBar(i / 10.0f, 40,
                                      atom::utils::ProgressBarStyle::ARROW);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << std::endl;

    std::cout << "Percentage Progress Bar:" << std::endl;
    for (int i = 0; i <= 10; ++i) {
        atom::utils::printProgressBar(
            i / 10.0f, 40, atom::utils::ProgressBarStyle::PERCENTAGE);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << std::endl << std::endl;

    // ==========================================
    // 4. Formatted Tables
    // ==========================================
    std::cout << "=== 4. Formatted Tables ===" << std::endl;

    // Simple table
    std::vector<std::vector<std::string>> simpleTable = {
        {"Header 1", "Header 2", "Header 3"},
        {"Value 1", "Value 2", "Value 3"},
        {"Longer Value", "Short", "Medium Value"}};

    std::cout << "Simple Table:" << std::endl;
    atom::utils::printTable(simpleTable);
    std::cout << std::endl;

    // Complex table with more rows
    std::vector<std::vector<std::string>> complexTable = {
        {"ID", "Name", "Department", "Position", "Salary"},
        {"1", "John Doe", "Engineering", "Senior Developer", "$120,000"},
        {"2", "Jane Smith", "Marketing", "Director", "$140,000"},
        {"3", "Bob Johnson", "Finance", "Analyst", "$95,000"},
        {"4", "Alice Williams", "HR", "Manager", "$105,000"},
        {"5", "Charlie Brown", "Engineering", "Lead Developer", "$130,000"}};

    std::cout << "Employee Information Table:" << std::endl;
    atom::utils::printTable(complexTable);
    std::cout << std::endl;

    // ==========================================
    // 5. JSON Formatting
    // ==========================================
    std::cout << "=== 5. JSON Formatting ===" << std::endl;

    std::cout << "Formatted JSON:" << std::endl;
    atom::utils::printJson(SAMPLE_JSON, 4);
    std::cout << std::endl;

    // ==========================================
    // 6. Bar Charts
    // ==========================================
    std::cout << "=== 6. Bar Charts ===" << std::endl;

    std::cout << "Simple Bar Chart:" << std::endl;
    std::map<std::string, int> fruitData = {{"Apples", 120},
                                            {"Oranges", 75},
                                            {"Bananas", 150},
                                            {"Grapes", 90},
                                            {"Strawberries", 60}};

    atom::utils::printBarChart(fruitData, 40);
    std::cout << std::endl;

    std::cout << "Random Data Bar Chart:" << std::endl;
    auto randomData = generateRandomData(8);
    atom::utils::printBarChart(randomData, 50);
    std::cout << std::endl;

    // ==========================================
    // 7. Timing Operations
    // ==========================================
    std::cout << "=== 7. Timing Operations ===" << std::endl;

    // Manual timing
    {
        atom::utils::Timer timer;
        std::cout << "Starting a heavy computation..." << std::endl;
        performHeavyComputation();
        std::cout << "Computation completed in " << timer.elapsed()
                  << " seconds" << std::endl;
    }

    // Automatic timing with return value
    int result = atom::utils::Timer::measure("Vector summation", []() {
        std::vector<int> data(5000000);
        std::iota(data.begin(), data.end(), 1);
        return std::accumulate(data.begin(), data.end(), 0);
    });
    std::cout << "Sum result: " << result << std::endl;

    // Automatic timing with void function
    atom::utils::Timer::measureVoid("Vector shuffling", []() {
        std::vector<int> data(3000000);
        std::iota(data.begin(), data.end(), 1);

        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(data.begin(), data.end(), g);
    });

    // ==========================================
    // 8. Code Block Formatting
    // ==========================================
    std::cout << "\n=== 8. Code Block Formatting ===" << std::endl;

    atom::utils::CodeBlock codeBlock;

    codeBlock.println("function calculateTotal(items) {");
    {
        auto indented = codeBlock.indent();
        codeBlock.println("let total = 0;");
        codeBlock.println("for (let i = 0; i < items.length; i++) {");
        {
            auto furtherIndented = codeBlock.indent();
            codeBlock.println("total += items[i].price * items[i].quantity;");
        }
        codeBlock.println("}");
        codeBlock.println("return total;");
    }
    codeBlock.println("}");

    std::cout << std::endl;

    // ==========================================
    // 9. Mathematical Statistics
    // ==========================================
    std::cout << "=== 9. Mathematical Statistics ===" << std::endl;

    std::vector<double> dataPoints = {12.5, 7.2,  15.8, 9.3,  11.1,
                                      8.7,  14.2, 10.5, 13.6, 6.9};

    std::cout << "Data points: ";
    for (const auto& point : dataPoints) {
        std::cout << point << " ";
    }
    std::cout << std::endl;

    try {
        double meanValue = atom::utils::MathStats::mean(dataPoints);
        double medianValue = atom::utils::MathStats::median(dataPoints);
        double stdDev = atom::utils::MathStats::standardDeviation(dataPoints);

        std::cout << "Mean: " << meanValue << std::endl;
        std::cout << "Median: " << medianValue << std::endl;
        std::cout << "Standard Deviation: " << stdDev << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error calculating statistics: " << e.what() << std::endl;
    }

    std::cout << std::endl;

    // ==========================================
    // 10. Memory Tracking
    // ==========================================
    std::cout << "=== 10. Memory Tracking ===" << std::endl;

    atom::utils::MemoryTracker memTracker;

    // Track allocations
    memTracker.allocate("Small Buffer", 1024);
    memTracker.allocate("Medium Buffer", 1024 * 1024);

    // Create a large object and track it
    {
        auto largeObject =
            std::make_unique<BigObject>(1000000);  // 8MB for doubles
        memTracker.allocate("Large Object", 1000000 * sizeof(double));

        std::cout << "Memory usage with Large Object:" << std::endl;
        memTracker.printUsage();

        // Deallocate the large object
        memTracker.deallocate("Large Object");
    }

    std::cout << "\nMemory usage after Large Object deallocation:" << std::endl;
    memTracker.printUsage();

    std::cout << std::endl;

    // ==========================================
    // 11. Logging System
    // ==========================================
    std::cout << "=== 11. Logging System ===" << std::endl;

    // Direct logging to console
    atom::utils::log(std::cout, atom::utils::LogLevel::INFO,
                     "This is an information message: {}", 42);
    atom::utils::log(std::cout, atom::utils::LogLevel::WARNING,
                     "This is a warning: {} is approaching threshold", "Value");
    atom::utils::log(std::cout, atom::utils::LogLevel::ERROR,
                     "This is an error: Could not process {}", "request");
    atom::utils::log(std::cout, atom::utils::LogLevel::DEBUG,
                     "Debug info: process took {} ms", 153.76);

    // Using the singleton logger
    atom::utils::Logger& logger = atom::utils::Logger::getInstance();
    logger.openLogFile("application.log");

    logger.log(atom::utils::LogLevel::INFO, "Application started");
    logger.log(atom::utils::LogLevel::DEBUG,
               "Configuration loaded with {} settings", 15);
    logger.log(atom::utils::LogLevel::WARNING, "Disk space below {}%", 20);
    logger.log(atom::utils::LogLevel::ERROR,
               "Failed to connect to database: {}", "Timeout");

    std::cout << "Log entries written to 'application.log'" << std::endl;
    std::cout << std::endl;

    // ==========================================
    // 12. Format Literal
    // ==========================================
    std::cout << "=== 12. Format Literal ===" << std::endl;

    // Using the user-defined literal for format strings
    auto greeting = "Hello, {}!"_fmt("world");
    std::cout << greeting << std::endl;

    auto calculation = "The sum of {} and {} is {}"_fmt(5, 7, 5 + 7);
    std::cout << calculation << std::endl;

    auto complex = "Object: {}, Value: {:.2f}, Status: {}"_fmt("UserAccount",
                                                               157.2543, true);
    std::cout << complex << std::endl;

    std::cout << std::endl;

    // ==========================================
    // 13. Container Formatting (C++23 feature)
    // ==========================================
    std::cout << "=== 13. Container Formatting (C++23 feature) ==="
              << std::endl;
    std::cout
        << "Note: This requires C++23 support. Examples shown for reference."
        << std::endl;

    std::cout << "Vector formatting: vector<int>{1, 2, 3, 4, 5}" << std::endl;
    std::cout << "Map formatting: map<string, int>{\"a\": 1, \"b\": 2}"
              << std::endl;
    std::cout << "Optional formatting: Optional(42) or Optional()" << std::endl;
    std::cout << "Tuple formatting: (1, \"text\", 3.14)" << std::endl;

    std::cout << std::endl;

    std::cout << "========================================================="
              << std::endl;
    std::cout << "                EXAMPLES COMPLETED                        "
              << std::endl;
    std::cout << "========================================================="
              << std::endl;

    return 0;
}