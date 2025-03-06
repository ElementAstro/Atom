#pragma once

#include <memory>
#include <string>
#include <vector>
#include "../ast/ast.h"
#include "error_reporter.h"
#include "type_analyzer.h"
#include "type_hierarchy.h"
#include "type_inference.h"
#include "type_registry.h"
#include "typechecker.h"

namespace tsx {

// 类型检查模块，整合所有类型系统相关功能
class TypeCheckerModule {
public:
    TypeCheckerModule()
        : typeChecker(std::make_unique<TypeChecker>()),
          typeRegistry(std::make_unique<TypeRegistry>()),
          typeInference(std::make_unique<TypeInference>(*typeChecker)),
          typeAnalyzer(std::make_unique<TypeAnalyzer>(*typeChecker)),
          typeHierarchy(std::make_unique<TypeHierarchy>(*typeChecker)) {
        // 初始化时将类型注册表中的类型添加到检查器
        initializeBuiltinTypes();
    }

    // 检查整个程序
    bool checkProgram(const AST::Program* program) {
        typeChecker->checkProgram(program);
        return typeChecker->getErrors().empty();
    }

    // 生成错误报告
    void reportErrors(const std::string& sourcePath = "") {
        ErrorReporter reporter(sourcePath);
        reporter.reportErrors(*typeChecker);
    }

    // 生成HTML错误报告
    bool generateHTMLReport(const std::string& outputPath) {
        ErrorReporter reporter;
        return reporter.saveHTMLReport(*typeChecker, outputPath);
    }

    // 获取类型错误
    const std::vector<TypeError>& getErrors() const {
        return typeChecker->getErrors();
    }

    // 获取表达式的类型
    std::unique_ptr<Type> getExpressionType(const AST::Expression* expr) {
        return typeChecker->getExpressionType(expr);
    }

    // 检查表达式的类型兼容性
    bool isAssignable(const AST::Expression* expr, const Type* type) {
        return typeChecker->checkAssignable(expr, type);
    }

    // 解析类型注解
    std::unique_ptr<Type> resolveTypeAnnotation(
        const AST::TypeAnnotation* annotation) {
        return typeChecker->resolveTypeAnnotation(annotation);
    }

    // 获取类型分析器
    TypeAnalyzer* getTypeAnalyzer() { return typeAnalyzer.get(); }

    // 获取类型推导器
    TypeInference* getTypeInference() { return typeInference.get(); }

    // 获取类型层次结构管理器
    TypeHierarchy* getTypeHierarchy() { return typeHierarchy.get(); }

    // 获取底层类型检查器
    TypeChecker* getTypeChecker() { return typeChecker.get(); }

    // 获取类型注册表
    TypeRegistry* getTypeRegistry() { return typeRegistry.get(); }

private:
    std::unique_ptr<TypeChecker> typeChecker;
    std::unique_ptr<TypeRegistry> typeRegistry;
    std::unique_ptr<TypeInference> typeInference;
    std::unique_ptr<TypeAnalyzer> typeAnalyzer;
    std::unique_ptr<TypeHierarchy> typeHierarchy;

    // 初始化内置类型
    void initializeBuiltinTypes() {
        // 获取所有注册的类型
        auto typeNames = typeRegistry->getRegisteredTypeNames();
        for (const auto& name : typeNames) {
            Type* type = typeRegistry->lookupType(name);
            if (type) {
                typeChecker->addSymbol(name, type->clone());
            }
        }

        // 添加全局对象
        auto consoleType = std::make_unique<ObjectType>();

        // 修复: 使用 emplace_back 代替初始化列表构造
        std::vector<std::unique_ptr<Type>> logFuncParams;
        logFuncParams.emplace_back(Type::createAny());

        auto logFuncReturn = Type::createUndefined();
        consoleType->addProperty(
            "log", std::make_unique<FunctionType>(std::move(logFuncParams),
                                                  std::move(logFuncReturn)));

        typeChecker->addSymbol("console", std::move(consoleType));
    }
};

}  // namespace tsx
