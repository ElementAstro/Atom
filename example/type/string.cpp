#include "../atom/type/string.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <thread>
#include <vector>

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

// Helper function to print subsection headers
void printSubsection(const std::string& title) {
    std::cout << "\n--- " << title << " ---\n";
}

// Helper to format output for better visualization
void printResult(const std::string& operation, const String& result) {
    std::cout << std::left << std::setw(30) << operation << " : \"" << result
              << "\"\n";
}

// Helper to print boolean results
void printBool(const std::string& operation, bool result) {
    std::cout << std::left << std::setw(30) << operation << " : "
              << (result ? "true" : "false") << "\n";
}

// Helper to print numeric results
template <typename T>
void printValue(const std::string& operation, T value) {
    std::cout << std::left << std::setw(30) << operation << " : " << value
              << "\n";
}

// Example 1: Basic Construction and Operations
void basicConstructionExample() {
    printSection("Basic Construction and Operations");

    // Default constructor
    String empty;
    printResult("Default constructor", empty);

    // From C-string
    String fromCString("Hello, world!");
    printResult("From C-string", fromCString);

    // From string_view
    std::string_view view = "Hello from string_view";
    String fromStringView(view);
    printResult("From string_view", fromStringView);

    // From std::string
    std::string stdStr = "Hello from std::string";
    String fromStdString(stdStr);
    printResult("From std::string", fromStdString);

    // Copy constructor
    String copy(fromCString);
    printResult("Copy constructor", copy);

    // Move constructor
    String temp("Temporary string");
    String moved(std::move(temp));
    printResult("Move constructor", moved);
    printResult("After move, original", temp);

    // Assignment operators
    String assigned;
    assigned = fromCString;
    printResult("Copy assignment", assigned);

    String moveAssigned;
    moveAssigned = String("Move assigned string");
    printResult("Move assignment", moveAssigned);

    // Equality and comparison
    printBool("fromCString == copy", fromCString == copy);
    printBool("fromCString != fromStdString", fromCString != fromStdString);

    String a("apple");
    String b("banana");
    printBool("'apple' < 'banana'", a < b);
    printBool("'banana' > 'apple'", b > a);

    // Concatenation
    String hello("Hello");
    String world(" world");
    String helloWorld = hello + world;
    printResult("Concatenation with +", helloWorld);

    hello += world;
    printResult("Concatenation with +=", hello);

    String s("String");
    s += " with C-string";
    printResult("Concatenation with C-string", s);

    String charConcat("Add char: ");
    charConcat += '!';
    printResult("Concatenation with char", charConcat);
}

// Example 2: Basic String Methods
void basicStringMethodsExample() {
    printSection("Basic String Methods");

    String str("Hello, world! This is a test.");

    // Basic information
    printValue("Length", str.length());
    printValue("Size", str.size());
    printValue("Empty", str.empty());

    // C-string access
    const char* cStr = str.cStr();
    std::cout << "C-string: " << cStr << std::endl;

    // Data access
    std::string data = str.data();
    std::cout << "Data: " << data << std::endl;

    // Character access
    printValue("Character at index 7", str[7]);
    try {
        printValue("Character at index 3 (bounds checked)", str.at(3));
        printValue("Character at index 999 (should throw)", str.at(999));
    } catch (const StringException& e) {
        std::cout << "Expected exception: " << e.what() << std::endl;
    }

    // Modification
    String mutable_str("Modify me");
    mutable_str[0] = 'm';  // Change 'M' to 'm'
    printResult("After modifying first character", mutable_str);

    // Clear
    String clearable("Content to clear");
    clearable.clear();
    printResult("After clear", clearable);
    printBool("Is empty after clear", clearable.empty());

    // Substring
    String source("Extract a substring from this text");
    String sub = source.substr(10, 9);
    printResult("Substring(10, 9)", sub);

    try {
        String outOfBounds = source.substr(100);
        // This should not print
        printResult("Out of bounds substring", outOfBounds);
    } catch (const StringException& e) {
        std::cout << "Expected exception: " << e.what() << std::endl;
    }

    // Capacity management
    String capacity_example("Testing capacity");
    printValue("Initial capacity", capacity_example.capacity());

    capacity_example.reserve(100);
    printValue("After reserve(100)", capacity_example.capacity());
}

