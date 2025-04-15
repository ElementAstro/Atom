#ifndef ATOM_TEST_REPORTER_HPP
#define ATOM_TEST_REPORTER_HPP

#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#else
#include "atom/type/json.hpp"
#endif

#include "atom/tests/test.hpp"
#include "atom/utils/color_print.hpp"

namespace atom::test {

/**
 * @brief 测试报告生成器接口
 * @details 定义了所有测试报告生成器必须实现的基本功能
 */
class TestReporter {
public:
    virtual ~TestReporter() = default;

    /**
     * @brief 在测试开始前调用
     * @param totalTests 要执行的测试总数
     */
    virtual void onTestRunStart(int totalTests) = 0;
    
    /**
     * @brief 在测试结束后调用
     * @param stats 测试统计信息
     */
    virtual void onTestRunEnd(const TestStats& stats) = 0;
    
    /**
     * @brief 在单个测试开始前调用
     * @param testCase 将要执行的测试用例
     */
    virtual void onTestStart(const TestCase& testCase) = 0;
    
    /**
     * @brief 在单个测试结束后调用
     * @param result 测试结果
     */
    virtual void onTestEnd(const TestResult& result) = 0;
    
    /**
     * @brief 生成最终报告
     * @param stats 测试统计信息
     * @param outputPath 输出路径
     */
    virtual void generateReport(const TestStats& stats, const std::string& outputPath) = 0;
};

/**
 * @brief 控制台测试报告生成器
 * @details 在控制台上实时显示测试进度和结果
 */
class ConsoleReporter : public TestReporter {
public:
    void onTestRunStart(int totalTests) override {
        std::cout << "开始执行 " << totalTests << " 个测试用例...\n";
        std::cout << "======================================================\n";
    }
    
    void onTestRunEnd(const TestStats& stats) override {
        std::cout << "======================================================\n";
        std::cout << "测试完成：共 " << stats.totalTests << " 个测试，"
                  << "通过 " << stats.passedAsserts << " 个断言，"
                  << "失败 " << stats.failedAsserts << " 个断言，"
                  << "跳过 " << stats.skippedTests << " 个测试\n";
                  
        if (stats.failedAsserts > 0) {
            std::cout << "\n失败的测试：\n";
            for (const auto& result : stats.results) {
                if (!result.passed && !result.skipped) {
                    std::cout << "- " << result.name << ": " << result.message << "\n";
                }
            }
        }
    }
    
    void onTestStart(const TestCase& testCase) override {
        std::cout << "执行测试：" << testCase.name << " ... ";
        std::cout.flush();
    }
    
    void onTestEnd(const TestResult& result) override {
        if (result.skipped) {
            ColorPrinter::printColored("跳过", ColorCode::Yellow);
        } else if (result.passed) {
            ColorPrinter::printColored("通过", ColorCode::Green);
        } else {
            ColorPrinter::printColored("失败", ColorCode::Red);
        }
        
        std::cout << " (" << result.duration << " ms)";
        
        if (!result.passed && !result.skipped) {
            std::cout << "\n    错误：" << result.message;
        }
        
        std::cout << "\n";
    }
    
    void generateReport(const TestStats& stats, const std::string& outputPath) override {
        // 控制台报告已经实时显示，不需要额外生成文件
        (void)stats;
        (void)outputPath;
    }
};

/**
 * @brief JSON格式测试报告生成器
 */
class JsonReporter : public TestReporter {
public:
    void onTestRunStart(int totalTests) override {
        (void)totalTests; // 不需要在JSON报告器中使用
    }
    
    void onTestRunEnd(const TestStats& stats) override {
        (void)stats; // 将在generateReport中使用
    }
    
    void onTestStart(const TestCase& testCase) override {
        (void)testCase; // 不需要在JSON报告器中使用
    }
    
    void onTestEnd(const TestResult& result) override {
        results_.push_back(result);
    }
    
    void generateReport(const TestStats& stats, const std::string& outputPath) override {
        nlohmann::json report;
        report["total_tests"] = stats.totalTests;
        report["total_asserts"] = stats.totalAsserts;
        report["passed_asserts"] = stats.passedAsserts;
        report["failed_asserts"] = stats.failedAsserts;
        report["skipped_tests"] = stats.skippedTests;
        
        report["results"] = nlohmann::json::array();
        for (const auto& result : results_) {
            nlohmann::json jsonResult;
            jsonResult["name"] = result.name;
            jsonResult["passed"] = result.passed;
            jsonResult["skipped"] = result.skipped;
            jsonResult["message"] = result.message;
            jsonResult["duration"] = result.duration;
            jsonResult["timed_out"] = result.timedOut;
            report["results"].push_back(jsonResult);
        }
        
        std::filesystem::path filePath(outputPath);
        if (std::filesystem::is_directory(filePath)) {
            filePath /= "test_report.json";
        }
        
        std::ofstream file(filePath);
        file << std::setw(4) << report << std::endl;
        std::cout << "JSON报告已保存到：" << filePath << std::endl;
    }
    
private:
    std::vector<TestResult> results_;
};

/**
 * @brief XML格式测试报告生成器
 */
class XmlReporter : public TestReporter {
public:
    void onTestRunStart(int totalTests) override {
        (void)totalTests; // 不需要在XML报告器中使用
    }
    
    void onTestRunEnd(const TestStats& stats) override {
        (void)stats; // 将在generateReport中使用
    }
    
    void onTestStart(const TestCase& testCase) override {
        (void)testCase; // 不需要在XML报告器中使用
    }
    
    void onTestEnd(const TestResult& result) override {
        results_.push_back(result);
    }
    
