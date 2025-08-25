/**
 * @file refl_yaml.cpp
 * @brief Comprehensive example demonstrating YAML reflection capabilities
 *
 * This example shows how to:
 * - Use reflection for YAML serialization and deserialization
 * - Define field metadata with validation and defaults
 * - Handle required and optional fields
 * - Implement custom validators
 * - Work with nested objects and complex types
 * - Demonstrate error handling and validation
 * - Show differences and similarities with JSON reflection
 *
 * @author Max Qian
 * @date 2024-12-19
 */

#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>


// Check if YAML-cpp is available
#if __has_include(<yaml-cpp/yaml.h>)

// Atom Meta YAML reflection headers
#include "atom/meta/refl_yaml.hpp"

using namespace atom::meta;

// Example structures for demonstration
struct DatabaseConfig {
    std::string host;
    int port;
    std::string username;
    std::string password;
    std::string database;
    bool ssl_enabled;
};

struct ServerConfig {
    std::string bind_address;
    int port;
    int max_connections;
    int timeout_seconds;
    std::vector<std::string> allowed_origins;
};

struct ApplicationConfig {
    std::string name;
    std::string version;
    std::string environment;
    DatabaseConfig database;
    ServerConfig server;
    std::optional<std::string> log_level;
    bool debug_mode;
};

// Custom validators
auto validatePort = [](const int& port) -> bool {
    return port > 0 && port <= 65535;
};

auto validateHost = [](const std::string& host) -> bool {
    return !host.empty() && host.length() <= 255;
};

auto validateEnvironment = [](const std::string& env) -> bool {
    return env == "development" || env == "staging" || env == "production";
};

auto validateLogLevel = [](const std::string& level) -> bool {
    return level == "debug" || level == "info" || level == "warn" ||
           level == "error";
};

auto validateTimeout = [](const int& timeout) -> bool {
    return timeout >= 1 && timeout <= 3600;  // 1 second to 1 hour
};

/**
 * @brief Demonstrates basic YAML reflection with simple structures
 */
