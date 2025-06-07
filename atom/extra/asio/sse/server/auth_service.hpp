#pragma once

/**
 * @file auth_service.hpp
 * @brief Authentication service for SSE server
 */

#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace atom::extra::asio::sse {

/**
 * @brief Authentication service managing API keys and user credentials.
 *
 * This class provides methods for authenticating clients using API keys or
 * username/password pairs. It supports adding and removing API keys and users,
 * and persists authentication data to a file.
 */
class AuthService {
public:
    /**
     * @brief Construct the AuthService and load authentication data from file.
     * @param auth_file Path to the authentication data file (e.g., JSON).
     */
    explicit AuthService(const std::string& auth_file);

    /**
     * @brief Authenticate a client using an API key.
     * @param api_key The API key to authenticate.
     * @return True if the API key is valid, false otherwise.
     */
    bool authenticate(const std::string& api_key) const;

    /**
     * @brief Authenticate a client using username and password.
     * @param username The username to authenticate.
     * @param password The password to authenticate.
     * @return True if the username and password are valid, false otherwise.
     */
    bool authenticate(const std::string& username,
                      const std::string& password) const;

    /**
     * @brief Add a new API key to the authentication service.
     * @param api_key The API key to add.
     */
    void add_api_key(const std::string& api_key);

    /**
     * @brief Remove an API key from the authentication service.
     * @param api_key The API key to remove.
     */
    void remove_api_key(const std::string& api_key);

    /**
     * @brief Add a new user with username and password.
     * @param username The username to add.
     * @param password The password for the user.
     */
    void add_user(const std::string& username, const std::string& password);

    /**
     * @brief Remove a user from the authentication service.
     * @param username The username to remove.
     */
    void remove_user(const std::string& username);

private:
    /**
     * @brief Path to the authentication data file.
     */
    std::string auth_file_;

    /**
     * @brief Set of valid API keys.
     */
    std::unordered_set<std::string> api_keys_;

    /**
     * @brief Map of usernames to passwords.
     */
    std::unordered_map<std::string, std::string> user_credentials_;

    /**
     * @brief Mutex for thread-safe access to authentication data.
     */
    mutable std::shared_mutex mutex_;

    /**
     * @brief Load authentication data from the file.
     *
     * Reads API keys and user credentials from the specified file and
     * populates the internal data structures.
     */
    void load_auth_data();

    /**
     * @brief Save authentication data to the file.
     *
     * Writes the current API keys and user credentials to the specified file.
     */
    void save_auth_data();
};

}  // namespace atom::extra::asio::sse