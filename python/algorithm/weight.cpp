#include "atom/algorithm/weight.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <sstream>

namespace py = pybind11;

// Enumeration for selection strategies
enum class SelectionStrategyType {
    DEFAULT,
    BOTTOM_HEAVY,
    TOP_HEAVY,
    RANDOM,
    POWER_LAW
};

// Helper template for creating the appropriate strategy
template <typename T>
std::unique_ptr<typename atom::algorithm::WeightSelector<T>::SelectionStrategy>
create_strategy(SelectionStrategyType type, uint32_t seed = 0,
                T exponent = 2.0) {
    using namespace atom::algorithm;
    using WST = WeightSelector<T>;

    switch (type) {
        case SelectionStrategyType::DEFAULT:
            return std::make_unique<typename WST::DefaultSelectionStrategy>(
                seed);
        case SelectionStrategyType::BOTTOM_HEAVY:
            return std::make_unique<typename WST::BottomHeavySelectionStrategy>(
                seed);
        case SelectionStrategyType::TOP_HEAVY:
            return std::make_unique<typename WST::TopHeavySelectionStrategy>(
                seed);
        case SelectionStrategyType::RANDOM:
            return std::make_unique<typename WST::RandomSelectionStrategy>(
                0, seed);
        case SelectionStrategyType::POWER_LAW:
            return std::make_unique<typename WST::PowerLawSelectionStrategy>(
                exponent, seed);
        default:
            return std::make_unique<typename WST::DefaultSelectionStrategy>(
                seed);
    }
}

