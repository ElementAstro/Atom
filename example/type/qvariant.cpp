#include "../atom/type/qvariant.hpp"
#include <cassert>
#include <chrono>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

using namespace atom::type;
using namespace std::string_literals;

// Custom struct for testing
struct MyData {
    int id;
    std::string name;

    MyData(int i, std::string n) : id(i), name(std::move(n)) {}

    // Make it streamable
    friend std::ostream& operator<<(std::ostream& os, const MyData& data) {
        os << "MyData{id=" << data.id << ", name=\"" << data.name << "\"}";
        return os;
    }

    // Equality operator
    bool operator==(const MyData& other) const {
        return id == other.id && name == other.name;
    }
};

// Helper function to print a header
void printHeader(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

// Helper function to print a section
void printSection(const std::string& section) {
    std::cout << "\n--- " << section << " ---\n";
}

// 1. Basic Usage
void basicUsageExample() {
    printHeader("Basic Usage");

    // Create empty variant
    printSection("Creating and checking empty variant");
    VariantWrapper<int, double, std::string, MyData> empty;
    std::cout << "Empty variant type: " << empty.typeName() << std::endl;
    std::cout << "Has value: " << (empty.hasValue() ? "true" : "false")
              << std::endl;

    // Create with different types
    printSection("Creating variants with different types");
    VariantWrapper<int, double, std::string, MyData> intVar(42);
    VariantWrapper<int, double, std::string, MyData> doubleVar(3.14);
    VariantWrapper<int, double, std::string, MyData> stringVar(
        "Hello, world!"s);
    VariantWrapper<int, double, std::string, MyData> customVar(
        MyData(1, "test"));

    // Print values
    std::cout << "Integer variant: ";
    intVar.print();

    std::cout << "Double variant: ";
    doubleVar.print();

    std::cout << "String variant: ";
    stringVar.print();

    std::cout << "Custom data variant: ";
    customVar.print();

    // Get type information
    printSection("Type information");
    std::cout << "intVar type name: " << intVar.typeName() << std::endl;
    std::cout << "doubleVar type name: " << doubleVar.typeName() << std::endl;
    std::cout << "stringVar type name: " << stringVar.typeName() << std::endl;
    std::cout << "customVar type name: " << customVar.typeName() << std::endl;

    // Check types
    printSection("Type checking");
    std::cout << "intVar holds int: " << (intVar.is<int>() ? "true" : "false")
              << std::endl;
    std::cout << "intVar holds double: "
              << (intVar.is<double>() ? "true" : "false") << std::endl;
    std::cout << "doubleVar holds double: "
              << (doubleVar.is<double>() ? "true" : "false") << std::endl;
    std::cout << "stringVar holds string: "
              << (stringVar.is<std::string>() ? "true" : "false") << std::endl;
    std::cout << "customVar holds MyData: "
              << (customVar.is<MyData>() ? "true" : "false") << std::endl;
}

// 2. Accessing and Modifying Values
void accessingValuesExample() {
    printHeader("Accessing and Modifying Values");

    // Create variants
    VariantWrapper<int, double, std::string, MyData> intVar(42);
    VariantWrapper<int, double, std::string, MyData> doubleVar(3.14);
    VariantWrapper<int, double, std::string, MyData> stringVar(
        "Hello, world!"s);
    VariantWrapper<int, double, std::string, MyData> customVar(
        MyData(1, "test"));

    // Get values using get()
    printSection("Getting values with get()");
    try {
        int i = intVar.get<int>();
        double d = doubleVar.get<double>();
        std::string s = stringVar.get<std::string>();
        MyData c = customVar.get<MyData>();

        std::cout << "intVar value: " << i << std::endl;
        std::cout << "doubleVar value: " << d << std::endl;
        std::cout << "stringVar value: " << s << std::endl;
        std::cout << "customVar value: " << c << std::endl;
    } catch (const VariantException& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }

    // Try to get incorrect types
    printSection("Error handling with get()");
    try {
        // This should throw an exception
        double wrongType = intVar.get<double>();
        std::cout << "This should not print: " << wrongType << std::endl;
    } catch (const VariantException& e) {
        std::cout << "Expected exception: " << e.what() << std::endl;
    }

    // Use tryGet()
    printSection("Safe access with tryGet()");
    if (auto optInt = intVar.tryGet<int>()) {
        std::cout << "Successfully got int: " << *optInt << std::endl;
    } else {
        std::cout << "Failed to get int" << std::endl;
    }

    if (auto optDouble = intVar.tryGet<double>()) {
        std::cout << "This should not print: " << *optDouble << std::endl;
    } else {
        std::cout << "As expected, failed to get double from int variant"
                  << std::endl;
    }

    // Modify values
    printSection("Modifying values");
    intVar = 99;
    doubleVar = 2.71828;
    stringVar = "Modified string"s;
    customVar = MyData(2, "updated");

    std::cout << "After modification:" << std::endl;
    std::cout << "intVar: " << intVar.get<int>() << std::endl;
    std::cout << "doubleVar: " << doubleVar.get<double>() << std::endl;
    std::cout << "stringVar: " << stringVar.get<std::string>() << std::endl;
    std::cout << "customVar: " << customVar.get<MyData>() << std::endl;

    // Reset a variant
    printSection("Resetting a variant");
    intVar.reset();
    std::cout << "After reset, intVar has value: "
              << (intVar.hasValue() ? "true" : "false") << std::endl;
    std::cout << "intVar type after reset: " << intVar.typeName() << std::endl;
}

// 3. Type Conversion
void typeConversionExample() {
    printHeader("Type Conversion");

    // Create variants of different types
    VariantWrapper<int, double, std::string, bool> intVar(42);
    VariantWrapper<int, double, std::string, bool> doubleVar(3.14);
    VariantWrapper<int, double, std::string, bool> stringVar1("123"s);
    VariantWrapper<int, double, std::string, bool> stringVar2("3.14"s);
    VariantWrapper<int, double, std::string, bool> stringVar3("true"s);
    VariantWrapper<int, double, std::string, bool> boolVar(true);

    // Convert to int
    printSection("Converting to int");
    if (auto val = intVar.toInt()) {
        std::cout << "int -> int: " << *val << std::endl;
    }

    if (auto val = doubleVar.toInt()) {
        std::cout << "double -> int: " << *val << std::endl;
    }

    if (auto val = stringVar1.toInt()) {
        std::cout << "string \"123\" -> int: " << *val << std::endl;
    }

    if (auto val = stringVar2.toInt()) {
        std::cout << "string \"3.14\" -> int: " << *val << std::endl;
    }

    if (auto val = boolVar.toInt()) {
        std::cout << "bool -> int: " << *val << std::endl;
    }

    // Convert to double
    printSection("Converting to double");
    if (auto val = intVar.toDouble()) {
        std::cout << "int -> double: " << *val << std::endl;
    }

    if (auto val = doubleVar.toDouble()) {
        std::cout << "double -> double: " << *val << std::endl;
    }

    if (auto val = stringVar1.toDouble()) {
        std::cout << "string \"123\" -> double: " << *val << std::endl;
    }

    if (auto val = stringVar2.toDouble()) {
        std::cout << "string \"3.14\" -> double: " << *val << std::endl;
    }

    if (auto val = boolVar.toDouble()) {
        std::cout << "bool -> double: " << *val << std::endl;
    }

    // Convert to bool
    printSection("Converting to bool");
    VariantWrapper<int, double, std::string, bool> intZero(0);
    VariantWrapper<int, double, std::string, bool> intOne(1);
    VariantWrapper<int, double, std::string, bool> stringTrue("true"s);
    VariantWrapper<int, double, std::string, bool> stringYes("yes"s);
    VariantWrapper<int, double, std::string, bool> stringFalse("false"s);
    VariantWrapper<int, double, std::string, bool> stringNo("no"s);

    if (auto val = intZero.toBool()) {
        std::cout << "int(0) -> bool: " << (*val ? "true" : "false")
                  << std::endl;
    }

    if (auto val = intOne.toBool()) {
        std::cout << "int(1) -> bool: " << (*val ? "true" : "false")
                  << std::endl;
    }

    if (auto val = stringTrue.toBool()) {
        std::cout << "string \"true\" -> bool: " << (*val ? "true" : "false")
                  << std::endl;
    }

    if (auto val = stringYes.toBool()) {
        std::cout << "string \"yes\" -> bool: " << (*val ? "true" : "false")
                  << std::endl;
    }

    if (auto val = stringFalse.toBool()) {
        std::cout << "string \"false\" -> bool: " << (*val ? "true" : "false")
                  << std::endl;
    }

    if (auto val = stringNo.toBool()) {
        std::cout << "string \"no\" -> bool: " << (*val ? "true" : "false")
                  << std::endl;
    }

    // Convert to string
    printSection("Converting to string");
    std::cout << "int -> string: " << intVar.toString() << std::endl;
    std::cout << "double -> string: " << doubleVar.toString() << std::endl;
    std::cout << "bool -> string: " << boolVar.toString() << std::endl;
    std::cout << "string -> string: " << stringVar1.toString() << std::endl;
}

// 4. Visiting Pattern
void visitingPatternExample() {
    printHeader("Visiting Pattern");

    // Create variants
    VariantWrapper<int, double, std::string, MyData> intVar(42);
    VariantWrapper<int, double, std::string, MyData> doubleVar(3.14);
    VariantWrapper<int, double, std::string, MyData> stringVar("Hello"s);
    VariantWrapper<int, double, std::string, MyData> customVar(
        MyData(3, "custom"));
    VariantWrapper<int, double, std::string, MyData> emptyVar;

    printSection("Simple visitor");
    // Define a simple visitor that returns a description
    auto describer = [](const auto& value) -> std::string {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
            return "This variant is empty";
        } else if constexpr (std::is_same_v<T, int>) {
            return "This variant contains an integer: " + std::to_string(value);
        } else if constexpr (std::is_same_v<T, double>) {
            return "This variant contains a double: " + std::to_string(value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return "This variant contains a string: \"" + value + "\"";
        } else if constexpr (std::is_same_v<T, MyData>) {
            return "This variant contains a MyData object with id: " +
                   std::to_string(value.id);
        } else {
            return "Unknown type";
        }
    };

    // Apply visitor to each variant
    std::cout << "intVar description: " << intVar.visit(describer) << std::endl;
    std::cout << "doubleVar description: " << doubleVar.visit(describer)
              << std::endl;
    std::cout << "stringVar description: " << stringVar.visit(describer)
              << std::endl;
    std::cout << "customVar description: " << customVar.visit(describer)
              << std::endl;
    std::cout << "emptyVar description: " << emptyVar.visit(describer)
              << std::endl;

    printSection("Modifying visitor");
    // Create a visitor that can potentially modify the value
    auto doubler = [](auto& value) -> std::string {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
            return "Can't modify an empty variant";
        } else if constexpr (std::is_same_v<T, int>) {
            value *= 2;
            return "Doubled the integer to: " + std::to_string(value);
        } else if constexpr (std::is_same_v<T, double>) {
            value *= 2.0;
            return "Doubled the double to: " + std::to_string(value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            std::string original = value;
            value = value + value;  // concatenate with itself
            return "Doubled the string from \"" + original + "\" to \"" +
                   value + "\"";
        } else if constexpr (std::is_same_v<T, MyData>) {
            value.id *= 2;
            return "Doubled the MyData id to: " + std::to_string(value.id);
        } else {
            return "Unknown type";
        }
    };

    // Cannot directly use modifier visitor with const visit() function
    // This example shows how type information can be used in the visitor

    printSection("Complex visitor with return type deduction");
    // A visitor that returns different types depending on the variant content
    auto processor =
        [](const auto& value) -> std::variant<int, double, std::string> {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
            return 0;  // Return a default int for empty variant
        } else if constexpr (std::is_same_v<T, int>) {
            return value * value;  // Square the int
        } else if constexpr (std::is_same_v<T, double>) {
            return std::sqrt(value);  // Return square root
        } else if constexpr (std::is_same_v<T, std::string>) {
            return "Processed: " + value;  // Add prefix
        } else if constexpr (std::is_same_v<T, MyData>) {
            return "ID: " + std::to_string(value.id);  // Extract ID as string
        } else {
            return "Unknown type";
        }
    };

    // Get processed values
    auto result1 = intVar.visit(processor);
    auto result2 = doubleVar.visit(processor);
    auto result3 = stringVar.visit(processor);
    auto result4 = customVar.visit(processor);

    // Print processed results using another visitor
    auto resultPrinter = [](const auto& val) {
        std::cout << "Processed result: " << val << std::endl;
    };

    std::visit(resultPrinter, result1);
    std::visit(resultPrinter, result2);
    std::visit(resultPrinter, result3);
    std::visit(resultPrinter, result4);
}

