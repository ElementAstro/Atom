#pragma once

#include <any>
#include <optional>
#include <string>
#include <unordered_map>

namespace modern_log {

/**
 * @class LogContext
 * @brief Structured logging context for carrying contextual information.
 *
 * This class encapsulates structured context information for log entries,
 * such as user ID, session ID, trace ID, request ID, and arbitrary custom
 * fields. It supports chainable setters for convenient construction and
 * provides accessors for retrieving context values. The context can be
 * serialized to JSON, merged with another context, cleared, and checked for
 * emptiness.
 */
class LogContext {
private:
    std::string user_id_;  ///< User identifier associated with the log context.
    std::string session_id_;  ///< Session identifier for the current context.
    std::string trace_id_;    ///< Trace identifier for distributed tracing.
    std::string request_id_;  ///< Request identifier for correlating logs.
    std::unordered_map<std::string, std::any>
        custom_fields_;  ///< Arbitrary custom fields.

public:
    /**
     * @brief Set the user ID for the context (chainable).
     * @param user The user identifier.
     * @return Reference to this context for chaining.
     */
    LogContext& with_user(std::string_view user) {
        user_id_ = user;
        return *this;
    }

    /**
     * @brief Set the session ID for the context (chainable).
     * @param session The session identifier.
     * @return Reference to this context for chaining.
     */
    LogContext& with_session(std::string_view session) {
        session_id_ = session;
        return *this;
    }

    /**
     * @brief Set the trace ID for the context (chainable).
     * @param trace The trace identifier.
     * @return Reference to this context for chaining.
     */
    LogContext& with_trace(std::string_view trace) {
        trace_id_ = trace;
        return *this;
    }

    /**
     * @brief Set the request ID for the context (chainable).
     * @param request The request identifier.
     * @return Reference to this context for chaining.
     */
    LogContext& with_request(std::string_view request) {
        request_id_ = request;
        return *this;
    }

    /**
     * @brief Add or update a custom field in the context (chainable).
     * @tparam T The type of the value.
     * @param key The field name.
     * @param value The field value.
     * @return Reference to this context for chaining.
     */
    template <typename T>
    LogContext& with_field(std::string_view key, T&& value) {
        custom_fields_[std::string(key)] = std::forward<T>(value);
        return *this;
    }

    /**
     * @brief Get the user ID from the context.
     * @return The user identifier string.
     */
    const std::string& user_id() const { return user_id_; }

    /**
     * @brief Get the session ID from the context.
     * @return The session identifier string.
     */
    const std::string& session_id() const { return session_id_; }

    /**
     * @brief Get the trace ID from the context.
     * @return The trace identifier string.
     */
    const std::string& trace_id() const { return trace_id_; }

    /**
     * @brief Get the request ID from the context.
     * @return The request identifier string.
     */
    const std::string& request_id() const { return request_id_; }

    /**
     * @brief Retrieve a custom field value from the context.
     * @tparam T The expected type of the value.
     * @param key The field name.
     * @return std::optional containing the value if present and type matches,
     * std::nullopt otherwise.
     */
    template <typename T>
    std::optional<T> get_field(std::string_view key) const {
        if (auto it = custom_fields_.find(std::string(key));
            it != custom_fields_.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Serialize the context to a JSON string.
     * @return JSON representation of the context.
     */
    std::string to_json() const;

    /**
     * @brief Merge this context with another, preferring values from the other
     * context.
     * @param other The other LogContext to merge from.
     * @return A new LogContext containing merged values.
     */
    LogContext merge(const LogContext& other) const;

    /**
     * @brief Clear all fields in the context.
     */
    void clear();

    /**
     * @brief Check if the context is empty (no fields set).
     * @return True if all fields are empty, false otherwise.
     */
    bool empty() const;
};

}  // namespace modern_log