void basicYamlReflectionExample() {
    std::cout << "\n=== Basic YAML Reflection Example ===\n";

    try {
        // Define reflection metadata for DatabaseConfig
        auto dbReflection = Reflectable<
            DatabaseConfig, Field<DatabaseConfig, std::string>,
            Field<DatabaseConfig, int>, Field<DatabaseConfig, std::string>,
            Field<DatabaseConfig, std::string>,
            Field<DatabaseConfig, std::string>, Field<DatabaseConfig, bool>>(
            make_field<DatabaseConfig>("host", &DatabaseConfig::host, true,
                                       std::string("localhost"), validateHost),
            make_field<DatabaseConfig>("port", &DatabaseConfig::port, true,
                                       5432, validatePort),
            make_field<DatabaseConfig>("username", &DatabaseConfig::username,
                                       true, std::string(""), nullptr),
            make_field<DatabaseConfig>("password", &DatabaseConfig::password,
                                       true, std::string(""), nullptr),
            make_field<DatabaseConfig>("database", &DatabaseConfig::database,
                                       true, std::string(""), nullptr),
            make_field<DatabaseConfig>("ssl_enabled",
                                       &DatabaseConfig::ssl_enabled, false,
                                       false, nullptr));

        // Create a DatabaseConfig object
        DatabaseConfig dbConfig{"localhost", 5432,    "admin",
                                "secret",    "myapp", true};

        // Serialize to YAML
        YAML::Node dbYaml = dbReflection.to_yaml(dbConfig);
        std::cout << "Database Config YAML:\n" << dbYaml << "\n\n";

        // Create YAML input for deserialization
        std::string yamlInput = R"(
host: "192.168.1.100"
port: 3306
username: "dbuser"
password: "dbpass"
database: "production_db"
)";

        YAML::Node inputNode = YAML::Load(yamlInput);
        DatabaseConfig deserializedConfig = dbReflection.from_yaml(inputNode);

        std::cout << "Deserialized Database Config:\n";
        std::cout << "  Host: " << deserializedConfig.host << "\n";
        std::cout << "  Port: " << deserializedConfig.port << "\n";
        std::cout << "  Username: " << deserializedConfig.username << "\n";
        std::cout << "  Database: " << deserializedConfig.database << "\n";
        std::cout << "  SSL Enabled: "
                  << (deserializedConfig.ssl_enabled ? "true" : "false")
                  << " (default)\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in basic YAML reflection example: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates advanced YAML reflection with validation
 */
void advancedYamlReflectionExample() {
    std::cout << "\n=== Advanced YAML Reflection Example ===\n";

    try {
        // Define reflection metadata for ServerConfig with validation
        auto serverReflection =
            Reflectable<ServerConfig, Field<ServerConfig, std::string>,
                        Field<ServerConfig, int>, Field<ServerConfig, int>,
                        Field<ServerConfig, int>,
                        Field<ServerConfig, std::vector<std::string>>>(
                make_field<ServerConfig>("bind_address",
                                         &ServerConfig::bind_address, true,
                                         std::string("0.0.0.0"), validateHost),
                make_field<ServerConfig>("port", &ServerConfig::port, true,
                                         8080, validatePort),
                make_field<ServerConfig>("max_connections",
                                         &ServerConfig::max_connections, false,
                                         100, nullptr),
                make_field<ServerConfig>("timeout_seconds",
                                         &ServerConfig::timeout_seconds, false,
                                         30, validateTimeout),
                make_field<ServerConfig>("allowed_origins",
                                         &ServerConfig::allowed_origins, false,
                                         std::vector<std::string>{}, nullptr));

        // Test valid configuration
        std::cout << "Testing valid server configuration:\n";
        std::string validYaml = R"(
bind_address: "127.0.0.1"
port: 9000
max_connections: 200
timeout_seconds: 60
allowed_origins:
  - "https://example.com"
  - "https://api.example.com"
  - "http://localhost:3000"
)";

        YAML::Node validNode = YAML::Load(validYaml);
        ServerConfig validConfig = serverReflection.from_yaml(validNode);

        std::cout << "  Successfully created server config:\n";
        std::cout << "    Bind Address: " << validConfig.bind_address << "\n";
        std::cout << "    Port: " << validConfig.port << "\n";
        std::cout << "    Max Connections: " << validConfig.max_connections
                  << "\n";
        std::cout << "    Timeout: " << validConfig.timeout_seconds
                  << " seconds\n";
        std::cout << "    Allowed Origins: ";
        for (const auto& origin : validConfig.allowed_origins) {
            std::cout << origin << " ";
        }
        std::cout << "\n";

        // Test serialization
        YAML::Node serialized = serverReflection.to_yaml(validConfig);
        std::cout << "  Serialized YAML:\n" << serialized << "\n";

        // Test invalid port
        std::cout << "\nTesting invalid port:\n";
        std::string invalidPortYaml = R"(
bind_address: "127.0.0.1"
port: 70000
)";

        try {
            YAML::Node invalidNode = YAML::Load(invalidPortYaml);
            ServerConfig invalidConfig =
                serverReflection.from_yaml(invalidNode);
            std::cout << "  ERROR: Should have failed validation!\n";
        } catch (const std::exception& e) {
            std::cout << "  Correctly caught validation error: " << e.what()
                      << "\n";
        }

        // Test invalid timeout
        std::cout << "\nTesting invalid timeout:\n";
        std::string invalidTimeoutYaml = R"(
bind_address: "127.0.0.1"
port: 8080
timeout_seconds: 5000
)";

        try {
            YAML::Node invalidNode = YAML::Load(invalidTimeoutYaml);
            ServerConfig invalidConfig =
                serverReflection.from_yaml(invalidNode);
            std::cout << "  ERROR: Should have failed validation!\n";
        } catch (const std::exception& e) {
            std::cout << "  Correctly caught validation error: " << e.what()
                      << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in advanced YAML reflection example: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates complex nested YAML structures
 */
void nestedYamlExample() {
    std::cout << "\n=== Nested YAML Structures Example ===\n";

    try {
        // This would require more complex reflection setup for nested
        // structures For now, demonstrate with a simpler approach

        std::cout << "Complex nested YAML configuration example:\n";
        std::string complexYaml = R"(
application:
  name: "MyWebApp"
  version: "1.2.3"
  environment: "production"
  debug_mode: false

database:
  host: "db.example.com"
  port: 5432
  username: "app_user"
  password: "secure_password"
  database: "production_db"
  ssl_enabled: true

server:
  bind_address: "0.0.0.0"
  port: 8080
  max_connections: 1000
  timeout_seconds: 120
  allowed_origins:
    - "https://mywebapp.com"
    - "https://api.mywebapp.com"

logging:
  level: "info"
  file: "/var/log/myapp.log"
  max_size: "100MB"
)";

        YAML::Node complexNode = YAML::Load(complexYaml);

        std::cout << "Parsed complex YAML structure:\n";
        std::cout << "  Application: "
                  << complexNode["application"]["name"].as<std::string>()
                  << "\n";
        std::cout << "  Version: "
                  << complexNode["application"]["version"].as<std::string>()
                  << "\n";
        std::cout << "  Environment: "
                  << complexNode["application"]["environment"].as<std::string>()
                  << "\n";
        std::cout << "  Database Host: "
                  << complexNode["database"]["host"].as<std::string>() << "\n";
        std::cout << "  Database Port: "
                  << complexNode["database"]["port"].as<int>() << "\n";
        std::cout << "  Server Port: "
                  << complexNode["server"]["port"].as<int>() << "\n";
        std::cout << "  Log Level: "
                  << complexNode["logging"]["level"].as<std::string>() << "\n";

        // Show allowed origins
        std::cout << "  Allowed Origins: ";
        for (const auto& origin : complexNode["server"]["allowed_origins"]) {
            std::cout << origin.as<std::string>() << " ";
        }
        std::cout << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in nested YAML example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates YAML vs JSON differences
 */
void yamlVsJsonExample() {
    std::cout << "\n=== YAML vs JSON Differences Example ===\n";

    try {
        // Show YAML-specific features
        std::string yamlFeatures = R"(
# YAML supports comments (JSON doesn't)
application:
  name: "MyApp"
  # Multi-line strings with different styles
  description: |
    This is a multi-line description
    that preserves line breaks
    and formatting.

  # Another multi-line style
  summary: >
    This is a folded multi-line string
    that will be converted to a single line
    with spaces replacing line breaks.

  # YAML supports more readable lists
  features:
    - authentication
    - authorization
    - logging
    - monitoring

  # YAML supports anchors and references
  defaults: &defaults
    timeout: 30
    retries: 3

  # Reference the anchor
  api_config:
    <<: *defaults
    endpoint: "/api/v1"

  web_config:
    <<: *defaults
    endpoint: "/web"
)";

        YAML::Node yamlNode = YAML::Load(yamlFeatures);

        std::cout << "YAML-specific features demonstrated:\n";
        std::cout << "  Application: "
                  << yamlNode["application"]["name"].as<std::string>() << "\n";
        std::cout << "  Description (literal): "
                  << yamlNode["application"]["description"].as<std::string>()
                  << "\n";
        std::cout << "  Summary (folded): "
                  << yamlNode["application"]["summary"].as<std::string>()
                  << "\n";

        std::cout << "  Features: ";
        for (const auto& feature : yamlNode["application"]["features"]) {
            std::cout << feature.as<std::string>() << " ";
        }
        std::cout << "\n";

        std::cout << "  API Config Timeout: "
                  << yamlNode["application"]["api_config"]["timeout"].as<int>()
                  << "\n";
        std::cout << "  Web Config Timeout: "
                  << yamlNode["application"]["web_config"]["timeout"].as<int>()
                  << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in YAML vs JSON example: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating all YAML reflection capabilities
 */
int main() {
    std::cout << "================================================\n";
    std::cout << "  Atom Meta YAML Reflection Examples\n";
    std::cout << "================================================\n";

    try {
        basicYamlReflectionExample();
        advancedYamlReflectionExample();
        nestedYamlExample();
        yamlVsJsonExample();

        std::cout << "\n=== All YAML Reflection Examples Completed "
                     "Successfully ===\n";
        std::cout << "The YAML reflection system provides:\n";
        std::cout << "  ✓ Automatic YAML serialization/deserialization\n";
        std::cout << "  ✓ Field validation with custom validators\n";
        std::cout << "  ✓ Required and optional field handling\n";
        std::cout << "  ✓ Default value support\n";
        std::cout << "  ✓ Type-safe field mapping\n";
        std::cout << "  ✓ Comprehensive error handling\n";
        std::cout << "  ✓ Support for YAML-specific features (comments, "
                     "multi-line, anchors)\n";
        std::cout << "  ✓ Human-readable configuration format\n";

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

#else

/**
 * @brief Fallback main function when YAML-cpp is not available
 */
int main() {
    std::cout << "================================================\n";
    std::cout << "  Atom Meta YAML Reflection Examples\n";
    std::cout << "================================================\n";
    std::cout << "\nYAML-cpp library is not available.\n";
    std::cout << "To run YAML reflection examples, please install yaml-cpp:\n";
    std::cout << "  - Ubuntu/Debian: sudo apt-get install libyaml-cpp-dev\n";
    std::cout << "  - macOS: brew install yaml-cpp\n";
    std::cout << "  - Windows: vcpkg install yaml-cpp\n";
    std::cout << "\nThe YAML reflection system would provide:\n";
    std::cout << "  ✓ Automatic YAML serialization/deserialization\n";
    std::cout << "  ✓ Field validation with custom validators\n";
    std::cout << "  ✓ Required and optional field handling\n";
    std::cout << "  ✓ Default value support\n";
    std::cout << "  ✓ Type-safe field mapping\n";
    std::cout << "  ✓ Comprehensive error handling\n";
    std::cout << "  ✓ Support for YAML-specific features (comments, "
                 "multi-line, anchors)\n";
    std::cout << "  ✓ Human-readable configuration format\n";

    return 0;
}

#endif
