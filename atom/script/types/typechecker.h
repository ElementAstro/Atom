#pragma once

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "../ast/ast.h"
#include "types.h"

namespace tsx {

// 新增: 类型错误类
struct TypeError {
    enum class ErrorKind {
        Incompatible,
        Undefined,
        Generic,
        TooFewArguments,
        TooManyArguments,
        PropertyNotExist,
        NotCallable,
        InvalidOperation
    };

    ErrorKind kind;
    std::string message;
    AST::Position position;  // 错误发生位置

    TypeError(ErrorKind k, const std::string& msg, const AST::Position& pos)
        : kind(k), message(msg), position(pos) {}
};

class TypeChecker {
public:
    TypeChecker();
    ~TypeChecker() = default;

    // 类型检查一个完整的程序
    void checkProgram(const AST::Program* program);

    // 获取表达式的类型
    std::unique_ptr<Type> getExpressionType(const AST::Expression* expr);

    // 检查表达式与期望类型的兼容性
    bool checkAssignable(const AST::Expression* expr, const Type* expectedType);

    // 添加符号和类型到当前作用域
    void addSymbol(const std::string& name, std::unique_ptr<Type> type);

    // 查找符号的类型
    Type* lookupSymbol(const std::string& name);

    // 进入新的作用域
    void enterScope();

    // 离开当前作用域
    void exitScope();

    // 新增: 获取所有类型错误
    const std::vector<TypeError>& getErrors() const { return errors; }

    // 新增: 添加类型错误
    void addError(TypeError::ErrorKind kind, const std::string& message,
                  const AST::Position& position);

    // 新增: 类型推导
    std::unique_ptr<Type> inferTypeFromExpression(const AST::Expression* expr);

    // 新增: 处理类型守卫
    void applyTypeGuard(const std::string& name,
                        std::unique_ptr<Type> guardedType);

    // 新增: 解析类型注解
    std::unique_ptr<Type> resolveTypeAnnotation(
        const AST::TypeAnnotation* typeAnnotation);

    // 新增: 处理泛型实例化
    std::unique_ptr<Type> instantiateGenericType(
        const std::string& genericName,
        const std::vector<std::unique_ptr<Type>>& typeArgs);

private:
    struct Scope {
        std::unordered_map<std::string, std::unique_ptr<Type>> symbols;
        std::shared_ptr<Scope> parent;
        std::set<std::string> narrowedTypes;  // 跟踪被类型守卫缩小的类型
    };

    std::shared_ptr<Scope> currentScope;
    std::vector<TypeError> errors;  // 收集类型错误

    // 新增: 泛型类型映射
    std::unordered_map<std::string, std::vector<std::string>> genericTypeParams;

    // 类型检查各类声明
    void checkStatement(const AST::Statement* stmt);
    void checkVariableDeclaration(const AST::VariableDeclaration* decl);
    void checkFunctionDeclaration(const AST::FunctionDeclaration* decl);
    void checkClassDeclaration(const AST::ClassDeclaration* decl);
    void checkInterfaceDeclaration(const AST::InterfaceDeclaration* decl);
    void checkBlockStatement(const AST::BlockStatement* block);
    void checkIfStatement(const AST::IfStatement* ifStmt);
    void checkExpressionStatement(const AST::ExpressionStatement* exprStmt);

    // 新增: 检查类型断言
    void checkTypeAssertion(const AST::TypeAssertionExpression* assertion);

    // 新增: 检查类型守卫
    void checkTypeGuard(const AST::Expression* condition);

    // 新增: 辅助函数：检查非空守卫模式
    void checkNullUndefinedGuard(const AST::BinaryExpression* andExpr);

    // 新增: 辅助函数：从类型中移除null和undefined
    std::unique_ptr<Type> removeNullAndUndefined(std::unique_ptr<Type> type);

    // 类型检查各类表达式
    std::unique_ptr<Type> checkIdentifierExpression(
        const AST::IdentifierExpression* expr);
    std::unique_ptr<Type> checkLiteralExpression(
        const AST::LiteralExpression* expr);
    std::unique_ptr<Type> checkArrayLiteralExpression(
        const AST::ArrayLiteralExpression* expr);
    std::unique_ptr<Type> checkObjectLiteralExpression(
        const AST::ObjectLiteralExpression* expr);
    std::unique_ptr<Type> checkCallExpression(const AST::CallExpression* expr);
    std::unique_ptr<Type> checkMemberExpression(
        const AST::MemberExpression* expr);
    std::unique_ptr<Type> checkBinaryExpression(
        const AST::BinaryExpression* expr);
    std::unique_ptr<Type> checkUnaryExpression(
        const AST::UnaryExpression* expr);
    std::unique_ptr<Type> checkConditionalExpression(
        const AST::ConditionalExpression* expr);

    // 新增: 检查类型断言表达式
    std::unique_ptr<Type> checkTypeAssertionExpression(
        const AST::TypeAssertionExpression* expr);
};

}  // namespace tsx
