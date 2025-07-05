#include "atom/algorithm/hash.hpp"

#include <any>
#include <array>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

int main() {
    // Example usage of computeHash for a single value
    {
        int value = 42;
        std::size_t hashValue = atom::algorithm::computeHash(value);
        std::cout << "Hash of int value 42: " << hashValue << std::endl;
    }

    // Example usage of computeHash for a vector of values
    {
        std::vector<int> values = {1, 2, 3, 4, 5};
        std::size_t hashValue = atom::algorithm::computeHash(values);
        std::cout << "Hash of vector {1, 2, 3, 4, 5}: " << hashValue
                  << std::endl;
    }

    // Example usage of computeHash for a tuple of values
    {
        std::tuple<int, std::string, double> tuple = {42, "hello", 3.14};
        std::size_t hashValue = atom::algorithm::computeHash(tuple);
        std::cout << "Hash of tuple {42, \"hello\", 3.14}: " << hashValue
                  << std::endl;
    }

    // Example usage of computeHash for an array of values
    {
        std::array<int, 5> array = {1, 2, 3, 4, 5};
        std::size_t hashValue = atom::algorithm::computeHash(array);
        std::cout << "Hash of array {1, 2, 3, 4, 5}: " << hashValue
                  << std::endl;
    }

    // Example usage of computeHash for a pair of values
    {
        std::pair<int, std::string> pair = {42, "hello"};
        std::size_t hashValue = atom::algorithm::computeHash(pair);
        std::cout << "Hash of pair {42, \"hello\"}: " << hashValue << std::endl;
    }

    // Example usage of computeHash for an optional value
    {
        std::optional<int> opt = 42;
        std::size_t hashValue = atom::algorithm::computeHash(opt);
        std::cout << "Hash of optional value 42: " << hashValue << std::endl;

        std::optional<int> emptyOpt;
        std::size_t emptyHashValue = atom::algorithm::computeHash(emptyOpt);
        std::cout << "Hash of empty optional: " << emptyHashValue << std::endl;
    }

    // Example usage of computeHash for a variant of values
    {
        std::variant<int, std::string> var = "hello";
        std::size_t hashValue = atom::algorithm::computeHash(var);
        std::cout << "Hash of variant \"hello\": " << hashValue << std::endl;

        var = 42;
        hashValue = atom::algorithm::computeHash(var);
        std::cout << "Hash of variant 42: " << hashValue << std::endl;
    }

    // Example usage of computeHash for an any value
    {
        std::any anyValue = 42;
        std::size_t hashValue = atom::algorithm::computeHash(anyValue);
        std::cout << "Hash of any value 42: " << hashValue << std::endl;

        anyValue = std::string("hello");
        hashValue = atom::algorithm::computeHash(anyValue);
        std::cout << "Hash of any value \"hello\": " << hashValue << std::endl;
    }

    // Example usage of hash function for a null-terminated string
    {
        const char* str = "example";
        std::size_t hashValue = atom::algorithm::hash(str);
        std::cout << "Hash of string \"example\": " << hashValue << std::endl;
    }

    // Example usage of user-defined literal for computing hash values
    {
        std::size_t hashValue = "example"_hash;
        std::cout << "Hash of string literal \"example\": " << hashValue
                  << std::endl;
    }

    return 0;
}
