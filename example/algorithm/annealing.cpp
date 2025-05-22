#include "atom/algorithm/annealing.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>

// Define progress callback function
void progressCallback(int iteration, double energy,
                      const std::vector<int>& solution) {
    if (iteration % 100 == 0) {  // Reduce output frequency
        std::cout << "Iteration: " << iteration << ", Energy: " << std::fixed
                  << std::setprecision(4) << energy << std::endl;
    }
}

// Define stop condition function
bool stopCondition(int iteration, double energy,
                   const std::vector<int>& solution) {
    // Stop when energy is below threshold or iterations exceed 5000
    return energy < 10.0 || iteration > 5000;
}

// Helper function to display the solution
void printSolution(const std::vector<int>& solution,
                   const std::vector<std::pair<double, double>>& cities) {
    std::cout << "Path order: ";
    for (int city : solution) {
        std::cout << city << " ";
    }
    std::cout << std::endl;

    std::cout << "City coordinates:" << std::endl;
    for (int idx : solution) {
        std::cout << "City " << idx << ": (" << cities[idx].first << ", "
                  << cities[idx].second << ")" << std::endl;
    }
}

int main() {
    // Define cities for the TSP problem
    std::vector<std::pair<double, double>> cities = {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.5, 0.5}, {0.5, 1.5},
        {1.5, 0.5}, {1.5, 1.5}, {2.0, 2.0}, {2.5, 1.0}, {3.0, 3.0}, {0.5, 2.5}};

    // Create TSP problem instance
    TSP tspProblem(cities);

    // Use Builder pattern to create SimulatedAnnealing object
    SimulatedAnnealing<TSP, std::vector<int>>::Builder builder(tspProblem);
    builder.setCoolingStrategy(AnnealingStrategy::EXPONENTIAL)
        .setMaxIterations(10000)
        .setInitialTemperature(1000.0)
        .setCoolingRate(0.97)
        .setRestartInterval(500);

    SimulatedAnnealing<TSP, std::vector<int>> sa = builder.build();

    // Set progress callback
    sa.setProgressCallback(progressCallback);

    // Set stop condition
    sa.setStopCondition(stopCondition);

    std::cout << "=== Simulated Annealing with Exponential Cooling ==="
              << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    // Optimize TSP problem using 4 threads
    std::vector<int> bestSolution = sa.optimize(4);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    // Get the best energy
    double bestEnergy = sa.getBestEnergy();

    // Print the best solution and energy
    std::cout << "\nBest solution:" << std::endl;
    printSolution(bestSolution, cities);
    std::cout << "Best path length: " << std::fixed << std::setprecision(6)
              << bestEnergy << std::endl;
    std::cout << "Optimization time: " << elapsed.count() << " seconds"
              << std::endl;

    // Demonstrate different cooling strategies
    std::cout << "\n=== Simulated Annealing with Linear Cooling ==="
              << std::endl;
    sa.setCoolingSchedule(AnnealingStrategy::LINEAR);
    bestSolution = sa.optimize(2);
    bestEnergy = sa.getBestEnergy();
    std::cout << "Linear cooling best path length: " << bestEnergy << std::endl;

    std::cout << "\n=== Simulated Annealing with Adaptive Cooling ==="
              << std::endl;
    sa.setCoolingSchedule(AnnealingStrategy::ADAPTIVE);
    bestSolution = sa.optimize(2);
    bestEnergy = sa.getBestEnergy();
    std::cout << "Adaptive cooling best path length: " << bestEnergy
              << std::endl;

    // Show how to modify cooling rate
    std::cout << "\n=== Simulated Annealing with Custom Parameters ==="
              << std::endl;
    sa.setCoolingRate(0.99);          // Slower cooling
    sa.setInitialTemperature(500.0);  // Lower initial temperature
    bestSolution = sa.optimize(1);
    bestEnergy = sa.getBestEnergy();
    std::cout << "Custom parameters best path length: " << bestEnergy
              << std::endl;

    return 0;
}