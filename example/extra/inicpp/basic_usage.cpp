#include "atom/extra/inicpp/inicpp.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace inicpp;

// Helper function to create test INI files
void create_test_ini_file(const std::string& filename,
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

int main() {
    try {
        std::cout << "=== inicpp Basic Usage Example ===" << std::endl;

        // Create test INI files
        create_test_ini_file("config.ini", R"(
# Application Configuration
[application]
name = MyApplication
version = 1.2.3
debug = true
max_connections = 100
timeout = 30.5

[database]
host = localhost
port = 5432
username = admin
password = secret123
ssl_enabled = true
connection_pool_size = 10

[logging]
level = info
file = /var/log/myapp.log
max_size = 10MB
rotate = true

[features]
feature_a = enabled
feature_b = disabled
feature_c = experimental
)");

        create_test_ini_file("advanced.ini", R"(
# Advanced Configuration with Comments
[server]
# Server binding configuration
bind_address = 0.0.0.0
bind_port = 8080
worker_threads = 4

# SSL Configuration
ssl_cert = /path/to/cert.pem
ssl_key = /path/to/key.pem
ssl_enabled = true

[cache]
# Redis cache configuration
redis_host = localhost
redis_port = 6379
redis_db = 0
ttl = 3600

[email]
smtp_host = smtp.example.com
smtp_port = 587
smtp_user = noreply@example.com
smtp_password = email_secret
use_tls = true
)");

        // 1. Basic file loading and reading
        std::cout << "\n1. Basic File Loading and Reading:" << std::endl;
        {
            IniFile ini;

            try {
                ini.load("config.ini");
                std::cout << "Successfully loaded config.ini" << std::endl;

                // Read string values
                auto app_name = ini["application"]["name"].as<std::string>();
                auto app_version =
                    ini["application"]["version"].as<std::string>();
                std::cout << "Application: " << app_name << " v" << app_version
                          << std::endl;

                // Read boolean values
                auto debug_mode = ini["application"]["debug"].as<bool>();
                std::cout << "Debug mode: "
                          << (debug_mode ? "enabled" : "disabled") << std::endl;

                // Read integer values
                auto max_connections =
                    ini["application"]["max_connections"].as<int>();
                std::cout << "Max connections: " << max_connections
                          << std::endl;

                // Read floating point values
                auto timeout = ini["application"]["timeout"].as<double>();
                std::cout << "Timeout: " << timeout << " seconds" << std::endl;

            } catch (const std::exception& e) {
                std::cerr << "Error loading config.ini: " << e.what()
                          << std::endl;
            }
        }

        // 2. Type-safe field access
        std::cout << "\n2. Type-Safe Field Access:" << std::endl;
        {
            IniFile ini;
            ini.load("config.ini");

            // Database configuration
            std::cout << "Database Configuration:" << std::endl;
            try {
                auto db_host = ini["database"]["host"].as<std::string>();
                auto db_port = ini["database"]["port"].as<int>();
                auto db_user = ini["database"]["username"].as<std::string>();
                auto ssl_enabled = ini["database"]["ssl_enabled"].as<bool>();
                auto pool_size =
                    ini["database"]["connection_pool_size"].as<int>();

                std::cout << "  Host: " << db_host << std::endl;
                std::cout << "  Port: " << db_port << std::endl;
                std::cout << "  User: " << db_user << std::endl;
                std::cout << "  SSL: " << (ssl_enabled ? "enabled" : "disabled")
                          << std::endl;
                std::cout << "  Pool size: " << pool_size << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error reading database config: " << e.what()
                          << std::endl;
            }
        }

        // 3. Default values and error handling
        std::cout << "\n3. Default Values and Error Handling:" << std::endl;
        {
            IniFile ini;
            ini.load("config.ini");

            // Use default values for missing fields
            auto retry_count = ini["application"].get("retry_count", 3);
            auto backup_enabled = ini["database"].get("backup_enabled", false);
            auto log_format = ini["logging"].get("format", std::string("json"));

            std::cout << "Retry count (default): " << retry_count << std::endl;
            std::cout << "Backup enabled (default): "
                      << (backup_enabled ? "yes" : "no") << std::endl;
            std::cout << "Log format (default): " << log_format << std::endl;

            // Check if fields exist
            if (ini["application"].has("name")) {
                std::cout << "Application name field exists" << std::endl;
            }

            if (!ini["application"].has("non_existent_field")) {
                std::cout << "Non-existent field correctly not found"
                          << std::endl;
            }
        }

        // 4. Writing and modifying INI files
        std::cout << "\n4. Writing and Modifying INI Files:" << std::endl;
        {
            IniFile ini;

            // Create new configuration
            ini["application"]["name"] = "ModifiedApp";
            ini["application"]["version"] = "2.0.0";
            ini["application"]["debug"] = false;
            ini["application"]["new_feature"] = true;

            ini["database"]["host"] = "db.example.com";
            ini["database"]["port"] = 3306;
            ini["database"]["type"] = "mysql";

            ini["cache"]["enabled"] = true;
            ini["cache"]["ttl"] = 7200;
            ini["cache"]["max_size"] = "256MB";

            // Save to file
            ini.save("modified_config.ini");
            std::cout << "Saved modified configuration to modified_config.ini"
                      << std::endl;

            // Verify by loading and reading
            IniFile verify_ini;
            verify_ini.load("modified_config.ini");

            std::cout << "Verification:" << std::endl;
            std::cout << "  App name: "
                      << verify_ini["application"]["name"].as<std::string>()
                      << std::endl;
            std::cout << "  App version: "
                      << verify_ini["application"]["version"].as<std::string>()
                      << std::endl;
            std::cout << "  Debug: "
                      << verify_ini["application"]["debug"].as<bool>()
                      << std::endl;
            std::cout << "  DB type: "
                      << verify_ini["database"]["type"].as<std::string>()
                      << std::endl;
        }

        // 5. Section iteration
        std::cout << "\n5. Section Iteration:" << std::endl;
        {
            IniFile ini;
            ini.load("config.ini");

            std::cout << "All sections in config.ini:" << std::endl;
            for (const auto& [section_name, section] : ini) {
                std::cout << "  [" << section_name << "]" << std::endl;

                // Iterate through fields in each section
                for (const auto& [field_name, field] : section) {
                    std::cout << "    " << field_name << " = "
                              << field.as<std::string>() << std::endl;
                }
            }
        }

        // 6. Custom separators and comments
        std::cout << "\n6. Custom Separators and Comments:" << std::endl;
        {
            // Create INI file with custom format
            create_test_ini_file("custom_format.ini", R"(
// Custom comment style
[server]
host : localhost    // Using colon as separator
port : 8080
workers : 4

[database]
url : postgresql://localhost:5432/mydb
timeout : 30
)");

            IniFile ini;
            ini.setFieldSeparator(':');
            ini.setCommentPrefixes({"//", "#"});

            try {
                ini.load("custom_format.ini");
                std::cout << "Loaded custom format INI file" << std::endl;

                auto host = ini["server"]["host"].as<std::string>();
                auto port = ini["server"]["port"].as<int>();
                auto workers = ini["server"]["workers"].as<int>();

                std::cout << "Server config:" << std::endl;
                std::cout << "  Host: " << host << std::endl;
                std::cout << "  Port: " << port << std::endl;
                std::cout << "  Workers: " << workers << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error with custom format: " << e.what()
                          << std::endl;
            }
        }

        // 7. Multi-line values
        std::cout << "\n7. Multi-line Values:" << std::endl;
        {
            create_test_ini_file("multiline.ini", R"(
[description]
short = Single line description
long = This is a very long description
	that spans multiple lines
	and contains detailed information
	about the application

[sql]
query = SELECT * FROM users
	WHERE active = true
	AND created_at > '2023-01-01'
	ORDER BY name
)");

            IniFile ini;
            ini.setMultiLineValues(true);

            try {
                ini.load("multiline.ini");
                std::cout << "Loaded multi-line INI file" << std::endl;

                auto short_desc = ini["description"]["short"].as<std::string>();
                auto long_desc = ini["description"]["long"].as<std::string>();
                auto sql_query = ini["sql"]["query"].as<std::string>();

                std::cout << "Short description: " << short_desc << std::endl;
                std::cout << "Long description: " << long_desc << std::endl;
                std::cout << "SQL query: " << sql_query << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error with multi-line values: " << e.what()
                          << std::endl;
            }
        }

        // 8. Case-sensitive vs case-insensitive
        std::cout << "\n8. Case Sensitivity:" << std::endl;
        {
            create_test_ini_file("case_test.ini", R"(
[Section]
Key = value1

[section]
key = value2
KEY = value3
)");

            // Case-sensitive (default)
            std::cout << "Case-sensitive mode:" << std::endl;
            IniFile case_sensitive;
            case_sensitive.load("case_test.ini");

            for (const auto& [section_name, section] : case_sensitive) {
                std::cout << "  [" << section_name << "]" << std::endl;
                for (const auto& [field_name, field] : section) {
                    std::cout << "    " << field_name << " = "
                              << field.as<std::string>() << std::endl;
                }
            }

            // Case-insensitive
            std::cout << "\nCase-insensitive mode:" << std::endl;
            IniFileCaseInsensitive case_insensitive;
            case_insensitive.load("case_test.ini");

            for (const auto& [section_name, section] : case_insensitive) {
                std::cout << "  [" << section_name << "]" << std::endl;
                for (const auto& [field_name, field] : section) {
                    std::cout << "    " << field_name << " = "
                              << field.as<std::string>() << std::endl;
                }
            }
        }

        // 9. Error handling and validation
        std::cout << "\n9. Error Handling and Validation:" << std::endl;
        {
            create_test_ini_file("invalid.ini", R"(
[valid_section]
valid_key = valid_value

[invalid section with spaces
missing_bracket = value

valid_section2]
another_key = another_value
)");

            IniFile ini;
            try {
                ini.load("invalid.ini");
                std::cout << "Loaded file with some invalid content"
                          << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Expected error loading invalid INI: " << e.what()
                          << std::endl;
            }

            // Test type conversion errors
            ini["test"]["number"] = "not_a_number";
            try {
                auto number = ini["test"]["number"].as<int>();
                std::cout << "Number: " << number << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Expected type conversion error: " << e.what()
                          << std::endl;
            }
        }

        // Cleanup test files
        std::cout << "\nCleaning up test files..." << std::endl;
        std::filesystem::remove("config.ini");
        std::filesystem::remove("advanced.ini");
        std::filesystem::remove("modified_config.ini");
        std::filesystem::remove("custom_format.ini");
        std::filesystem::remove("multiline.ini");
        std::filesystem::remove("case_test.ini");
        std::filesystem::remove("invalid.ini");

        std::cout << "\n=== inicpp Basic Usage Example Completed ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
