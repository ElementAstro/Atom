#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "codegen/codegen.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "types/typechecker.h"
#include "types/error_reporter.h"
#include "types/type_registry.h"
#include "vm/vm.h"

using namespace tsx;

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Could not open file: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int runFile(const std::string& path, bool typeCheckOnly = false, 
            bool generateHtmlReport = false) {
    try {
        // 读取文件
        std::string source = readFile(path);

        // 词法分析
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.tokenize();

        // 语法分析
        Parser parser(tokens);
        auto program = parser.parse();
        
        // 创建类型注册表
        TypeRegistry typeRegistry;
        
        // 类型检查
        TypeChecker typeChecker;
        typeChecker.checkProgram(program.get());
        
        // 错误报告
        ErrorReporter reporter(path);
        const auto& errors = typeChecker.getErrors();
        
        if (!errors.empty()) {
            reporter.reportErrors(typeChecker);
            
            // 如果需要，生成HTML错误报告
            if (generateHtmlReport) {
                std::string htmlPath = path + ".type-errors.html";
                reporter.saveHTMLReport(typeChecker, htmlPath);
                std::cout << "HTML error report saved to: " << htmlPath << std::endl;
            }
            
            // 如果存在类型错误，返回错误代码
            return 1;
        }
        
        // 如果只进行类型检查，这里退出
        if (typeCheckOnly) {
            std::cout << "Type check passed with no errors." << std::endl;
            return 0;
        }

        // 代码生成
        CodeGenerator codegen;
        Function* mainFunction = codegen.compile(program.get());

        // 执行
        VirtualMachine vm;
        vm.execute(mainFunction, {}, nullptr);

        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}

void repl() {
    std::string line;
    VirtualMachine vm;
    TypeChecker typeChecker;
    
    std::cout << "TypeScript-like REPL (with type checking)" << std::endl;
    std::cout << "Type .exit to quit, .typeson to enable type checking, .typeoff to disable" << std::endl;
    
    bool typeCheckEnabled = true;

    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            std::cout << std::endl;
            break;
        }

        if (line.empty()) continue;
        
        // REPL命令处理
        if (line == ".exit") {
            break;
        } else if (line == ".typeson") {
            typeCheckEnabled = true;
            std::cout << "Type checking enabled" << std::endl;
            continue;
        } else if (line == ".typeoff") {
            typeCheckEnabled = false;
            std::cout << "Type checking disabled" << std::endl;
            continue;
        }

        try {
            // 词法分析
            Lexer lexer(line);
            std::vector<Token> tokens = lexer.tokenize();

            // 语法分析
            Parser parser(tokens);
            auto program = parser.parse();
            
            // 类型检查（如果启用）
            if (typeCheckEnabled) {
                typeChecker.checkProgram(program.get());
                
                // 检查是否有类型错误
                const auto& errors = typeChecker.getErrors();
                if (!errors.empty()) {
                    for (const auto& error : errors) {
                        std::cerr << "Type error: " << error.message << std::endl;
                    }
                    continue; // 有错误，不执行
                }
            }
            
            // 执行表达式
            CodeGenerator codegen;
            Function* mainFunction = codegen.compile(program.get());
            vm.execute(mainFunction, {}, nullptr);
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }
}

void printUsage(const std::string& programName) {
    std::cout << "Usage: " << programName << " [options] [script]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --help             Show this help message" << std::endl;
    std::cout << "  --version          Show version information" << std::endl;
    std::cout << "  --typecheck        Only perform type checking without execution" << std::endl;
    std::cout << "  --html-report      Generate HTML error report if type errors are found" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        // 无参数，运行REPL
        repl();
        return 0;
    }
    
    // 解析命令行选项
    bool typeCheckOnly = false;
    bool generateHtmlReport = false;
    std::string scriptPath;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--version") {
            std::cout << "TypeScript-like Interpreter v1.0" << std::endl;
            return 0;
        } else if (arg == "--typecheck") {
            typeCheckOnly = true;
        } else if (arg == "--html-report") {
            generateHtmlReport = true;
        } else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        } else {
            // 假定是脚本路径
            scriptPath = arg;
        }
    }
    
    if (scriptPath.empty()) {
        std::cerr << "No script file specified" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // 运行脚本文件
    return runFile(scriptPath, typeCheckOnly, generateHtmlReport);
}