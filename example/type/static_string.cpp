/**
 * @file static_string_examples.cpp
 * @brief Comprehensive examples demonstrating the StaticString class
 *
 * This file showcases all features of the StaticString template class including
 * constructors, element access, modifications, searching, and more.
 */

#include "atom/type/static_string.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==========================================" << std::endl;
}

// Helper function to print subsection headers
void printSubsection(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

// Helper function to display StaticString details
template <std::size_t N>
void printString(const StaticString<N>& str, const std::string& name) {
    std::cout << name << " (size=" << str.size()
              << ", capacity=" << str.capacity() << "): \"" << str << "\""
              << std::endl;
}

// Helper function to test and time string operations
template <typename Func>
void timeOperation(const std::string& operation, Func&& func) {
    std::cout << "Executing: " << operation << "... ";

    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::micro> elapsed = end - start;
    std::cout << "Done in " << elapsed.count() << " µs" << std::endl;
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "  StaticString Class Demonstration" << std::endl;
    std::cout << "==========================================" << std::endl;

    try {
        // Example 1: Constructors
        printSection("1. Constructors");

        // Default constructor
        StaticString<20> empty_str;
        printString(empty_str, "Default constructor");

        // C-string constructor
        StaticString<20> cstr_str("Hello, World!");
        printString(cstr_str, "C-string constructor");

        // String literal constructor
        StaticString<20> lit_str = "C++ StaticString";
        printString(lit_str, "String literal constructor");

        // String view constructor
        std::string_view sv = "String view example";
        StaticString<30> sv_str(sv);
        printString(sv_str, "String view constructor");

        // Array constructor
        std::array<char, 21> char_array{};
        std::copy_n("Array initialization", 19, char_array.begin());
        StaticString<20> array_str(char_array);
        printString(array_str, "Array constructor");

        // Copy constructor
        StaticString<20> copy_str(cstr_str);
        printString(copy_str, "Copy constructor");

        // Move constructor
        StaticString<20> move_src = "Move source";
        StaticString<20> move_str(std::move(move_src));
        printString(move_str, "Move constructor");
        printString(move_src, "Source after move");  // Should be empty

        // Constructor with size check
        try {
            StaticString<10> overflow_str(
                "This string is too long for capacity 10");
            std::cout << "Construction should have thrown an exception!"
                      << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // Deduction guide
        StaticString auto_sized = "Auto-sized string";
        printString(auto_sized, "Using deduction guide");
        std::cout << "Deduced capacity: " << auto_sized.capacity() << std::endl;

        // Example 2: Assignment Operators
        printSection("2. Assignment Operators");

        // Copy assignment
        StaticString<20> assign_target;
        assign_target = copy_str;
        printString(assign_target, "After copy assignment");

        // Move assignment
        StaticString<30> move_target;
        StaticString<30> move_source = "Move assignment source";
        move_target = std::move(move_source);
        printString(move_target, "After move assignment");
        printString(move_source,
                    "Source after move assignment");  // Should be empty

        // Example 3: Size and Capacity
        printSection("3. Size and Capacity");

        // size(), empty(), capacity()
        StaticString<50> size_test = "Testing size and capacity";
        std::cout << "size(): " << size_test.size() << std::endl;
        std::cout << "capacity(): " << size_test.capacity() << std::endl;
        std::cout << "empty(): " << (size_test.empty() ? "true" : "false")
                  << std::endl;

        size_test.clear();
        std::cout << "After clear():" << std::endl;
        std::cout << "size(): " << size_test.size() << std::endl;
        std::cout << "empty(): " << (size_test.empty() ? "true" : "false")
                  << std::endl;

        // Example 4: Element Access
        printSection("4. Element Access");

        StaticString<20> access_str = "Element access";

        // c_str() and data()
        printSubsection("c_str() and data()");
        std::cout << "c_str(): " << access_str.c_str() << std::endl;

        const char* data_ptr = access_str.data();
        std::cout << "First 5 chars via data(): ";
        for (int i = 0; i < 5; ++i) {
            std::cout << data_ptr[i];
        }
        std::cout << std::endl;

        // Operator [] and at()
        printSubsection("Operator [] and at()");
        std::cout << "access_str[0]: " << access_str[0] << std::endl;
        std::cout << "access_str[7]: " << access_str[7] << std::endl;

        std::cout << "access_str.at(0): " << access_str.at(0) << std::endl;
        std::cout << "access_str.at(7): " << access_str.at(7) << std::endl;

        // Bounds checking with at()
        try {
            char c = access_str.at(20);  // Out of bounds
            std::cout << "This should not be printed: " << c << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // front() and back()
        printSubsection("front() and back()");
        std::cout << "front(): " << access_str.front() << std::endl;
        std::cout << "back(): " << access_str.back() << std::endl;

        // front() and back() with empty string
        StaticString<10> empty_access;
        try {
            char c = empty_access.front();
            std::cout << "This should not be printed: " << c << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected exception (front on empty): " << e.what()
                      << std::endl;
        }

        try {
            char c = empty_access.back();
            std::cout << "This should not be printed: " << c << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected exception (back on empty): " << e.what()
                      << std::endl;
        }

        // Example 5: Iterators
        printSection("5. Iterators");

        StaticString<20> iter_str = "Iterator test";

        // Standard iteration
        printSubsection("Standard Iteration");
        std::cout << "Characters: ";
        for (auto it = iter_str.begin(); it != iter_str.end(); ++it) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;

        // Range-based for loop
        printSubsection("Range-based for loop");
        std::cout << "Characters: ";
        for (char c : iter_str) {
            std::cout << c << " ";
        }
        std::cout << std::endl;

        // Const iterators
        printSubsection("Const Iterators");
        const StaticString<20> const_iter_str = "Const test";
        std::cout << "Characters via cbegin/cend: ";
        for (auto it = const_iter_str.cbegin(); it != const_iter_str.cend();
             ++it) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;

        // Example 6: Modifiers
        printSection("6. Modifiers");

        // push_back()
        printSubsection("push_back()");
        StaticString<10> push_str = "ABCD";
        push_str.push_back('E');
        push_str.push_back('F');
        printString(push_str, "After push_back");

        // push_back() with overflow
        try {
            // Fill to capacity
            while (push_str.size() < push_str.capacity()) {
                push_str.push_back('X');
            }
            printString(push_str, "Filled to capacity");

            // Now try to overflow
            push_str.push_back('!');
            std::cout << "This should not be printed!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // pop_back()
        printSubsection("pop_back()");
        StaticString<20> pop_str = "Hello there";
        std::cout << "Before pop_back(): " << pop_str << std::endl;
        pop_str.pop_back();
        pop_str.pop_back();
        std::cout << "After two pop_back(): " << pop_str << std::endl;

        // pop_back() on empty string
        StaticString<10> empty_pop;
        try {
            empty_pop.pop_back();
            std::cout << "This should not be printed!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // append()
        printSubsection("append()");
        StaticString<50> append_str = "Hello";
        append_str.append(", ");
        append_str.append("world!");
        printString(append_str, "After append");

        // append() with different StaticString
        StaticString<20> append_source = " (Appended)";
        append_str.append(append_source);
        printString(append_str, "After appending StaticString");

        // append() with overflow check
        StaticString<15> small_str = "Small";
        try {
            small_str.append(" string that will overflow");
            std::cout << "This should not be printed!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
            printString(small_str, "After failed append");
        }

        // resize()
        printSubsection("resize()");
        StaticString<20> resize_str = "Resize test";
        std::cout << "Before resize: " << resize_str << std::endl;

        // Resize smaller
        resize_str.resize(7);
        std::cout << "After resize(7): " << resize_str << std::endl;

        // Resize larger with fill character
        resize_str.resize(12, '+');
        std::cout << "After resize(12, '+'): " << resize_str << std::endl;

        // Resize beyond capacity
        try {
            resize_str.resize(30);
            std::cout << "This should not be printed!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // clear()
        printSubsection("clear()");
        StaticString<20> clear_str = "Will be cleared";
        std::cout << "Before clear: " << clear_str << std::endl;
        clear_str.clear();
        std::cout << "After clear: \"" << clear_str << "\"" << std::endl;
        std::cout << "Size after clear: " << clear_str.size() << std::endl;

        // Example 7: String Operations
        printSection("7. String Operations");

        // substr()
        printSubsection("substr()");
        StaticString<50> source_str =
            "The quick brown fox jumps over the lazy dog";

        auto substr1 = source_str.substr(4, 5);
        printString(substr1, "substr(4, 5)");

        auto substr2 = source_str.substr(10);
        printString(substr2, "substr(10)");

        auto substr3 = source_str.substr(0, 3);
        printString(substr3, "substr(0, 3)");

        // substr() with out of bounds
        try {
            auto invalid_substr = source_str.substr(50);
            std::cout << "This should not be printed!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // find()
        printSubsection("find()");
        StaticString<50> find_str = "Finding a needle in a haystack";

        // Find character
        std::cout << "find('n'): " << find_str.find('n') << std::endl;
        std::cout << "find('n', 5): " << find_str.find('n', 5) << std::endl;
        std::cout << "find('z'): "
                  << (find_str.find('z') == StaticString<50>::npos
                          ? "npos"
                          : std::to_string(find_str.find('z')))
                  << std::endl;

        // Find substring
        std::cout << "find(\"needle\"): " << find_str.find("needle")
                  << std::endl;
        std::cout << "find(\"stack\"): " << find_str.find("stack") << std::endl;
        std::cout << "find(\"missing\"): "
                  << (find_str.find("missing") == StaticString<50>::npos
                          ? "npos"
                          : std::to_string(find_str.find("missing")))
                  << std::endl;

        // replace()
        printSubsection("replace()");
        StaticString<50> replace_str = "Replace part of this string";
        std::cout << "Before replace: " << replace_str << std::endl;

        replace_str.replace(8, 4, "section");
        std::cout << "After replace(8, 4, \"section\"): " << replace_str
                  << std::endl;

        try {
            replace_str.replace(100, 5, "invalid");
            std::cout << "This should not be printed!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        try {
            StaticString<20> small_replace = "Small buffer";
            small_replace.replace(
                0, 5, "This is a very long replacement that will overflow");
            std::cout << "This should not be printed!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // insert()
        printSubsection("insert()");
        StaticString<50> insert_str = "Insert into string";
        std::cout << "Before insert: " << insert_str << std::endl;

        insert_str.insert(7, "something ");
        std::cout << "After insert(7, \"something \"): " << insert_str
                  << std::endl;

        try {
            insert_str.insert(100, "invalid");
            std::cout << "This should not be printed!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // erase()
        printSubsection("erase()");
        StaticString<50> erase_str = "This contains unnecessary text to remove";
        std::cout << "Before erase: " << erase_str << std::endl;

        erase_str.erase(12, 13);
        std::cout << "After erase(12, 13): " << erase_str << std::endl;

        erase_str.erase(5);  // Erase from position 5 to the end
        std::cout << "After erase(5): " << erase_str << std::endl;

        try {
            erase_str.erase(100);
            std::cout << "This should not be printed!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // Example 8: Operators
        printSection("8. Operators");

        // operator== and operator!=
        printSubsection("Comparison operators");
        StaticString<20> str1 = "Compare me";
        StaticString<20> str2 = "Compare me";
        StaticString<20> str3 = "Different";

        std::cout << "str1 == str2: " << (str1 == str2 ? "true" : "false")
                  << std::endl;
        std::cout << "str1 != str3: " << (str1 != str3 ? "true" : "false")
                  << std::endl;

        // Comparison with string_view
        std::string_view sv_compare = "Compare me";
        std::cout << "str1 == sv_compare: "
                  << (str1 == sv_compare ? "true" : "false") << std::endl;

        // operator+=
        printSubsection("Append operators");
        StaticString<50> append_op_str = "Start";
        std::cout << "Initial: " << append_op_str << std::endl;

        // Append character
        append_op_str += '!';
        std::cout << "After += '!': " << append_op_str << std::endl;

        // Append string view
        append_op_str += std::string_view(" Adding more");
        std::cout << "After += string_view: " << append_op_str << std::endl;

        // Append StaticString
        StaticString<20> append_suffix = " (suffix)";
        append_op_str += append_suffix;
        std::cout << "After += StaticString: " << append_op_str << std::endl;

        // operator+
        printSubsection("Concatenation operator");
        StaticString<10> lhs = "Left";
        StaticString<15> rhs = "-Right";
        auto concat_result = lhs + rhs;
        printString(concat_result, "lhs + rhs");
        std::cout << "Result capacity: " << concat_result.capacity()
                  << std::endl;

        try {
            StaticString<5> small_lhs = "ABCDE";
            StaticString<10> large_rhs = "0123456789";
            auto overflow_result = small_lhs + large_rhs;
            std::cout << "This should not be printed!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
        }

        // string_view conversion
        printSubsection("string_view conversion");
        StaticString<20> conv_str = "Convert me";
        std::string_view converted = conv_str;
        std::cout << "Converted to string_view: " << converted << std::endl;

        // Example 9: Safe Creation
        printSection("9. Safe Creation");

        // make_safe with valid input
        std::string_view safe_input = "Safe input";
        auto safe_result = StaticString<20>::make_safe(safe_input);
        if (safe_result) {
            printString(*safe_result, "Successfully created safe string");
        } else {
            std::cout << "Failed to create safe string (should not happen)"
                      << std::endl;
        }

        // make_safe with too large input
        std::string_view unsafe_input =
            "This string is too long to fit in a StaticString<10>";
        auto unsafe_result = StaticString<10>::make_safe(unsafe_input);
        if (unsafe_result) {
            std::cout << "This should not be printed!" << std::endl;
        } else {
            std::cout << "Correctly returned nullopt for oversized input"
                      << std::endl;
        }

        // Example 10: Stream Operations
        printSection("10. Stream Operations");

        StaticString<50> stream_str = "This string will be streamed";

        // Output to stream
        std::cout << "Streaming to cout: " << stream_str << std::endl;

        // Output to stringstream
        std::stringstream ss;
        ss << "Prefix: " << stream_str << " :Suffix";
        std::cout << "Stringstream result: " << ss.str() << std::endl;

        // Example 11: Performance Comparison
        printSection("11. Performance Comparison");

        // Create strings for performance testing
        constexpr size_t test_size = 1000;
        StaticString<test_size> static_test_str;
        std::string std_test_str;

        // Fill strings with same content
        for (int i = 0; i < 900; ++i) {
            static_test_str.push_back(static_cast<char>('a' + (i % 26)));
            std_test_str.push_back(static_cast<char>('a' + (i % 26)));
        }

        // Time find operations
        std::cout << "Finding character 'z' near the end:" << std::endl;

        timeOperation("StaticString::find", [&]() {
            auto pos = static_test_str.find('z', 800);
            // Use the result to prevent optimization
            if (pos == StaticString<test_size>::npos) {
                // This is just to use the result
            }
        });

        timeOperation("std::string::find", [&]() {
            auto pos = std_test_str.find('z', 800);
            // Use the result to prevent optimization
            if (pos == std::string::npos) {
                // This is just to use the result
            }
        });

        // Time substring operations
        std::cout << "\nPerforming substring operations:" << std::endl;

        timeOperation("StaticString::substr", [&]() {
            auto substr = static_test_str.substr(100, 100);
            // Use the result to prevent optimization
            if (substr.size() > 0) {
                // This is just to use the result
            }
        });

        timeOperation("std::string::substr", [&]() {
            auto substr = std_test_str.substr(100, 100);
            // Use the result to prevent optimization
            if (substr.size() > 0) {
                // This is just to use the result
            }
        });

        // Example 12: SIMD Optimizations
        printSection("12. SIMD Optimizations");

        // Create a large string with repeated patterns to test SIMD operations
        StaticString<2000> simd_test_str;
        for (int i = 0; i < 1900; ++i) {
            simd_test_str.push_back(static_cast<char>('a' + (i % 26)));
        }

        // Time character find with SIMD
        std::cout << "Finding all occurrences of 'x':" << std::endl;

        timeOperation("StaticString::find with potential SIMD", [&]() {
            std::vector<size_t> positions;
            size_t pos = 0;
            while ((pos = simd_test_str.find('x', pos)) !=
                   StaticString<2000>::npos) {
                positions.push_back(pos);
                pos++;
            }
            // Use the result to prevent optimization
            if (!positions.empty()) {
                // This is just to use the result
            }
        });

        // Time string equality with SIMD
        std::cout << "\nComparing two large identical strings:" << std::endl;

        StaticString<2000> simd_test_str2;
        for (int i = 0; i < 1900; ++i) {
            simd_test_str2.push_back(static_cast<char>('a' + (i % 26)));
        }

        timeOperation("StaticString::operator== with potential SIMD", [&]() {
            bool equal = (simd_test_str == simd_test_str2);
            // Use the result to prevent optimization
            if (equal) {
                // This is just to use the result
            }
        });

        // Example 13: Edge Cases and Corner Conditions
        printSection("13. Edge Cases and Corner Conditions");

        // Empty string operations
        printSubsection("Empty string operations");
        StaticString<10> empty_tests;

        std::cout << "Empty string size: " << empty_tests.size() << std::endl;
        std::cout << "Empty string is empty: "
                  << (empty_tests.empty() ? "true" : "false") << std::endl;
        std::cout << "Empty string c_str: \"" << empty_tests.c_str() << "\""
                  << std::endl;

        // Handling special characters
        printSubsection("Special characters");
        StaticString<50> special_str;
        special_str.push_back('\0');  // Embedded null character
        special_str.push_back('\t');
        special_str.push_back('\n');
        special_str.push_back('\r');

        std::cout << "Size after adding special chars: " << special_str.size()
                  << std::endl;
        std::cout << "Raw bytes in special_str:";
        for (size_t i = 0; i < special_str.size(); ++i) {
            std::cout << " " << static_cast<int>(special_str[i]);
        }
        std::cout << std::endl;

        // Edge case: Zero-capacity string
        // This would be caught at compile time with static_assert
        /*
        try {
            StaticString<0> zero_str;
        } catch (const std::exception& e) {
            std::cout << "Exception caught: " << e.what() << std::endl;
        }
        */

        std::cout << "\nAll examples completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
