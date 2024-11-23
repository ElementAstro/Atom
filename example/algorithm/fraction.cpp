#include "fraction.hpp"
#include <iostream>

int main() {
    // Example usage of Fraction constructors
    {
        atom::algorithm::Fraction f1(3, 4);  // 3/4
        atom::algorithm::Fraction f2(5);     // 5/1
        atom::algorithm::Fraction f3;        // 0/1

        std::cout << "Fraction f1: " << f1 << std::endl;
        std::cout << "Fraction f2: " << f2 << std::endl;
        std::cout << "Fraction f3: " << f3 << std::endl;
    }

    // Example usage of arithmetic operations
    {
        atom::algorithm::Fraction f1(3, 4);
        atom::algorithm::Fraction f2(2, 3);

        atom::algorithm::Fraction sum = f1 + f2;
        atom::algorithm::Fraction difference = f1 - f2;
        atom::algorithm::Fraction product = f1 * f2;
        atom::algorithm::Fraction quotient = f1 / f2;

        std::cout << "\nSum: " << sum << std::endl;
        std::cout << "Difference: " << difference << std::endl;
        std::cout << "Product: " << product << std::endl;
        std::cout << "Quotient: " << quotient << std::endl;
    }

    // Example usage of comparison operators
    {
        atom::algorithm::Fraction f1(3, 4);
        atom::algorithm::Fraction f2(2, 3);

        std::cout << "\nComparison:" << std::endl;
        std::cout << "f1 == f2: " << (f1 == f2) << std::endl;
        std::cout << "f1 < f2: " << (f1 < f2) << std::endl;
        std::cout << "f1 > f2: " << (f1 > f2) << std::endl;
    }

    // Example usage of type conversion
    {
        atom::algorithm::Fraction f(3, 4);

        double d = static_cast<double>(f);
        float fl = static_cast<float>(f);
        int i = static_cast<int>(f);

        std::cout << "\nType conversion:" << std::endl;
        std::cout << "Fraction as double: " << d << std::endl;
        std::cout << "Fraction as float: " << fl << std::endl;
        std::cout << "Fraction as int: " << i << std::endl;
    }

    // Example usage of utility functions
    {
        atom::algorithm::Fraction f(-3, 4);

        std::cout << "\nUtility functions:" << std::endl;
        std::cout << "Fraction: " << f << std::endl;
        std::cout << "Absolute value: " << f.abs() << std::endl;
        std::cout << "Is zero: " << f.isZero() << std::endl;
        std::cout << "Is positive: " << f.isPositive() << std::endl;
        std::cout << "Is negative: " << f.isNegative() << std::endl;
    }

    // Example usage of makeFraction functions
    {
        atom::algorithm::Fraction f1 = atom::algorithm::makeFraction(5);
        atom::algorithm::Fraction f2 = atom::algorithm::makeFraction(3.14159);

        std::cout << "\nmakeFraction functions:" << std::endl;
        std::cout << "Fraction from integer: " << f1 << std::endl;
        std::cout << "Fraction from double: " << f2 << std::endl;
    }

    // Example usage of input/output stream operators
    {
        atom::algorithm::Fraction f;
        std::cout << "\nEnter a fraction (numerator/denominator): ";
        std::cin >> f;
        std::cout << "You entered: " << f << std::endl;
    }

    return 0;
}