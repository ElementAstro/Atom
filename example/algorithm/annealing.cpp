#include "annealing.hpp"
#include <iostream>

// Define a custom progress callback function
void progressCallback(int iteration, double energy,
                      const std::vector<int>& solution) {
    std::cout << "Iteration: " << iteration << ", Energy: " << energy
              << std::endl;
}

// Define a custom stop condition function
bool stopCondition(int iteration, double energy,
                   const std::vector<int>& solution) {
    // Stop if the energy is below a certain threshold
    return energy < 100.0;
}

int main() {
    // Define the cities for the TSP problem
    std::vector<std::pair<double, double>> cities = {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0},
        {0.5, 0.5}, {0.5, 1.5}, {1.5, 0.5}, {1.5, 1.5}};

    // Create a TSP problem instance
    TSP tspProblem(cities);

    // Create a SimulatedAnnealing object with the TSP problem
    SimulatedAnnealing<TSP, std::vector<int>> sa(tspProblem);

    // Set the cooling schedule to LINEAR
    sa.setCoolingSchedule(AnnealingStrategy::LINEAR);

    // Set the progress callback
    sa.setProgressCallback(progressCallback);

    // Set the stop condition
    sa.setStopCondition(stopCondition);

    // Optimize the TSP problem using 4 threads
    std::vector<int> bestSolution = sa.optimize(4);

    // Get the best energy found
    double bestEnergy = sa.getBestEnergy();

    // Print the best solution and its energy
    std::cout << "Best solution found: ";
    for (int city : bestSolution) {
        std::cout << city << " ";
    }
    std::cout << "\nBest energy: " << bestEnergy << std::endl;

    return 0;
}