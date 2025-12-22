/**
 * @file loader_parser.cpp
 * @brief Demonstration of atom::extra::dotenv Loader and Parser functionality
 *
 * @details This example demonstrates:
 * - Loading .env files with the Loader class
 * - Parsing .env content with the Parser class
 * - Custom parsing options
 * - Error handling for malformed files
 * - Environment variable expansion
 *
 * @level Intermediate
 * @prerequisites Basic understanding of environment variables
 * @related_examples basic_usage.cpp, validation.cpp
 *
 * @author Atom Extra Examples
 * @date 2024
 */

#include "atom/extra/dotenv/exceptions.hpp"
#include "atom/extra/dotenv/loader.hpp"
#include "atom/extra/dotenv/parser.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace dotenv;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void printSeparator(const std::string& title) {
    std::cout << "\n===== " << title << " =====\n" << std::endl;
}

// Create a temporary .env file for testingfs::path createTempEnvFile(const
// std::string& content) {
fs::path tempPath = fs::temp_directory_path() / "test_dotenv.env";
std::ofstream ofs(tempPath);
ofs << content;
return tempPath;
}

// ============================================================================
// PARSER EXAMPLES
// ============================================================================

/**
 * @brief Demonstrates basic parser usage
 *
 * Shows how to parse .env content strings.
 */
void basicParserExample() {
    printSeparator("Basic Parser Example");

    Parser parser;

    std::string content = R"(
# Database configurationDB_HOST=localhostDB_PORT=5432DB_NAME=myapp

# API settingsAPI_KEY=secret123API_URL=https://api.example.com
)";

    std::cout << "Parsing .env content..." << std::endl;
    auto result = parser.parse(content);

    std::cout << "Parsed " << result.size() << " variables:" << std::endl;
    for (const auto& [key, value] : result) {
        std::cout << "  " << key << " = " << value << std::endl;
    }
}

/**
 * @brief Demonstrates parsing with quotes and special characters
 *
 * Shows how the parser handles quoted values and special characters.
 */
void quotedValuesExample() {
    printSeparator("Quoted Values Example");

    Parser parser;

    std::string content = R"(
# Single quotes preserve literal valuesSINGLE_QUOTED='Hello $USER'

# Double quotes allow variable expansionDOUBLE_QUOTED="Hello World"

# Values with spacesMESSAGE="This is a message with spaces"

# Values with special charactersSPECIAL="value=with=equals"
JSON_DATA='{"key": "value"}'
)";

    auto result = parser.parse(content);

    std::cout << "Parsed quoted values:" << std::endl;
    for (const auto& [key, value] : result) {
        std::cout << "  " << key << " = " << value << std::endl;
    }
}

/**
 * @brief Demonstrates multiline values
 *
 * Shows how to handle multiline values in .env files.
 */
void multilineValuesExample() {
    printSeparator("Multiline Values Example");

    Parser parser;

    std::string content = R"(
# Multiline value with escaped newlinesMULTILINE="Line 1\nLine 2\nLine 3"

# Certificate or key contentPRIVATE_KEY="-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEF
-----END PRIVATE KEY-----"
)";

    auto result = parser.parse(content);

    std::cout << "Parsed multiline values:" << std::endl;
    for (const auto& [key, value] : result) {
        std::cout << "  " << key << " = " << value.substr(0, 50)
                  << (value.length() > 50 ? "..." : "") << std::endl;
    }
}

// ============================================================================
// LOADER EXAMPLES
// ============================================================================

/**
 * @brief Demonstrates basic loader usage
 *
 * Shows how to load .env files from disk.
 */
void basicLoaderExample() {
    printSeparator("Basic Loader Example");

    // Create a temporary .env file
    std::string content = R"(
APP_NAME=MyApplicationAPP_VERSION=1.0.0DEBUG=trueLOG_LEVEL=info
)";

    fs::path envPath = createTempEnvFile(content);
    std::cout << "Created temp .env file: " << envPath << std::endl;

    FileLoader loader;
    Parser parser;

    std::cout << "Loading .env file..." << std::endl;
    std::string fileContent = loader.load(envPath);
    auto result = parser.parse(fileContent);

    std::cout << "Loaded " << result.size() << " variables:" << std::endl;
    for (const auto& [key, value] : result) {
        std::cout << "  " << key << " = " << value << std::endl;
    }

    // Cleanup
    fs::remove(envPath);
}

