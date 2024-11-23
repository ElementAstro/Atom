#include "atom/utils/print.hpp"

#include <iostream>
#include <map>
#include <optional>
#include <tuple>
#include <variant>
#include <vector>

using namespace atom::utils;

int main() {
    // Log messages with different log levels
    log(std::cout, LogLevel::DEBUG, "This is a debug message");
    log(std::cout, LogLevel::INFO, "This is an info message");
    log(std::cout, LogLevel::WARNING, "This is a warning message");
    log(std::cout, LogLevel::ERROR, "This is an error message");

    // Print formatted messages to the console
    print("Hello, {}!", "World");
    println("Hello, {}!", "World");

    // Print formatted messages to a file
    printToFile("log.txt", "Logging to a file: {}", 123);

    // Print colored messages to the console
    printColored(Color::RED, "This is a red message\n");
    printColored(Color::GREEN, "This is a green message\n");

    // Use a timer to measure elapsed time
    Timer timer;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    double elapsed = timer.elapsed();
    println("Elapsed time: {} seconds", elapsed);

    // Use a code block with indentation
    CodeBlock codeBlock;
    codeBlock.println("This is a code block");
    codeBlock.increaseIndent();
    codeBlock.println("Indented line");
    codeBlock.decreaseIndent();
    codeBlock.println("Back to original indentation");

    // Print styled text
    printStyled(TextStyle::BOLD, "This is bold text\n");
    printStyled(TextStyle::UNDERLINE, "This is underlined text\n");

    // Calculate and print mathematical statistics
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0};
    double meanValue = MathStats::mean(data);
    double medianValue = MathStats::median(data);
    double stddevValue = MathStats::standardDeviation(data);
    println("Mean: {}", meanValue);
    println("Median: {}", medianValue);
    println("Standard Deviation: {}", stddevValue);

    // Track and print memory usage
    MemoryTracker memoryTracker;
    memoryTracker.allocate("Array1", 1024);
    memoryTracker.allocate("Array2", 2048);
    memoryTracker.printUsage();
    memoryTracker.deallocate("Array1");
    memoryTracker.printUsage();

    // Use format literals
    auto formattedString = "Hello, {}!"_fmt("World");
    println(formattedString);

    // Print containers using custom formatters
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::map<std::string, int> map = {{"one", 1}, {"two", 2}, {"three", 3}};
    std::optional<int> opt = 42;
    std::variant<int, std::string> var = "variant";
    std::tuple<int, std::string, double> tup = {1, "tuple", 3.14};

    println("Vector: {}", vec);
    println("Map: {}", map);
    println("Optional: {}", opt);
    println("Variant: {}", var);
    println("Tuple: {}", tup);

    return 0;
}