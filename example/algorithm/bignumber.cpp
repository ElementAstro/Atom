#include "bignumber.hpp"

#include <iostream>

using namespace atom::algorithm;

int main() {
    // Example usage of BigNumber constructors
    {
        BigNumber num1("12345678901234567890");
        BigNumber num2(9876543210987654321LL);

        std::cout << "BigNumber from string: " << num1 << std::endl;
        std::cout << "BigNumber from long long: " << num2 << std::endl;
    }

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

    // Example usage of exponentiation
    {
        BigNumber base("2");
        int exponent = 10;
        BigNumber result = base ^ exponent;

        std::cout << "2^10: " << result << std::endl;
    }

    // Example usage of comparison operators
    {
        BigNumber num1("12345678901234567890");
        BigNumber num2("9876543210987654321");

        std::cout << "num1 == num2: " << (num1 == num2) << std::endl;
        std::cout << "num1 > num2: " << (num1 > num2) << std::endl;
        std::cout << "num1 < num2: " << (num1 < num2) << std::endl;
        std::cout << "num1 >= num2: " << (num1 >= num2) << std::endl;
        std::cout << "num1 <= num2: " << (num1 <= num2) << std::endl;
    }

    // Example usage of negation and absolute value
    {
        BigNumber num("-12345678901234567890");
        BigNumber negated = num.negate();
        BigNumber absolute = num.abs();

        std::cout << "Negated: " << negated << std::endl;
        std::cout << "Absolute: " << absolute << std::endl;
    }

    // Example usage of increment and decrement operators
    {
        BigNumber num("12345678901234567890");

        ++num;
        std::cout << "After prefix increment: " << num << std::endl;

        num++;
        std::cout << "After postfix increment: " << num << std::endl;

        --num;
        std::cout << "After prefix decrement: " << num << std::endl;

        num--;
        std::cout << "After postfix decrement: " << num << std::endl;
    }

    // Example usage of digit access
    {
        BigNumber num("12345678901234567890");

        std::cout << "Digit at index 0: " << num[0] << std::endl;
        std::cout << "Digit at index 5: " << num[5] << std::endl;
        std::cout << "Digit at index 10: " << num[10] << std::endl;
    }

    // Example usage of utility functions
    {
        BigNumber num("12345678901234567890");

        std::cout << "Number of digits: " << num.digits() << std::endl;
        std::cout << "Is negative: " << num.isNegative() << std::endl;
        std::cout << "Is positive: " << num.isPositive() << std::endl;
        std::cout << "Is even: " << num.isEven() << std::endl;
        std::cout << "Is odd: " << num.isOdd() << std::endl;
    }

    return 0;
}