// Example 3: String Search and Manipulation
void searchAndManipulationExample() {
    printSection("String Search and Manipulation");

    String haystack("The quick brown fox jumps over the lazy dog");

    // Find
    printSubsection("Find operations");
    printValue("Find 'quick'", haystack.find(String("quick")));
    printValue("Find 'lazy' starting at position 10",
               haystack.find(String("lazy"), 10));
    printValue("Find 'cat' (not present)", haystack.find(String("cat")));

    // Optimized find for large strings
    String largeHaystack(
        "This is a much longer string that would potentially benefit from SIMD "
        "operations "
        "if they are available on your platform. The findOptimized method "
        "should automatically "
        "choose the best implementation based on the string size and available "
        "hardware.");
    String needle("benefit");

    auto start = std::chrono::high_resolution_clock::now();
    size_t pos1 = largeHaystack.find(needle);
    auto end1 = std::chrono::high_resolution_clock::now();

    start = std::chrono::high_resolution_clock::now();
    size_t pos2 = largeHaystack.findOptimized(needle);
    auto end2 = std::chrono::high_resolution_clock::now();

    printValue("Standard find position", pos1);
    printValue("Optimized find position", pos2);

    // Contains
    printSubsection("Contains operations");
    printBool("Contains 'fox'", haystack.contains(String("fox")));
    printBool("Contains 'bear'", haystack.contains(String("bear")));
    printBool("Contains character 'q'", haystack.contains('q'));
    printBool("Contains character 'z'", haystack.contains('z'));

    // StartsWith/EndsWith
    printSubsection("StartsWith/EndsWith operations");
    printBool("Starts with 'The'", haystack.startsWith(String("The")));
    printBool("Starts with 'A'", haystack.startsWith(String("A")));
    printBool("Ends with 'dog'", haystack.endsWith(String("dog")));
    printBool("Ends with 'fox'", haystack.endsWith(String("fox")));

    // Replace
    printSubsection("Replace operations");
    String replaceable("The quick brown fox jumps over the lazy dog");
    bool replaced = replaceable.replace(String("brown"), String("red"));
    printResult("Replace 'brown' with 'red'", replaceable);
    printBool("Replacement successful", replaced);

    bool notReplaced = replaceable.replace(String("purple"), String("orange"));
    printBool("Non-existent string replacement", notReplaced);

    // ReplaceAll
    String multiReplace("one two one two one three one four");
    size_t count = multiReplace.replaceAll(String("one"), String("ONE"));
    printResult("ReplaceAll 'one' with 'ONE'", multiReplace);
    printValue("Number of replacements", count);

    // Very long string for parallel replace
    String veryLongString;
    veryLongString.reserve(20000);
    for (int i = 0; i < 1000; ++i) {
        veryLongString +=
            String("The quick brown fox jumps over the lazy dog. ");
    }

    count = veryLongString.replaceAllParallel(String("fox"), String("cat"));
    printValue("Parallel replace count", count);
    printResult("Sample of result", String(veryLongString.substr(0, 100)));

    // Replace character
    String charReplace("Replace spaces with underscores");
    count = charReplace.replace(' ', '_');
    printResult("Replace spaces with underscores", charReplace);
    printValue("Number of replacements", count);

    // Remove character
    String removeChar("Remove all spaces from this string");
    count = removeChar.remove(' ');
    printResult("Remove all spaces", removeChar);
    printValue("Number of characters removed", count);

    // RemoveAll
    String removeSubstring(
        "Remove all occurrences of 'all' from this string, including the word "
        "all");
    count = removeSubstring.removeAll(String("all"));
    printResult("Remove all 'all'", removeSubstring);
    printValue("Number of occurrences removed", count);

    // Erase
    String erasable("Erase a portion of this string");
    erasable.erase(6, 9);
    printResult("After erase(6, 9)", erasable);

    // Insert
    String insertable("Insert here");
    insertable.insert(7, String(" text"));
    printResult("Insert ' text' at position 7", insertable);

    String insertChar("Insert character");
    insertChar.insert(7, '_');
    printResult("Insert '_' at position 7", insertChar);
}