    void generateReport(const TestStats& stats, const std::string& outputPath) override {
        std::filesystem::path filePath(outputPath);
        if (std::filesystem::is_directory(filePath)) {
            filePath /= "test_report.xml";
        }
        
        std::ofstream file(filePath);
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        file << "<testsuites>\n";
        file << "    <testsuite name=\"AtomTests\" tests=\"" << stats.totalTests 
             << "\" failures=\"" << stats.failedAsserts 
             << "\" skipped=\"" << stats.skippedTests << "\">\n";
        
        for (const auto& result : results_) {
            file << "        <testcase name=\"" << result.name << "\" time=\"" << (result.duration / 1000.0) << "\"";
            
            if (result.skipped) {
                file << ">\n            <skipped/>\n        </testcase>\n";
            } else if (!result.passed) {
                file << ">\n            <failure message=\"" << result.message << "\"></failure>\n        </testcase>\n";
            } else {
                file << "/>\n";
            }
        }
        
        file << "    </testsuite>\n";
        file << "</testsuites>\n";
        
        std::cout << "XML报告已保存到：" << filePath << std::endl;
    }
    
private:
    std::vector<TestResult> results_;
};

/**
 * @brief HTML格式测试报告生成器
 */
class HtmlReporter : public TestReporter {
public:
    void onTestRunStart(int totalTests) override {
        (void)totalTests; // 不需要在HTML报告器中使用
    }
    
    void onTestRunEnd(const TestStats& stats) override {
        (void)stats; // 将在generateReport中使用
    }
    
    void onTestStart(const TestCase& testCase) override {
        (void)testCase; // 不需要在HTML报告器中使用
    }
    
    void onTestEnd(const TestResult& result) override {
        results_.push_back(result);
    }
    
    void generateReport(const TestStats& stats, const std::string& outputPath) override {
        std::filesystem::path filePath(outputPath);
        if (std::filesystem::is_directory(filePath)) {
            filePath /= "test_report.html";
        }
        
        std::ofstream file(filePath);
        file << "<!DOCTYPE html>\n";
        file << "<html lang=\"zh-CN\">\n";
        file << "<head>\n";
        file << "    <meta charset=\"UTF-8\">\n";
        file << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
        file << "    <title>Atom 测试报告</title>\n";
        file << "    <style>\n";
        file << "        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; }\n";
        file << "        h1 { color: #333; }\n";
        file << "        .summary { background-color: #f0f0f0; padding: 15px; border-radius: 5px; margin-bottom: 20px; }\n";
        file << "        .passed { color: green; }\n";
        file << "        .failed { color: red; }\n";
        file << "        .skipped { color: orange; }\n";
        file << "        table { width: 100%; border-collapse: collapse; }\n";
        file << "        th, td { text-align: left; padding: 8px; border-bottom: 1px solid #ddd; }\n";
        file << "        tr:hover { background-color: #f5f5f5; }\n";
        file << "        th { background-color: #4CAF50; color: white; }\n";
        file << "    </style>\n";
        file << "</head>\n";
        file << "<body>\n";
        file << "    <h1>Atom 测试报告</h1>\n";
        
        file << "    <div class=\"summary\">\n";
        file << "        <h2>测试总结</h2>\n";
        file << "        <p>总测试数: " << stats.totalTests << "</p>\n";
        file << "        <p>总断言数: " << stats.totalAsserts << "</p>\n";
        file << "        <p>通过断言: <span class=\"passed\">" << stats.passedAsserts << "</span></p>\n";
        file << "        <p>失败断言: <span class=\"failed\">" << stats.failedAsserts << "</span></p>\n";
        file << "        <p>跳过测试: <span class=\"skipped\">" << stats.skippedTests << "</span></p>\n";
        file << "    </div>\n";
        
        file << "    <h2>测试详情</h2>\n";
        file << "    <table>\n";
        file << "        <tr>\n";
        file << "            <th>测试名称</th>\n";
        file << "            <th>状态</th>\n";
        file << "            <th>持续时间 (ms)</th>\n";
        file << "            <th>消息</th>\n";
        file << "        </tr>\n";
        
        for (const auto& result : results_) {
            file << "        <tr>\n";
            file << "            <td>" << result.name << "</td>\n";
            
            if (result.skipped) {
                file << "            <td><span class=\"skipped\">跳过</span></td>\n";
            } else if (result.passed) {
                file << "            <td><span class=\"passed\">通过</span></td>\n";
            } else {
                file << "            <td><span class=\"failed\">失败</span></td>\n";
            }
            
            file << "            <td>" << result.duration << "</td>\n";
            file << "            <td>" << result.message << "</td>\n";
            file << "        </tr>\n";
        }
        
        file << "    </table>\n";
        file << "</body>\n";
        file << "</html>\n";
        
        std::cout << "HTML报告已保存到：" << filePath << std::endl;
    }
    
private:
    std::vector<TestResult> results_;
};

/**
 * @brief 创建对应格式的报告生成器
 * @param format 报告格式 ("console", "json", "xml", "html")
 * @return 对应格式的报告生成器的智能指针
 */
[[nodiscard]] inline auto createReporter(const std::string& format) -> std::unique_ptr<TestReporter> {
    if (format == "json") {
        return std::make_unique<JsonReporter>();
    } else if (format == "xml") {
        return std::make_unique<XmlReporter>();
    } else if (format == "html") {
        return std::make_unique<HtmlReporter>();
    }
    // 默认使用控制台报告器
    return std::make_unique<ConsoleReporter>();
}

}  // namespace atom::test

#endif  // ATOM_TEST_REPORTER_HPP