/**
 * @brief Demonstrates loading with environment variable expansion
 *
 * Shows how variables can reference other variables.
 */
void variableExpansionExample() {
    printSeparator("Variable Expansion Example");

    std::string content = R"(
BASE_URL=https://api.example.comAPI_VERSION=v1API_ENDPOINT=${BASE_URL}/${API_VERSION}

HOME_DIR=/home/userCONFIG_PATH=${HOME_DIR}/.configDATA_PATH=${HOME_DIR}/data
)";

    fs::path envPath = createTempEnvFile(content);

    FileLoader loader;
    ParseOptions options;
    options.expand_variables = true;
    Parser parser(options);

    std::string fileContent = loader.load(envPath);
    auto result = parser.parse(fileContent);

    std::cout << "Variables with expansion:" << std::endl;
    for (const auto& [key, value] : result) {
        std::cout << "  " << key << " = " << value << std::endl;
    }

    // Cleanup
    fs::remove(envPath);
}

/**
 * @brief Demonstrates loading multiple .env files
 *
 * Shows how to load and merge multiple .env files.
 */
void multipleFilesExample() {
    printSeparator("Multiple Files Example");

    // Create base .env file
    std::string baseContent = R"(
APP_NAME=MyAppDB_HOST=localhostDB_PORT=5432
)";

    // Create override .env file
    std::string overrideContent = R"(
DB_HOST=production-db.example.comDB_PORT=5433NEW_VAR=added
)";

    fs::path basePath = fs::temp_directory_path() / "base.env";
    fs::path overridePath = fs::temp_directory_path() / "override.env";

    {
        std::ofstream(basePath) << baseContent;
        std::ofstream(overridePath) << overrideContent;
    }

    FileLoader loader;
    Parser parser;

    // Load base file
    std::string baseFileContent = loader.load(basePath);
    auto result = parser.parse(baseFileContent);
    std::cout << "After loading base.env:" << std::endl;
    for (const auto& [key, value] : result) {
        std::cout << "  " << key << " = " << value << std::endl;
    }

    // Load override file (merges with existing)
    std::string overrideFileContent = loader.load(overridePath);
    auto overrideResult = parser.parse(overrideFileContent);
    for (const auto& [key, value] : overrideResult) {
        result[key] = value;
    }

    std::cout << "\nAfter merging override.env:" << std::endl;
    for (const auto& [key, value] : result) {
        std::cout << "  " << key << " = " << value << std::endl;
    }

    // Cleanup
    fs::remove(basePath);
    fs::remove(overridePath);
}

/**
 * @brief Demonstrates error handling
 *
 * Shows how to handle parsing errors gracefully.
 */
void errorHandlingExample() {
    printSeparator("Error Handling Example");

    Parser parser;

    // Malformed content
    std::string malformedContent = R"(
VALID_VAR=valueINVALID LINE WITHOUT EQUALSANOTHER_VALID=test
)";

    std::cout << "Parsing potentially malformed content..." << std::endl;

    try {
        auto result = parser.parse(malformedContent);
        std::cout << "Parsed " << result.size() << " valid variables"
                  << std::endl;
        for (const auto& [key, value] : result) {
            std::cout << "  " << key << " = " << value << std::endl;
        }
    } catch (const DotenvException& e) {
        std::cout << "Parse error: " << e.what() << std::endl;
    }

    // Non-existent file
    FileLoader loader;
    std::cout << "\nTrying to load non-existent file..." << std::endl;

    try {
        loader.load(fs::path("/nonexistent/path/.env"));
    } catch (const std::exception& e) {
        std::cout << "Load error: " << e.what() << std::endl;
    }
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main() {
    std::cout << "=================================================="
              << std::endl;
    std::cout << "  Atom Extra Dotenv Loader/Parser Examples" << std::endl;
    std::cout << "=================================================="
              << std::endl;

    try {
        // Parser examples
        basicParserExample();
        quotedValuesExample();
        multilineValuesExample();

        // Loader examples
        basicLoaderExample();
        variableExpansionExample();
        multipleFilesExample();

        // Error handling
        errorHandlingExample();

        std::cout << "\n=================================================="
                  << std::endl;
        std::cout << "  All examples completed successfully!" << std::endl;
        std::cout << "=================================================="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
