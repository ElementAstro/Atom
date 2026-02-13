/*
 * basic_usage.cpp - Dotenv Basic Usage Example
 */

#include <iostream>
#include <string>
#include <unordered_map>
#include "atom/extra/dotenv/dotenv.hpp"

using namespace dotenv;

int main() {
    std::cout << "=== Dotenv Basic Usage Example ===" << std::endl;

    try {
        // 1. Basic .env file loading
        std::cout << "\n1. Basic .env File Loading:" << std::endl;
        {
            auto result = Dotenv::quickLoad(".env");
            if (result.success) {
                std::cout << "Successfully loaded " << result.variables.size()
                          << " variables" << std::endl;
                for (const auto& [key, value] : result.variables) {
                    std::cout << "  " << key << " = " << value << std::endl;
                }
            } else {
                std::cout << "Failed to load .env file" << std::endl;
                for (const auto& error : result.errors) {
                    std::cout << "  Error: " << error << std::endl;
                }
            }
        }

        // 2. Loading with custom options
        std::cout << "\n2. Loading with Custom Options:" << std::endl;
        {
            DotenvOptions options;
            options.debug = true;
            options.load_options.override_existing = true;
            options.parse_options.expand_variables = true;

            Dotenv loader(options);
            auto result = loader.load("config.env");
            if (result.success) {
                std::cout << "Successfully loaded config.env with "
                          << result.variables.size() << " variables"
                          << std::endl;
            }
        }

        // 3. Quick load and apply
        std::cout << "\n3. Quick Load and Apply:" << std::endl;
        {
            try {
                Dotenv::config(".env", true);
                std::cout << "Environment variables applied successfully"
                          << std::endl;
            } catch (const DotenvException& e) {
                std::cout << "Config failed: " << e.what() << std::endl;
            }
        }

        // 4. Parsing content directly
        std::cout << "\n4. Parsing Content Directly:" << std::endl;
        {
            std::string env_content = R"(
APP_NAME=DirectParsedAppAPP_VERSION=3.0.0DEBUG=trueDATABASE_URL=postgresql://localhost:5432/mydbAPI_KEY=secret123
)";

            Dotenv loader;
            auto result = loader.loadFromString(env_content);
            std::cout << "Parsed " << result.variables.size()
                      << " variables:" << std::endl;
            for (const auto& [key, value] : result.variables) {
                std::cout << "  " << key << " = " << value << std::endl;
            }
        }

        // 5. Variable expansion
        std::cout << "\n5. Variable Expansion:" << std::endl;
        {
            std::string env_content = R"(
BASE_PATH=/usr/localBIN_PATH=${BASE_PATH}/binLIB_PATH=${BASE_PATH}/lib
)";

            Dotenv loader;
            auto result = loader.loadFromString(env_content);
            std::cout << "Variables with expansion:" << std::endl;
            for (const auto& [key, value] : result.variables) {
                std::cout << "  " << key << " = " << value << std::endl;
            }
        }

        // 6. Quoted values and escape sequences
        std::cout << "\n6. Quoted Values and Escape Sequences:" << std::endl;
        {
            std::string env_content = R"(
SIMPLE="Hello World"
WITH_NEWLINE="Line 1\nLine 2"
WITH_TAB="Col1\tCol2"
SINGLE_QUOTED='No expansion: ${VAR}'
)";

            Dotenv loader;
            auto result = loader.loadFromString(env_content);
            std::cout << "Quoted values:" << std::endl;
            for (const auto& [key, value] : result.variables) {
                std::cout << "  " << key << " = " << value << std::endl;
            }
        }

        // 7. Saving environment variables
        std::cout << "\n7. Saving Environment Variables:" << std::endl;
        {
            std::unordered_map<std::string, std::string> vars = {
                {"APP_NAME", "MyApp"},
                {"APP_VERSION", "1.0.0"},
                {"DEBUG", "true"}};

            Dotenv loader;
            loader.save("output.env", vars);
            std::cout << "Saved " << vars.size() << " variables to output.env"
                      << std::endl;
        }

        std::cout << "\n=== Dotenv Basic Usage Example Complete ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in Dotenv basic usage examples: " << e.what()
                  << std::endl;
        return 1;
    }

    return 0;
}
