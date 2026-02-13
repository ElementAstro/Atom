#ifndef ATOM_ALGORITHM_UTILS_WEIGHT_HPP
#define ATOM_ALGORITHM_UTILS_WEIGHT_HPP

/**
 * @file weight.hpp
 * @brief Aggregate header for weight selection and sampling algorithms.
 *
 * This header includes all weight-related components:
 * - weight_common.hpp: WeightType concept and WeightError exception
 * - weight_collection.hpp: Thread-safe key-value weight collection
 * - weight_strategy.hpp: Selection strategy classes
 * - weight_sampler.hpp: Weighted random sampling utilities
 * - weight_selector.hpp: Core weight selector with strategy pattern
 */

#include "weight_collection.hpp"
#include "weight_common.hpp"
#include "weight_sampler.hpp"
#include "weight_selector.hpp"
#include "weight_strategy.hpp"

#endif  // ATOM_ALGORITHM_UTILS_WEIGHT_HPP
