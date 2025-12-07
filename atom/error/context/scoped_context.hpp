/*
 * scoped_context.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: RAII wrapper for error context management

**************************************************/

#ifndef ATOM_ERROR_CONTEXT_SCOPED_CONTEXT_HPP
#define ATOM_ERROR_CONTEXT_SCOPED_CONTEXT_HPP

#include <memory>
#include <string>

#include "error_context.hpp"

namespace atom::error {

/**
 * @brief RAII wrapper for error context management
 *
 * Automatically registers and manages error context lifecycle
 */
class ScopedErrorContext {
public:
    /**
     * @brief Constructor with error code and message
     */
    ScopedErrorContext(int errorCode, const std::string& message = "");

    /**
     * @brief Constructor with existing context
     */
    explicit ScopedErrorContext(std::shared_ptr<ErrorContext> context);

    /**
     * @brief Destructor - automatically cleans up context
     */
    ~ScopedErrorContext();

    // Non-copyable
    ScopedErrorContext(const ScopedErrorContext&) = delete;
    ScopedErrorContext& operator=(const ScopedErrorContext&) = delete;

    // Movable
    ScopedErrorContext(ScopedErrorContext&& other) noexcept;
    ScopedErrorContext& operator=(ScopedErrorContext&& other) noexcept;

    /**
     * @brief Get the managed context
     */
    [[nodiscard]] auto getContext() const -> std::shared_ptr<ErrorContext>;

    /**
     * @brief Access context members directly
     */
    auto operator->() const -> ErrorContext*;

    /**
     * @brief Check if context is valid
     */
    explicit operator bool() const;

    /**
     * @brief Add user data to context
     */
    ScopedErrorContext& setUserData(const std::string& key, std::any value);

    /**
     * @brief Add tag to context
     */
    ScopedErrorContext& addTag(const std::string& tag);

    /**
     * @brief Set correlation ID
     */
    ScopedErrorContext& setCorrelationId(const std::string& correlationId);

private:
    std::shared_ptr<ErrorContext> context_;
};

/**
 * @brief Macro for creating scoped error context
 */
#define SCOPED_ERROR_CONTEXT(errorCode, message) \
    atom::error::ScopedErrorContext _scopedErrorContext(errorCode, message)

#define SCOPED_ERROR_CONTEXT_WITH_CORRELATION(errorCode, correlationId, \
                                              message)                  \
    atom::error::ScopedErrorContext _scopedErrorContext(                \
        atom::error::ErrorContext::createWithCorrelation(               \
            errorCode, correlationId, message));                        \
    _scopedErrorContext->setSystemInfo("file", ATOM_FILE_NAME);         \
    _scopedErrorContext->setSystemInfo("line",                          \
                                       std::to_string(ATOM_FILE_LINE)); \
    _scopedErrorContext->setSystemInfo("function", ATOM_FUNC_NAME)

}  // namespace atom::error

#endif  // ATOM_ERROR_CONTEXT_SCOPED_CONTEXT_HPP