// 5. Comparison and Stream Output
void comparisonAndOutputExample() {
    printHeader("Comparison and Stream Output");

    // Create variants for comparison
    VariantWrapper<int, double, std::string> var1(42);
    VariantWrapper<int, double, std::string> var2(42);
    VariantWrapper<int, double, std::string> var3(99);
    VariantWrapper<int, double, std::string> var4(3.14);

    printSection("Equality comparison");
    std::cout << "var1 == var2: " << (var1 == var2 ? "true" : "false")
              << std::endl;
    std::cout << "var1 == var3: " << (var1 == var3 ? "true" : "false")
              << std::endl;
    std::cout << "var1 == var4: " << (var1 == var4 ? "true" : "false")
              << std::endl;

    printSection("Inequality comparison");
    std::cout << "var1 != var2: " << (var1 != var2 ? "true" : "false")
              << std::endl;
    std::cout << "var1 != var3: " << (var1 != var3 ? "true" : "false")
              << std::endl;
    std::cout << "var1 != var4: " << (var1 != var4 ? "true" : "false")
              << std::endl;

    printSection("Stream output");
    std::cout << "var1 stream output: " << var1 << std::endl;
    std::cout << "var4 stream output: " << var4 << std::endl;

    // Create a more complex variant
    VariantWrapper<int, double, std::string, MyData> customVar(
        MyData(5, "stream test"));
    std::cout << "customVar stream output: " << customVar << std::endl;

    // Stream an empty variant
    VariantWrapper<int, double, std::string> emptyVar;
    std::cout << "emptyVar stream output: " << emptyVar << std::endl;
}

