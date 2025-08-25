#include "atom/extra/dotenv/dotenv.hpp"
#include "atom/extra/dotenv/parser.hpp"
#include "atom/extra/dotenv/validator.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace dotenv;

// Helper function to create test .env files
void create_test_env_file(const std::string& filename,
                          const std::string& content) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << content;
        file.close();
        std::cout << "Created test file: " << filename << std::endl;
    } else {
        std::cerr << "Failed to create test file: " << filename << std::endl;
    }
}

// Helper function to display environment variable
void show_env_var(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    std::cout << "  " << name << " = " << (value ? value : "(not set)")
              << std::endl;
}

int main() {
    try {
        std::cout << "=== Dotenv Basic Usage Example ===" << std::endl;

        // Create test .env files
        create_test_env_file(".env", R"(
# Database configuration
DB_HOST=localhost
DB_PORT=5432
DB_NAME=myapp
DB_USER=admin
DB_PASSWORD=secret123

# Application settings
APP_NAME="My Application"
APP_VERSION=1.0.0
DEBUG=true
LOG_LEVEL=info

# API configuration
API_KEY=abc123def456
API_URL=https://api.example.com
TIMEOUT=30

# Features
FEATURE_X=enabled
FEATURE_Y=disabled
)");

        create_test_env_file(".env.local", R"(
# Local overrides
DB_HOST=127.0.0.1
DEBUG=false
LOG_LEVEL=debug
LOCAL_SETTING=local_value
)");

        create_test_env_file(".env.production", R"(
# Production settings
DB_HOST=prod-db.example.com
DB_PASSWORD=prod_secret_password
DEBUG=false
LOG_LEVEL=error
API_URL=https://prod-api.example.com
)");

        // 1. Basic loading
        std::cout << "\n1. Basic .env File Loading:" << std::endl;
        {
            Dotenv dotenv;
            auto result = dotenv.load(".env");

            if (result.success) {
                std::cout << "Successfully loaded " << result.variables.size()
                          << " variables" << std::endl;
                std::cout << "Loaded files: ";
                for (const auto& file : result.loaded_files) {
                    std::cout << file << " ";
                }
                std::cout << std::endl;

                // Display some variables
                std::cout << "Sample variables:" << std::endl;
                for (const auto& [key, value] : result.variables) {
                    if (key.find("DB_") == 0 || key == "APP_NAME") {
                        std::cout << "  " << key << " = " << value << std::endl;
                    }
                }
            } else {
                std::cerr << "Failed to load .env file" << std::endl;
                for (const auto& error : result.errors) {
                    std::cerr << "  Error: " << error << std::endl;
                }
            }
        }

        // 2. Loading multiple files
        std::cout << "\n2. Loading Multiple .env Files:" << std::endl;
        {
            Dotenv dotenv;
            std::vector<std::filesystem::path> files = {".env", ".env.local"};
            auto result = dotenv.loadMultiple(files);

            if (result.success) {
                std::cout << "Successfully loaded from "
                          << result.loaded_files.size() << " files"
                          << std::endl;
                std::cout << "Total variables: " << result.variables.size()
                          << std::endl;

                // Show how local overrides work
                std::cout << "Variable overrides:" << std::endl;
                std::cout << "  DB_HOST = " << result.variables["DB_HOST"]
                          << " (overridden by .env.local)" << std::endl;
                std::cout << "  DEBUG = " << result.variables["DEBUG"]
                          << " (overridden by .env.local)" << std::endl;
                std::cout << "  LOCAL_SETTING = "
                          << result.variables["LOCAL_SETTING"]
                          << " (only in .env.local)" << std::endl;
            }
        }

        // 3. Auto-discovery
        std::cout << "\n3. Auto-Discovery of .env Files:" << std::endl;
        {
            Dotenv dotenv;
            auto result = dotenv.autoLoad(".");

            std::cout << "Auto-discovered files: ";
            for (const auto& file : result.loaded_files) {
                std::cout << file.filename() << " ";
            }
            std::cout << std::endl;
            std::cout << "Total variables from auto-discovery: "
                      << result.variables.size() << std::endl;
        }

        // 4. Loading from string
        std::cout << "\n4. Loading from String:" << std::endl;
        {
            std::string env_content = R"(
STRING_VAR=hello_world
NUMBER_VAR=42
BOOL_VAR=true
QUOTED_VAR="This is a quoted string"
MULTILINE_VAR="Line 1\nLine 2\nLine 3"
)";

            Dotenv dotenv;
            auto result = dotenv.loadFromString(env_content);

            if (result.success) {
                std::cout << "Loaded from string: " << result.variables.size()
                          << " variables" << std::endl;
                for (const auto& [key, value] : result.variables) {
                    std::cout << "  " << key << " = " << value << std::endl;
                }
            }
        }

        // 5. Applying to environment
        std::cout << "\n5. Applying Variables to System Environment:"
                  << std::endl;
        {
            Dotenv dotenv;
            auto result = dotenv.load(".env");

            if (result.success) {
                std::cout << "Before applying to environment:" << std::endl;
                show_env_var("DB_HOST");
                show_env_var("APP_NAME");
                show_env_var("DEBUG");

                // Apply to environment (without overriding existing)
                dotenv.applyToEnvironment(result.variables, false);

                std::cout << "\nAfter applying to environment:" << std::endl;
                show_env_var("DB_HOST");
                show_env_var("APP_NAME");
                show_env_var("DEBUG");
            }
        }

        // 6. Quick load (static method)
        std::cout << "\n6. Quick Load (Static Method):" << std::endl;
        {
            auto result = Dotenv::quickLoad(".env");
            if (result.success) {
                std::cout << "Quick load successful: "
                          << result.variables.size() << " variables"
                          << std::endl;
                std::cout << "API_KEY = " << result.variables["API_KEY"]
                          << std::endl;
                std::cout << "API_URL = " << result.variables["API_URL"]
                          << std::endl;
            }
        }

        // 7. Quick load and apply (static method)
        std::cout << "\n7. Quick Load and Apply:" << std::endl;
        {
            std::cout << "Before quick apply - TIMEOUT: ";
            show_env_var("TIMEOUT");

            Dotenv::quickLoadAndApply(".env");

            std::cout << "After quick apply - TIMEOUT: ";
            show_env_var("TIMEOUT");
        }

        // 8. Saving variables to file
        std::cout << "\n8. Saving Variables to File:" << std::endl;
        {
            std::unordered_map<std::string, std::string> vars_to_save = {
                {"SAVED_VAR1", "value1"},
                {"SAVED_VAR2", "value2"},
                {"SAVED_TIMESTAMP", std::to_string(std::time(nullptr))},
                {"SAVED_BOOL", "true"},
                {"SAVED_NUMBER", "123"}};

            Dotenv dotenv;
            dotenv.save("saved_vars.env", vars_to_save);

            std::cout << "Saved " << vars_to_save.size()
                      << " variables to saved_vars.env" << std::endl;

            // Verify by loading the saved file
            auto result = dotenv.load("saved_vars.env");
            if (result.success) {
                std::cout << "Verification - loaded back "
                          << result.variables.size()
                          << " variables:" << std::endl;
                for (const auto& [key, value] : result.variables) {
                    std::cout << "  " << key << " = " << value << std::endl;
                }
            }
        }

        // 9. Error handling
        std::cout << "\n9. Error Handling:" << std::endl;
        {
            Dotenv dotenv;

            // Try to load non-existent file
            auto result = dotenv.load("non_existent.env");
            if (!result.success) {
                std::cout << "Expected error for non-existent file:"
                          << std::endl;
                for (const auto& error : result.errors) {
                    std::cout << "  " << error << std::endl;
                }
            }

            // Try to load malformed content
            std::string malformed_content = R"(
VALID_VAR=value
INVALID LINE WITHOUT EQUALS
ANOTHER_VALID=value2
=INVALID_KEY
)";

            auto string_result = dotenv.loadFromString(malformed_content);
            std::cout << "\nMalformed content results:" << std::endl;
            std::cout << "Success: " << string_result.success << std::endl;
            std::cout << "Variables loaded: " << string_result.variables.size()
                      << std::endl;
            std::cout << "Errors: " << string_result.errors.size() << std::endl;
            std::cout << "Warnings: " << string_result.warnings.size()
                      << std::endl;

            for (const auto& error : string_result.errors) {
                std::cout << "  Error: " << error << std::endl;
            }
            for (const auto& warning : string_result.warnings) {
                std::cout << "  Warning: " << warning << std::endl;
            }
        }

        // 10. Custom options
        std::cout << "\n10. Custom Options:" << std::endl;
        {
            DotenvOptions options;
            options.debug = true;
            options.logger = [](const std::string& message) {
                std::cout << "[DOTENV DEBUG] " << message << std::endl;
            };

            // Configure parser options
            options.parse_options.allow_empty_values = true;
            options.parse_options.trim_whitespace = true;
            options.parse_options.expand_variables = true;

            // Configure loader options
            options.load_options.ignore_missing_files = true;
            options.load_options.encoding = "utf-8";

            Dotenv dotenv(options);

            // Create a test file with variable expansion
            create_test_env_file("test_expansion.env", R"(
BASE_URL=https://api.example.com
API_VERSION=v1
FULL_API_URL=${BASE_URL}/${API_VERSION}
EMPTY_VAR=
WHITESPACE_VAR=  trimmed value  
)");

            auto result = dotenv.load("test_expansion.env");
            if (result.success) {
                std::cout << "Custom options test results:" << std::endl;
                for (const auto& [key, value] : result.variables) {
                    std::cout << "  " << key << " = '" << value << "'"
                              << std::endl;
                }
            }
        }

        // Cleanup test files
        std::cout << "\nCleaning up test files..." << std::endl;
        std::filesystem::remove(".env");
        std::filesystem::remove(".env.local");
        std::filesystem::remove(".env.production");
        std::filesystem::remove("saved_vars.env");
        std::filesystem::remove("test_expansion.env");

        std::cout << "\n=== Dotenv Basic Usage Example Completed ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
