#include "atom/algorithm/hash.hpp"

#include <any>
#include <array>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

/**
 * @brief Demonstrates hash computation for basic data types.
 *
 * This function shows how to compute hash values for fundamental types
 * like integers, demonstrating the basic usage of the hash algorithm.
 */
void demonstrateBasicTypeHashing() {
    std::cout << "=== Basic Type Hashing Examples ===" << std::endl;

    int value = 42;
    std::size_t hashValue = atom::algorithm::computeHash(value);
    std::cout << "Hash of int value 42: " << hashValue << std::endl;

    std::cout << std::endl;
}

/**
 * @brief Demonstrates hash computation for container types.
 *
 * This function shows how to compute hash values for various container types
 * including vectors, arrays, and demonstrates the algorithm's ability to
 * handle collections of data.
 */
void demonstrateContainerHashing() {
    std::cout << "=== Container Type Hashing Examples ===" << std::endl;

    // Vector hashing
    std::vector<int> values = {1, 2, 3, 4, 5};
    std::size_t hashValue = atom::algorithm::computeHash(values);
    std::cout << "Hash of vector {1, 2, 3, 4, 5}: " << hashValue << std::endl;

    // Array hashing
    std::array<int, 5> array = {1, 2, 3, 4, 5};
    hashValue = atom::algorithm::computeHash(array);
    std::cout << "Hash of array {1, 2, 3, 4, 5}: " << hashValue << std::endl;

    std::cout << std::endl;
}

/**
 * @brief Demonstrates hash computation for composite types.
 *
 * This function shows how to compute hash values for composite types
 * like tuples and pairs, demonstrating the algorithm's ability to
 * handle complex data structures.
 */
void demonstrateCompositeTypeHashing() {
    std::cout << "=== Composite Type Hashing Examples ===" << std::endl;

    // Tuple hashing
    std::tuple<int, std::string, double> tuple = {42, "hello", 3.14};
    std::size_t hashValue = atom::algorithm::computeHash(tuple);
    std::cout << "Hash of tuple {42, \"hello\", 3.14}: " << hashValue << std::endl;

    // Pair hashing
    std::pair<int, std::string> pair = {42, "hello"};
    hashValue = atom::algorithm::computeHash(pair);
    std::cout << "Hash of pair {42, \"hello\"}: " << hashValue << std::endl;

    std::cout << std::endl;
}

/**
 * @brief Demonstrates hash computation for modern C++ wrapper types.
 *
 * This function shows how to compute hash values for modern C++ wrapper types
 * like std::optional, std::variant, and std::any, demonstrating the algorithm's
 * flexibility with different value semantics.
 */
void demonstrateWrapperTypeHashing() {
    std::cout << "=== Wrapper Type Hashing Examples ===" << std::endl;

    // Optional hashing
    std::optional<int> opt = 42;
    std::size_t hashValue = atom::algorithm::computeHash(opt);
    std::cout << "Hash of optional value 42: " << hashValue << std::endl;

    std::optional<int> emptyOpt;
    std::size_t emptyHashValue = atom::algorithm::computeHash(emptyOpt);
    std::cout << "Hash of empty optional: " << emptyHashValue << std::endl;

    // Variant hashing
    std::variant<int, std::string> var = "hello";
    hashValue = atom::algorithm::computeHash(var);
    std::cout << "Hash of variant \"hello\": " << hashValue << std::endl;

    var = 42;
    hashValue = atom::algorithm::computeHash(var);
    std::cout << "Hash of variant 42: " << hashValue << std::endl;

    // Any hashing
    std::any anyValue = 42;
    hashValue = atom::algorithm::computeHash(anyValue);
    std::cout << "Hash of any value 42: " << hashValue << std::endl;

    anyValue = std::string("hello");
    hashValue = atom::algorithm::computeHash(anyValue);
    std::cout << "Hash of any value \"hello\": " << hashValue << std::endl;

    std::cout << std::endl;
}

/**
 * @brief Demonstrates string hashing capabilities.
 *
 * This function shows different ways to compute hash values for strings,
 * including C-style strings and user-defined literals.
 */
void demonstrateStringHashing() {
    std::cout << "=== String Hashing Examples ===" << std::endl;

    // C-style string hashing
    const char* str = "example";
    std::size_t hashValue = atom::algorithm::hash(str);
    std::cout << "Hash of string \"example\": " << hashValue << std::endl;

    // User-defined literal hashing
    hashValue = "example"_hash;
    std::cout << "Hash of string literal \"example\": " << hashValue << std::endl;

    std::cout << std::endl;
}

/**
 * @brief Main function demonstrating various hash algorithm capabilities.
 *
 * This function orchestrates the demonstration of different hash computation
 * capabilities, showing the versatility and ease of use of the atom hash algorithm.
 *
 * @return int Exit status (0 for success)
 */
int main() {
    std::cout << "Atom Hash Algorithm Demonstration" << std::endl;
    std::cout << "=================================" << std::endl << std::endl;

    // Demonstrate different categories of hash computation
    demonstrateBasicTypeHashing();
    demonstrateContainerHashing();
    demonstrateCompositeTypeHashing();
    demonstrateWrapperTypeHashing();
    demonstrateStringHashing();

    std::cout << "Demonstration completed successfully!" << std::endl;
    return 0;
}