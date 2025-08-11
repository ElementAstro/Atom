#include "atom/algorithm/md5.hpp"

#include <iostream>

int main() {
    // Example input string to be hashed
    std::string input = "Hello, World!";

    // Encrypt the input string using the MD5 algorithm
    std::string hash = atom::algorithm::MD5::encrypt(input);

    // Print the resulting MD5 hash
    std::cout << "Input: " << input << std::endl;
    std::cout << "MD5 Hash: " << hash << std::endl;

    return 0;
}