// Example 4: String Transformation
void transformationExample() {
    printSection("String Transformation");

    String original("Transform This String In Various Ways!");

    // Case conversion
    String upper = original.toUpper();
    printResult("ToUpper", upper);

    String lower = original.toLower();
    printResult("ToLower", lower);

    // Trim
    String withSpaces("  Trim spaces from both ends  ");
    String trimmed = withSpaces;
    trimmed.trim();
    printResult("Original", withSpaces);
    printResult("After trim()", trimmed);

    String leftSpaces("  Trim spaces from left end");
    leftSpaces.ltrim();
    printResult("After ltrim()", leftSpaces);

    String rightSpaces("Trim spaces from right end  ");
    rightSpaces.rtrim();
    printResult("After rtrim()", rightSpaces);

    // Reverse
    String reversible("Reverse this string");
    String reversed = reversible.reverse();
    printResult("Original", reversible);
    printResult("Reversed", reversed);

    // Reverse words
    String sentence("The quick brown fox");
    String reversedWords = sentence.reverseWords();
    printResult("Original sentence", sentence);
    printResult("Reversed words", reversedWords);

    // Pad
    String padMe("Pad");
    padMe.padLeft(10, '-');
    printResult("After padLeft(10, '-')", padMe);

    String padMeRight("Pad");
    padMeRight.padRight(10, '-');
    printResult("After padRight(10, '-')", padMeRight);

    // Remove prefix/suffix
    String withPrefix("prefix-content");
    bool prefixRemoved = withPrefix.removePrefix(String("prefix-"));
    printResult("After removePrefix", withPrefix);
    printBool("Prefix removed", prefixRemoved);

    String withSuffix("content-suffix");
    bool suffixRemoved = withSuffix.removeSuffix(String("-suffix"));
    printResult("After removeSuffix", withSuffix);
    printBool("Suffix removed", suffixRemoved);

    // Compress spaces
    String withExtraSpaces(
        "This    has     multiple    spaces   between    words");
    withExtraSpaces.compressSpaces();
    printResult("After compressSpaces", withExtraSpaces);

    // Replace with regex
    String forRegex("Replace digits 123 and 456 with X");
    String afterRegex = forRegex.replaceRegex("\\d+", "X");
    printResult("Original", forRegex);
    printResult("After replaceRegex", afterRegex);
}

// Example 5: String Splitting and Joining
void splitAndJoinExample() {
    printSection("String Splitting and Joining");

    // Split by delimiter
    String csv("apple,banana,cherry,date,elderberry");
    std::vector<String> fruits = csv.split(String(","));

    std::cout << "Split by comma:" << std::endl;
    for (size_t i = 0; i < fruits.size(); ++i) {
        std::cout << "  " << i + 1 << ": " << fruits[i] << std::endl;
    }

    // Split with empty delimiter
    String unsplittable("Can't split this");
    std::vector<String> result = unsplittable.split(String(""));
    std::cout << "\nSplit with empty delimiter:" << std::endl;
    for (size_t i = 0; i < result.size(); ++i) {
        std::cout << "  " << i + 1 << ": " << result[i] << std::endl;
    }

    // Split with multi-character delimiter
    String text("part1::part2::part3::part4");
    std::vector<String> parts = text.split(String("::"));

    std::cout << "\nSplit by '::':" << std::endl;
    for (size_t i = 0; i < parts.size(); ++i) {
        std::cout << "  " << i + 1 << ": " << parts[i] << std::endl;
    }

    // Join with separator
    std::vector<String> words = {String("The"), String("quick"),
                                 String("brown"), String("fox")};
    String joined = String::join(words, String(" "));
    printResult("Join with space", joined);

    String joinedComma = String::join(words, String(", "));
    printResult("Join with comma and space", joinedComma);

    // Join empty vector
    std::vector<String> empty;
    String joinedEmpty = String::join(empty, String(","));
    printResult("Join empty vector", joinedEmpty);
}

