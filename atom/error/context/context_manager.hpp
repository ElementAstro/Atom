/*
 * context_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Error context manager for tracking contexts across the application

**************************************************/

#ifndef ATOM_ERROR_CONTEXT_MANAGER_HPP
#define ATOM_ERROR_CONTEXT_MANAGER_HPP

#include <chrono>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "error_context.hpp"

namespace atom::error {

/**
 * @brief Error context manager for tracking error contexts across the
 * application
 */
class ErrorContextManager {
public:
    /**
     * @brief Get the singleton instance
     */
    static auto getInstance() -> ErrorContextManager&;

    /**
     * @brief Register an error context
     */
    void registerContext(std::shared_ptr<ErrorContext> context);

    /**
     * @brief Get error context by ID
     */
    [[nodiscard]] auto getContext(const ErrorId& errorId) const
        -> std::shared_ptr<ErrorContext>;

    /**
     * @brief Get all contexts with a specific correlation ID
     */
    [[nodiscard]] auto getContextsByCorrelation(
        const std::string& correlationId) const
        -> std::vector<std::shared_ptr<ErrorContext>>;

    /**
     * @brief Get error context statistics
     */
    [[nodiscard]] auto getStatistics() const
        -> std::unordered_map<std::string, int>;

    /**
     * @brief Clear old error contexts (cleanup)
     */
    void cleanup(std::chrono::minutes maxAge = std::chrono::minutes(60));

    /**
     * @brief Clear all error contexts
     */
    void clear();

    /**
     * @brief Get total number of registered contexts
     */
    [[nodiscard]] auto size() const -> size_t;

private:
    ErrorContextManager() = default;
    ~ErrorContextManager() = default;
    ErrorContextManager(const ErrorContextManager&) = delete;
    ErrorContextManager& operator=(const ErrorContextManager&) = delete;

    std::unordered_map<ErrorId, std::shared_ptr<ErrorContext>> contexts_;
    mutable std::shared_mutex contextsMutex_;
};

}  // namespace atom::error

#endif  // ATOM_ERROR_CONTEXT_MANAGER_HPP
