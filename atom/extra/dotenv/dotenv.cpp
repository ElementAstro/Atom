#include "dotenv.hpp"
#include "exceptions.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace dotenv {

Dotenv::Dotenv(const DotenvOptions& options) : options_(options) {
    initializeComponents();
}

void Dotenv::initializeComponents() {
    parser_ = std::make_unique<Parser>(options_.parse_options);
    validator_ = std::make_unique<Validator>();
    loader_ = std::make_unique<FileLoader>(options_.load_options);
}

LoadResult Dotenv::load(const std::filesystem::path& filepath) {
    LoadResult result;

    try {
        log("Loading environment variables from: " + filepath.string());

        std::string content = loader_->load(filepath);
        result = processLoadedContent(content, {filepath});
        result.loaded_files.push_back(filepath);

        log("Successfully loaded " + std::to_string(result.variables.size()) +
            " variables");

    } catch (const std::exception& e) {
        result.addError("Failed to load " + filepath.string() + ": " +
                        e.what());
        log("Error: " + std::string(e.what()));
    }

    return result;
}

LoadResult Dotenv::loadMultiple(
    const std::vector<std::filesystem::path>& filepaths) {
    LoadResult combined_result;

    for (const auto& filepath : filepaths) {
        LoadResult single_result = load(filepath);

        // Merge results
        for (const auto& [key, value] : single_result.variables) {
            if (!options_.load_options.override_existing &&
                combined_result.variables.find(key) !=
                    combined_result.variables.end()) {
                combined_result.addWarning("Variable '" + key +
                                           "' already exists, skipping from " +
                                           filepath.string());
            } else {
                combined_result.variables[key] = value;
            }
        }

        // Merge errors and warnings
        combined_result.errors.insert(combined_result.errors.end(),
                                      single_result.errors.begin(),
                                      single_result.errors.end());
        combined_result.warnings.insert(combined_result.warnings.end(),
                                        single_result.warnings.begin(),
                                        single_result.warnings.end());
        combined_result.loaded_files.insert(combined_result.loaded_files.end(),
                                            single_result.loaded_files.begin(),
                                            single_result.loaded_files.end());

        if (!single_result.success) {
            combined_result.success = false;
        }
    }

    return combined_result;
}

LoadResult Dotenv::autoLoad(const std::filesystem::path& base_path) {
    LoadResult result;

    try {
        log("Auto-discovering .env files from: " + base_path.string());

        std::string content = loader_->autoLoad(base_path);
        result = processLoadedContent(content);

        log("Auto-loaded " + std::to_string(result.variables.size()) +
            " variables");

    } catch (const std::exception& e) {
        result.addError("Auto-load failed: " + std::string(e.what()));
        log("Error: " + std::string(e.what()));
    }

    return result;
}

LoadResult Dotenv::loadFromString(const std::string& content) {
    return processLoadedContent(content);
}

LoadResult Dotenv::loadAndValidate(const std::filesystem::path& filepath,
                                   const ValidationSchema& schema) {
    LoadResult result = load(filepath);

    if (result.success) {
        ValidationResult validation =
            validator_->validateWithDefaults(result.variables, schema);

        if (!validation.is_valid) {
            for (const auto& error : validation.errors) {
                result.addError("Validation: " + error);
            }
        }

        // Update with processed variables (including defaults)
        result.variables = validation.processed_vars;
    }

    return result;
}

void Dotenv::applyToEnvironment(
    const std::unordered_map<std::string, std::string>& variables,
    bool override_existing) {
    for (const auto& [key, value] : variables) {
        if (!override_existing && std::getenv(key.c_str()) != nullptr) {
            log("Skipping existing environment variable: " + key);
            continue;
        }

#ifdef _WIN32
        std::string env_string = key + "=" + value;
        if (_putenv(env_string.c_str()) != 0) {
            log("Warning: Failed to set environment variable: " + key);
        }
#else
        if (setenv(key.c_str(), value.c_str(), override_existing ? 1 : 0) !=
            0) {
            log("Warning: Failed to set environment variable: " + key);
        }
#endif
        else {
            log("Set environment variable: " + key);
        }
    }
}

void Dotenv::save(
    const std::filesystem::path& filepath,
    const std::unordered_map<std::string, std::string>& variables) {
    try {
        loader_->save(filepath, variables);
        log("Saved " + std::to_string(variables.size()) +
            " variables to: " + filepath.string());
    } catch (const std::exception& e) {
        throw FileException("Failed to save to " + filepath.string() + ": " +
                            e.what());
    }
}

void Dotenv::watch(const std::filesystem::path& filepath,
                   std::function<void(const LoadResult&)> callback) {
    if (watching_) {
        stopWatching();
    }

    watching_ = true;
    watcher_thread_ = std::make_unique<std::thread>([this, filepath,
                                                     callback]() {
        std::filesystem::file_time_type last_write_time;

        try {
            if (std::filesystem::exists(filepath)) {
                last_write_time = loader_->getModificationTime(filepath);
            }
        } catch (const std::exception& e) {
            log("Warning: Cannot get initial modification time: " +
                std::string(e.what()));
        }

        while (watching_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

            if (!watching_)
                break;

            try {
                if (std::filesystem::exists(filepath)) {
                    auto current_time = loader_->getModificationTime(filepath);

                    if (current_time != last_write_time) {
                        log("File changed, reloading: " + filepath.string());

                        LoadResult result = load(filepath);
                        callback(result);

                        last_write_time = current_time;
                    }
                }
            } catch (const std::exception& e) {
                log("Error during file watching: " + std::string(e.what()));
            }
        }
    });
}

void Dotenv::stopWatching() {
    if (watching_) {
        watching_ = false;
        if (watcher_thread_ && watcher_thread_->joinable()) {
            watcher_thread_->join();
        }
        watcher_thread_.reset();
        log("Stopped watching for file changes");
    }
}

LoadResult Dotenv::processLoadedContent(
    const std::string& content,
    const std::vector<std::filesystem::path>& source_files) {
    LoadResult result;

    try {
        result.variables = parser_->parse(content);
        result.loaded_files = source_files;
        log("Parsed " + std::to_string(result.variables.size()) + " variables");
    } catch (const std::exception& e) {
        result.addError("Parse error: " + std::string(e.what()));
    }

    return result;
}

void Dotenv::log(const std::string& message) {
    if (options_.debug) {
        if (options_.logger) {
            options_.logger("[dotenv] " + message);
        } else {
            std::cout << "[dotenv] " << message << std::endl;
        }
    }
}

// Static convenience methods
LoadResult Dotenv::quickLoad(const std::filesystem::path& filepath) {
    Dotenv dotenv;
    return dotenv.load(filepath);
}

void Dotenv::config(const std::filesystem::path& filepath,
                    bool override_existing) {
    Dotenv dotenv;
    LoadResult result = dotenv.load(filepath);

    if (result.success) {
        dotenv.applyToEnvironment(result.variables, override_existing);
    } else {
        throw DotenvException(
            "Configuration failed: " +
            (result.errors.empty() ? "Unknown error" : result.errors[0]));
    }
}

}  // namespace dotenv
