#include "atom/utils/bit.hpp"

#include <bitset>
#include <iostream>

using namespace atom::utils;

int main() {
    // Create a bitmask with the specified number of bits set to 1
    uint32_t mask = createMask<uint32_t>(5);
    std::cout << "Bitmask with 5 bits set: " << std::bitset<32>(mask)
              << std::endl;

    // Count the number of set bits (1s) in the given value
    uint32_t value = 0b10101010;
    uint32_t setBits = countBytes(value);
    std::cout << "Number of set bits in " << std::bitset<32>(value) << ": "
              << setBits << std::endl;

    // Reverse the bits in the given value
    uint32_t reversedValue = reverseBits(value);
    std::cout << "Reversed bits: " << std::bitset<32>(reversedValue)
              << std::endl;

    // Perform a left rotation on the bits of the given value
    uint32_t rotatedLeftValue = rotateLeft(value, 3);
    std::cout << "Left rotated bits: " << std::bitset<32>(rotatedLeftValue)
              << std::endl;

    // Perform a right rotation on the bits of the given value
    uint32_t rotatedRightValue = rotateRight(value, 3);
    std::cout << "Right rotated bits: " << std::bitset<32>(rotatedRightValue)
              << std::endl;

    // Merge two bitmasks into one
    uint32_t mask1 = 0b11110000;
    uint32_t mask2 = 0b00001111;
    uint32_t mergedMask = mergeMasks(mask1, mask2);
    std::cout << "Merged bitmask: " << std::bitset<32>(mergedMask) << std::endl;

    // Split a bitmask into two parts
    uint32_t splitMaskValue = 0b11111111;
    auto [lowerPart, upperPart] = splitMask(splitMaskValue, 4);
    std::cout << "Lower part of split bitmask: " << std::bitset<32>(lowerPart)
              << std::endl;
    std::cout << "Upper part of split bitmask: " << std::bitset<32>(upperPart)
              << std::endl;

    return 0;
}