// 6. Thread Safety
void threadSafetyExample() {
    printHeader("Thread Safety");

    // Create a shared variant
    VariantWrapper<int, std::string> sharedVar(0);

    printSection("Concurrent reads and writes");
    std::cout << "Starting concurrent operations on shared variant..."
              << std::endl;

    // Create multiple reader threads
    std::vector<std::thread> readers;
    for (int i = 0; i < 3; i++) {
        readers.emplace_back([&sharedVar, i]() {
            for (int j = 0; j < 5; j++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                try {
                    // Use withThreadSafety to execute multiple operations
                    // atomically
                    sharedVar.withThreadSafety([&]() {
                        if (sharedVar.is<int>()) {
                            int val = sharedVar.get<int>();
                            std::cout << "Reader " << i << ": Read int value "
                                      << val << std::endl;
                        } else if (sharedVar.is<std::string>()) {
                            std::string val = sharedVar.get<std::string>();
                            std::cout << "Reader " << i
                                      << ": Read string value \"" << val << "\""
                                      << std::endl;
                        } else {
                            std::cout << "Reader " << i << ": Unknown type"
                                      << std::endl;
                        }
                    });
                } catch (const std::exception& e) {
                    std::cout << "Reader " << i << " exception: " << e.what()
                              << std::endl;
                }
            }
        });
    }

    // Create writer thread
    std::thread writer([&sharedVar]() {
        for (int i = 0; i < 5; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            try {
                if (i % 2 == 0) {
                    int newVal = i * 10;
                    sharedVar = newVal;
                    std::cout << "Writer: Set int value to " << newVal
                              << std::endl;
                } else {
                    std::string newVal = "String value " + std::to_string(i);
                    sharedVar = newVal;
                    std::cout << "Writer: Set string value to \"" << newVal
                              << "\"" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cout << "Writer exception: " << e.what() << std::endl;
            }
        }
    });

    // Join threads
    writer.join();
    for (auto& thread : readers) {
        thread.join();
    }

    std::cout << "All threads completed" << std::endl;
    std::cout << "Final variant value: " << sharedVar << std::endl;
}

// 7. Error Handling
void errorHandlingExample() {
    printHeader("Error Handling");

    // Create some variants
    VariantWrapper<int, double, std::string> intVar(42);
    VariantWrapper<int, double, std::string> emptyVar;

    printSection("Type mismatch errors");
    try {
        // Try to get a string from an int variant
        std::string s = intVar.get<std::string>();
        std::cout << "This should not print: " << s << std::endl;
    } catch (const VariantException& e) {
        std::cout << "Expected exception: " << e.what() << std::endl;
    }

    printSection("Operation on empty variant");
    try {
        // Try to get value from empty variant
        int val = emptyVar.get<int>();
        std::cout << "This should not print: " << val << std::endl;
    } catch (const VariantException& e) {
        std::cout << "Expected exception: " << e.what() << std::endl;
    }

    printSection("Safe alternatives to throwing functions");
    // Using tryGet instead of get
    if (auto val = intVar.tryGet<int>()) {
        std::cout << "Successfully got int value: " << *val << std::endl;
    } else {
        std::cout << "Failed to get int value" << std::endl;
    }

    if (auto val = intVar.tryGet<std::string>()) {
        std::cout << "This should not print" << std::endl;
    } else {
        std::cout << "As expected, failed to get string from int variant"
                  << std::endl;
    }

    // Using hasValue
    if (emptyVar.hasValue()) {
        std::cout << "This should not print" << std::endl;
    } else {
        std::cout << "Correctly detected empty variant" << std::endl;
    }
}

// 8. Performance Comparison
void performanceExample() {
    printHeader("Performance Comparison");

    constexpr int iterations = 1000000;

    printSection("Construction and assignment");

    // Measure time for VariantWrapper construction
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        VariantWrapper<int, double, std::string> var(i);
        (void)var;  // Prevent optimization
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 =
        std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1)
            .count();

    // Measure time for std::variant construction
    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::variant<std::monostate, int, double, std::string> var(i);
        (void)var;  // Prevent optimization
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 =
        std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2)
            .count();

    std::cout << "Time to construct " << iterations
              << " variants:" << std::endl;
    std::cout << "  VariantWrapper: " << duration1 << " microseconds"
              << std::endl;
    std::cout << "  std::variant:   " << duration2 << " microseconds"
              << std::endl;

    printSection("Access performance");

    // Create variants for access testing
    VariantWrapper<int, double, std::string> wrappedVar(42);
    std::variant<std::monostate, int, double, std::string> stdVar(42);

    // Measure VariantWrapper access
    start1 = std::chrono::high_resolution_clock::now();
    int sum1 = 0;
    for (int i = 0; i < iterations; ++i) {
        if (auto val = wrappedVar.tryGet<int>()) {
            sum1 += *val;
        }
    }
    end1 = std::chrono::high_resolution_clock::now();
    duration1 =
        std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1)
            .count();

    // Measure std::variant access
    start2 = std::chrono::high_resolution_clock::now();
    int sum2 = 0;
    for (int i = 0; i < iterations; ++i) {
        if (std::holds_alternative<int>(stdVar)) {
            sum2 += std::get<int>(stdVar);
        }
    }
    end2 = std::chrono::high_resolution_clock::now();
    duration2 =
        std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2)
            .count();

    std::cout << "Time to access " << iterations << " times:" << std::endl;
    std::cout << "  VariantWrapper: " << duration1
              << " microseconds (sum: " << sum1 << ")" << std::endl;
    std::cout << "  std::variant:   " << duration2
              << " microseconds (sum: " << sum2 << ")" << std::endl;
}

