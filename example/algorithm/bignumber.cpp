#include "atom/algorithm/bignumber.hpp"

#include <chrono>
#include <iostream>

using namespace atom::algorithm;

int main() {
    std::cout << "=== BigNumber Constructors ===" << std::endl;
    // Example usage of BigNumber constructors
    {
        BigNumber num1("12345678901234567890");
        BigNumber num2(9876543210LL);

        std::cout << "BigNumber from string: " << num1 << std::endl;
        std::cout << "BigNumber from long long: " << num2 << std::endl;
    }

    std::cout << "\n=== Basic Arithmetic Operations ===" << std::endl;
    // Example usage of addition, subtraction, multiplication, and division
    {
        BigNumber num1("12345678901234567890");
        BigNumber num2("9876543210987654321");

        BigNumber sum = num1 + num2;
        BigNumber difference = num1 - num2;
        BigNumber product = num1 * num2;
        BigNumber quotient = num1 / num2;

        std::cout << "Sum: " << sum << std::endl;
        std::cout << "Difference: " << difference << std::endl;
        std::cout << "Product: " << product << std::endl;
        std::cout << "Quotient: " << quotient << std::endl;
    }

    std::cout << "\n=== Compound Assignments ===" << std::endl;
    // Example usage of compound assignments
    {
        BigNumber num("12345678901234567890");
        std::cout << "Original number: " << num << std::endl;

        num += BigNumber("111111111111111111");
        std::cout << "After +=: " << num << std::endl;

        num -= BigNumber("222222222222222222");
        std::cout << "After -=: " << num << std::endl;

        num *= BigNumber("2");
        std::cout << "After *=: " << num << std::endl;

        num /= BigNumber("5");
        std::cout << "After /=: " << num << std::endl;
    }

    std::cout << "\n=== Exponentiation ===" << std::endl;
    // Example usage of exponentiation
    {
        BigNumber base("2");
        int exponent = 100;  // Calculating 2^100
        BigNumber result = base ^ exponent;

        std::cout << "2^100: " << result << std::endl;
    }

    std::cout << "\n=== Comparison Operators ===" << std::endl;
    // Example usage of comparison operators
    {
        BigNumber num1("12345678901234567890");
        BigNumber num2("9876543210987654321");

        std::cout << "num1 == num2: " << std::boolalpha << (num1 == num2)
                  << std::endl;
        std::cout << "num1 > num2: " << (num1 > num2) << std::endl;
        std::cout << "num1 < num2: " << (num1 < num2) << std::endl;
        std::cout << "num1 >= num2: " << (num1 >= num2) << std::endl;
        std::cout << "num1 <= num2: " << (num1 <= num2) << std::endl;
    }

    std::cout << "\n=== Sign and Absolute Value ===" << std::endl;
    // Example usage of negation and absolute value
    {
        BigNumber num("-12345678901234567890");
        BigNumber negated = num.negate();
        BigNumber absolute = num.abs();

        std::cout << "Original: " << num << std::endl;
        std::cout << "Negated: " << negated << std::endl;
        std::cout << "Absolute: " << absolute << std::endl;
    }

    std::cout << "\n=== Increment and Decrement ===" << std::endl;
    // Example usage of increment and decrement operators
    {
        BigNumber num("12345678901234567890");
        std::cout << "Original: " << num << std::endl;

        ++num;
        std::cout << "After prefix increment: " << num << std::endl;

        num++;
        std::cout << "After postfix increment: " << num << std::endl;

        --num;
        std::cout << "After prefix decrement: " << num << std::endl;

        num--;
        std::cout << "After postfix decrement: " << num << std::endl;
    }

    std::cout << "\n=== Digit Access ===" << std::endl;
    // Example usage of digit access
    {
        BigNumber num("12345678901234567890");

        std::cout << "Digits (from least significant): ";
        for (size_t i = 0; i < num.digits(); ++i) {
            std::cout << static_cast<int>(num[i]);
            if (i < num.digits() - 1)
                std::cout << ", ";
        }
        std::cout << std::endl;

        // Using at() with error handling
        try {
            std::cout << "Digit at position 5: " << static_cast<int>(num.at(5))
                      << std::endl;
            std::cout << "Digit at position 100: "
                      << static_cast<int>(num.at(100)) << std::endl;
        } catch (const std::out_of_range& e) {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }

    std::cout << "\n=== Utility Functions ===" << std::endl;
    // Example usage of utility functions
    {
        BigNumber num("12345678901234567890");

        std::cout << "Number: " << num << std::endl;
        std::cout << "Number of digits: " << num.digits() << std::endl;
        std::cout << "Is negative: " << std::boolalpha << num.isNegative()
                  << std::endl;
        std::cout << "Is positive: " << num.isPositive() << std::endl;
        std::cout << "Is even: " << num.isEven() << std::endl;
        std::cout << "Is odd: " << num.isOdd() << std::endl;
    }

    std::cout << "\n=== String Conversion ===" << std::endl;
    // Example of string conversion
    {
        BigNumber num("9999999999999999999");
        std::string str = num.toString();
        std::cout << "String representation: " << str << std::endl;

        num.setString("1234567890987654321");
        std::cout << "After setString: " << num << std::endl;
    }

    std::cout << "\n=== Performance Comparison ===" << std::endl;
    // Compare performance of different multiplication algorithms
    {
        BigNumber a(
            "3141592653589793238462643383279502884197169399375105820974944592");
        BigNumber b(
            "2718281828459045235360287471352662497757247093699959574966967627");

        auto start = std::chrono::high_resolution_clock::now();
        BigNumber result1 = a.multiply(b);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> std_time = end - start;

        start = std::chrono::high_resolution_clock::now();
        BigNumber result2 = a.multiplyKaratsuba(b);
        end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> karatsuba_time = end - start;

        start = std::chrono::high_resolution_clock::now();
        BigNumber result3 = a.parallelMultiply(b);
        end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> parallel_time = end - start;

        std::cout << "Standard multiplication: " << std_time.count() << " ms"
                  << std::endl;
        std::cout << "Karatsuba multiplication: " << karatsuba_time.count()
                  << " ms" << std::endl;
        std::cout << "Parallel multiplication: " << parallel_time.count()
                  << " ms" << std::endl;

        // Verify all methods produce the same result
        std::cout << "All results match: " << std::boolalpha
                  << (result1 == result2 && result2 == result3) << std::endl;
    }

    std::cout << "\n=== Trimming Leading Zeros ===" << std::endl;
    // Example of trimming leading zeros
    {
        // Creating a number with leading zeros
        BigNumber num("00012345");
        std::cout << "Original: " << num << std::endl;

        BigNumber trimmed = num.trimLeadingZeros();
        std::cout << "Trimmed: " << trimmed << std::endl;
    }

    return 0;
}
