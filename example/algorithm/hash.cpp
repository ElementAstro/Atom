#include <any>
#include <array>
#include <iostream>
#include <optional>
#include <tuple>
#include <variant>
#include <vector>

#include "atom/algorithm/hash.hpp"

using namespace atom::algorithm;

int main() {
    // Example 1: Hashing a single value
    int value = 42;
    std::size_t hashValue = computeHash(value);
    std::cout << "Hash of single value (42): " << hashValue << std::endl;

    // Example 2: Hashing a vector of values
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::size_t hashVec = computeHash(vec);
    std::cout << "Hash of vector {1, 2, 3, 4, 5}: " << hashVec << std::endl;

    // Example 3: Hashing a tuple of values
    std::tuple<int, std::string, double> tup = {42, "hello", 3.14};
    std::size_t hashTup = computeHash(tup);
    std::cout << "Hash of tuple (42, \"hello\", 3.14): " << hashTup
              << std::endl;

    // Example 4: Hashing an array of values
    std::array<int, 3> arr = {1, 2, 3};
    std::size_t hashArr = computeHash(arr);
    std::cout << "Hash of array {1, 2, 3}: " << hashArr << std::endl;

    // Example 5: Hashing a pair of values
    std::pair<int, std::string> pair = {42, "world"};
    std::size_t hashPair = computeHash(pair);
    std::cout << "Hash of pair (42, \"world\"): " << hashPair << std::endl;

    // Example 6: Hashing an optional value
    std::optional<int> opt = 42;
    std::size_t hashOpt = computeHash(opt);
    std::cout << "Hash of optional (42): " << hashOpt << std::endl;

    // Example 7: Hashing a variant of values
    std::variant<int, std::string> var = "variant";
    std::size_t hashVar = computeHash(var);
    std::cout << "Hash of variant (\"variant\"): " << hashVar << std::endl;

    // Example 8: Hashing an any value
    std::any anyValue = 42;
    std::size_t hashAny = computeHash(anyValue);
    std::cout << "Hash of any (42): " << hashAny << std::endl;

    // Example 9: Hashing a null-terminated string using FNV-1a algorithm
    const char* str = "example";
    std::size_t hashStr = hash(str);
    std::cout << "Hash of string (\"example\"): " << hashStr << std::endl;

    // Example 10: Using user-defined literal for computing hash values of
    // string literals
    std::size_t hashLiteral = "example"_hash;
    std::cout << "Hash of string literal (\"example\"): " << hashLiteral
              << std::endl;

    return 0;
}