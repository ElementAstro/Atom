#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "../ast/ast.h"
#include "typechecker.h"

namespace tsx {

class ErrorReporter {
public:
    // 构造函数，可选地接收源代码文件路径
    explicit ErrorReporter(const std::string& sourcePath = "")
        : sourcePath(sourcePath) {
        if (!sourcePath.empty()) {
            loadSourceFile(sourcePath);
        }
    }

    // 加载源代码文件
    bool loadSourceFile(const std::string& path) {
        sourcePath = path;
        sourceLines.clear();

        std::ifstream file(path);
        if (!file) {
            std::cerr << "Error: Could not open source file: " << path
                      << std::endl;
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            sourceLines.push_back(line);
        }

        return true;
    }

    // 从TypeChecker获取并报告所有错误
    void reportErrors(const TypeChecker& checker) {
        const auto& errors = checker.getErrors();
        if (errors.empty()) {
            std::cout << "No type errors found." << std::endl;
            return;
        }

        std::cout << "Found " << errors.size()
                  << " type error(s):" << std::endl;

        // 按文件位置排序错误
        std::map<int, std::vector<const TypeError*>> errorsByLine;
        for (const auto& error : errors) {
            errorsByLine[error.position.line].push_back(&error);
        }

        // 逐行报告错误
        for (const auto& [line, lineErrors] : errorsByLine) {
            // 打印源代码行（如果有）
            if (!sourceLines.empty() && line > 0 &&
                line <= static_cast<int>(sourceLines.size())) {
                std::cout << "Line " << line << ": " << sourceLines[line - 1]
                          << std::endl;

                // 打印错误位置指示
                for (const auto* error : lineErrors) {
                    std::string marker(error->position.column - 1, ' ');
                    marker += "^";
                    std::cout << marker << " "
                              << getErrorKindString(error->kind) << ": "
                              << error->message << std::endl;
                }
            } else {
                // 无法显示源代码行
                for (const auto* error : lineErrors) {
                    std::cout << "Line " << line << ", Column "
                              << error->position.column << ": "
                              << getErrorKindString(error->kind) << ": "
                              << error->message << std::endl;
                }
            }
            std::cout << std::endl;
        }
    }

    // 生成HTML格式的错误报告
    std::string generateHTMLReport(const TypeChecker& checker) {
        const auto& errors = checker.getErrors();
        std::stringstream html;

        html << "<!DOCTYPE html>\n"
             << "<html>\n"
             << "<head>\n"
             << "  <title>TypeScript Type Error Report</title>\n"
             << "  <style>\n"
             << "    body { font-family: Arial, sans-serif; margin: 20px; }\n"
             << "    .error-count { font-weight: bold; margin-bottom: 10px; }\n"
             << "    .error { margin-bottom: 20px; border-left: 3px solid "
                "#ff5555; padding-left: 10px; }\n"
             << "    .error-location { color: #777; }\n"
             << "    .error-kind { font-weight: bold; color: #ff5555; }\n"
             << "    .error-message { margin-bottom: 5px; }\n"
             << "    .source-code { background-color: #f5f5f5; padding: 10px; "
                "border-radius: 3px; }\n"
             << "    .error-marker { color: #ff5555; }\n"
             << "  </style>\n"
             << "</head>\n"
             << "<body>\n";

        html << "  <h1>Type Error Report</h1>\n";
        html << "  <div class=\"error-count\">" << errors.size()
             << " error(s) found</div>\n";

        // 按文件位置排序错误
        std::map<int, std::vector<const TypeError*>> errorsByLine;
        for (const auto& error : errors) {
            errorsByLine[error.position.line].push_back(&error);
        }

        // 逐行报告错误
        for (const auto& [line, lineErrors] : errorsByLine) {
            html << "  <div class=\"error\">\n";

            // 打印源代码行（如果有）
            if (!sourceLines.empty() && line > 0 &&
                line <= static_cast<int>(sourceLines.size())) {
                html << "    <div class=\"source-code\">\n";
                html << "      <code>" << escapeHTML(sourceLines[line - 1])
                     << "</code>\n";

                // 打印错误位置指示
                for (const auto* error : lineErrors) {
                    std::string marker(error->position.column - 1, ' ');
                    marker += "^";
                    html << "      <br><code class=\"error-marker\">" << marker
                         << "</code>\n";
                }
                html << "    </div>\n";
            }

            for (const auto* error : lineErrors) {
                html << "    <div class=\"error-location\">Line " << line
                     << ", Column " << error->position.column << "</div>\n";
                html << "    <div class=\"error-kind\">"
                     << getErrorKindString(error->kind) << "</div>\n";
                html << "    <div class=\"error-message\">"
                     << escapeHTML(error->message) << "</div>\n";
            }

            html << "  </div>\n";
        }

        html << "</body>\n</html>";

        return html.str();
    }

    // 将HTML报告保存到文件
    bool saveHTMLReport(const TypeChecker& checker,
                        const std::string& outputPath) {
        std::string html = generateHTMLReport(checker);
        std::ofstream file(outputPath);
        if (!file) {
            std::cerr << "Error: Could not create output file: " << outputPath
                      << std::endl;
            return false;
        }

        file << html;
        return true;
    }

private:
    std::string sourcePath;
    std::vector<std::string> sourceLines;

    // 将错误类型转换为字符串
    std::string getErrorKindString(TypeError::ErrorKind kind) const {
        switch (kind) {
            case TypeError::ErrorKind::Incompatible:
                return "Type Error";
            case TypeError::ErrorKind::Undefined:
                return "Undefined";
            case TypeError::ErrorKind::Generic:
                return "Generic Error";
            case TypeError::ErrorKind::TooFewArguments:
                return "Too Few Arguments";
            case TypeError::ErrorKind::TooManyArguments:
                return "Too Many Arguments";
            case TypeError::ErrorKind::PropertyNotExist:
                return "Property Not Exist";
            case TypeError::ErrorKind::NotCallable:
                return "Not Callable";
            case TypeError::ErrorKind::InvalidOperation:
                return "Invalid Operation";
            default:
                return "Type Error";
        }
    }

    // HTML转义字符
    std::string escapeHTML(const std::string& str) const {
        std::string result;
        for (char c : str) {
            switch (c) {
                case '&':
                    result += "&amp;";
                    break;
                case '<':
                    result += "&lt;";
                    break;
                case '>':
                    result += "&gt;";
                    break;
                case '"':
                    result += "&quot;";
                    break;
                case '\'':
                    result += "&#39;";
                    break;
                default:
                    result += c;
                    break;
            }
        }
        return result;
    }
};

}  // namespace tsx