/*
 * random_example.cpp
 *
 * Copyright (C) 2024 Max Q.
 *
 * Example usage of the atom::utils random number generation utilities
 */

#include <deque>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "atom/utils/random.hpp"

// Helper function to print vectors
template <typename T>
void printVector(const std::vector<T>& vec, const std::string& label) {
    std::cout << label << ": [";
    for (size_t i = 0; i < vec.size(); ++i) {
        std::cout << vec[i];
        if (i < vec.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << "]\n";
}

// Helper function to print containers
template <typename Container>
void printContainer(const Container& container, const std::string& label) {
    std::cout << label << ": [";
    bool first = true;
    for (const auto& item : container) {
        if (!first) {
            std::cout << ", ";
        }
        std::cout << item;
        first = false;
    }
    std::cout << "]\n";
}

int main() {
    std::cout << "=== Atom Random Utilities Examples ===\n\n";

    // Example 1: Basic integer random generator
    std::cout << "Example 1: Basic integer random generation\n";

    // Create a random integer generator between 1 and 100
    atom::utils::Random<std::mt19937, std::uniform_int_distribution<int>>
        intRandom(1, 100);

    std::cout << "Five random integers between 1 and 100:\n";
    for (int i = 0; i < 5; ++i) {
        std::cout << "  " << intRandom() << "\n";
    }
    std::cout << "\n";

    // Example 2: Using the range static method
    std::cout << "Example 2: Generate a range of random integers\n";
    auto randomInts =
        atom::utils::Random<std::mt19937,
                            std::uniform_int_distribution<int>>::range(10, 1,
                                                                       6);
    printVector(randomInts, "10 random dice rolls (1-6)");
    std::cout << "\n";

    // Example 3: Float random generator
    std::cout << "Example 3: Random floating-point values\n";
    atom::utils::Random<std::mt19937_64, std::uniform_real_distribution<double>>
        floatRandom(0.0, 1.0);

    std::cout << "Five random doubles between 0.0 and 1.0:\n";
    for (int i = 0; i < 5; ++i) {
        std::cout << "  " << std::fixed << std::setprecision(6) << floatRandom()
                  << "\n";
    }
    std::cout << "\n";

    // Example 4: Reseeding the generator
    std::cout << "Example 4: Reseeding the random generator\n";
    intRandom.seed(12345);  // Set a fixed seed for reproducibility

    std::cout << "Five random integers after setting seed to 12345:\n";
    for (int i = 0; i < 5; ++i) {
        std::cout << "  " << intRandom() << "\n";
    }
    std::cout << "\n";

    // Example 5: Filling a container with random values
    std::cout << "Example 5: Filling a vector with random values\n";
    std::vector<int> randomVector(8);
    intRandom.generate(randomVector);
    printVector(randomVector, "Vector filled with random integers");
    std::cout << "\n";

    // Example 6: Working with custom ranges
    std::cout << "Example 6: Using iterators to fill a container\n";
    std::deque<int> randomDeque(10);
    intRandom.generate(randomDeque.begin(), randomDeque.end());
    printContainer(randomDeque, "Deque filled with random integers");
    std::cout << "\n";

    // Example 7: Creating a vector directly
    std::cout << "Example 7: Creating a vector with the vector() method\n";
    auto randomVector2 = intRandom.vector(8);
    printVector(randomVector2, "Vector created with random integers");
    std::cout << "\n";

    // Example 8: Using normal distribution
    std::cout << "Example 8: Using normal distribution\n";
    atom::utils::Random<std::mt19937, std::normal_distribution<double>>
        normalRandom(50.0, 10.0);  // mean=50, stddev=10

    std::cout << "Five random values from normal distribution (mean=50, "
                 "stddev=10):\n";
    for (int i = 0; i < 5; ++i) {
        std::cout << "  " << std::fixed << std::setprecision(2)
                  << normalRandom() << "\n";
    }
    std::cout << "\n";

    // Example 9: Changing distribution parameters
    std::cout << "Example 9: Changing distribution parameters\n";
    auto newParams = std::uniform_int_distribution<int>::param_type(100, 200);
    intRandom.param(newParams);

    std::cout << "Five random integers after changing range to 100-200:\n";
    for (int i = 0; i < 5; ++i) {
        std::cout << "  " << intRandom() << "\n";
    }
    std::cout << "\n";

    // Example 10: Generate random string
    std::cout << "Example 10: Generating random strings\n";
    std::string randomStr = atom::utils::generateRandomString(15);
    std::cout << "Random alphanumeric string (15 chars): " << randomStr << "\n";

    std::string randomHexStr =
        atom::utils::generateRandomString(10, "0123456789ABCDEF");
    std::cout << "Random hex string (10 chars): " << randomHexStr << "\n\n";

    // Example 11: Generate secure random string
    std::cout << "Example 11: Generating secure random strings\n";
    std::string secureRandomStr = atom::utils::generateSecureRandomString(20);
    std::cout << "Secure random string (20 chars): " << secureRandomStr
              << "\n\n";

    // Example 12: Secure shuffle
    std::cout << "Example 12: Secure shuffling of containers\n";
    std::vector<int> toShuffle = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    printVector(toShuffle, "Before shuffle");

    atom::utils::secureShuffleRange(toShuffle);
    printVector(toShuffle, "After secure shuffle");
    std::cout << "\n";

    // Example 13: Using different engines
    std::cout << "Example 13: Using different random engines\n";
    atom::utils::Random<std::minstd_rand0, std::uniform_int_distribution<int>>
        fastRandom(1, 100);

    std::cout << "Five random integers using minstd_rand0 engine:\n";
    for (int i = 0; i < 5; ++i) {
        std::cout << "  " << fastRandom() << "\n";
    }
    std::cout << "\n";

    // Example 14: Using Bernoulli distribution (coin flip)
    std::cout << "Example 14: Using Bernoulli distribution (coin flips)\n";
    atom::utils::Random<std::mt19937, std::bernoulli_distribution> coinFlip(
        0.5);  // 50% chance for true

    std::cout << "10 random coin flips (1=heads, 0=tails):\n  ";
    for (int i = 0; i < 10; ++i) {
        std::cout << (coinFlip() ? "H" : "T") << " ";
    }
    std::cout << "\n\n";

    // Example 15: Using discrete distribution
    std::cout
        << "Example 15: Using discrete distribution with custom weights\n";
    std::vector<double> weights = {10.0, 30.0, 15.0, 5.0,
                                   40.0};  // Higher chance for index 1 and 4
    atom::utils::Random<std::mt19937, std::discrete_distribution<int>>
        weightedRandom(weights.begin(), weights.end());

    std::map<int, int> results;
    for (int i = 0; i < 1000; ++i) {
        ++results[weightedRandom()];
    }

    std::cout << "Distribution of 1000 weighted random draws (higher weights "
                 "for 1 and 4):\n";
    for (const auto& [value, count] : results) {
        std::cout << "  " << value << ": " << count << " (" << std::fixed
                  << std::setprecision(1) << (count / 10.0) << "%)\n";
    }

    return 0;
}