// Example 6: String Comparison and Hashing
void comparisonAndHashingExample() {
    printSection("String Comparison and Hashing");

    String s1("Hello");
    String s2("hello");
    String s3("Hello");

    // Case sensitive comparison
    printBool("s1 == s3 (case sensitive)", s1 == s3);
    printBool("s1 == s2 (case sensitive)", s1 == s2);

    // Case insensitive comparison
    printBool("s1 equalsIgnoreCase s2", s1.equalsIgnoreCase(s2));
    printBool("s1 equalsIgnoreCase s3", s1.equalsIgnoreCase(s3));

    // Ordering
    printBool("s1 < s2", s1 < s2);  // 'H' comes before 'h' in ASCII
    printBool("s2 > s1", s2 > s1);

    // Hashing
    String hashMe1("Hash this string");
    String hashMe2("Hash this string");
    String hashMe3("Different string");

    size_t hash1 = hashMe1.hash();
    size_t hash2 = hashMe2.hash();
    size_t hash3 = hashMe3.hash();

    printValue("Hash of string 1", hash1);
    printValue("Hash of identical string 2", hash2);
    printValue("Hash of different string 3", hash3);
    printBool("hash1 == hash2", hash1 == hash2);
    printBool("hash1 == hash3", hash1 == hash3);

    // std::hash compatibility
    std::hash<String> hasher;
    printValue("std::hash of string 1", hasher(hashMe1));
    printBool("std::hash matches .hash()", hasher(hashMe1) == hash1);

    // Swap
    String a("String A");
    String b("String B");
    printResult("a before swap", a);
    printResult("b before swap", b);

    a.swap(b);
    printResult("a after swap", a);
    printResult("b after swap", b);

    // Global swap
    swap(a, b);
    printResult("a after global swap", a);
    printResult("b after global swap", b);
}

// Example 7: String Formatting
void formattingExample() {
    printSection("String Formatting");

    // Basic formatting
    String formatted = String::format("Hello, {}!", "world");
    printResult("Basic formatting", formatted);

    // Multiple arguments
    String multiFormat =
        String::format("Name: {}, Age: {}, Height: {:.2f}m", "John", 30, 1.85);
    printResult("Multiple arguments", multiFormat);

    // Numeric formatting
    String numFormat =
        String::format("Integer: {:d}, Float: {:.3f}, Scientific: {:e}", 42,
                       3.14159, 0.0000123);
    printResult("Numeric formatting", numFormat);

    // Width and alignment
    String alignFormat =
        String::format("|{:<10}|{:^10}|{:>10}|", "left", "center", "right");
    printResult("Width and alignment", alignFormat);

    // Error handling
    try {
        String badFormat = String::format(
            "This will {} fail because {} too many placeholders", "definitely");
        // This should not print
        printResult("Bad format", badFormat);
    } catch (const StringException& e) {
        std::cout << "Expected exception: " << e.what() << std::endl;
    }

    // Safe formatting
    if (auto result = String::formatSafe(
            "This will {} because {} too many placeholders", "fail")) {
        // This should not print
        printResult("This should not print", *result);
    } else {
        std::cout << "formatSafe correctly returned nullopt for invalid format"
                  << std::endl;
    }

    if (auto result = String::formatSafe("This will {} correctly", "format")) {
        printResult("Safe format success", *result);
    } else {
        std::cout << "This should not print - format was valid" << std::endl;
    }
}

