#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/algorithm/annealing.hpp"

namespace py = pybind11;

double compute_tour_length(const std::vector<std::pair<double, double>>& cities,
                           const std::vector<int>& tour) {
    if (tour.empty())
        return 0.0;

    double total_distance = 0.0;
    for (size_t i = 0; i < tour.size(); ++i) {
        size_t from = tour[i];
        size_t to = tour[(i + 1) % tour.size()];

        double dx = cities[from].first - cities[to].first;
        double dy = cities[from].second - cities[to].second;
        total_distance += std::sqrt(dx * dx + dy * dy);
    }

    return total_distance;
}

PYBIND11_MODULE(annealing, m) {
    m.doc() = R"pbdoc(
        Simulated Annealing optimization module
        ---------------------------------------

        This module provides implementation of the Simulated Annealing algorithm
        for combinatorial optimization problems, with a focus on the Traveling
        Salesman Problem (TSP).

        Example:
            >>> import atom.algorithm.annealing as sa
            >>> # Create a TSP with 5 cities
            >>> cities = [(0,0), (1,1), (2,3), (4,2), (3,0)]
            >>> tsp = sa.TSP(cities)
            >>> # Build and run the annealing optimizer
            >>> builder = sa.TspAnnealingBuilder(tsp)
            >>> builder.set_max_iterations(10000)
            >>> builder.set_cooling_strategy(sa.AnnealingStrategy.EXPONENTIAL)
            >>> annealing = builder.build()
            >>> best_tour = annealing.optimize()
            >>> print(f"Best tour length: {tsp.energy(best_tour)}")

            # Alternatively, use the convenience function:
            >>> best_tour = sa.solve_tsp(cities, max_iterations=10000)
    )pbdoc";

    // Register exception translation
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const atom::error::Exception& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        }
    });

    // Expose AnnealingStrategy enum
    py::enum_<AnnealingStrategy>(m, "AnnealingStrategy",
                                 "Cooling strategies for simulated annealing")
        .value("LINEAR", AnnealingStrategy::LINEAR, "Linear cooling schedule")
        .value("EXPONENTIAL", AnnealingStrategy::EXPONENTIAL,
               "Exponential cooling schedule")
        .value("LOGARITHMIC", AnnealingStrategy::LOGARITHMIC,
               "Logarithmic cooling schedule")
        .value("GEOMETRIC", AnnealingStrategy::GEOMETRIC,
               "Geometric cooling schedule")
        .value("QUADRATIC", AnnealingStrategy::QUADRATIC,
               "Quadratic cooling schedule")
        .value("HYPERBOLIC", AnnealingStrategy::HYPERBOLIC,
               "Hyperbolic cooling schedule")
        .value("ADAPTIVE", AnnealingStrategy::ADAPTIVE,
               "Adaptive cooling schedule")
        .export_values();

    // Expose TSP class
    py::class_<TSP>(m, "TSP", R"pbdoc(
        Traveling Salesman Problem implementation.

        This class represents a TSP problem instance with cities at specific coordinates.
        It provides methods to evaluate solutions, generate neighbors, and create random tours.

        Args:
            cities: List of (x,y) coordinates for each city
    )pbdoc")
        .def(py::init<const std::vector<std::pair<double, double>>&>(),
             py::arg("cities"),
             "Create a TSP instance with a list of city coordinates")
        .def("energy", &TSP::energy, py::arg("solution"),
             "Calculate the total distance of a tour")
        .def_static("neighbor", &TSP::neighbor, py::arg("solution"),
             "Generate a neighboring solution by swapping two cities")
        .def("random_solution", &TSP::randomSolution,
             "Generate a random tour visiting all cities once")
        .def(
            "validate",
            []([[maybe_unused]] const TSP& self,
               const std::vector<int>& solution) {
                // Simple validation to check that all cities are visited once
                if (solution.empty())
                    return false;

                std::vector<bool> visited(solution.size(), false);
                for (int city : solution) {
                    if (city < 0 || city >= static_cast<int>(solution.size()))
                        return false;
                    if (visited[city])
                        return false;
                    visited[city] = true;
                }
                return std::all_of(visited.begin(), visited.end(),
                                   [](bool v) { return v; });
            },
            py::arg("solution"), "Validate that a solution is a valid tour");

    // Type alias for SimulatedAnnealing with TSP
    using TspAnnealing = SimulatedAnnealing<TSP, std::vector<int>>;

    // Builder class for TSP Annealing
    py::class_<TspAnnealing::Builder>(m, "TspAnnealingBuilder", R"pbdoc(
        Builder for configuring and creating a Simulated Annealing optimizer for TSP.

        This builder allows you to configure all aspects of the simulated annealing
        algorithm before creating the optimizer instance.

        Args:
            problem_instance: A TSP problem instance
    )pbdoc")
        .def(py::init<TSP&>(), py::arg("problem_instance"),
             "Create a builder with a TSP problem instance")
        .def("set_cooling_strategy", &TspAnnealing::Builder::setCoolingStrategy,
             py::arg("strategy"),
             "Set the cooling strategy for temperature reduction")
        .def("set_max_iterations", &TspAnnealing::Builder::setMaxIterations,
             py::arg("iterations"), "Set the maximum number of iterations")
        .def("set_initial_temperature",
             &TspAnnealing::Builder::setInitialTemperature,
             py::arg("temperature"), "Set the initial temperature")
        .def("set_cooling_rate", &TspAnnealing::Builder::setCoolingRate,
             py::arg("rate"), "Set the cooling rate for temperature reduction")
        .def("set_restart_interval", &TspAnnealing::Builder::setRestartInterval,
             py::arg("interval"),
             "Set the interval for restarting the optimization with a new "
             "random solution")
        .def("build", &TspAnnealing::Builder::build,
             "Create a SimulatedAnnealing instance");

    // SimulatedAnnealing class for TSP
    py::class_<TspAnnealing>(m, "TspAnnealing", R"pbdoc(
        Simulated Annealing optimizer for the Traveling Salesman Problem.

        This class implements the simulated annealing algorithm to find
        near-optimal solutions to TSP instances.

        Args:
            builder: A configured TspAnnealingBuilder
    )pbdoc")
        .def(py::init<const TspAnnealing::Builder&>(), py::arg("builder"),
             "Create from a builder")
        .def("set_cooling_schedule", &TspAnnealing::setCoolingSchedule,
             py::arg("strategy"), "Set the cooling schedule strategy")
        .def(
            "set_progress_callback",
            [](TspAnnealing& self, py::function callback) {
                self.setProgressCallback([callback](
                                             int iter, double energy,
                                             const std::vector<int>& solution) {
                    // Use Python's GIL when calling back to Python code
                    py::gil_scoped_acquire acquire;
                    try {
                        callback(iter, energy, solution);
                    } catch (py::error_already_set& e) {
                        // Convert Python exceptions back to C++ exceptions
                        throw std::runtime_error(
                            std::string("Python callback error: ") + e.what());
                    }
                });
            },
            py::arg("callback"),
            "Set a callback function to report progress (iteration, energy, "
            "solution)")
        .def(
            "set_stop_condition",
            [](TspAnnealing& self, py::function condition) {
                self.setStopCondition([condition](
                                          int iter, double energy,
                                          const std::vector<int>& solution) {
                    // Use Python's GIL when calling back to Python code
                    py::gil_scoped_acquire acquire;
                    try {
                        return condition(iter, energy, solution).cast<bool>();
                    } catch (py::error_already_set& e) {
                        // Convert Python exceptions back to C++ exceptions
                        throw std::runtime_error(
                            std::string("Python callback error: ") + e.what());
                    }
                });
            },
            py::arg("condition"),
            "Set a function that determines when to stop optimization "
            "(iteration, energy, solution)")
        .def("optimize", &TspAnnealing::optimize,
             "Run the optimization with optional parallel threads")
        .def("get_best_energy", &TspAnnealing::getBestEnergy,
             "Get the energy of the best solution found")
        .def("set_initial_temperature", &TspAnnealing::setInitialTemperature,
             py::arg("temperature"), "Set the initial temperature")
        .def("set_cooling_rate", &TspAnnealing::setCoolingRate, py::arg("rate"),
             "Set the cooling rate for temperature reduction");

    // Utility functions
    m.def(
        "solve_tsp",
        [](const std::vector<std::pair<double, double>>& cities,
           double initial_temp = 100.0, int max_iterations = 10000,
           AnnealingStrategy strategy = AnnealingStrategy::EXPONENTIAL,
           double cooling_rate = 0.95, int num_threads = 1) {
            // Convenience function to solve a TSP problem with minimal setup
            TSP tsp(cities);

            auto builder = TspAnnealing::Builder(tsp)
                               .setInitialTemperature(initial_temp)
                               .setMaxIterations(max_iterations)
                               .setCoolingStrategy(strategy)
                               .setCoolingRate(cooling_rate);

            auto annealing = builder.build();
            return annealing.optimize(num_threads);
        },
        py::arg("cities"), py::arg("initial_temp") = 100.0,
        py::arg("max_iterations") = 10000,
        py::arg("strategy") = AnnealingStrategy::EXPONENTIAL,
        py::arg("cooling_rate") = 0.95, py::arg("num_threads") = 1,
        R"pbdoc(
        Solve a TSP problem with simulated annealing.

        This is a convenience function that sets up and runs the simulated annealing
        algorithm with sensible defaults.

        Args:
            cities: List of (x,y) coordinates for each city
            initial_temp: Starting temperature (default: 100.0)
            max_iterations: Maximum number of iterations (default: 10000)
            strategy: Cooling strategy to use (default: EXPONENTIAL)
            cooling_rate: Rate of temperature reduction (default: 0.95)
            num_threads: Number of parallel optimization threads (default: 1)

        Returns:
            The best tour found as a list of city indices
       )pbdoc");

    // Calculate optimal cooling rate based on desired acceptance rate change
    m.def(
        "calculate_cooling_rate",
        [](double initial_acceptance_rate, double final_acceptance_rate,
           int iterations) {
            // alpha = (final_rate / initial_rate)^(1/iterations)
            return std::pow(final_acceptance_rate / initial_acceptance_rate,
                            1.0 / iterations);
        },
        py::arg("initial_acceptance_rate"), py::arg("final_acceptance_rate"),
        py::arg("iterations"),
        R"pbdoc(
        Calculate a cooling rate for exponential cooling.

        This function computes a cooling rate that will reduce the acceptance 
        probability from an initial value to a final value over the specified 
        number of iterations.

        Args:
            initial_acceptance_rate: Desired initial probability of accepting worse solutions
            final_acceptance_rate: Desired final probability of accepting worse solutions
            iterations: Number of iterations over which to transition

        Returns:
            The cooling rate to use with EXPONENTIAL cooling strategy
       )pbdoc");

    // Utility to estimate initial temperature
    m.def(
        "estimate_initial_temperature",
        [](const TSP& tsp, double desired_acceptance_rate = 0.8,
           int samples = 100) {
            // Generate random solutions and calculate energy differences
            std::vector<double> energy_diffs;
            energy_diffs.reserve(samples);

            auto solution = tsp.randomSolution();
            double base_energy = tsp.energy(solution);

            for (int i = 0; i < samples; ++i) {
                auto neighbor = tsp.neighbor(solution);
                double neighbor_energy = tsp.energy(neighbor);
                if (neighbor_energy > base_energy) {
                    energy_diffs.push_back(neighbor_energy - base_energy);
                }
                solution = std::move(neighbor);
                base_energy = neighbor_energy;
            }

            if (energy_diffs.empty()) {
                return 1.0;  // Default if no uphill moves found
            }

            // Calculate median energy difference
            std::sort(energy_diffs.begin(), energy_diffs.end());
            double median_diff = energy_diffs[energy_diffs.size() / 2];

            // T = -median_diff / ln(p)
            return -median_diff / std::log(desired_acceptance_rate);
        },
        py::arg("tsp"), py::arg("desired_acceptance_rate") = 0.8,
        py::arg("samples") = 100,
        R"pbdoc(
        Estimate a good initial temperature for the given TSP instance.

        This function samples random moves in the solution space and calculates
        a temperature that would accept uphill moves with the desired probability.

        Args:
            tsp: The TSP problem instance
            desired_acceptance_rate: Initial probability of accepting uphill moves (default: 0.8)
            samples: Number of random moves to sample (default: 100)

        Returns:
            Estimated initial temperature
       )pbdoc");

    // Add a function to generate random TSP instances
    m.def(
        "generate_random_tsp",
        [](int num_cities, double min_coord = 0.0, double max_coord = 100.0) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<double> dist(min_coord, max_coord);

            std::vector<std::pair<double, double>> cities;
            cities.reserve(num_cities);

            for (int i = 0; i < num_cities; ++i) {
                cities.emplace_back(dist(gen), dist(gen));
            }

            return cities;
        },
        py::arg("num_cities"), py::arg("min_coord") = 0.0,
        py::arg("max_coord") = 100.0,
        R"pbdoc(
        Generate a random TSP instance.

        This function creates a random set of city coordinates that can be
        used to initialize a TSP problem.

        Args:
            num_cities: Number of cities to generate
            min_coord: Minimum coordinate value (default: 0.0)
            max_coord: Maximum coordinate value (default: 100.0)

        Returns:
            List of (x,y) coordinates for the generated cities
       )pbdoc");

    // Add a benchmark function
    m.def(
        "benchmark_strategies",
        [](int num_cities = 20, int num_runs = 5) {
            // Generate a random TSP instance
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<double> dist(0.0, 100.0);

            std::vector<std::pair<double, double>> cities;
            cities.reserve(num_cities);

            for (int i = 0; i < num_cities; ++i) {
                cities.emplace_back(dist(gen), dist(gen));
            }

            TSP tsp(cities);

            // Run benchmarks with different cooling strategies
            std::vector<std::pair<std::string, AnnealingStrategy>> strategies =
                {{"Linear", AnnealingStrategy::LINEAR},
                 {"Exponential", AnnealingStrategy::EXPONENTIAL},
                 {"Logarithmic", AnnealingStrategy::LOGARITHMIC},
                 {"Geometric", AnnealingStrategy::GEOMETRIC},
                 {"Quadratic", AnnealingStrategy::QUADRATIC},
                 {"Hyperbolic", AnnealingStrategy::HYPERBOLIC},
                 {"Adaptive", AnnealingStrategy::ADAPTIVE}};

            std::vector<std::tuple<std::string, double, double>> results;

            for (const auto& [name, strategy] : strategies) {
                std::vector<double> tour_lengths;

                auto start_time = std::chrono::high_resolution_clock::now();

                for (int run = 0; run < num_runs; ++run) {
                    auto builder = TspAnnealing::Builder(tsp)
                                       .setInitialTemperature(100.0)
                                       .setMaxIterations(1000)
                                       .setCoolingStrategy(strategy)
                                       .setCoolingRate(0.95);

                    // annealing_bindings.cpp (continued)
                    auto annealing = builder.build();
                    auto solution = annealing.optimize(
                        1);  // Using single thread for consistent benchmark

                    tour_lengths.push_back(tsp.energy(solution));
                }

                auto end_time = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> elapsed = end_time - start_time;

                // Calculate average tour length and total time
                double avg_length = std::accumulate(tour_lengths.begin(),
                                                    tour_lengths.end(), 0.0) /
                                    tour_lengths.size();
                double time_seconds = elapsed.count();

                results.emplace_back(name, avg_length, time_seconds);
            }

            return results;
        },
        py::arg("num_cities") = 20, py::arg("num_runs") = 5,
        R"pbdoc(
      Benchmark different cooling strategies for TSP.
      
      This function runs the simulated annealing algorithm with different
      cooling strategies and reports the average tour length and execution time.
      
      Args:
          num_cities: Number of cities in the random TSP instance (default: 20)
          num_runs: Number of runs per strategy (default: 5)
          
      Returns:
          List of (strategy_name, avg_tour_length, execution_time) tuples
     )pbdoc");

    // Helper to visualize a TSP tour
    m.def(
        "plot_tour",
        [](const std::vector<std::pair<double, double>>& cities,
           const std::vector<int>& tour) {
            // Check if matplotlib is available
            py::object plt = py::module::import("matplotlib.pyplot");

            // Extract city coordinates in tour order
            std::vector<double> x_coords, y_coords;
            for (int city_idx : tour) {
                x_coords.push_back(cities[city_idx].first);
                y_coords.push_back(cities[city_idx].second);
            }

            // Add the first city again to close the loop
            if (!tour.empty()) {
                x_coords.push_back(cities[tour[0]].first);
                y_coords.push_back(cities[tour[0]].second);
            }

            // Plot the tour
            plt.attr("figure")();
            plt.attr("plot")(x_coords, y_coords, "b-o");

            // Add city labels
            for (size_t i = 0; i < cities.size(); ++i) {
                plt.attr("text")(cities[i].first, cities[i].second,
                                 std::to_string(i), py::arg("fontsize") = 12);
            }

            plt.attr("title")("TSP Tour");
            plt.attr("xlabel")("X");
            plt.attr("ylabel")("Y");
            plt.attr("grid")(true);
            plt.attr("show")();
        },
        py::arg("cities"), py::arg("tour"),
        R"pbdoc(
      Visualize a TSP tour using matplotlib.
      
      This function plots the cities and the tour path connecting them.
      
      Args:
          cities: List of (x,y) coordinates for each city
          tour: List of city indices representing the tour
          
      Note:
          This function requires matplotlib to be installed
     )pbdoc");

    // Add a function to compute tour distance
    m.def(
        "compute_tour_length",
        [](const std::vector<std::pair<double, double>>& cities,
           const std::vector<int>& tour) {
            if (tour.empty())
                return 0.0;

            double total_distance = 0.0;
            for (size_t i = 0; i < tour.size(); ++i) {
                size_t from = tour[i];
                size_t to = tour[(i + 1) % tour.size()];

                double dx = cities[from].first - cities[to].first;
                double dy = cities[from].second - cities[to].second;
                total_distance += std::sqrt(dx * dx + dy * dy);
            }

            return total_distance;
        },
        py::arg("cities"), py::arg("tour"),
        R"pbdoc(
      Compute the total length of a TSP tour.
      
      This is a convenience function to calculate the total distance
      of a tour without creating a TSP instance.
      
      Args:
          cities: List of (x,y) coordinates for each city
          tour: List of city indices representing the tour
          
      Returns:
          The total distance of the tour
     )pbdoc");

    // Add a greedy heuristic for TSP
    m.def(
        "greedy_tsp",
        [](const std::vector<std::pair<double, double>>& cities,
           int start_city = 0) {
            if (cities.empty())
                return std::vector<int>();

            std::vector<int> tour;
            std::vector<bool> visited(cities.size(), false);

            // Start with the given city
            int current = start_city % cities.size();
            tour.push_back(current);
            visited[current] = true;

            // While there are unvisited cities, find the closest one
            while (tour.size() < cities.size()) {
                int closest = -1;
                double min_dist = std::numeric_limits<double>::max();

                for (size_t i = 0; i < cities.size(); ++i) {
                    if (visited[i])
                        continue;

                    double dx = cities[current].first - cities[i].first;
                    double dy = cities[current].second - cities[i].second;
                    double dist = std::sqrt(dx * dx + dy * dy);

                    if (dist < min_dist) {
                        min_dist = dist;
                        closest = i;
                    }
                }

                current = closest;
                tour.push_back(current);
                visited[current] = true;
            }

            return tour;
        },
        py::arg("cities"), py::arg("start_city") = 0,
        R"pbdoc(
      Generate a TSP tour using a greedy nearest neighbor heuristic.
      
      This function builds a tour by always choosing the closest unvisited city.
      
      Args:
          cities: List of (x,y) coordinates for each city
          start_city: Index of the starting city (default: 0)
          
      Returns:
          A tour constructed using the nearest neighbor heuristic
     )pbdoc");

    // Add a function for 2-opt local improvement
    m.def(
        "two_opt_improvement",
        [](const std::vector<std::pair<double, double>>& cities,
           std::vector<int> tour, int max_iterations = 1000) {
            if (tour.size() <= 3)
                return tour;  // Can't improve tours with 3 or fewer cities

            bool improved = true;
            int iterations = 0;
            double best_distance = compute_tour_length(cities, tour);

            while (improved && iterations < max_iterations) {
                improved = false;
                iterations++;

                for (size_t i = 0; i < tour.size() - 2; i++) {
                    for (size_t j = i + 2; j < tour.size(); j++) {
                        // Skip adjacent edges
                        if (i == 0 && j == tour.size() - 1)
                            continue;

                        // Try reversing the segment between i+1 and j
                        std::reverse(tour.begin() + i + 1,
                                     tour.begin() + j + 1);

                        double new_distance = compute_tour_length(cities, tour);

                        if (new_distance < best_distance) {
                            // Keep the improvement
                            best_distance = new_distance;
                            improved = true;
                            break;  // Move to the next i
                        } else {
                            // Revert the change
                            std::reverse(tour.begin() + i + 1,
                                         tour.begin() + j + 1);
                        }
                    }
                    if (improved)
                        break;  // Start over with the improved tour
                }
            }

            return tour;
        },
        py::arg("cities"), py::arg("tour"), py::arg("max_iterations") = 1000,
        R"pbdoc(
      Improve a TSP tour using the 2-opt local search heuristic.
      
      This algorithm iteratively removes two edges and reconnects the tour in the
      other possible way, keeping the change if it improves the tour length.
      
      Args:
          cities: List of (x,y) coordinates for each city
          tour: Initial tour to improve
          max_iterations: Maximum number of improvement iterations
          
      Returns:
          An improved tour
     )pbdoc");

    // Add a function to help users create their own problem types
    m.def(
        "create_problem_template",
        []() {
            return R"code(
# Template for creating a custom problem for simulated annealing
import random
from typing import List, Tuple, Any, Callable

class CustomProblem:
  """
  Example custom problem implementation compatible with the C++ AnnealingProblem concept.
  Replace with your own problem definition.
  """
  
  def __init__(self, problem_data: Any):
      """Initialize your problem with specific data"""
      self.problem_data = problem_data
  
  def energy(self, solution: Any) -> float:
      """
      Calculate the objective function value (energy) of a solution.
      Lower values are better.
      """
      # Replace with your actual objective function
      return 0.0
  
  def neighbor(self, solution: Any) -> Any:
      """Generate a slightly modified neighboring solution"""
      # Replace with your neighbor generation logic
      return solution
  
  def random_solution(self) -> Any:
      """Generate a random initial solution"""
      # Replace with code to generate a valid random solution
      return None
  
  def validate(self, solution: Any) -> bool:
      """Check if a solution is valid"""
      # Replace with your validation logic
      return True

# Example usage with the atom.algorithm.annealing module:
def solve_custom_problem():
  from atom.algorithm.annealing import SimulatedAnnealing, AnnealingStrategy
  
  # Create your problem instance
  problem = CustomProblem(your_problem_data)
  
  # Set up the annealing solver
  annealing = SimulatedAnnealing(problem)
  annealing.set_max_iterations(10000)
  annealing.set_initial_temperature(100.0)
  annealing.set_cooling_strategy(AnnealingStrategy.EXPONENTIAL)
  
  # Run the optimization
  best_solution = annealing.optimize()
  
  return best_solution
)code";
        },
        R"pbdoc(
      Provides a Python template for creating custom problem types.
      
      This function returns a string containing Python code that shows
      how to create a custom problem compatible with the simulated annealing
      algorithm interface.
      
      Returns:
          Python code template as a string
     )pbdoc");
}
