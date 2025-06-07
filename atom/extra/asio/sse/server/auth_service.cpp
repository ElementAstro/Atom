#include "auth_service.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include "atom/type/json.hpp"


using json = nlohmann::json;

namespace atom::extra::asio::sse {

AuthService::AuthService(const std::string& auth_file) : auth_file_(auth_file) {
    load_auth_data();
}

bool AuthService::authenticate(const std::string& api_key) const {
    std::shared_lock lock(mutex_);
    return api_keys_.find(api_key) != api_keys_.end();
}

bool AuthService::authenticate(const std::string& username,
                               const std::string& password) const {
    std::shared_lock lock(mutex_);
    auto it = user_credentials_.find(username);
    return it != user_credentials_.end() && it->second == password;
}

void AuthService::add_api_key(const std::string& api_key) {
    std::unique_lock lock(mutex_);
    api_keys_.insert(api_key);
    save_auth_data();
}

void AuthService::remove_api_key(const std::string& api_key) {
    std::unique_lock lock(mutex_);
    api_keys_.erase(api_key);
    save_auth_data();
}

void AuthService::add_user(const std::string& username,
                           const std::string& password) {
    std::unique_lock lock(mutex_);
    user_credentials_[username] = password;
    save_auth_data();
}

void AuthService::remove_user(const std::string& username) {
    std::unique_lock lock(mutex_);
    user_credentials_.erase(username);
    save_auth_data();
}

void AuthService::load_auth_data() {
    try {
        std::ifstream file(auth_file_);
        if (file.is_open()) {
            json j;
            file >> j;

            if (j.contains("api_keys") && j["api_keys"].is_array()) {
                for (const auto& key : j["api_keys"]) {
                    api_keys_.insert(key);
                }
            }

            if (j.contains("users") && j["users"].is_object()) {
                for (auto it = j["users"].begin(); it != j["users"].end();
                     ++it) {
                    user_credentials_[it.key()] = it.value();
                }
            }
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error loading auth data: {}", e.what());
    }
}

void AuthService::save_auth_data() {
    try {
        json j;

        j["api_keys"] = json::array();
        for (const auto& key : api_keys_) {
            j["api_keys"].push_back(key);
        }

        j["users"] = json::object();
        for (const auto& [username, password] : user_credentials_) {
            j["users"][username] = password;
        }

        std::ofstream file(auth_file_);
        file << j.dump(4);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error saving auth data: {}", e.what());
    }
}

}  // namespace atom::extra::asio::sse