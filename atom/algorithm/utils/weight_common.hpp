#ifndef ATOM_ALGORITHM_UTILS_WEIGHT_COMMON_HPP
#define ATOM_ALGORITHM_UTILS_WEIGHT_COMMON_HPP

#include <concepts>
#include <source_location>
#include <string>

#include "atom/error/exception.hpp"

namespace atom::algorithm {

/**
 * @brief Concept for numeric types that can be used for weights
 */
template <typename T>
concept WeightType = std::floating_point<T> || std::integral<T>;

/**
 * @brief Exception class for weight-related errors
 */
class WeightError : public error::RuntimeError {
public:
    explicit WeightError(
        const std::string& message,
        const std::source_location& loc = std::source_location::current())
        : error::RuntimeError(loc.file_name(), loc.line(), loc.function_name(),
                              message) {}
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_UTILS_WEIGHT_COMMON_HPP
