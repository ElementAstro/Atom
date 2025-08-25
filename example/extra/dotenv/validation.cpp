#include "atom/extra/dotenv/dotenv.hpp"
#include "atom/extra/dotenv/validator.hpp"

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
    }
}

int main() {
    try {
        std::cout << "=== Dotenv Validation Example ===" << std::endl;

        // Create test .env files for validation
        create_test_env_file("valid.env", R"(
# Database configuration
DB_HOST=localhost
DB_PORT=5432
DB_NAME=myapp
DB_USER=admin
DB_PASSWORD=secret123

# Application settings
APP_NAME=MyApplication
APP_VERSION=1.2.3
DEBUG=true
LOG_LEVEL=info

# API configuration
API_KEY=abc123def456ghi789
API_URL=https://api.example.com
TIMEOUT=30
MAX_RETRIES=3

# Email settings
EMAIL_HOST=smtp.example.com
EMAIL_PORT=587
EMAIL_USER=noreply@example.com
EMAIL_PASSWORD=email_secret
)");

        create_test_env_file("invalid.env", R"(
# Invalid configuration for testing
DB_HOST=localhost
DB_PORT=invalid_port
DB_NAME=
DB_USER=admin
# DB_PASSWORD is missing

APP_NAME=MyApp
APP_VERSION=invalid_version
DEBUG=maybe
LOG_LEVEL=invalid_level

API_KEY=short
API_URL=not_a_url
TIMEOUT=-5
MAX_RETRIES=too_many

EMAIL_HOST=
EMAIL_PORT=99999
EMAIL_USER=invalid_email
EMAIL_PASSWORD=weak
)");

        // 1. Basic validation schema
        std::cout << "\n1. Basic Validation Schema:" << std::endl;
        {
            ValidationSchema schema;

            // Database validation rules
            schema.addRule("DB_HOST", ValidationRule::required().pattern(
                                          R"(^[a-zA-Z0-9.-]+$)"));
            schema.addRule("DB_PORT",
                           ValidationRule::required().range(1, 65535));
            schema.addRule("DB_NAME", ValidationRule::required().minLength(1));
            schema.addRule("DB_USER", ValidationRule::required().minLength(1));
            schema.addRule("DB_PASSWORD",
                           ValidationRule::required().minLength(8));

            // Application validation rules
            schema.addRule("APP_NAME", ValidationRule::required().pattern(
                                           R"(^[a-zA-Z0-9_]+$)"));
            schema.addRule("APP_VERSION", ValidationRule::required().pattern(
                                              R"(^\d+\.\d+\.\d+$)"));
            schema.addRule("DEBUG",
                           ValidationRule::required().oneOf({"true", "false"}));
            schema.addRule("LOG_LEVEL",
                           ValidationRule::required().oneOf(
                               {"debug", "info", "warn", "error"}));

            Dotenv dotenv;

            // Test with valid configuration
            std::cout << "Validating valid.env:" << std::endl;
            auto valid_result = dotenv.loadAndValidate("valid.env", schema);

            std::cout << "  Success: " << valid_result.success << std::endl;
            std::cout << "  Variables loaded: " << valid_result.variables.size()
                      << std::endl;
            std::cout << "  Errors: " << valid_result.errors.size()
                      << std::endl;
            std::cout << "  Warnings: " << valid_result.warnings.size()
                      << std::endl;

            if (!valid_result.errors.empty()) {
                for (const auto& error : valid_result.errors) {
                    std::cout << "    Error: " << error << std::endl;
                }
            }

            // Test with invalid configuration
            std::cout << "\nValidating invalid.env:" << std::endl;
            auto invalid_result = dotenv.loadAndValidate("invalid.env", schema);

            std::cout << "  Success: " << invalid_result.success << std::endl;
            std::cout << "  Variables loaded: "
                      << invalid_result.variables.size() << std::endl;
            std::cout << "  Errors: " << invalid_result.errors.size()
                      << std::endl;
            std::cout << "  Warnings: " << invalid_result.warnings.size()
                      << std::endl;

            for (const auto& error : invalid_result.errors) {
                std::cout << "    Error: " << error << std::endl;
            }
        }

        // 2. Advanced validation rules
        std::cout << "\n2. Advanced Validation Rules:" << std::endl;
        {
            ValidationSchema advanced_schema;

            // API validation with complex rules
            advanced_schema.addRule(
                "API_KEY",
                ValidationRule::required()
                    .minLength(16)
                    .maxLength(64)
                    .pattern(R"(^[a-zA-Z0-9]+$)")
                    .description(
                        "API key must be 16-64 alphanumeric characters"));

            advanced_schema.addRule(
                "API_URL",
                ValidationRule::required()
                    .pattern(R"(^https?://[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}(/.*)?$)")
                    .description("Must be a valid HTTP/HTTPS URL"));

            advanced_schema.addRule(
                "TIMEOUT", ValidationRule::required().range(1, 300).description(
                               "Timeout must be between 1 and 300 seconds"));

            advanced_schema.addRule(
                "MAX_RETRIES",
                ValidationRule::required().range(0, 10).description(
                    "Max retries must be between 0 and 10"));

            // Email validation
            advanced_schema.addRule(
                "EMAIL_HOST", ValidationRule::required()
                                  .pattern(R"(^[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)")
                                  .description("Must be a valid hostname"));

            advanced_schema.addRule(
                "EMAIL_PORT",
                ValidationRule::required()
                    .oneOf({"25", "587", "465", "993", "995"})
                    .description("Must be a standard email port"));

            advanced_schema.addRule(
                "EMAIL_USER",
                ValidationRule::required()
                    .pattern(
                        R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)")
                    .description("Must be a valid email address"));

            advanced_schema.addRule(
                "EMAIL_PASSWORD",
                ValidationRule::required().minLength(8).description(
                    "Email password must be at least 8 characters"));

            Dotenv dotenv;

            std::cout << "Advanced validation of valid.env:" << std::endl;
            auto result = dotenv.loadAndValidate("valid.env", advanced_schema);

            std::cout << "  Success: " << result.success << std::endl;
            std::cout << "  Errors: " << result.errors.size() << std::endl;

            for (const auto& error : result.errors) {
                std::cout << "    " << error << std::endl;
            }

            std::cout << "\nAdvanced validation of invalid.env:" << std::endl;
            auto invalid_result =
                dotenv.loadAndValidate("invalid.env", advanced_schema);

            std::cout << "  Success: " << invalid_result.success << std::endl;
            std::cout << "  Errors: " << invalid_result.errors.size()
                      << std::endl;

            for (const auto& error : invalid_result.errors) {
                std::cout << "    " << error << std::endl;
            }
        }

        // 3. Optional and default values
        std::cout << "\n3. Optional Variables and Default Values:" << std::endl;
        {
            ValidationSchema schema_with_defaults;

            // Required variables
            schema_with_defaults.addRule("APP_NAME",
                                         ValidationRule::required());
            schema_with_defaults.addRule("APP_VERSION",
                                         ValidationRule::required());

            // Optional variables with defaults
            schema_with_defaults.addRule(
                "DEBUG", ValidationRule::optional("false")
                             .oneOf({"true", "false"})
                             .description("Debug mode (default: false)"));

            schema_with_defaults.addRule(
                "LOG_LEVEL", ValidationRule::optional("info")
                                 .oneOf({"debug", "info", "warn", "error"})
                                 .description("Log level (default: info)"));

            schema_with_defaults.addRule(
                "CACHE_TTL",
                ValidationRule::optional("3600").range(60, 86400).description(
                    "Cache TTL in seconds (default: 3600)"));

            schema_with_defaults.addRule(
                "FEATURE_FLAG",
                ValidationRule::optional("disabled")
                    .oneOf({"enabled", "disabled"})
                    .description("Feature flag (default: disabled)"));

            // Create minimal .env file
            create_test_env_file("minimal.env", R"(
APP_NAME=MinimalApp
APP_VERSION=1.0.0
DEBUG=true
)");

            Dotenv dotenv;
            auto result =
                dotenv.loadAndValidate("minimal.env", schema_with_defaults);

            std::cout << "Validation with defaults:" << std::endl;
            std::cout << "  Success: " << result.success << std::endl;
            std::cout << "  Variables (including defaults):" << std::endl;

            for (const auto& [key, value] : result.variables) {
                std::cout << "    " << key << " = " << value << std::endl;
            }
        }

        // 4. Custom validation functions
        std::cout << "\n4. Custom Validation Functions:" << std::endl;
        {
            ValidationSchema custom_schema;

            // Add custom validator for database URL
            custom_schema.addRule(
                "DATABASE_URL",
                ValidationRule::required()
                    .custom([](const std::string& value) -> ValidationResult {
                        if (value.find("://") == std::string::npos) {
                            return ValidationResult::error(
                                "Database URL must contain protocol");
                        }
                        if (value.find("@") == std::string::npos) {
                            return ValidationResult::error(
                                "Database URL must contain credentials");
                        }
                        return ValidationResult::success();
                    })
                    .description("Must be a valid database connection URL"));

            // Add custom validator for comma-separated list
            custom_schema.addRule(
                "ALLOWED_ORIGINS",
                ValidationRule::required()
                    .custom([](const std::string& value) -> ValidationResult {
                        if (value.empty()) {
                            return ValidationResult::error(
                                "Allowed origins cannot be empty");
                        }

                        // Split by comma and validate each origin
                        std::stringstream ss(value);
                        std::string origin;
                        int count = 0;

                        while (std::getline(ss, origin, ',')) {
                            // Trim whitespace
                            origin.erase(0, origin.find_first_not_of(" \t"));
                            origin.erase(origin.find_last_not_of(" \t") + 1);

                            if (origin != "*" && origin.find("http") != 0) {
                                return ValidationResult::error(
                                    "Invalid origin: " + origin);
                            }
                            count++;
                        }

                        if (count > 10) {
                            return ValidationResult::warning(
                                "Too many allowed origins (>10)");
                        }

                        return ValidationResult::success();
                    })
                    .description("Comma-separated list of allowed origins"));

            // Create test file for custom validation
            create_test_env_file("custom_test.env", R"(
DATABASE_URL=postgresql://user:pass@localhost:5432/dbname
ALLOWED_ORIGINS=https://example.com,https://app.example.com,*
)");

            create_test_env_file("custom_invalid.env", R"(
DATABASE_URL=invalid_url
ALLOWED_ORIGINS=invalid_origin,another_invalid
)");

            Dotenv dotenv;

            std::cout << "Custom validation - valid file:" << std::endl;
            auto valid_result =
                dotenv.loadAndValidate("custom_test.env", custom_schema);
            std::cout << "  Success: " << valid_result.success << std::endl;
            for (const auto& error : valid_result.errors) {
                std::cout << "    Error: " << error << std::endl;
            }
            for (const auto& warning : valid_result.warnings) {
                std::cout << "    Warning: " << warning << std::endl;
            }

            std::cout << "\nCustom validation - invalid file:" << std::endl;
            auto invalid_result =
                dotenv.loadAndValidate("custom_invalid.env", custom_schema);
            std::cout << "  Success: " << invalid_result.success << std::endl;
            for (const auto& error : invalid_result.errors) {
                std::cout << "    Error: " << error << std::endl;
            }
        }

        // 5. Validation schema from JSON/YAML (conceptual)
        std::cout << "\n5. Schema Validation Summary:" << std::endl;
        {
            ValidationSchema comprehensive_schema;

            // Build a comprehensive schema
            comprehensive_schema.addRule(
                "APP_NAME", ValidationRule::required().minLength(1));
            comprehensive_schema.addRule(
                "APP_VERSION",
                ValidationRule::required().pattern(R"(^\d+\.\d+\.\d+$)"));
            comprehensive_schema.addRule(
                "DEBUG",
                ValidationRule::optional("false").oneOf({"true", "false"}));
            comprehensive_schema.addRule(
                "PORT", ValidationRule::optional("3000").range(1000, 9999));
            comprehensive_schema.addRule("HOST",
                                         ValidationRule::optional("localhost"));

            // Test with a comprehensive .env file
            create_test_env_file("comprehensive.env", R"(
APP_NAME=ComprehensiveApp
APP_VERSION=2.1.0
DEBUG=false
PORT=8080
HOST=0.0.0.0
EXTRA_VAR=not_in_schema
)");

            Dotenv dotenv;
            auto result = dotenv.loadAndValidate("comprehensive.env",
                                                 comprehensive_schema);

            std::cout << "Comprehensive validation results:" << std::endl;
            std::cout << "  Success: " << result.success << std::endl;
            std::cout << "  Variables validated: " << result.variables.size()
                      << std::endl;
            std::cout << "  Errors: " << result.errors.size() << std::endl;
            std::cout << "  Warnings: " << result.warnings.size() << std::endl;

            // Show which variables passed validation
            std::cout << "  Validated variables:" << std::endl;
            for (const auto& [key, value] : result.variables) {
                std::cout << "    " << key << " = " << value << std::endl;
            }
        }

        // Cleanup test files
        std::cout << "\nCleaning up test files..." << std::endl;
        std::filesystem::remove("valid.env");
        std::filesystem::remove("invalid.env");
        std::filesystem::remove("minimal.env");
        std::filesystem::remove("custom_test.env");
        std::filesystem::remove("custom_invalid.env");
        std::filesystem::remove("comprehensive.env");

        std::cout << "\n=== Dotenv Validation Example Completed ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