// Example 8: Stream Operations
void streamOperationsExample() {
    printSection("Stream Operations");

    // Output stream
    String outStr("String for output stream demonstration");
    std::cout << "Direct stream output: " << outStr << std::endl;

    // Format with streams
    std::ostringstream oss;
    oss << "Combined stream: " << outStr << " (length: " << outStr.length()
        << ")";
    std::cout << oss.str() << std::endl;

    // Input stream
    String inStr;
    std::cout << "\nPlease type a string for input demonstration: ";
    std::cin >> inStr;
    printResult("String from input", inStr);

    // Input with format validation
    std::istringstream iss("InputFromStringStream");
    String streamStr;
    iss >> streamStr;
    printResult("String from input stream", streamStr);
}

// Example 9: Error Handling
void errorHandlingExample() {
    printSection("Error Handling");

    // Constructor error handling
    try {
        // This should succeed
        String validString("Valid string");
        printResult("Valid constructor call", validString);

        // This should also be handled gracefully
        String nullString(nullptr);
        printResult("Constructor with nullptr", nullString);

        // Let's try some potentially problematic operations
        String veryLong(
            std::string(10000000, 'x'));  // Try to allocate a very large string
        std::cout << "Successfully created very long string of length "
                  << veryLong.length() << std::endl;
    } catch (const StringException& e) {
        std::cout << "StringException caught: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "std::exception caught: " << e.what() << std::endl;
    }

    // Out of bounds access
    try {
        String s("Short");
        char c = s.at(10);  // This should throw
        std::cout << "This should not print: " << c << std::endl;
    } catch (const StringException& e) {
        std::cout << "Expected exception: " << e.what() << std::endl;
    }

    // Invalid operations
    try {
        String s("");
        s.replaceAll(String(""), String("replacement"));
        std::cout << "This should not print" << std::endl;
    } catch (const StringException& e) {
        std::cout << "Expected exception: " << e.what() << std::endl;
    }

    try {
        String s("Test string");
        s.insert(100, 'x');  // Out of bounds
        std::cout << "This should not print" << std::endl;
    } catch (const StringException& e) {
        std::cout << "Expected exception: " << e.what() << std::endl;
    }

    // Regex errors
    try {
        String s("Test string");
        s.replaceRegex("[", "replacement");  // Invalid regex
        std::cout << "This should not print" << std::endl;
    } catch (const StringException& e) {
        std::cout << "Expected exception: " << e.what() << std::endl;
    }
}

