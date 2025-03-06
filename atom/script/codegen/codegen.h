// codegen.h
#pragma once

#include <stack>
#include <string>
#include <vector>
#include "../ast/ast.h"
#include "../types/typechecker.h"
#include "../vm/vm.h"

namespace tsx {

class CodeGenerator {
public:
    CodeGenerator() : typeCheckEnabled(true) {}

    // 启用/禁用类型检查
    void setTypeCheckEnabled(bool enabled) { typeCheckEnabled = enabled; }

    Function* compile(const AST::Program* program);

private:
    // 本地变量结构
    struct Local {
        std::string name;
        int depth;
        bool isCaptured;
    };

    // 上值结构
    struct Upvalue {
        uint8_t index;
        bool isLocal;
    };

    // 编译器状态
    struct CompilerState {
        Function* function;
        std::vector<Local> locals;
        std::vector<Upvalue> upvalues;
        int scopeDepth;
        bool isInitializer = false;
    };

    std::stack<CompilerState> compilerStack;
    TypeChecker typeChecker;
    bool typeCheckEnabled;

    // 函数编译
    Function* compileFunction(AST::FunctionDeclaration* function,
                              const std::string& name);

    // 作用域管理
    void beginScope();
    void endScope();

    // 变量管理
    void declareVariable(const std::string& name);
    void defineVariable(uint8_t global);
    uint8_t identifierConstant(const std::string& name);
    int resolveLocal(const std::string& name);
    int resolveUpvalue(const std::string& name);
    int addUpvalue(uint8_t index, bool isLocal);

    // 字节码生成
    uint8_t makeConstant(Value&& value);
    void emitByte(uint8_t byte);
    void emitBytes(uint8_t byte1, uint8_t byte2);
    void emitLoop(int loopStart);
    int emitJump(uint8_t instruction);
    void patchJump(int offset);
    void emitReturn();
    void emitConstant(Value&& value);

    // AST访问方法
    void visitStatement(const AST::Statement* stmt);
    void visitExpression(const AST::Expression* expr);

    // 语句访问方法
    void visitExpressionStatement(const AST::ExpressionStatement* stmt);
    void visitBlockStatement(const AST::BlockStatement* stmt);
    void visitVariableDeclaration(const AST::VariableDeclaration* stmt);
    void visitIfStatement(const AST::IfStatement* stmt);
    void visitFunctionDeclaration(const AST::FunctionDeclaration* stmt);
    void visitClassDeclaration(const AST::ClassDeclaration* stmt);

    // 表达式访问方法
    void visitBinaryExpression(const AST::BinaryExpression* expr);
    void visitUnaryExpression(const AST::UnaryExpression* expr);
    void visitLiteralExpression(const AST::LiteralExpression* expr);
    void visitIdentifierExpression(const AST::IdentifierExpression* expr);
    void visitCallExpression(const AST::CallExpression* expr);
    void visitMemberExpression(const AST::MemberExpression* expr);
    void visitArrayLiteralExpression(const AST::ArrayLiteralExpression* expr);
    void visitObjectLiteralExpression(const AST::ObjectLiteralExpression* expr);

    // 用表达式类型信息优化代码生成
    void optimizeWithTypeInfo(const AST::Expression* expr) {
        // 仅在类型检查启用时进行优化
        if (!typeCheckEnabled)
            return;

        // 获取表达式类型
        auto exprType = typeChecker.getExpressionType(expr);

        // 基于类型信息进行优化
        // 例如：如果确定是数字类型，可以使用更快的数字操作指令
        // 这里仅为示例，实际优化取决于VM的实现
    }
};

}  // namespace tsx