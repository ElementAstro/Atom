#include "atom/utils/random.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace atom::utils;

int main() {
    // Create a Random object for generating integers between 1 and 100
    Random<std::mt19937, std::uniform_int_distribution<>> randomInt(1, 100);

    // Generate a single random integer
    int randomValue = randomInt();
    std::cout << "Random integer: " << randomValue << std::endl;

    // Generate a vector of 10 random integers
    std::vector<int> randomVector = randomInt.vector(10);
    std::cout << "Random vector: ";
    for (int value : randomVector) {
        std::cout << value << " ";
    }
    std::cout << std::endl;

    // Create a Random object for generating doubles between 0.0 and 1.0
    Random<std::mt19937, std::uniform_real_distribution<>> randomDouble(0.0,
                                                                        1.0);

    // Generate a single random double
    double randomDoubleValue = randomDouble();
    std::cout << "Random double: " << randomDoubleValue << std::endl;

    // Generate a vector of 5 random doubles
    std::vector<double> randomDoubleVector = randomDouble.vector(5);
    std::cout << "Random double vector: ";
    for (double value : randomDoubleVector) {
        std::cout << value << " ";
    }
    std::cout << std::endl;

    // Generate a random string of length 10
    std::string randomString = generateRandomString(10);
    std::cout << "Random string: " << randomString << std::endl;

    return 0;
}