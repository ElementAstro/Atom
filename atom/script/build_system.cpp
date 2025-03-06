#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "types/typechecker_module.h"

namespace tsx {

class BuildSystem {
public:
    BuildSystem() : 
        verbose(false), 
        failOnError(true),
        generateHtmlReports(false) {}
    
    // 设置配置选项
    void setVerbose(bool v) { verbose = v; }
    void setFailOnError(bool f) { failOnError = f; }
    void setGenerateHtmlReports(bool g) { generateHtmlReports = g; }
    void setOutputDir(const std::string& dir) { outputDir = dir; }
    
    // 添加源文件或目录
    void addSource(const std::string& path) {
        sourcePaths.push_back(path);
    }
    
    // 运行构建
    bool build() {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // 收集所有源文件
        std::vector<std::string> files = collectSourceFiles();
        
        if (verbose) {
            std::cout << "Found " << files.size() << " source files." << std::endl;
        }
        
        int errorCount = 0;
        int fileCount = 0;
        
        // 处理每个文件
        for (const auto& file : files) {
            fileCount++;
            if (verbose) {
                std::cout << "Processing [" << fileCount << "/" << files.size() 
                          << "]: " << file << std::endl;
            }
            
            if (!typeCheckFile(file) && failOnError) {
                std::cerr << "Build failed due to type errors in " << file << std::endl;
                return false;
            }
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
        
        std::cout << "Build completed in " << duration << "ms. "
                  << "Processed " << fileCount << " files with "
                  << errorCount << " errors." << std::endl;
        
        return errorCount == 0;
    }
    
private:
    bool verbose;
    bool failOnError;
    bool generateHtmlReports;
    std::string outputDir;
    std::vector<std::string> sourcePaths;
    
    // 收集所有源文件
    std::vector<std::string> collectSourceFiles() {
        std::vector<std::string> files;
        
        for (const auto& path : sourcePaths) {
            if (std::filesystem::is_directory(path)) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                    if (entry.is_regular_file() && isSourceFile(entry.path().string())) {
                        files.push_back(entry.path().string());
                    }
                }
            } else if (isSourceFile(path)) {
                files.push_back(path);
            }
        }
        
        return files;
    }
    
    // 判断是否是源文件
    bool isSourceFile(const std::string& path) {
        std::string ext = std::filesystem::path(path).extension().string();
        return ext == ".ts" || ext == ".tsx" || ext == ".js";
    }
    
    // 对单个文件进行类型检查
    bool typeCheckFile(const std::string& path) {
        try {
            // 读取文件
            std::ifstream file(path);
            if (!file) {
                std::cerr << "Error: Could not open file: " << path << std::endl;
                return false;
            }
            
            std::string source((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            
            // 词法分析
            Lexer lexer(source);
            auto tokens = lexer.tokenize();
            
            // 语法分析
            Parser parser(tokens);
            auto program = parser.parse();
            
            // 类型检查
            TypeCheckerModule typeCheckerModule;
            bool success = typeCheckerModule.checkProgram(program.get());
            
            if (!success) {
                std::cerr << "Type errors in " << path << ":" << std::endl;
                typeCheckerModule.reportErrors(path);
                
                // 生成HTML报告
                if (generateHtmlReports) {
                    std::string htmlPath;
                    if (!outputDir.empty()) {
                        std::filesystem::create_directories(outputDir);
                        htmlPath = outputDir + "/" + 
                                   std::filesystem::path(path).filename().string() + 
                                   ".type-errors.html";
                    } else {
                        htmlPath = path + ".type-errors.html";
                    }
                    
                    typeCheckerModule.generateHTMLReport(htmlPath);
                    if (verbose) {
                        std::cout << "Generated error report: " << htmlPath << std::endl;
                    }
                }
            }
            
            return success;
        } catch (const std::exception& e) {
            std::cerr << "Error processing file " << path << ": " << e.what() << std::endl;
            return false;
        }
    }
};

} // namespace tsx
