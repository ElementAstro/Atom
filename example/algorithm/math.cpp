#include "atom/algorithm/math.hpp"

#include <iostream>

int main() {
    // Example usage of mulDiv64
    {
        uint64_t operant = 10;
        uint64_t multiplier = 20;
        uint64_t divider = 5;
        uint64_t result =
            atom::algorithm::mulDiv64(operant, multiplier, divider);
        std::cout << "mulDiv64(10, 20, 5) = " << result << std::endl;
    }

    // Example usage of safeAdd
    {
        uint64_t a = 18446744073709551615ULL;  // Maximum value for uint64_t
        uint64_t b = 1;
        uint64_t result = atom::algorithm::safeAdd(a, b);
        std::cout << "safeAdd(18446744073709551615, 1) = " << result
                  << std::endl;
    }

    // Example usage of safeMul
    {
        uint64_t a = 18446744073709551615ULL;  // Maximum value for uint64_t
        uint64_t b = 2;
        uint64_t result = atom::algorithm::safeMul(a, b);
        std::cout << "safeMul(18446744073709551615, 2) = " << result
                  << std::endl;
    }

    // Example usage of rotl64
    {
        uint64_t n = 1;
        unsigned int c = 1;
        uint64_t result = atom::algorithm::rotl64(n, c);
        std::cout << "rotl64(1, 1) = " << result << std::endl;
    }

    // Example usage of rotr64
    {
        uint64_t n = 1;
        unsigned int c = 1;
        uint64_t result = atom::algorithm::rotr64(n, c);
        std::cout << "rotr64(1, 1) = " << result << std::endl;
    }

    // Example usage of clz64
    {
        uint64_t x = 1;
        int result = atom::algorithm::clz64(x);
        std::cout << "clz64(1) = " << result << std::endl;
    }

    // Example usage of normalize
    {
        uint64_t x = 8;
        uint64_t result = atom::algorithm::normalize(x);
        std::cout << "normalize(8) = " << result << std::endl;
    }

    // Example usage of safeSub
    {
        uint64_t a = 10;
        uint64_t b = 20;
        uint64_t result = atom::algorithm::safeSub(a, b);
        std::cout << "safeSub(10, 20) = " << result << std::endl;
    }

    // Example usage of safeDiv
    {
        uint64_t a = 10;
        uint64_t b = 0;
        uint64_t result = atom::algorithm::safeDiv(a, b);
        std::cout << "safeDiv(10, 0) = " << result << std::endl;
    }

    // Example usage of bitReverse64
    {
        uint64_t n = 1;
        uint64_t result = atom::algorithm::bitReverse64(n);
        std::cout << "bitReverse64(1) = " << result << std::endl;
    }

    // Example usage of approximateSqrt
    {
        uint64_t n = 16;
        uint64_t result = atom::algorithm::approximateSqrt(n);
        std::cout << "approximateSqrt(16) = " << result << std::endl;
    }

    // Example usage of gcd64
    {
        uint64_t a = 48;
        uint64_t b = 18;
        uint64_t result = atom::algorithm::gcd64(a, b);
        std::cout << "gcd64(48, 18) = " << result << std::endl;
    }

    // Example usage of lcm64
    {
        uint64_t a = 48;
        uint64_t b = 18;
        uint64_t result = atom::algorithm::lcm64(a, b);
        std::cout << "lcm64(48, 18) = " << result << std::endl;
    }

    // Example usage of isPowerOfTwo
    {
        uint64_t n = 16;
        bool result = atom::algorithm::isPowerOfTwo(n);
        std::cout << "isPowerOfTwo(16) = " << std::boolalpha << result
                  << std::endl;
    }

    // Example usage of nextPowerOfTwo
    {
        uint64_t n = 17;
        uint64_t result = atom::algorithm::nextPowerOfTwo(n);
        std::cout << "nextPowerOfTwo(17) = " << result << std::endl;
    }

    return 0;
}
