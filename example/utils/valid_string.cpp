#include "atom/utils/valid_string.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// Helper function to measure execution time
template <typename Func>
double measureExecutionTime(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    return duration.count();
}

// Helper function to print validation results
void printValidationResult(const atom::utils::ValidationResult& result,
                           const std::string& description) {
    std::cout << "=== " << description << " ===" << std::endl;
    std::cout << "Is valid: " << (result.isValid ? "Yes" : "No") << std::endl;

    if (!result.isValid) {
        std::cout << "Error count: " << result.errorMessages.size()
                  << std::endl;

        for (size_t i = 0; i < result.errorMessages.size(); ++i) {
            std::cout << "Error " << (i + 1) << ": " << result.errorMessages[i]
                      << std::endl;

            if (i < result.invalidBrackets.size()) {
                std::cout << "  Character: '"
                          << result.invalidBrackets[i].character
                          << "' at position: "
                          << result.invalidBrackets[i].position << std::endl;
            }
        }
    }
    std::cout << std::endl;
}

// Helper function to test validation performance with different input sizes
void performanceTest() {
    std::cout << "=== Performance Testing ===" << std::endl;
    std::cout << std::setw(15) << "Input Size" << std::setw(15) << "Time (ms)"
              << std::endl;
    std::cout << std::string(30, '-') << std::endl;

    // Generate test strings of different sizes
    std::vector<size_t> sizes = {100, 1000, 10000, 100000, 1000000};

    for (size_t size : sizes) {
        // Create a valid string with nested brackets
        std::string testString;
        testString.reserve(size);

        const size_t pattern_length = 10;  // Length of repeating pattern
        std::string pattern = "({}[<>]){}";

        for (size_t i = 0; i < size / pattern_length + 1; ++i) {
            testString.append(pattern);
            if (testString.size() >= size)
                break;
        }
        testString.resize(size, ' ');

        // Measure validation time
        double time = measureExecutionTime(
            [&]() { atom::utils::isValidBracket(testString); });

        std::cout << std::setw(15) << size << std::setw(15) << std::fixed
                  << std::setprecision(3) << time << std::endl;
    }
    std::cout << std::endl;
}

// Example of writing and validating a file
void fileValidationExample(const std::string& filename) {
    std::cout << "=== File Validation Example ===" << std::endl;

    // Create a test file
    {
        std::ofstream file(filename);
        file << "{\n";
        file << "    \"name\": \"Example\",\n";
        file << "    \"values\": [1, 2, 3],\n";
        file << "    \"nested\": {\n";
        file << "        \"array\": [{\n";
        file << "            \"key\": \"value\"\n";
        file << "        }]\n";
        file << "    }\n";
        file << "}\n";
    }

    // Read the file
    std::string fileContent;
    {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) {
            std::cerr << "Failed to open file: " << filename << std::endl;
            return;
        }

        auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        fileContent.resize(static_cast<size_t>(size));
        file.read(fileContent.data(), size);
    }

    // Validate file content
    auto result = atom::utils::isValidBracket(fileContent);
    printValidationResult(result, "File Validation");

    // Cleanup
    std::filesystem::remove(filename);
    std::cout << std::endl;
}