// Template for declaring WeightSelector with different numeric types
template <typename T>
void declare_weight_selector(py::module& m, const std::string& type_name) {
    using namespace atom::algorithm;
    using WeightSelectorT = WeightSelector<T>;
    using SamplerT = typename WeightSelector<T>::WeightedRandomSampler;

    std::string class_name = "WeightSelector" + type_name;

    // Create Python enum for selection strategies
    py::enum_<SelectionStrategyType>(m,
                                     ("SelectionStrategy" + type_name).c_str())
        .value("DEFAULT", SelectionStrategyType::DEFAULT)
        .value("BOTTOM_HEAVY", SelectionStrategyType::BOTTOM_HEAVY)
        .value("TOP_HEAVY", SelectionStrategyType::TOP_HEAVY)
        .value("RANDOM", SelectionStrategyType::RANDOM)
        .value("POWER_LAW", SelectionStrategyType::POWER_LAW)
        .export_values();

    // Define the Python class for WeightSelector
    py::class_<WeightSelectorT>(
        m, class_name.c_str(),
        R"(Core weight selection class with multiple selection strategies.

This class provides methods for weighted random selection with different probability distributions.

Args:
    weights: Initial weights for selection
    seed: Optional random seed
    strategy: Selection strategy to use (default is DEFAULT)
    exponent: Power law exponent when using POWER_LAW strategy (default is 2.0)

Examples:
    >>> from atom.algorithm.weight import WeightSelectorFloat, SelectionStrategyFloat
    >>> 
    >>> # Create a selector with default strategy
    >>> selector = WeightSelectorFloat([1.0, 2.0, 3.0, 4.0])
    >>> 
    >>> # Select an index based on weights
    >>> selected_index = selector.select()
    >>>
    >>> # Use bottom-heavy distribution (favors lower weights)
    >>> selector2 = WeightSelectorFloat([1.0, 2.0, 3.0, 4.0], 
    >>>                                strategy=SelectionStrategyFloat.BOTTOM_HEAVY)
    >>>
    >>> # Multiple selections without replacement
    >>> indices = selector.select_unique_multiple(2)
        )")

        // Constructors
        .def(py::init([](const std::vector<T>& weights, uint32_t seed,
                         SelectionStrategyType strategy, T exponent) {
                 auto strat = create_strategy<T>(strategy, seed, exponent);
                 return WeightSelectorT(weights, seed, std::move(strat));
             }),
             py::arg("weights"), py::arg("seed") = 0,
             py::arg("strategy") = SelectionStrategyType::DEFAULT,
             py::arg("exponent") = static_cast<T>(2.0),
             "Constructs a WeightSelector with given weights and strategy.")

        // Core selection methods
        .def("select", &WeightSelectorT::select,
             "Selects an index based on weights using the current strategy.")

        .def("select_multiple", &WeightSelectorT::selectMultiple, py::arg("n"),
             R"(Selects multiple indices based on weights.

Args:
    n: Number of selections to make

Returns:
    List of selected indices
)")

        .def(
            "select_unique_multiple", &WeightSelectorT::selectUniqueMultiple,
            py::arg("n"),
            R"(Selects multiple unique indices based on weights (without replacement).

Args:
    n: Number of selections to make

Returns:
    List of unique selected indices

Raises:
    ValueError: If n > number of weights
)")

        // Weight manipulation methods
        .def("update_weight", &WeightSelectorT::updateWeight, py::arg("index"),
             py::arg("new_weight"),
             R"(Updates a single weight.

Args:
    index: Index of the weight to update
    new_weight: New weight value

Raises:
    IndexError: If index is out of bounds
    ValueError: If new_weight is negative
)")

        .def("add_weight", &WeightSelectorT::addWeight, py::arg("new_weight"),
             R"(Adds a new weight to the collection.

Args:
    new_weight: Weight to add

Raises:
    ValueError: If new_weight is negative
)")

        .def("remove_weight", &WeightSelectorT::removeWeight, py::arg("index"),
             R"(Removes a weight at the specified index.

Args:
    index: Index of the weight to remove

Raises:
    IndexError: If index is out of bounds
)")

        .def("normalize_weights", &WeightSelectorT::normalizeWeights,
             R"(Normalizes weights so they sum to 1.0.

Raises:
    ValueError: If all weights are zero
)")

        .def(
            "apply_function_to_weights",
            [](WeightSelectorT& self, py::function func) {
                self.applyFunctionToWeights([func](T weight) -> T {
                    py::gil_scoped_acquire acquire;
                    return py::cast<T>(func(weight));
                });
            },
            py::arg("func"),
            R"(Applies a function to all weights.

Args:
    func: Function that takes a weight value and returns a new weight value

Raises:
    ValueError: If resulting weights are negative
             
Examples:
    >>> # Double all weights
    >>> selector.apply_function_to_weights(lambda w: w * 2)
)")

        .def("batch_update_weights", &WeightSelectorT::batchUpdateWeights,
             py::arg("updates"),
             R"(Updates multiple weights in batch.

Args:
    updates: List of (index, new_weight) pairs

Raises:
    IndexError: If any index is out of bounds
    ValueError: If any new weight is negative
)")

        // Weight query methods
        .def("get_weight", &WeightSelectorT::getWeight, py::arg("index"),
             R"(Gets the weight at the specified index.

Args:
    index: Index of the weight to retrieve

Returns:
    Weight value or None if index is out of bounds
)")

        .def("get_max_weight_index", &WeightSelectorT::getMaxWeightIndex,
             R"(Gets the index of the maximum weight.

Returns:
    Index of the maximum weight

Raises:
    ValueError: If weights collection is empty
)")

        .def("get_min_weight_index", &WeightSelectorT::getMinWeightIndex,
             R"(Gets the index of the minimum weight.

Returns:
    Index of the minimum weight

Raises:
    ValueError: If weights collection is empty
)")

        .def("size", &WeightSelectorT::size, "Gets the number of weights.")

        .def("get_weights", &WeightSelectorT::getWeights,
             "Gets a copy of the weights.")

        .def("get_total_weight", &WeightSelectorT::getTotalWeight,
             "Gets the sum of all weights.")

        .def(
            "reset_weights",
            [](WeightSelectorT& self, const std::vector<T>& new_weights) {
                self.resetWeights(new_weights);
            },
            py::arg("new_weights"),
            R"(Replaces all weights with new values.

Args:
    new_weights: New weights collection

Raises:
    ValueError: If any weight is negative
)")

        .def("scale_weights", &WeightSelectorT::scaleWeights, py::arg("factor"),
             R"(Multiplies all weights by a factor.

Args:
    factor: Scaling factor

Raises:
    ValueError: If factor is negative
)")

        .def("get_average_weight", &WeightSelectorT::getAverageWeight,
             R"(Calculates the average of all weights.

Returns:
    Average weight

Raises:
    ValueError: If weights collection is empty
)")

        .def(
            "print_weights",
            [](const WeightSelectorT& self) {
                std::ostringstream oss;
                self.printWeights(oss);
                return oss.str();
            },
            "Returns a string representation of the weights.")

        .def("set_seed", &WeightSelectorT::setSeed, py::arg("seed"),
             "Sets the random seed for selection strategies.")

        .def("clear", &WeightSelectorT::clear, "Clears all weights.")

        .def("reserve", &WeightSelectorT::reserve, py::arg("capacity"),
             "Reserves space for weights.")

        .def("empty", &WeightSelectorT::empty,
             "Checks if the weights collection is empty.")

        .def("get_max_weight", &WeightSelectorT::getMaxWeight,
             R"(Gets the weight with the maximum value.

Returns:
    Maximum weight value

Raises:
    ValueError: If weights collection is empty
)")

        .def("get_min_weight", &WeightSelectorT::getMinWeight,
             R"(Gets the weight with the minimum value.

Returns:
    Minimum weight value

Raises:
    ValueError: If weights collection is empty
)")

        .def(
            "find_indices",
            [](const WeightSelectorT& self, py::function predicate) {
                return self.findIndices([predicate](T weight) -> bool {
                    py::gil_scoped_acquire acquire;
                    return py::cast<bool>(predicate(weight));
                });
            },
            py::arg("predicate"),
            R"(Finds indices of weights matching a predicate.

Args:
    predicate: Function that takes a weight and returns a boolean

Returns:
    List of indices where predicate returns true

Examples:
    >>> # Find indices of weights greater than 2.0
    >>> indices = selector.find_indices(lambda w: w > 2.0)
)")

        // Python-specific methods
        .def("__len__", &WeightSelectorT::size)
        .def("__bool__",
             [](const WeightSelectorT& self) { return !self.empty(); })
        .def("__getitem__",
             [](const WeightSelectorT& self, size_t index) {
                 auto weight = self.getWeight(index);
                 if (!weight.has_value()) {
                     throw py::index_error("Index out of range");
                 }
                 return weight.value();
             })
        .def("__str__", [](const WeightSelectorT& self) {
            std::ostringstream oss;
            self.printWeights(oss);
            return oss.str();
        });

    // Create WeightedRandomSampler class
    py::class_<SamplerT>(m, ("WeightedRandomSampler" + type_name).c_str(),
                         R"(Utility class for batch sampling with replacement.

This class provides methods for weighted random sampling with or without replacement.

Args:
    seed: Optional random seed for reproducible sampling
        
Examples:
    >>> from atom.algorithm.weight import WeightedRandomSamplerFloat
    >>> 
    >>> sampler = WeightedRandomSamplerFloat(seed=42)
    >>> 
    >>> # Sample 3 indices with replacement
    >>> indices1 = sampler.sample([1.0, 2.0, 3.0, 4.0], 3)
    >>> 
    >>> # Sample 2 unique indices (no replacement)
    >>> indices2 = sampler.sample_unique([1.0, 2.0, 3.0, 4.0], 2)
        )")
        .def(py::init<>())
        .def(py::init<uint32_t>(), py::arg("seed"))
        .def("sample", &SamplerT::sample, py::arg("weights"), py::arg("n"),
             R"(Sample n indices according to their weights.

Args:
    weights: The weights for each index
    n: Number of samples to draw

Returns:
    List of sampled indices
        
Raises:
    ValueError: If weights is empty
)")
        .def(
            "sample_unique", &SamplerT::sampleUnique, py::arg("weights"),
            py::arg("n"),
            R"(Sample n unique indices according to their weights (no replacement).

Args:
    weights: The weights for each index
    n: Number of samples to draw

Returns:
    List of sampled indices
        
Raises:
    ValueError: If n is greater than the number of weights or if weights is empty
)");
}

