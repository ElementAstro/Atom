#pragma once

#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace tsx {

// TypeScript配置类，类似于tsconfig.json
class TSConfig {
public:
    struct CompilerOptions {
        bool strict = false;
        bool noImplicitAny = false;
        bool strictNullChecks = false;
        bool strictFunctionTypes = false;
        bool strictPropertyInitialization = false;
        bool noImplicitThis = false;
        bool noImplicitReturns = false;
        bool noUnusedLocals = false;
        bool noUnusedParameters = false;
        std::string target = "es2015";
        std::string module = "commonjs";
        std::vector<std::string> lib;
        std::optional<std::string> outDir;
        std::optional<std::string> rootDir;
    };
    
    TSConfig() = default;
    
    // 从文件加载配置
    bool loadFromFile(const std::string& path) {
        try {
            std::ifstream file(path);
            if (!file) {
                std::cerr << "Could not open config file: " << path << std::endl;
                return false;
            }
            
            nlohmann::json config;
            file >> config;
            
            // 解析include列表
            if (config.contains("include") && config["include"].is_array()) {
                for (const auto& item : config["include"]) {
                    if (item.is_string()) {
                        include.push_back(item.get<std::string>());
                    }
                }
            }
            
            // 解析exclude列表
            if (config.contains("exclude") && config["exclude"].is_array()) {
                for (const auto& item : config["exclude"]) {
                    if (item.is_string()) {
                        exclude.push_back(item.get<std::string>());
                    }
                }
            }
            
            // 解析编译器选项
            if (config.contains("compilerOptions") && config["compilerOptions"].is_object()) {
                auto& opts = config["compilerOptions"];
                
                if (opts.contains("strict") && opts["strict"].is_boolean()) {
                    compilerOptions.strict = opts["strict"].get<bool>();
                }
                
                if (opts.contains("noImplicitAny") && opts["noImplicitAny"].is_boolean()) {
                    compilerOptions.noImplicitAny = opts["noImplicitAny"].get<bool>();
                }
                
                if (opts.contains("strictNullChecks") && opts["strictNullChecks"].is_boolean()) {
                    compilerOptions.strictNullChecks = opts["strictNullChecks"].get<bool>();
                }
                
                if (opts.contains("target") && opts["target"].is_string()) {
                    compilerOptions.target = opts["target"].get<std::string>();
                }
                
                if (opts.contains("module") && opts["module"].is_string()) {
                    compilerOptions.module = opts["module"].get<std::string>();
                }
                
                if (opts.contains("outDir") && opts["outDir"].is_string()) {
                    compilerOptions.outDir = opts["outDir"].get<std::string>();
                }
                
                if (opts.contains("rootDir") && opts["rootDir"].is_string()) {
                    compilerOptions.rootDir = opts["rootDir"].get<std::string>();
                }
                
                if (opts.contains("lib") && opts["lib"].is_array()) {
                    for (const auto& item : opts["lib"]) {
                        if (item.is_string()) {
                            compilerOptions.lib.push_back(item.get<std::string>());
                        }
                    }
                }
            }
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error parsing config file: " << e.what() << std::endl;
            return false;
        }
    }
    
    // 创建并保存默认配置
    static bool createDefaultConfig(const std::string& path) {
        nlohmann::json config;
        
        config["compilerOptions"] = {
            {"target", "es2015"},
            {"module", "commonjs"},
            {"strict", true},
            {"noImplicitAny", true},
            {"strictNullChecks", true},
            {"outDir", "./dist"},
            {"lib", {"dom", "es2015"}}
        };
        
        config["include"] = {"src/**/*"};
        config["exclude"] = {"node_modules", "**/*.spec.ts"};
        
        try {
            std::ofstream file(path);
            if (!file) {
                std::cerr << "Could not create config file: " << path << std::endl;
                return false;
            }
            
            file << config.dump(2);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error creating config file: " << e.what() << std::endl;
            return false;
        }
    }
    
    // 获取配置选项
    const CompilerOptions& getCompilerOptions() const {
        return compilerOptions;
    }
    
    const std::vector<std::string>& getInclude() const {
        return include;
    }
    
    const std::vector<std::string>& getExclude() const {
        return exclude;
    }
    
private:
    std::vector<std::string> include;
    std::vector<std::string> exclude;
    CompilerOptions compilerOptions;
};

} // namespace tsx
