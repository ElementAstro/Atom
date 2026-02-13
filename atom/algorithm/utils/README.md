# Utility Algorithms and Helpers

This directory contains miscellaneous utility algorithms and helper functions that don't fit into other specific categories.

## Contents

### Filename Matching

- **`fnmatch.hpp/cpp`** - Filename pattern matching with glob-style wildcards

### Snowflake ID Generation

- **`snowflake.hpp`** - Distributed unique ID generation using the Snowflake algorithm (aggregate)
- **`snowflake_exception.hpp`** - Exception classes and lock types for Snowflake

### UUID Generation

- **`uuid.hpp`** - UUID generation and manipulation (v1, v4, v5)

### Weighted Selection & Sampling

- **`weight.hpp`** - Aggregate header for all weight-related components
- **`weight_common.hpp`** - `WeightType` concept and `WeightError` exception
- **`weight_collection.hpp`** - Thread-safe key-value weight collection (`WeightCollection<T>`)
- **`weight_strategy.hpp`** - Selection strategy classes (`DefaultSelectionStrategy`, `BottomHeavySelectionStrategy`, `RandomSelectionStrategy`, `TopHeavySelectionStrategy`, `PowerLawSelectionStrategy`)
- **`weight_sampler.hpp`** - Batch weighted random sampling (`WeightedRandomSampler<T>`)
- **`weight_selector.hpp`** - Core weight selector with strategy pattern (`WeightSelector<T>`)

### Error Calibration

- **`error_calibration.hpp`** - Error analysis and calibration utilities (aggregate)
- **`linear_solver.hpp`** - Gaussian elimination linear system solver
- **`levenberg_marquardt.hpp`** - Levenberg-Marquardt nonlinear least-squares optimizer
- **`async_calibration.hpp`** - Coroutine-based asynchronous calibration support

## Features

### Filename Matching

- **Glob Patterns**: Support for `*`, `?`, and `[...]` wildcards
- **Case Sensitivity**: Configurable case-sensitive/insensitive matching
- **Path Handling**: Proper handling of directory separators
- **Unicode Support**: Works with UTF-8 encoded filenames
- **Performance Optimized**: Efficient pattern matching algorithms

### Snowflake ID Generation

- **Distributed IDs**: Unique IDs across multiple machines/processes
- **Time-Ordered**: IDs are roughly time-ordered for better database performance
- **Configurable**: Customizable epoch, worker ID, and datacenter ID
- **Thread-Safe**: Concurrent ID generation without conflicts
- **High Throughput**: Capable of generating millions of IDs per second

### Weighted Sampling

- **Multiple Algorithms**: Reservoir sampling, alias method, binary search
- **Dynamic Weights**: Support for changing weights during sampling
- **Memory Efficient**: Optimized for large weight distributions
- **Statistical Quality**: High-quality random number generation
- **Parallel Sampling**: Multi-threaded sampling for large datasets

### Error Calibration

- **Numerical Analysis**: Error propagation and uncertainty quantification
- **Calibration Curves**: Generate calibration data for numerical methods
- **Statistical Validation**: Validate algorithm accuracy and precision
- **Benchmark Support**: Performance and accuracy benchmarking utilities
- **Visualization**: Generate data for error analysis plots

## Use Cases

### Filename Matching

- **File System Operations**: Find files matching patterns
- **Configuration**: Pattern-based configuration file selection
- **Build Systems**: Source file discovery and filtering
- **Backup Tools**: Include/exclude file patterns
- **Shell Utilities**: Command-line file processing tools

### Snowflake IDs

- **Distributed Databases**: Unique primary keys across shards
- **Microservices**: Service-independent ID generation
- **Event Logging**: Ordered event identifiers
- **Message Queues**: Unique message identifiers
- **Real-Time Systems**: High-throughput ID generation

### Weighted Sampling

- **Machine Learning**: Weighted dataset sampling
- **Game Development**: Probability-based item generation
- **Simulation**: Monte Carlo sampling with custom distributions
- **A/B Testing**: Weighted traffic distribution
- **Load Balancing**: Weighted server selection

### Error Calibration

- **Scientific Computing**: Validate numerical algorithm accuracy
- **Financial Modeling**: Risk assessment and error bounds
- **Engineering Simulation**: Uncertainty quantification
- **Quality Assurance**: Algorithm validation and testing
- **Performance Tuning**: Identify accuracy vs performance trade-offs

## Usage Examples

```cpp
#include "atom/algorithm/utils/fnmatch.hpp"
#include "atom/algorithm/utils/snowflake.hpp"
#include "atom/algorithm/utils/weight.hpp"

// Filename pattern matching
bool matches = atom::algorithm::fnmatch("*.cpp", "example.cpp");  // true
bool case_insensitive = atom::algorithm::fnmatch("*.CPP", "example.cpp",
                                                 FNM_CASEFOLD);

// Snowflake ID generation
atom::algorithm::Snowflake<1640995200000> generator(1, 1);  // worker=1, datacenter=1
auto unique_id = generator.nextId();

// Weighted sampling
std::vector<double> weights = {0.1, 0.3, 0.4, 0.2};
atom::algorithm::WeightedSampler sampler(weights);
auto selected_index = sampler.sample();
```

## Algorithm Details

### Filename Matching

- Uses finite state automaton for efficient pattern matching
- Supports POSIX fnmatch semantics with extensions
- Optimized for common patterns like `*.ext`
- Handles edge cases like escaped characters

### Snowflake Algorithm

- 64-bit IDs: 1 bit sign + 41 bits timestamp + 10 bits machine + 12 bits sequence
- Configurable epoch reduces timestamp bits needed
- Automatic sequence number management
- Clock drift protection and handling

### Weighted Sampling

- **Alias Method**: O(1) sampling after O(n) preprocessing
- **Binary Search**: O(log n) sampling with O(n) space
- **Reservoir Sampling**: For streaming data with unknown size
- **Adaptive**: Automatically selects best algorithm based on usage pattern

## Performance Notes

- Filename matching is optimized for common glob patterns
- Snowflake generation can achieve >1M IDs/second per thread
- Weighted sampling algorithms are chosen based on usage patterns
- Error calibration utilities are designed for batch processing

## Dependencies

- Core algorithm components
- Standard C++ library (C++20)
- atom/utils for random number generation
- Optional: Boost for additional random distributions
- Optional: TBB for parallel processing
