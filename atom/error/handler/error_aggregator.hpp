/*
 * error_aggregator.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Thread-safe error aggregator for collecting related errors

**************************************************/

#ifndef ATOM_ERROR_HANDLER_AGGREGATOR_HPP
#define ATOM_ERROR_HANDLER_AGGREGATOR_HPP

#include <chrono>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../context/error_context.hpp"

namespace atom::error {

/**
 * @brief Thread-safe error aggregator for collecting related errors
 */
class ErrorAggregator {
public:
    /**
     * @brief Constructor
     */
    ErrorAggregator();

    /**
     * @brief Destructor
     */
    ~ErrorAggregator() = default;

    /**
     * @brief Add error to aggregation
     */
    void addError(std::shared_ptr<ErrorContext> context);

    /**
     * @brief Get aggregated errors by key
     */
    [[nodiscard]] auto getAggregatedErrors(const std::string& key) const
        -> std::vector<std::shared_ptr<ErrorContext>>;

    /**
     * @brief Get all aggregation keys
     */
    [[nodiscard]] auto getAggregationKeys() const -> std::vector<std::string>;

    /**
     * @brief Clear aggregated errors
     */
    void clear();

    /**
     * @brief Clear aggregated errors older than specified time
     */
    void clearOlderThan(std::chrono::minutes maxAge);

    /**
     * @brief Get aggregation statistics
     */
    [[nodiscard]] auto getStatistics() const
        -> std::unordered_map<std::string, int>;

private:
    std::unordered_map<std::string, std::vector<std::shared_ptr<ErrorContext>>>
        aggregatedErrors_;
    mutable std::shared_mutex aggregationMutex_;
};

}  // namespace atom::error

#endif  // ATOM_ERROR_HANDLER_AGGREGATOR_HPP