// Exception translator for WeightError
void register_exceptions(py::module& m) {
    py::register_exception<atom::algorithm::WeightError>(m, "WeightError");

    // Add general exception translation
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::algorithm::WeightError& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::out_of_range& e) {
            PyErr_SetString(PyExc_IndexError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });
}

PYBIND11_MODULE(weight, m) {
    m.doc() = R"pbdoc(
        Weighted Random Selection Algorithms
        -----------------------------------

        This module provides flexible weighted random selection algorithms with 
        multiple probability distributions and thread-safe operations.
        
        The module includes:
          - Various selection strategies (uniform, bottom-heavy, top-heavy, etc.)
          - Methods for selecting with and without replacement
          - Thread-safe weight updates and manipulations
          - Utilities for normalizing and transforming weights
          - Detailed statistics and weight information
          
        Example:
            >>> from atom.algorithm import weight
            >>> 
            >>> # Create a selector with weights
            >>> selector = weight.WeightSelectorFloat([1.0, 2.0, 3.0, 4.0])
            >>> 
            >>> # Select an index based on weights
            >>> selected_idx = selector.select()
            >>> print(selected_idx)
            
            >>> # Select using a bottom-heavy distribution
            >>> selector2 = weight.WeightSelectorFloat(
            >>>     [1.0, 2.0, 3.0, 4.0], 
            >>>     strategy=weight.SelectionStrategyFloat.BOTTOM_HEAVY
            >>> )
            >>> 
            >>> # Select multiple unique indices
            >>> indices = selector.select_unique_multiple(2)
    )pbdoc";

    // Register exceptions
    register_exceptions(m);

    // Register WeightSelector with different numeric types
    declare_weight_selector<float>(m, "Float");
    declare_weight_selector<double>(m, "Double");
    declare_weight_selector<int>(m, "Int");

    // Add factory function for creating appropriate selector based on input
    // type
    m.def(
        "create_selector",
        [m](const py::object& weights, uint32_t seed,
            SelectionStrategyType strategy, double exponent) {
            if (py::isinstance<py::list>(weights)) {
                // Check the type of the first element to determine which
                // WeightSelector to create
                py::list weight_list = weights.cast<py::list>();
                if (weight_list.size() > 0) {
                    py::object first = weight_list[0];

                    if (py::isinstance<py::int_>(first)) {
                        std::vector<int> int_weights =
                            weights.cast<std::vector<int>>();
                        auto strat = create_strategy<int>(
                            strategy, seed, static_cast<int>(exponent));
                        return py::cast(atom::algorithm::WeightSelector<int>(
                            int_weights, seed, std::move(strat)));
                    } else if (py::isinstance<py::float_>(first)) {
                        std::vector<double> double_weights =
                            weights.cast<std::vector<double>>();
                        auto strat =
                            create_strategy<double>(strategy, seed, exponent);
                        return py::cast(atom::algorithm::WeightSelector<double>(
                            double_weights, seed, std::move(strat)));
                    }
                }

                // Default to float weights if list is empty or type is unclear
                auto strat = create_strategy<float>(
                    strategy, seed, static_cast<float>(exponent));
                return py::cast(atom::algorithm::WeightSelector<float>(
                    std::vector<float>(), seed, std::move(strat)));
            }

            // Default to float weights for any other input type
            try {
                std::vector<float> float_weights =
                    weights.cast<std::vector<float>>();
                auto strat = create_strategy<float>(
                    strategy, seed, static_cast<float>(exponent));
                return py::cast(atom::algorithm::WeightSelector<float>(
                    float_weights, seed, std::move(strat)));
            } catch (const py::error_already_set&) {
                throw py::type_error(
                    "Weights must be a list of numeric values");
            }
        },
        py::arg("weights"), py::arg("seed") = 0,
        py::arg("strategy") = SelectionStrategyType::DEFAULT,
        py::arg("exponent") = 2.0,
        R"(Factory function to create a WeightSelector with the appropriate numeric type.

The function automatically selects the WeightSelector type based on the input weights.

Args:
    weights: List of weights (int, float, or double)
    seed: Random seed for reproducible selections (default: 0)
    strategy: Selection strategy to use (default: DEFAULT)
    exponent: Power law exponent when using POWER_LAW strategy (default: 2.0)

Returns:
    A WeightSelector instance with the appropriate type

Examples:
    >>> # Create a selector with integer weights
    >>> int_selector = weight.create_selector([1, 2, 3, 4])
    >>> 
    >>> # Create a selector with float weights and power law distribution
    >>> float_selector = weight.create_selector(
    >>>     [1.0, 2.0, 3.0, 4.0],
    >>>     strategy=weight.SelectionStrategyFloat.POWER_LAW,
    >>>     exponent=1.5
    >>> )
)");
}