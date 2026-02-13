# Optimization and Search Algorithms

This directory contains algorithms for optimization problems and pathfinding.

## Contents

### Simulated Annealing (`annealing.hpp` — aggregation header)

- **`annealing/annealing_concept.hpp`** — `AnnealingProblem` concept and `AnnealingStrategy` enum
- **`annealing/cooling_schedule.hpp`** — Cooling schedule factory function (`createCoolingSchedule`)
- **`annealing/simulated_annealing.hpp`** — `SimulatedAnnealing` template class with Builder pattern
- **`annealing/tsp.hpp`** — TSP (Traveling Salesman Problem) example implementation

### Pathfinding (`pathfinding.hpp` — aggregation header)

- **`pathfinding/point.hpp`** — `Point` struct with arithmetic operators and `std::hash` specialization
- **`pathfinding/graph.hpp`** — `IGraph` abstract interface and `Graph` concept
- **`pathfinding/heuristics.hpp/.cpp`** — Heuristic concept and functions (Manhattan, Euclidean, diagonal, octile, zero)
- **`pathfinding/grid_map.hpp/.cpp`** — `GridMap` class with terrain types and JPS support
- **`pathfinding/pathfinder.hpp/.cpp`** — `PathFinder` class with A\*, Dijkstra, bidirectional search, JPS, path smoothing, funnel algorithm

## Features

### Simulated Annealing

- **Multiple Cooling Strategies**: Linear, exponential, logarithmic, geometric, adaptive
- **Generic Problem Interface**: Works with any problem type satisfying the concept
- **Configurable Parameters**: Temperature schedules, iteration limits, convergence criteria
- **Modern C++ Design**: Uses concepts and templates for type safety

### Pathfinding Algorithms

- **A\* Search**: Optimal pathfinding with heuristic guidance
- **Dijkstra's Algorithm**: Guaranteed shortest path without heuristics
- **Bidirectional Search**: Search from both start and goal simultaneously
- **Jump Point Search (JPS)**: Optimized A\* for grid-based pathfinding
- **Multiple Heuristics**: Manhattan, Euclidean, diagonal, octile distance

## Optimization Features

### Simulated Annealing

- **Adaptive Cooling**: Automatically adjusts temperature based on acceptance rates
- **Convergence Detection**: Stops early when solution quality stabilizes
- **Parallel Evaluation**: Multi-threaded neighbor evaluation when possible
- **Statistics Tracking**: Detailed optimization progress monitoring

### Pathfinding

- **Grid Optimization**: Specialized optimizations for grid-based maps
- **Path Smoothing**: Post-processing to create more natural paths
- **Dynamic Obstacles**: Support for changing environments
- **Memory Efficient**: Optimized data structures for large search spaces

## Use Cases

### Simulated Annealing

- **Traveling Salesman Problem**: Route optimization
- **Scheduling**: Task and resource allocation
- **Circuit Design**: Component placement optimization
- **Machine Learning**: Hyperparameter tuning
- **Engineering Design**: Parameter optimization

### Pathfinding

- **Game Development**: NPC movement and AI navigation
- **Robotics**: Robot path planning and navigation
- **GPS Navigation**: Route finding in road networks
- **Network Routing**: Optimal packet routing
- **Logistics**: Delivery route optimization

## Usage Examples

```cpp
#include "atom/algorithm/optimization/annealing.hpp"
#include "atom/algorithm/optimization/pathfinding.hpp"

// Simulated annealing
MyProblem problem;  // Must satisfy AnnealingProblem concept
auto solution = atom::algorithm::simulatedAnnealing(
    problem,
    1000.0,  // initial temperature
    0.01,    // final temperature
    0.95,    // cooling rate
    atom::algorithm::AnnealingStrategy::EXPONENTIAL
);

// Pathfinding
atom::algorithm::GridMap map(width, height);
atom::algorithm::PathFinder pathfinder;
auto path = pathfinder.findPath(
    map,
    {start_x, start_y},
    {goal_x, goal_y},
    atom::algorithm::AlgorithmType::AStar,
    atom::algorithm::HeuristicType::Euclidean
);
```

## Algorithm Details

### Simulated Annealing

- Accepts worse solutions with probability based on temperature
- Temperature decreases according to cooling schedule
- Balances exploration vs exploitation automatically
- Converges to global optimum with proper parameters

### Pathfinding

- A\* uses f(n) = g(n) + h(n) evaluation function
- Dijkstra guarantees optimal paths without heuristics
- JPS reduces node expansions by jumping over symmetric paths
- Bidirectional search can reduce search space significantly

## Dependencies

- Core algorithm components
- Standard C++ library (C++20)
- spdlog for logging and debugging
- Optional: TBB for parallel processing
