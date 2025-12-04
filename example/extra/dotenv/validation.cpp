/*
 * validation.cpp - Dotenv Validation Example
 */

#include <fstream>
#include <iostream>
#include <regex>
#include "atom/extra/dotenv/dotenv.hpp"
#include "atom/extra/dotenv/validator.hpp"

using namespace dotenv;

int main() {
    std::cout << "=== Dotenv Validation Example ===" << std::endl;

    try {
        // 1. Basic validation with required variables
        std::cout << "\n1. Basic Validation with Required Variables:"
                  << std::endl;
        {
            ValidationSchema schema;
            schema.required("DATABASE_URL")
                .required("API_KEY")
                .optional("PORT", "3000");

            std::string env_content = R"(
DATABASE_URL=postgresql://localhost:5432/mydb
API_KEY=secret123
)";

            Dotenv loader;
            auto result = loader.loadFromString(env_content);

            Validator validator;
            auto validation =
                validator.validateWithDefaults(result.variables, schema);

            if (validation.is_valid) {
                std::cout << "Validation passed!" << std::endl;
                std::cout << "Variables (with defaults):" << std::endl;
                for (const auto& [key, value] : validation.processed_vars) {
                    std::cout << "  " << key << " = " << value << std::endl;
                }
            } else {
                std::cout << "Validation failed:" << std::endl;
                for (const auto& error : validation.errors) {
                    std::cout << "  " << error << std::endl;
                }
            }
        }

        // 2. Validation with built-in rules
        std::cout << "\n2. Validation with Built-in Rules:" << std::endl;
        {
            ValidationSchema schema;
            schema.required("PORT").rule("PORT", rules::integer());
            schema.required("DEBUG").rule("DEBUG", rules::boolean());
            schema.required("EMAIL").rule("EMAIL", rules::email());
            schema.required("URL").rule("URL", rules::url());

            std::string env_content = R"(
PORT=3000
DEBUG=true
EMAIL=user@example.com
URL=https://example.com
)";

            Dotenv loader;
            auto result = loader.loadFromString(env_content);

            Validator validator;
            auto validation = validator.validate(result.variables, schema);

            if (validation.is_valid) {
                std::cout << "All validation rules passed!" << std::endl;
            } else {
                std::cout << "Validation failed:" << std::endl;
                for (const auto& error : validation.errors) {
                    std::cout << "  " << error << std::endl;
                }
            }
        }

        // 3. String length validation
        std::cout << "\n3. String Length Validation:" << std::endl;
        {
            ValidationSchema schema;
            schema.required("USERNAME")
                .rule("USERNAME", rules::minLength(3))
                .rule("USERNAME", rules::maxLength(20));
            schema.required("PASSWORD").rule("PASSWORD", rules::minLength(8));

            std::string env_content = R"(
USERNAME=john_doe
PASSWORD=securepass123
)";

            Dotenv loader;
            auto result = loader.loadFromString(env_content);

            Validator validator;
            auto validation = validator.validate(result.variables, schema);

            if (validation.is_valid) {
                std::cout << "Length validation passed!" << std::endl;
            } else {
                std::cout << "Length validation failed:" << std::endl;
                for (const auto& error : validation.errors) {
                    std::cout << "  " << error << std::endl;
                }
            }
        }

        // 4. Pattern validation
        std::cout << "\n4. Pattern Validation:" << std::endl;
        {
            ValidationSchema schema;
            schema.required("VERSION").rule(
                "VERSION", rules::pattern(std::regex("^\\d+\\.\\d+\\.\\d+$"),
                                          "Version must be in format X.Y.Z"));

            std::string env_content = R"(
VERSION=1.2.3
)";

            Dotenv loader;
            auto result = loader.loadFromString(env_content);

            Validator validator;
            auto validation = validator.validate(result.variables, schema);

            if (validation.is_valid) {
                std::cout << "Pattern validation passed!" << std::endl;
            } else {
                std::cout << "Pattern validation failed:" << std::endl;
                for (const auto& error : validation.errors) {
                    std::cout << "  " << error << std::endl;
                }
            }
        }

        // 5. OneOf validation
        std::cout << "\n5. OneOf Validation:" << std::endl;
        {
            ValidationSchema schema;
            schema.required("ENVIRONMENT")
                .rule("ENVIRONMENT",
                      rules::oneOf({"development", "staging", "production"}));

            std::string env_content = R"(
ENVIRONMENT=production
)";

            Dotenv loader;
            auto result = loader.loadFromString(env_content);

            Validator validator;
            auto validation = validator.validate(result.variables, schema);

            if (validation.is_valid) {
                std::cout << "OneOf validation passed!" << std::endl;
            } else {
                std::cout << "OneOf validation failed:" << std::endl;
                for (const auto& error : validation.errors) {
                    std::cout << "  " << error << std::endl;
                }
            }
        }

        // 6. Custom validation rule
        std::cout << "\n6. Custom Validation Rule:" << std::endl;
        {
            ValidationSchema schema;
            schema.required("CUSTOM_VALUE")
                .rule("CUSTOM_VALUE", rules::custom(
                                          [](const std::string& value) {
                                              return value.length() > 5 &&
                                                     value.find("test") !=
                                                         std::string::npos;
                                          },
                                          "Value must be longer than 5 "
                                          "characters and contain 'test'"));

            std::string env_content = R"(
CUSTOM_VALUE=testing123
)";

            Dotenv loader;
            auto result = loader.loadFromString(env_content);

            Validator validator;
            auto validation = validator.validate(result.variables, schema);

            if (validation.is_valid) {
                std::cout << "Custom validation passed!" << std::endl;
            } else {
                std::cout << "Custom validation failed:" << std::endl;
                for (const auto& error : validation.errors) {
                    std::cout << "  " << error << std::endl;
                }
            }
        }

        // 7. Load and validate in one step
        std::cout << "\n7. Load and Validate in One Step:" << std::endl;
        {
            ValidationSchema schema;
            schema.required("APP_NAME").rule("APP_NAME", rules::notEmpty());
            schema.required("APP_PORT").rule("APP_PORT", rules::integer());
            schema.optional("APP_DEBUG", "false")
                .rule("APP_DEBUG", rules::boolean());

            std::string env_content = R"(
APP_NAME=MyApplication
APP_PORT=8080
)";

            // Write to temp file
            std::ofstream temp_file("temp_validation.env");
            temp_file << env_content;
            temp_file.close();

            Dotenv loader;
            auto result = loader.loadAndValidate("temp_validation.env", schema);

            if (result.success) {
                std::cout << "Load and validate succeeded!" << std::endl;
                std::cout << "Variables:" << std::endl;
                for (const auto& [key, value] : result.variables) {
                    std::cout << "  " << key << " = " << value << std::endl;
                }
            } else {
                std::cout << "Load and validate failed:" << std::endl;
                for (const auto& error : result.errors) {
                    std::cout << "  " << error << std::endl;
                }
            }

            // Clean up
            std::remove("temp_validation.env");
        }

        std::cout << "\n=== Dotenv Validation Example Complete ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in Dotenv validation examples: " << e.what()
                  << std::endl;
        return 1;
    }

    return 0;
}
