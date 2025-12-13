/**
 * @file random_example.cpp
 * @brief Examples for atom::utils Random class
 */

#include "atom/utils/random/random.hpp"
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

void demonstrateIntegerRandom() {
    printSection("1. Integer Random Numbers");

    Random<std::mt19937, std::uniform_int_distribution<int>> rng(1, 100);

    std::cout << "Random integers [1, 100]:" << std::endl;
    std::cout << "  ";
    for (int i = 0; i < 10; ++i) {
        std::cout << rng() << " ";
    }
    std::cout << std::endl;

    std::cout << "\nGenerating vector of 5 random integers:" << std::endl;
    auto vec = rng.vector(5);
    std::cout << "  ";
    for (int val : vec) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
}

void demonstrateFloatRandom() {
    printSection("2. Floating Point Random Numbers");

    Random<std::mt19937, std::uniform_real_distribution<double>> rng(0.0, 1.0);

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Random doubles [0.0, 1.0):" << std::endl;
    std::cout << "  ";
    for (int i = 0; i < 5; ++i) {
        std::cout << rng() << " ";
    }
    std::cout << std::endl;

    Random<std::mt19937, std::uniform_real_distribution<double>> rng2(-10.0, 10.0);
    std::cout << "\nRandom doubles [-10.0, 10.0):" << std::endl;
    std::cout << "  ";
    for (int i = 0; i < 5; ++i) {
        std::cout << rng2() << " ";
    }
    std::cout << std::endl;
}

void demonstrateNormalDistribution() {
    printSection("3. Normal Distribution");

    Random<std::mt19937, std::normal_distribution<double>> rng(0.0, 1.0);

    std::cout << "Normal distribution (mean=0, stddev=1):" << std::endl;
    std::cout << "  ";
    for (int i = 0; i < 10; ++i) {
        std::cout << std::fixed << std::setprecision(2) << rng() << " ";
    }
    std::cout << std::endl;

    std::cout << "\nHistogram of 1000 samples:" << std::endl;
    std::map<int, int> histogram;
    for (int i = 0; i < 1000; ++i) {
        int bucket = static_cast<int>(std::round(rng()));
        histogram[bucket]++;
    }

    for (int i = -3; i <= 3; ++i) {
        std::cout << "  " << std::setw(2) << i << ": ";
        int count = histogram[i];
        for (int j = 0; j < count / 10; ++j) {
            std::cout << "*";
        }
        std::cout << " (" << count << ")" << std::endl;
    }
}

void demonstrateSeeding() {
    printSection("4. Seeding for Reproducibility");

    std::cout << "Same seed produces same sequence:" << std::endl;

    Random<std::mt19937, std::uniform_int_distribution<int>> rng1(1, 100);
    rng1.seed(12345);

    std::cout << "  Seed 12345, sequence 1: ";
    for (int i = 0; i < 5; ++i) {
        std::cout << rng1() << " ";
    }
    std::cout << std::endl;

    Random<std::mt19937, std::uniform_int_distribution<int>> rng2(1, 100);
    rng2.seed(12345);

    std::cout << "  Seed 12345, sequence 2: ";
    for (int i = 0; i < 5; ++i) {
        std::cout << rng2() << " ";
    }
    std::cout << std::endl;

    rng2.seed(99999);
    std::cout << "  Seed 99999, sequence 3: ";
    for (int i = 0; i < 5; ++i) {
        std::cout << rng2() << " ";
    }
    std::cout << std::endl;
}

void demonstrateRangeGeneration() {
    printSection("5. Range Generation");

    Random<std::mt19937, std::uniform_int_distribution<int>> rng(0, 9);

    std::vector<int> data(10);
    rng.generate(data);

    std::cout << "Generated into existing vector:" << std::endl;
    std::cout << "  ";
    for (int val : data) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    std::cout << "\nGenerated with iterators:" << std::endl;
    std::vector<int> data2(8);
    rng.generate(data2.begin(), data2.end());
    std::cout << "  ";
    for (int val : data2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
}

void demonstrateDiceSimulation() {
    printSection("6. Dice Simulation");

    Random<std::mt19937, std::uniform_int_distribution<int>> d6(1, 6);

    std::cout << "Rolling a 6-sided die 20 times:" << std::endl;
    std::cout << "  ";
    for (int i = 0; i < 20; ++i) {
        std::cout << d6() << " ";
    }
    std::cout << std::endl;

    std::cout << "\nDistribution of 6000 rolls:" << std::endl;
    std::map<int, int> counts;
    for (int i = 0; i < 6000; ++i) {
        counts[d6()]++;
    }

    for (int i = 1; i <= 6; ++i) {
        std::cout << "  " << i << ": " << counts[i] << " ("
                  << std::fixed << std::setprecision(1)
                  << (counts[i] / 60.0) << "%)" << std::endl;
    }
}

void demonstrateCardShuffle() {
    printSection("7. Card Shuffle Simulation");

    std::vector<std::string> deck;
    const char* suits[] = {"♠", "♥", "♦", "♣"};
    const char* ranks[] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};

    for (const char* suit : suits) {
        for (const char* rank : ranks) {
            deck.push_back(std::string(rank) + suit);
        }
    }

    std::cout << "Original deck (first 13 cards):" << std::endl;
    std::cout << "  ";
    for (int i = 0; i < 13; ++i) {
        std::cout << deck[i] << " ";
    }
    std::cout << std::endl;

    Random<std::mt19937, std::uniform_int_distribution<size_t>> rng(0, deck.size() - 1);

    for (size_t i = deck.size() - 1; i > 0; --i) {
        size_t j = rng() % (i + 1);
        std::swap(deck[i], deck[j]);
    }

    std::cout << "\nShuffled deck (first 13 cards):" << std::endl;
    std::cout << "  ";
    for (int i = 0; i < 13; ++i) {
        std::cout << deck[i] << " ";
    }
    std::cout << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Random Number Examples" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateIntegerRandom();
        demonstrateFloatRandom();
        demonstrateNormalDistribution();
        demonstrateSeeding();
        demonstrateRangeGeneration();
        demonstrateDiceSimulation();
        demonstrateCardShuffle();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All random examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
