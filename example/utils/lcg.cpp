#include "atom/utils/lcg.hpp"

#include <iostream>
#include <vector>

using namespace atom::utils;

int main() {
    // Create an LCG instance with an optional seed
    LCG lcg;

    // Generate the next random number in the sequence
    auto randomNumber = lcg.next();
    std::cout << "Next random number: " << randomNumber << std::endl;

    // Seed the generator with a new seed value
    lcg.seed(12345);

    // Save the current state of the generator to a file
    lcg.saveState("lcg_state.dat");

    // Load the state of the generator from a file
    lcg.loadState("lcg_state.dat");

    // Generate a random integer within a specified range
    int randomInt = lcg.nextInt(1, 100);
    std::cout << "Random integer: " << randomInt << std::endl;

    // Generate a random double within a specified range
    double randomDouble = lcg.nextDouble(0.0, 10.0);
    std::cout << "Random double: " << randomDouble << std::endl;

    // Generate a random boolean value based on a specified probability
    bool randomBool = lcg.nextBernoulli(0.7);
    std::cout << "Random boolean: " << std::boolalpha << randomBool
              << std::endl;

    // Generate a random number following a Gaussian (normal) distribution
    double randomGaussian = lcg.nextGaussian(0.0, 1.0);
    std::cout << "Random Gaussian: " << randomGaussian << std::endl;

    // Generate a random number following a Poisson distribution
    int randomPoisson = lcg.nextPoisson(4.0);
    std::cout << "Random Poisson: " << randomPoisson << std::endl;

    // Generate a random number following an Exponential distribution
    double randomExponential = lcg.nextExponential(1.0);
    std::cout << "Random Exponential: " << randomExponential << std::endl;

    // Generate a random number following a Geometric distribution
    int randomGeometric = lcg.nextGeometric(0.5);
    std::cout << "Random Geometric: " << randomGeometric << std::endl;

    // Generate a random number following a Gamma distribution
    double randomGamma = lcg.nextGamma(2.0, 2.0);
    std::cout << "Random Gamma: " << randomGamma << std::endl;

    // Generate a random number following a Beta distribution
    double randomBeta = lcg.nextBeta(2.0, 5.0);
    std::cout << "Random Beta: " << randomBeta << std::endl;

    // Generate a random number following a Chi-Squared distribution
    double randomChiSquared = lcg.nextChiSquared(2.0);
    std::cout << "Random Chi-Squared: " << randomChiSquared << std::endl;

    // Generate a random number following a Hypergeometric distribution
    int randomHypergeometric = lcg.nextHypergeometric(100, 50, 10);
    std::cout << "Random Hypergeometric: " << randomHypergeometric << std::endl;

    // Generate a random index based on a discrete distribution
    std::vector<double> weights = {0.1, 0.2, 0.3, 0.4};
    int randomDiscrete = lcg.nextDiscrete(weights);
    std::cout << "Random Discrete: " << randomDiscrete << std::endl;

    // Generate a multinomial distribution
    std::vector<int> randomMultinomial = lcg.nextMultinomial(10, weights);
    std::cout << "Random Multinomial: ";
    for (const auto& count : randomMultinomial) {
        std::cout << count << " ";
    }
    std::cout << std::endl;

    // Shuffle a vector of data
    std::vector<int> data = {1, 2, 3, 4, 5};
    lcg.shuffle(data);
    std::cout << "Shuffled data: ";
    for (const auto& elem : data) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Sample a subset of data from a vector
    auto sampledData = lcg.sample(data, 3);
    std::cout << "Sampled data: ";
    for (const auto& elem : sampledData) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    return 0;
}