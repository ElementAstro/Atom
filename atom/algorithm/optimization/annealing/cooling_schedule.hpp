#ifndef ATOM_ALGORITHM_OPTIMIZATION_COOLING_SCHEDULE_HPP
#define ATOM_ALGORITHM_OPTIMIZATION_COOLING_SCHEDULE_HPP

#include <algorithm>
#include <cmath>
#include <functional>

#include "annealing_concept.hpp"
#include "spdlog/spdlog.h"

// Factory function to create a cooling schedule from a strategy
inline std::function<double(int)> createCoolingSchedule(
    AnnealingStrategy strategy, double initial_temperature, int max_iterations,
    double cooling_rate) {
    spdlog::info("Setting cooling schedule to strategy: {}",
                 static_cast<int>(strategy));
    switch (strategy) {
        case AnnealingStrategy::LINEAR:
            return [initial_temperature, max_iterations](int iteration) {
                return initial_temperature *
                       (1 - static_cast<double>(iteration) / max_iterations);
            };
        case AnnealingStrategy::EXPONENTIAL:
            return [initial_temperature, cooling_rate](int iteration) {
                return initial_temperature *
                       std::pow(cooling_rate, iteration);
            };
        case AnnealingStrategy::LOGARITHMIC:
            return [initial_temperature](int iteration) {
                if (iteration == 0)
                    return initial_temperature;
                return initial_temperature / std::log(iteration + 2);
            };
        case AnnealingStrategy::GEOMETRIC:
            return [initial_temperature, cooling_rate](int iteration) {
                return initial_temperature / (1 + cooling_rate * iteration);
            };
        case AnnealingStrategy::QUADRATIC:
            return [initial_temperature, cooling_rate](int iteration) {
                return initial_temperature /
                       (1 + cooling_rate * iteration * iteration);
            };
        case AnnealingStrategy::HYPERBOLIC:
            return [initial_temperature, cooling_rate](int iteration) {
                return initial_temperature /
                       (1 + cooling_rate * std::sqrt(iteration));
            };
        case AnnealingStrategy::ADAPTIVE:
            return [initial_temperature, cooling_rate](int iteration) {
                return initial_temperature *
                       std::pow(cooling_rate, iteration);
            };
        default:
            spdlog::warn(
                "Unknown cooling strategy. Defaulting to EXPONENTIAL.");
            return [initial_temperature, cooling_rate](int iteration) {
                return initial_temperature *
                       std::pow(cooling_rate, iteration);
            };
    }
}

#endif  // ATOM_ALGORITHM_OPTIMIZATION_COOLING_SCHEDULE_HPP