// 9. Advanced Use Cases
void advancedUseCasesExample() {
    printHeader("Advanced Use Cases");

    printSection("Heterogeneous collection");
    // Create a vector of variants to store different types
    std::vector<VariantWrapper<int, double, std::string, MyData>> collection;

    // Add different types of data
    collection.emplace_back(42);
    collection.emplace_back(3.14159);
    collection.emplace_back("Hello, variant world!"s);
    collection.emplace_back(MyData(100, "Custom object"));

    // Process all elements
    std::cout << "Processing heterogeneous collection:" << std::endl;
    for (size_t i = 0; i < collection.size(); ++i) {
        std::cout << "Item " << i << ": " << collection[i].toString()
                  << " (Type: " << collection[i].typeName() << ")" << std::endl;
    }

    // Using variants for dynamic settings
    printSection("Configuration system");

    // Simple settings store using a map of variants
    std::map<std::string, VariantWrapper<int, double, bool, std::string>>
        settings;

    // Store different setting types
    settings["max_connections"] = 100;
    settings["timeout"] = 30.5;
    settings["debug_mode"] = true;
    settings["server_name"] = "variant_test_server"s;

    // Access settings
    std::cout << "Configuration settings:" << std::endl;
    for (const auto& [key, value] : settings) {
        std::cout << "  " << key << " = " << value.toString() << std::endl;
    }

    // Update a setting
    settings["max_connections"] = 200;
    std::cout << "Updated max_connections to: "
              << settings["max_connections"].toString() << std::endl;

    // Type-safe access to settings
    if (auto maxConn = settings["max_connections"].tryGet<int>()) {
        std::cout << "Max connections (typed): " << *maxConn << std::endl;
    }

    if (auto timeout = settings["timeout"].toDouble()) {
        std::cout << "Timeout (converted): " << *timeout << " seconds"
                  << std::endl;
    }

    printSection("Command pattern with variants");

    // Define a simple command processor function
    auto processCommand =
        [](const VariantWrapper<int, std::string, std::vector<double>>&
               command) {
            return command.visit([](const auto& value) -> std::string {
                using T = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<T, std::monostate>) {
                    return "Error: Empty command";
                } else if constexpr (std::is_same_v<T, int>) {
                    return "Executed numeric command: " + std::to_string(value);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return "Executed text command: " + value;
                } else if constexpr (std::is_same_v<T, std::vector<double>>) {
                    std::ostringstream result;
                    result << "Executed vector command with " << value.size()
                           << " elements: ";
                    for (size_t i = 0; i < value.size(); ++i) {
                        if (i > 0)
                            result << ", ";
                        result << value[i];
                    }
                    return result.str();
                } else {
                    return "Unknown command type";
                }
            });
        };

    // Execute different types of commands
    VariantWrapper<int, std::string, std::vector<double>> cmd1(42);
    VariantWrapper<int, std::string, std::vector<double>> cmd2("print"s);
    VariantWrapper<int, std::string, std::vector<double>> cmd3(
        std::vector<double>{1.1, 2.2, 3.3});

    std::cout << "Command results:" << std::endl;
    std::cout << "  Command 1: " << processCommand(cmd1) << std::endl;
    std::cout << "  Command 2: " << processCommand(cmd2) << std::endl;
    std::cout << "  Command 3: " << processCommand(cmd3) << std::endl;
}

