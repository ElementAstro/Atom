// codegen.cpp
#include "codegen.h"
#include <stdexcept>

namespace tsx {
Function* CodeGenerator::compile(const AST::Program* program) {
    // 类型检查（如果启用）
    if (typeCheckEnabled) {
        typeChecker.checkProgram(program);
        const auto& errors = typeChecker.getErrors();
        if (!errors.empty()) {
            // 有类型错误，抛出异常
            std::string errorMsg = "Type check failed with " +
                                   std::to_string(errors.size()) + " errors";
            throw std::runtime_error(errorMsg);
        }
    }

    // 创建主函数
    auto mainFunction = new Function("", {}, {}, 0, 0);

    // 设置编译器状态
    CompilerState state;
    state.function = mainFunction;
    state.scopeDepth = 0;
    compilerStack.push(state);

    // 编译程序语句
    for (const auto& stmt : program->getStatements()) {
        visitStatement(stmt.get());
    }

    // 添加隐式返回
    emitReturn();

    // 检查编译错误
    // (在实际实现中，我们会跟踪错误并报告它们)

    return mainFunction;
}

Function* CodeGenerator::compileFunction(AST::FunctionDeclaration* function,
                                         const std::string& name) {
    // Create a new function
    auto newFunction =
        new Function(name, {}, {}, function->getParameters().size(), 0);

    // Set up compiler state
    CompilerState state;
    state.function = newFunction;
    state.scopeDepth = 0;

    // Special case for constructors
    if (name == "constructor") {
        state.isInitializer = true;
    }

    compilerStack.push(state);

    // Declare parameters as local variables
    beginScope();
    for (const auto& param : function->getParameters()) {
        declareVariable(param.name);
        defineVariable(0);  // Local variable
    }

    // Compile function body
    visitStatement(function->getBody());

    // Add an implicit return
    if (state.isInitializer) {
        // Constructors implicitly return 'this'
        emitBytes(static_cast<uint8_t>(OpCode::GetLocal), 0);
    } else {
        // Regular functions return null by default
        emitByte(static_cast<uint8_t>(OpCode::Null));
    }

    emitByte(static_cast<uint8_t>(OpCode::Return));

    // Get the compiled function
    Function* compiledFunction = state.function;

    // Update the function's upvalues
    compiledFunction->setNumUpvalues(state.upvalues.size());

    // Pop the compiler state
    compilerStack.pop();

    return compiledFunction;
}

void CodeGenerator::beginScope() { compilerStack.top().scopeDepth++; }

void CodeGenerator::endScope() {
    CompilerState& state = compilerStack.top();
    state.scopeDepth--;

    // Pop locals from the stack
    while (!state.locals.empty() &&
           state.locals.back().depth > state.scopeDepth) {
        if (state.locals.back().isCaptured) {
            emitByte(static_cast<uint8_t>(OpCode::CloseUpvalue));
        } else {
            emitByte(static_cast<uint8_t>(OpCode::Pop));
        }
        state.locals.pop_back();
    }
}

void CodeGenerator::declareVariable(const std::string& name) {
    CompilerState& state = compilerStack.top();

    // Check for variable redeclaration in the same scope
    for (int i = state.locals.size() - 1; i >= 0; i--) {
        if (state.locals[i].depth < state.scopeDepth)
            break;

        if (state.locals[i].name == name) {
            throw std::runtime_error("Variable '" + name +
                                     "' already declared in this scope");
        }
    }

    // Add the local variable
    Local local;
    local.name = name;
    local.depth = state.scopeDepth;
    local.isCaptured = false;
    state.locals.push_back(local);
}

void CodeGenerator::defineVariable(uint8_t global) {
    CompilerState& state = compilerStack.top();

    if (state.scopeDepth > 0) {
        // Local variable, nothing to do
        return;
    }

    // Global variable
    emitBytes(static_cast<uint8_t>(OpCode::SetGlobal), global);
}

uint8_t CodeGenerator::identifierConstant(const std::string& name) {
    return makeConstant(Value(name));
}

int CodeGenerator::resolveLocal(const std::string& name) {
    CompilerState& state = compilerStack.top();

    for (int i = state.locals.size() - 1; i >= 0; i--) {
        if (state.locals[i].name == name) {
            return i;
        }
    }

    return -1;  // Not found
}

int CodeGenerator::resolveUpvalue(const std::string& name) {
    if (compilerStack.size() <= 1)
        return -1;

    // Try to find the variable in the enclosing function
    int local = resolveLocal(name);
    if (local != -1) {
        compilerStack.top().locals[local].isCaptured = true;
        return addUpvalue(local, true);
    }

    // If not found, try in the parent's upvalues
    int upvalue = resolveUpvalue(name);
    if (upvalue != -1) {
        return addUpvalue(upvalue, false);
    }

    return -1;  // Not found
}

int CodeGenerator::addUpvalue(uint8_t index, bool isLocal) {
    CompilerState& state = compilerStack.top();

    // Check if the upvalue already exists
    for (size_t i = 0; i < state.upvalues.size(); i++) {
        if (state.upvalues[i].index == index &&
            state.upvalues[i].isLocal == isLocal) {
            return i;
        }
    }

    // Add a new upvalue
    Upvalue upvalue;
    upvalue.index = index;
    upvalue.isLocal = isLocal;
    state.upvalues.push_back(upvalue);

    return state.upvalues.size() - 1;
}

uint8_t CodeGenerator::makeConstant(Value&& value) {
    CompilerState& state = compilerStack.top();
    state.function->addConstant(std::move(value));
    return state.function->getConstantCount() - 1;
}

void CodeGenerator::emitByte(uint8_t byte) {
    CompilerState& state = compilerStack.top();
    state.function->addCode(byte);
}

void CodeGenerator::emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

void CodeGenerator::emitLoop(int loopStart) {
    CompilerState& state = compilerStack.top();

    emitByte(static_cast<uint8_t>(OpCode::Jump));

    int offset = state.function->getCodeSize() - loopStart + 2;
    if (offset > UINT16_MAX) {
        throw std::runtime_error("Loop body too large");
    }

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

int CodeGenerator::emitJump(uint8_t instruction) {
    CompilerState& state = compilerStack.top();

    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);

    return state.function->getCodeSize() - 2;
}

void CodeGenerator::patchJump(int offset) {
    CompilerState& state = compilerStack.top();

    // -2 to adjust for the bytecode for the jump itself
    int jump = state.function->getCodeSize() - offset - 2;

    if (jump > UINT16_MAX) {
        throw std::runtime_error("Too much code to jump over");
    }

    state.function->setCode(offset, (jump >> 8) & 0xff);
    state.function->setCode(offset + 1, jump & 0xff);
}

void CodeGenerator::emitReturn() {
    CompilerState& state = compilerStack.top();

    if (state.isInitializer) {
        // Constructors implicitly return 'this'
        emitBytes(static_cast<uint8_t>(OpCode::GetLocal), 0);
    } else {
        // Regular functions return null by default
        emitByte(static_cast<uint8_t>(OpCode::Null));
    }

    emitByte(static_cast<uint8_t>(OpCode::Return));
}

void CodeGenerator::emitConstant(Value&& value) {
    emitBytes(static_cast<uint8_t>(OpCode::Constant),
              makeConstant(std::move(value)));
}

void CodeGenerator::visitStatement(const AST::Statement* stmt) {
    if (auto exprStmt = dynamic_cast<const AST::ExpressionStatement*>(stmt)) {
        visitExpressionStatement(exprStmt);
    } else if (auto blockStmt =
                   dynamic_cast<const AST::BlockStatement*>(stmt)) {
        visitBlockStatement(blockStmt);
    } else if (auto varDecl =
                   dynamic_cast<const AST::VariableDeclaration*>(stmt)) {
        visitVariableDeclaration(varDecl);
    } else if (auto ifStmt = dynamic_cast<const AST::IfStatement*>(stmt)) {
        visitIfStatement(ifStmt);
    } else if (auto fnDecl =
                   dynamic_cast<const AST::FunctionDeclaration*>(stmt)) {
        visitFunctionDeclaration(fnDecl);
    } else if (auto classDecl =
                   dynamic_cast<const AST::ClassDeclaration*>(stmt)) {
        visitClassDeclaration(classDecl);
    } else {
        throw std::runtime_error(
            "Unsupported statement type in code generator");
    }
}

void CodeGenerator::visitExpression(const AST::Expression* expr) {
    if (auto binary = dynamic_cast<const AST::BinaryExpression*>(expr)) {
        visitBinaryExpression(binary);
    } else if (auto unary = dynamic_cast<const AST::UnaryExpression*>(expr)) {
        visitUnaryExpression(unary);
    } else if (auto literal =
                   dynamic_cast<const AST::LiteralExpression*>(expr)) {
        visitLiteralExpression(literal);
    } else if (auto ident =
                   dynamic_cast<const AST::IdentifierExpression*>(expr)) {
        visitIdentifierExpression(ident);
    } else if (auto call = dynamic_cast<const AST::CallExpression*>(expr)) {
        visitCallExpression(call);
    } else if (auto member = dynamic_cast<const AST::MemberExpression*>(expr)) {
        visitMemberExpression(member);
    } else if (auto array =
                   dynamic_cast<const AST::ArrayLiteralExpression*>(expr)) {
        visitArrayLiteralExpression(array);
    } else if (auto object =
                   dynamic_cast<const AST::ObjectLiteralExpression*>(expr)) {
        visitObjectLiteralExpression(object);
    } else {
        throw std::runtime_error(
            "Unsupported expression type in code generator");
    }
}

void CodeGenerator::visitExpressionStatement(
    const AST::ExpressionStatement* stmt) {
    visitExpression(stmt->getExpression());
    emitByte(static_cast<uint8_t>(OpCode::Pop));  // Discard the result
}

void CodeGenerator::visitBlockStatement(const AST::BlockStatement* stmt) {
    beginScope();

    for (const auto& statement : stmt->getStatements()) {
        visitStatement(statement.get());
    }

    endScope();
}

void CodeGenerator::visitVariableDeclaration(
    const AST::VariableDeclaration* stmt) {
    for (const auto& decl : stmt->getDeclarations()) {
        uint8_t global = identifierConstant(decl.name);

        if (decl.initializer) {
            visitExpression(decl.initializer.get());
        } else {
            emitByte(static_cast<uint8_t>(OpCode::Null));
        }

        declareVariable(decl.name);
        defineVariable(global);
    }
}

void CodeGenerator::visitIfStatement(const AST::IfStatement* stmt) {
    visitExpression(stmt->getCondition());

    int thenJump = emitJump(static_cast<uint8_t>(OpCode::JumpIfFalse));
    emitByte(static_cast<uint8_t>(OpCode::Pop));
    visitStatement(stmt->getThenBranch());

    int elseJump = emitJump(static_cast<uint8_t>(OpCode::Jump));
    patchJump(thenJump);
    emitByte(static_cast<uint8_t>(OpCode::Pop));

    if (stmt->getElseBranch()) {
        visitStatement(stmt->getElseBranch());
    }

    patchJump(elseJump);
}

void CodeGenerator::visitFunctionDeclaration(
    const AST::FunctionDeclaration* stmt) {
    uint8_t global = identifierConstant(stmt->getName());
    declareVariable(stmt->getName());

    Function* function = compileFunction(
        const_cast<AST::FunctionDeclaration*>(stmt), stmt->getName());

    emitBytes(static_cast<uint8_t>(OpCode::Closure),
              makeConstant(Value(function)));

    for (const auto& upvalue : compilerStack.top().upvalues) {
        emitByte(upvalue.isLocal ? 1 : 0);
        emitByte(upvalue.index);
    }

    defineVariable(global);
}

void CodeGenerator::visitClassDeclaration(const AST::ClassDeclaration* stmt) {
    uint8_t nameConstant = identifierConstant(stmt->getName());
    declareVariable(stmt->getName());

    emitBytes(static_cast<uint8_t>(OpCode::CreateClass), nameConstant);
    defineVariable(nameConstant);

    if (!stmt->getBaseClassName().empty()) {
        // TODO: Handle inheritance
    }

    for (const auto& member : stmt->getMembers()) {
        if (member.kind == AST::ClassDeclaration::MemberKind::Method ||
            member.kind == AST::ClassDeclaration::MemberKind::Constructor) {
            Function* method = compileFunction(
                const_cast<AST::FunctionDeclaration*>(member.methodDecl.get()),
                member.name);

            emitBytes(static_cast<uint8_t>(OpCode::Closure),
                      makeConstant(Value(method)));

            for (const auto& upvalue : compilerStack.top().upvalues) {
                emitByte(upvalue.isLocal ? 1 : 0);
                emitByte(upvalue.index);
            }

            emitBytes(static_cast<uint8_t>(OpCode::Method),
                      identifierConstant(member.name));
        }
    }
}

void CodeGenerator::visitBinaryExpression(const AST::BinaryExpression* expr) {
    if (expr->getOperator() == AST::BinaryExpression::Operator::And) {
        visitExpression(expr->getLeft());
        int endJump = emitJump(static_cast<uint8_t>(OpCode::JumpIfFalse));
        emitByte(static_cast<uint8_t>(OpCode::Pop));
        visitExpression(expr->getRight());
        patchJump(endJump);
        return;
    }

    if (expr->getOperator() == AST::BinaryExpression::Operator::Or) {
        visitExpression(expr->getLeft());
        int elseJump = emitJump(static_cast<uint8_t>(OpCode::JumpIfFalse));
        int endJump = emitJump(static_cast<uint8_t>(OpCode::Jump));
        patchJump(elseJump);
        emitByte(static_cast<uint8_t>(OpCode::Pop));
        visitExpression(expr->getRight());
        patchJump(endJump);
        return;
    }

    visitExpression(expr->getLeft());
    visitExpression(expr->getRight());

    switch (expr->getOperator()) {
        case AST::BinaryExpression::Operator::Add:
            emitByte(static_cast<uint8_t>(OpCode::Add));
            break;
        case AST::BinaryExpression::Operator::Subtract:
            emitByte(static_cast<uint8_t>(OpCode::Subtract));
            break;
        case AST::BinaryExpression::Operator::Multiply:
            emitByte(static_cast<uint8_t>(OpCode::Multiply));
            break;
        case AST::BinaryExpression::Operator::Divide:
            emitByte(static_cast<uint8_t>(OpCode::Divide));
            break;
        case AST::BinaryExpression::Operator::Modulo:
            emitByte(static_cast<uint8_t>(OpCode::Modulo));
            break;
        case AST::BinaryExpression::Operator::Equal:
            emitByte(static_cast<uint8_t>(OpCode::Equal));
            break;
        case AST::BinaryExpression::Operator::NotEqual:
            emitByte(static_cast<uint8_t>(OpCode::NotEqual));
            break;
        case AST::BinaryExpression::Operator::Less:
            emitByte(static_cast<uint8_t>(OpCode::Less));
            break;
        case AST::BinaryExpression::Operator::LessEqual:
            emitByte(static_cast<uint8_t>(OpCode::LessEqual));
            break;
        case AST::BinaryExpression::Operator::Greater:
            emitByte(static_cast<uint8_t>(OpCode::Greater));
            break;
        case AST::BinaryExpression::Operator::GreaterEqual:
            emitByte(static_cast<uint8_t>(OpCode::GreaterEqual));
            break;
        default:
            throw std::runtime_error("Unsupported binary operator");
    }
}

void CodeGenerator::visitUnaryExpression(const AST::UnaryExpression* expr) {
    visitExpression(expr->getOperand());

    switch (expr->getOperator()) {
        case AST::UnaryExpression::Operator::Minus:
            emitByte(static_cast<uint8_t>(OpCode::Negate));
            break;
        case AST::UnaryExpression::Operator::Not:
            emitByte(static_cast<uint8_t>(OpCode::Not));
            break;
        default:
            throw std::runtime_error("Unsupported unary operator");
    }
}

void CodeGenerator::visitLiteralExpression(const AST::LiteralExpression* expr) {
    switch (expr->getKind()) {
        case AST::LiteralExpression::LiteralKind::Number:
            emitConstant(Value(std::get<double>(expr->getValue())));
            break;
        case AST::LiteralExpression::LiteralKind::String:
            emitConstant(Value(std::get<std::string>(expr->getValue())));
            break;
        case AST::LiteralExpression::LiteralKind::Boolean:
            emitByte(std::get<bool>(expr->getValue())
                         ? static_cast<uint8_t>(OpCode::True)
                         : static_cast<uint8_t>(OpCode::False));
            break;
        case AST::LiteralExpression::LiteralKind::Null:
            emitByte(static_cast<uint8_t>(OpCode::Null));
            break;
        case AST::LiteralExpression::LiteralKind::Undefined:
            emitByte(static_cast<uint8_t>(OpCode::Undefined));
            break;
    }
}

void CodeGenerator::visitIdentifierExpression(
    const AST::IdentifierExpression* expr) {
    // Resolve the variable
    std::string name = expr->getName();
    int arg = resolveLocal(name);

    if (arg != -1) {
        // Local variable
        emitBytes(static_cast<uint8_t>(OpCode::GetLocal), arg);
    } else if ((arg = resolveUpvalue(name)) != -1) {
        // Upvalue
        emitBytes(static_cast<uint8_t>(OpCode::GetUpvalue), arg);
    } else {
        // Global variable
        emitBytes(static_cast<uint8_t>(OpCode::GetGlobal),
                  identifierConstant(name));
    }
}

void CodeGenerator::visitCallExpression(const AST::CallExpression* expr) {
    // Evaluate the callee
    visitExpression(expr->getCallee());

    // Evaluate the arguments
    for (const auto& arg : expr->getArguments()) {
        visitExpression(arg.get());
    }

    // Emit the call instruction
    emitBytes(static_cast<uint8_t>(OpCode::Call), expr->getArguments().size());
}

void CodeGenerator::visitMemberExpression(const AST::MemberExpression* expr) {
    // Evaluate the object
    visitExpression(expr->getObject());

    if (expr->isComputed()) {
        // Computed property access: obj[expr]
        visitExpression(expr->getProperty());
        emitByte(static_cast<uint8_t>(OpCode::GetIndex));
    } else {
        // Direct property access: obj.prop
        auto* identProp =
            dynamic_cast<const AST::IdentifierExpression*>(expr->getProperty());
        if (!identProp) {
            throw std::runtime_error("Property must be an identifier");
        }

        emitBytes(static_cast<uint8_t>(OpCode::GetField),
                  identifierConstant(identProp->getName()));
    }
}

void CodeGenerator::visitArrayLiteralExpression(
    const AST::ArrayLiteralExpression* expr) {
    // Evaluate all elements
    for (const auto& element : expr->getElements()) {
        visitExpression(element.get());
    }

    // Create the array
    emitBytes(static_cast<uint8_t>(OpCode::Array), expr->getElements().size());
}

void CodeGenerator::visitObjectLiteralExpression(
    const AST::ObjectLiteralExpression* expr) {
    // Evaluate all properties
    for (const auto& prop : expr->getProperties()) {
        // Push the key (as a string)
        emitConstant(Value(prop.key));

        // Push the value
        visitExpression(prop.value.get());
    }

    // Create the object
    emitBytes(static_cast<uint8_t>(OpCode::Object),
              expr->getProperties().size());
}

}  // namespace tsx