// Example 10: Performance Comparison
void performanceExample() {
    printSection("Performance Comparison");

    constexpr int iterations = 10000;
    constexpr int stringSize = 1000;

    // Create test data
    std::string stdString(stringSize, 'a');
    String atomString(stdString);

    // Concatenation benchmark
    auto start = std::chrono::high_resolution_clock::now();
    std::string stdResult;
    for (int i = 0; i < iterations; ++i) {
        stdResult += std::to_string(i);
    }
    auto stdDuration = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::high_resolution_clock::now() - start)
                           .count();

    start = std::chrono::high_resolution_clock::now();
    String atomResult;
    for (int i = 0; i < iterations; ++i) {
        atomResult += String(std::to_string(i));
    }
    auto atomDuration = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::high_resolution_clock::now() - start)
                            .count();

    printValue("std::string concatenation (μs)", stdDuration);
    printValue("String concatenation (μs)", atomDuration);
    printValue("Size ratio", static_cast<double>(atomDuration) / stdDuration);

    // Replace benchmark
    std::string stdReplaceStr(stringSize, 'a');
    for (int i = 0; i < stringSize / 10; ++i) {
        stdReplaceStr[i * 10] = 'x';
    }
    String atomReplaceStr(stdReplaceStr);

    start = std::chrono::high_resolution_clock::now();
    int stdReplaceCount = 0;
    size_t pos = 0;
    while ((pos = stdReplaceStr.find("x", pos)) != std::string::npos) {
        stdReplaceStr.replace(pos, 1, "y");
        pos += 1;
        stdReplaceCount++;
    }
    stdDuration = std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::high_resolution_clock::now() - start)
                      .count();

    start = std::chrono::high_resolution_clock::now();
    size_t atomReplaceCount =
        atomReplaceStr.replaceAll(String("x"), String("y"));
    atomDuration = std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::high_resolution_clock::now() - start)
                       .count();

    printValue("std::string replace count", stdReplaceCount);
    printValue("String replace count", atomReplaceCount);
    printValue("std::string replace time (μs)", stdDuration);
    printValue("String replace time (μs)", atomDuration);
    printValue("Time ratio", static_cast<double>(atomDuration) / stdDuration);

    // Split and join benchmark
    std::string stdSplitStr;
    for (int i = 0; i < iterations; ++i) {
        stdSplitStr += std::to_string(i) + ",";
    }
    String atomSplitStr(stdSplitStr);

    start = std::chrono::high_resolution_clock::now();
    std::vector<std::string> stdTokens;
    size_t startPos = 0;
    while ((pos = stdSplitStr.find(",", startPos)) != std::string::npos) {
        stdTokens.push_back(stdSplitStr.substr(startPos, pos - startPos));
        startPos = pos + 1;
    }
    std::string stdJoined;
    for (size_t i = 0; i < stdTokens.size(); ++i) {
        if (i > 0)
            stdJoined += ";";
        stdJoined += stdTokens[i];
    }
    stdDuration = std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::high_resolution_clock::now() - start)
                      .count();

    start = std::chrono::high_resolution_clock::now();
    std::vector<String> atomTokens = atomSplitStr.split(String(","));
    String atomJoined = String::join(atomTokens, String(";"));
    atomDuration = std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::high_resolution_clock::now() - start)
                       .count();

    printValue("std::string split/join time (μs)", stdDuration);
    printValue("String split/join time (μs)", atomDuration);
    printValue("Time ratio", static_cast<double>(atomDuration) / stdDuration);

    // Parallel performance for large strings
    std::string veryLargeString(1000000, 'a');
    for (int i = 0; i < 10000; ++i) {
        veryLargeString[i * 100] = 'x';
    }
    String atomLargeString(veryLargeString);

    start = std::chrono::high_resolution_clock::now();
    size_t normalReplaceCount =
        atomLargeString.replaceAll(String("x"), String("y"));
    auto normalDuration = std::chrono::duration_cast<std::chrono::microseconds>(
                              std::chrono::high_resolution_clock::now() - start)
                              .count();

    // Reset the string
    atomLargeString = String(veryLargeString);

    start = std::chrono::high_resolution_clock::now();
    size_t parallelReplaceCount =
        atomLargeString.replaceAllParallel(String("x"), String("y"));
    auto parallelDuration =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start)
            .count();

    printValue("Normal replace count", normalReplaceCount);
    printValue("Parallel replace count", parallelReplaceCount);
    printValue("Normal replace time (μs)", normalDuration);
    printValue("Parallel replace time (μs)", parallelDuration);
    printValue("Speedup",
               static_cast<double>(normalDuration) / parallelDuration);
}

int main() {
    std::cout << "===== String Class Comprehensive Examples =====\n";

    try {
        basicConstructionExample();
        basicStringMethodsExample();
        searchAndManipulationExample();
        transformationExample();
        splitAndJoinExample();
        comparisonAndHashingExample();
        formattingExample();
        streamOperationsExample();
        errorHandlingExample();
        performanceExample();

        std::cout << "\nAll examples completed successfully!\n";
    } catch (const StringException& e) {
        std::cerr << "\nUnexpected StringException: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "\nUnexpected standard exception: " << e.what()
                  << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\nUnknown exception occurred" << std::endl;
        return 1;
    }

    return 0;
}