struct CustomStringable {
    int x;
    double y;

    friend std::ostream& operator<<(std::ostream& os,
                                    const CustomStringable& obj) {
        os << "CustomStringable{" << obj.x << ", " << obj.y << "}";
        return os;
    }
};

// 10. Compatibility and Conversions
void compatibilityExample() {
    printHeader("Compatibility and Conversions");

    printSection("Construction from different variant types");

    // Create a variant with one set of types
    VariantWrapper<int, double> simpleVar(3.14);

    // Create a variant with a superset of types from the first variant
    VariantWrapper<int, double, std::string> extendedVar(simpleVar);

    std::cout << "Original variant value: " << simpleVar << std::endl;
    std::cout << "Extended variant value: " << extendedVar << std::endl;
    std::cout << "Extended variant type: " << extendedVar.typeName()
              << std::endl;

    // Type index information
    printSection("Type index information");

    VariantWrapper<int, double, std::string, MyData> indexVar1(42);
    VariantWrapper<int, double, std::string, MyData> indexVar2(3.14);
    VariantWrapper<int, double, std::string, MyData> indexVar3("Hello"s);
    VariantWrapper<int, double, std::string, MyData> indexVar4(
        MyData(5, "test"));
    VariantWrapper<int, double, std::string, MyData> indexVar5;  // monostate

    std::cout << "Type indexes:" << std::endl;
    std::cout << "  int variant index: " << indexVar1.index() << std::endl;
    std::cout << "  double variant index: " << indexVar2.index() << std::endl;
    std::cout << "  string variant index: " << indexVar3.index() << std::endl;
    std::cout << "  MyData variant index: " << indexVar4.index() << std::endl;
    std::cout << "  monostate variant index: " << indexVar5.index()
              << std::endl;

    // String conversions from different types
    printSection("String conversion with custom types");

    VariantWrapper<int, CustomStringable> customVar(CustomStringable{10, 20.5});
    std::cout << "Custom streamable type to string: " << customVar.toString()
              << std::endl;
}

int main() {
    std::cout << "===== VariantWrapper<T...> Usage Examples =====" << std::endl;

    try {
        // Run all examples
        basicUsageExample();
        accessingValuesExample();
        typeConversionExample();
        visitingPatternExample();
        comparisonAndOutputExample();
        threadSafetyExample();
        errorHandlingExample();
        performanceExample();
        advancedUseCasesExample();
        compatibilityExample();

        std::cout << "\nAll examples completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\nError occurred in examples: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