int main() {
    std::cout << "String Validation Utilities - Example Usage\n" << std::endl;

    // 1. Basic validation examples
    std::cout << "--- Basic Validation Examples ---" << std::endl;

    // Valid strings
    std::string valid1 = "This is (a valid) string with [balanced] {brackets}.";
    std::string valid2 = "Nested brackets are fine too: {[()]}";
    std::string valid3 = "Quotes 'don't affect' bracket \"validation\"";
    std::string valid4 =
        "Escape sequences don't break validation: \\'quote\\' and \\\"double "
        "quote\\\"";

    // Invalid strings
    std::string invalid1 = "This has an extra closing bracket: )";
    std::string invalid2 = "This has an unclosed bracket: (";
    std::string invalid3 = "This has mismatched brackets: {]";
    std::string invalid4 = "Unclosed quote: \"unclosed";

    std::cout << "Validating string: \"" << valid1 << "\"" << std::endl;
    printValidationResult(atom::utils::isValidBracket(valid1),
                          "Valid String 1");

    std::cout << "Validating string: \"" << valid2 << "\"" << std::endl;
    printValidationResult(atom::utils::isValidBracket(valid2),
                          "Valid String 2");

    std::cout << "Validating string: \"" << invalid1 << "\"" << std::endl;
    printValidationResult(atom::utils::isValidBracket(invalid1),
                          "Invalid String 1");

    std::cout << "Validating string: \"" << invalid2 << "\"" << std::endl;
    printValidationResult(atom::utils::isValidBracket(invalid2),
                          "Invalid String 2");

    std::cout << "Validating string: \"" << invalid3 << "\"" << std::endl;
    printValidationResult(atom::utils::isValidBracket(invalid3),
                          "Invalid String 3");

    std::cout << "Validating string: \"" << invalid4 << "\"" << std::endl;
    printValidationResult(atom::utils::isValidBracket(invalid4),
                          "Invalid String 4");

    // 2. Different string types
    std::cout << "\n--- Testing Different String Types ---" << std::endl;

    // C-style string
    const char* cString = "C string with (balanced) brackets";
    printValidationResult(atom::utils::isValidBracket(std::string(cString)),
                          "C-Style String");

    // String view
    std::string_view stringView = "String view with [balanced] brackets";
    printValidationResult(atom::utils::isValidBracket(stringView),
                          "String View");

    // String literal
    auto literalResult =
        atom::utils::isValidBracket("String literal with {balanced} brackets");
    printValidationResult(literalResult, "String Literal");

    // 3. Compile-time validation
    std::cout << "\n--- Compile-Time Validation ---" << std::endl;

    // These are validated at compile time
    constexpr auto compileTimeValid = atom::utils::validateBrackets(
        "Compile-time (validation) is [working] {correctly}");
    constexpr auto compileTimeInvalid = atom::utils::validateBrackets(
        "Compile-time validation detects errors: (");

    std::cout << "Compile-time validation result 1: "
              << (compileTimeValid.isValid() ? "Valid" : "Invalid")
              << std::endl;
    std::cout << "Error count: " << compileTimeValid.getErrorCount()
              << std::endl;

    std::cout << "Compile-time validation result 2: "
              << (compileTimeInvalid.isValid() ? "Valid" : "Invalid")
              << std::endl;
    std::cout << "Error count: " << compileTimeInvalid.getErrorCount()
              << std::endl;

    if (!compileTimeInvalid.isValid()) {
        std::cout << "Error positions: ";
        for (auto pos : compileTimeInvalid.getErrorPositions()) {
            std::cout << pos << " ";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    // 4. Exception handling
    std::cout << "--- Exception Handling ---" << std::endl;

    try {
        atom::utils::validateBracketsWithExceptions(
            "This will throw an exception: {");
    } catch (const atom::utils::ValidationException& e) {
        std::cout << "Caught validation exception: " << e.what() << std::endl;

        // Access the full validation result from the exception
        const auto& result = e.getResult();
        std::cout << "Validation errors: " << result.errorMessages.size()
                  << std::endl;
        for (const auto& msg : result.errorMessages) {
            std::cout << "  " << msg << std::endl;
        }
    }
    std::cout << std::endl;

    // 5. Complex validation examples
    std::cout << "--- Complex Validation Examples ---" << std::endl;

    // Complex nested structure
    std::string complex =
        "function complexExample() { if (condition) { return [1, 2, {key: "
        "'value'}]; } }";
    printValidationResult(atom::utils::isValidBracket(complex),
                          "Complex Code Example");

    // String with comments
    std::string withComments =
        "/* This is a comment with brackets: [] */ function() { return true; }";
    printValidationResult(atom::utils::isValidBracket(withComments),
                          "String with Comments");

    // JSON-like structure
    std::string json = R"({
        "name": "Example",
        "properties": {
            "array": [1, 2, 3],
            "object": {"nested": true}
        },
        "escaped quotes": "Quote with \"escaped quotes\" inside"
    })";
    printValidationResult(atom::utils::isValidBracket(json), "JSON Example");
    std::cout << std::endl;

    // 6. Parallel validation example
    std::cout << "--- Parallel Validation Example ---" << std::endl;

    // Generate a large string to trigger parallel validation
    std::string largeString;
    largeString.reserve(20000);  // Large enough to trigger parallel validation

    // Create a pattern with nested brackets
    for (int i = 0; i < 1000; ++i) {
        largeString += "Level " + std::to_string(i) + ": {[(<" +
                       std::to_string(i) + ">)]} ";
    }

    // Add an error near the end
    largeString += " Error here: { unclosed bracket";

    // Time the validation
    double parallelTime = measureExecutionTime([&]() {
        auto result = atom::utils::isValidBracket(largeString);
        std::cout << "Large string validation result: "
                  << (result.isValid ? "Valid" : "Invalid") << std::endl;
        std::cout << "Error count: " << result.errorMessages.size()
                  << std::endl;

        if (!result.errorMessages.empty()) {
            std::cout << "First error: " << result.errorMessages[0]
                      << std::endl;
        }
    });

    std::cout << "Validation time for large string: " << parallelTime << " ms"
              << std::endl;
    std::cout << std::endl;

    // 7. Performance testing with different string sizes
    performanceTest();

    // 8. File validation example
    fileValidationExample("test_brackets.json");

    // 9. Generic string validation helper
    std::cout << "--- Generic String Validation ---" << std::endl;

    // This automatically detects whether to use compile-time or runtime
    // validation
    auto genericResult1 =
        atom::utils::validateString("String for generic validation (ok)");
    std::cout << "Generic validation result 1: "
              << (genericResult1.isValid ? "Valid" : "Invalid") << std::endl;

    auto genericResult2 =
        atom::utils::validateString(std::string("Runtime string {unclosed"));
    std::cout << "Generic validation result 2: "
              << (genericResult2.isValid ? "Valid" : "Invalid") << std::endl;

    if (!genericResult2.isValid) {
        std::cout << "Error message: " << genericResult2.errorMessages[0]
                  << std::endl;
    }

    return 0;
}
