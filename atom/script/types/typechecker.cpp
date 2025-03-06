#include "typechecker.h"
#include <iostream>

namespace tsx {

TypeChecker::TypeChecker() {
    // 初始化根作用域
    currentScope = std::make_shared<Scope>();
    currentScope->parent = nullptr;

    // 添加内置类型和函数到全局作用域
    addSymbol("console", std::make_unique<ObjectType>());
    addSymbol("undefined", Type::createUndefined());
    addSymbol("null", Type::createNull());
}

void TypeChecker::checkProgram(const AST::Program* program) {
    for (const auto& stmt : program->getStatements()) {
        checkStatement(stmt.get());
    }
}

void TypeChecker::checkStatement(const AST::Statement* stmt) {
    // 使用dynamic_cast来检查语句的具体类型
    if (auto varDecl = dynamic_cast<const AST::VariableDeclaration*>(stmt)) {
        checkVariableDeclaration(varDecl);
    } else if (auto funcDecl =
                   dynamic_cast<const AST::FunctionDeclaration*>(stmt)) {
        checkFunctionDeclaration(funcDecl);
    } else if (auto classDecl =
                   dynamic_cast<const AST::ClassDeclaration*>(stmt)) {
        checkClassDeclaration(classDecl);
    } else if (auto interfaceDecl =
                   dynamic_cast<const AST::InterfaceDeclaration*>(stmt)) {
        checkInterfaceDeclaration(interfaceDecl);
    } else if (auto blockStmt =
                   dynamic_cast<const AST::BlockStatement*>(stmt)) {
        checkBlockStatement(blockStmt);
    } else if (auto ifStmt = dynamic_cast<const AST::IfStatement*>(stmt)) {
        checkIfStatement(ifStmt);
    } else if (auto exprStmt =
                   dynamic_cast<const AST::ExpressionStatement*>(stmt)) {
        checkExpressionStatement(exprStmt);
    } else {
        std::cerr << "Unsupported statement kind for type checking"
                  << std::endl;
    }
}

std::unique_ptr<Type> TypeChecker::getExpressionType(
    const AST::Expression* expr) {
    // 使用dynamic_cast来检查表达式的具体类型
    if (auto idExpr = dynamic_cast<const AST::IdentifierExpression*>(expr)) {
        return checkIdentifierExpression(idExpr);
    } else if (auto literalExpr =
                   dynamic_cast<const AST::LiteralExpression*>(expr)) {
        return checkLiteralExpression(literalExpr);
    } else if (auto arrayLiteral =
                   dynamic_cast<const AST::ArrayLiteralExpression*>(expr)) {
        return checkArrayLiteralExpression(arrayLiteral);
    } else if (auto objLiteral =
                   dynamic_cast<const AST::ObjectLiteralExpression*>(expr)) {
        return checkObjectLiteralExpression(objLiteral);
    } else if (auto callExpr = dynamic_cast<const AST::CallExpression*>(expr)) {
        return checkCallExpression(callExpr);
    } else if (auto memberExpr =
                   dynamic_cast<const AST::MemberExpression*>(expr)) {
        return checkMemberExpression(memberExpr);
    } else if (auto binaryExpr =
                   dynamic_cast<const AST::BinaryExpression*>(expr)) {
        return checkBinaryExpression(binaryExpr);
    } else if (auto unaryExpr =
                   dynamic_cast<const AST::UnaryExpression*>(expr)) {
        return checkUnaryExpression(unaryExpr);
    } else if (auto condExpr =
                   dynamic_cast<const AST::ConditionalExpression*>(expr)) {
        return checkConditionalExpression(condExpr);
    } else {
        std::cerr << "Unsupported expression kind for type checking"
                  << std::endl;
        return Type::createAny();  // 默认为any类型
    }
}

bool TypeChecker::checkAssignable(const AST::Expression* expr,
                                  const Type* expectedType) {
    auto exprType = getExpressionType(expr);
    return exprType->isAssignableTo(expectedType);
}

void TypeChecker::addSymbol(const std::string& name,
                            std::unique_ptr<Type> type) {
    currentScope->symbols[name] = std::move(type);
}

Type* TypeChecker::lookupSymbol(const std::string& name) {
    auto scope = currentScope;
    while (scope) {
        auto it = scope->symbols.find(name);
        if (it != scope->symbols.end()) {
            return it->second.get();
        }
        scope = scope->parent;
    }
    return nullptr;
}

void TypeChecker::enterScope() {
    auto newScope = std::make_shared<Scope>();
    newScope->parent = currentScope;
    currentScope = newScope;
}

void TypeChecker::exitScope() {
    if (currentScope->parent) {
        currentScope = currentScope->parent;
    } else {
        std::cerr << "Cannot exit global scope" << std::endl;
    }
}

void TypeChecker::checkVariableDeclaration(
    const AST::VariableDeclaration* decl) {
    for (const auto& declarator : decl->getDeclarations()) {
        std::unique_ptr<Type> type;

        // 如果有显式类型注解，使用它
        if (declarator.typeAnnotation) {
            if (auto typeAnnotation = dynamic_cast<const AST::TypeAnnotation*>(
                    declarator.typeAnnotation.get())) {
                type = resolveTypeAnnotation(typeAnnotation);
            } else {
                type = declarator.typeAnnotation->clone();
            }
        }
        // 如果有初始值，推断类型
        else if (declarator.initializer) {
            type = inferTypeFromExpression(declarator.initializer.get());
        }
        // 否则默认为any类型
        else {
            type = Type::createAny();
        }

        // 如果有初始值且有类型注解，检查类型兼容性
        if (declarator.initializer && declarator.typeAnnotation) {
            auto initType = getExpressionType(declarator.initializer.get());
            if (!initType->isAssignableTo(type.get())) {
                addError(TypeError::ErrorKind::Incompatible,
                         "Cannot assign initializer of type " +
                             initType->toString() + " to variable '" +
                             declarator.name + "' of type " + type->toString(),
                         declarator.initializer->getPosition());
            }
        }

        // 将变量添加到当前作用域
        addSymbol(declarator.name, std::move(type));
    }
}

void TypeChecker::checkFunctionDeclaration(
    const AST::FunctionDeclaration* decl) {
    // 处理泛型类型参数
    std::unordered_map<std::string, std::unique_ptr<Type>> typeParams;
    for (const auto& typeParam : decl->getTypeParameters()) {
        typeParams[typeParam->getName()] =
            std::make_unique<GenericTypeParameter>(
                typeParam->getName(), typeParam->getConstraint()
                                          ? typeParam->getConstraint()->clone()
                                          : nullptr);
    }

    // 创建函数类型
    std::vector<std::unique_ptr<Type>> paramTypes;
    for (const auto& param : decl->getParameters()) {
        if (param.typeAnnotation) {
            paramTypes.push_back(param.typeAnnotation->clone());
        } else {
            paramTypes.push_back(Type::createAny());
        }
    }

    std::unique_ptr<Type> returnType;
    if (decl->getReturnType()) {
        returnType = decl->getReturnType()->clone();
    } else {
        returnType = Type::createAny();
    }

    auto funcType = std::make_unique<FunctionType>(std::move(paramTypes),
                                                   std::move(returnType));

    // 将函数添加到当前作用域
    addSymbol(decl->getName(), std::move(funcType));

    // 进入函数体的新作用域
    enterScope();

    // 添加参数到函数作用域
    for (size_t i = 0; i < decl->getParameters().size(); i++) {
        const auto& param = decl->getParameters()[i];
        std::unique_ptr<Type> paramType;

        if (param.typeAnnotation) {
            paramType = param.typeAnnotation->clone();
        } else {
            paramType = Type::createAny();
        }

        addSymbol(param.name, std::move(paramType));
    }

    // 检查函数体
    if (decl->getBody()) {
        checkBlockStatement(decl->getBody());
    }

    // 离开函数作用域
    exitScope();
}

void TypeChecker::checkClassDeclaration(const AST::ClassDeclaration* decl) {
    // 创建类类型（作为对象类型）
    auto classType = std::make_unique<ObjectType>();

    // 处理类成员
    for (const auto& member : decl->getMembers()) {
        switch (member.kind) {
            case AST::ClassDeclaration::MemberKind::Property: {
                std::unique_ptr<Type> propType;
                if (member.propertyType) {
                    propType = member.propertyType->clone();
                } else if (member.initializer) {
                    propType = getExpressionType(member.initializer.get());
                } else {
                    propType = Type::createAny();
                }
                classType->addProperty(member.name, std::move(propType));
            } break;
            case AST::ClassDeclaration::MemberKind::Method: {
                // 方法作为函数类型添加到类型中
                std::vector<std::unique_ptr<Type>> paramTypes;
                for (const auto& param : member.methodDecl->getParameters()) {
                    if (param.typeAnnotation) {
                        paramTypes.push_back(param.typeAnnotation->clone());
                    } else {
                        paramTypes.push_back(Type::createAny());
                    }
                }

                std::unique_ptr<Type> returnType;
                if (member.methodDecl->getReturnType()) {
                    returnType = member.methodDecl->getReturnType()->clone();
                } else {
                    returnType = Type::createAny();
                }

                auto methodType = std::make_unique<FunctionType>(
                    std::move(paramTypes), std::move(returnType));
                classType->addProperty(member.name, std::move(methodType));
            } break;
            default:
                // 简化处理，忽略其他成员类型
                break;
        }
    }

    // 将类添加到当前作用域
    addSymbol(decl->getName(), std::move(classType));
}

void TypeChecker::checkInterfaceDeclaration(
    const AST::InterfaceDeclaration* decl) {
    // 创建接口类型（作为对象类型）
    auto interfaceType = std::make_unique<ObjectType>();
    interfaceType->setIsInterface(true);

    // 处理接口属性
    for (const auto& prop : decl->getProperties()) {
        if (prop.type) {
            interfaceType->addProperty(prop.name, prop.type->clone());
        } else {
            interfaceType->addProperty(prop.name, Type::createAny());
        }
    }

    // 处理接口方法
    for (const auto& method : decl->getMethods()) {
        std::vector<std::unique_ptr<Type>> paramTypes;
        for (const auto& param : method.parameters) {
            if (param.typeAnnotation) {
                paramTypes.push_back(param.typeAnnotation->clone());
            } else {
                paramTypes.push_back(Type::createAny());
            }
        }

        std::unique_ptr<Type> returnType;
        if (method.returnType) {
            returnType = method.returnType->clone();
        } else {
            returnType = Type::createAny();
        }

        auto methodType = std::make_unique<FunctionType>(std::move(paramTypes),
                                                         std::move(returnType));
        interfaceType->addProperty(method.name, std::move(methodType));
    }

    // 将接口添加到当前作用域
    addSymbol(decl->getName(), std::move(interfaceType));
}

void TypeChecker::checkBlockStatement(const AST::BlockStatement* block) {
    // 进入块作用域
    enterScope();

    // 检查块中的所有语句
    for (const auto& stmt : block->getStatements()) {
        checkStatement(stmt.get());
    }

    // 离开块作用域
    exitScope();
}

void TypeChecker::checkIfStatement(const AST::IfStatement* ifStmt) {
    // 检查条件表达式
    auto conditionType = getExpressionType(ifStmt->getCondition());
    auto boolType = Type::createBoolean();

    if (!conditionType->isAssignableTo(boolType.get())) {
        addError(TypeError::ErrorKind::Incompatible,
                 "If condition must be assignable to boolean",
                 ifStmt->getCondition()->getPosition());
    }

    // 新增: 分析条件是否包含类型守卫
    checkTypeGuard(ifStmt->getCondition());

    // 检查then分支
    enterScope();  // 为if分支创建新作用域，这样类型守卫只在if块内有效
    checkStatement(ifStmt->getThenBranch());
    exitScope();

    // 检查else分支（如果存在）
    if (ifStmt->getElseBranch()) {
        enterScope();  // 为else分支创建新作用域
        checkStatement(ifStmt->getElseBranch());
        exitScope();
    }
}

void TypeChecker::checkExpressionStatement(
    const AST::ExpressionStatement* exprStmt) {
    // 只需检查表达式的类型
    getExpressionType(exprStmt->getExpression());
}

std::unique_ptr<Type> TypeChecker::checkIdentifierExpression(
    const AST::IdentifierExpression* expr) {
    Type* type = lookupSymbol(expr->getName());
    if (type) {
        return type->clone();
    } else {
        std::cerr << "Undefined identifier: " << expr->getName() << std::endl;
        return Type::createAny();
    }
}

std::unique_ptr<Type> TypeChecker::checkLiteralExpression(
    const AST::LiteralExpression* expr) {
    switch (expr->getKind()) {
        case AST::LiteralExpression::LiteralKind::Number:
            return Type::createNumber();
        case AST::LiteralExpression::LiteralKind::String:
            return Type::createString();
        case AST::LiteralExpression::LiteralKind::Boolean:
            return Type::createBoolean();
        case AST::LiteralExpression::LiteralKind::Null:
            return Type::createNull();
        case AST::LiteralExpression::LiteralKind::Undefined:
            return Type::createUndefined();
        default:
            return Type::createAny();
    }
}

std::unique_ptr<Type> TypeChecker::checkArrayLiteralExpression(
    const AST::ArrayLiteralExpression* expr) {
    // 检查所有元素的类型，找出共同类型
    const auto& elements = expr->getElements();
    if (elements.empty()) {
        // 空数组，假设为any[]
        return std::make_unique<ArrayType>(Type::createAny());
    }

    // 获取第一个元素类型作为基础
    auto elementType = getExpressionType(elements[0].get());

    // 检查其他元素是否与第一个元素类型兼容
    for (size_t i = 1; i < elements.size(); i++) {
        auto currentType = getExpressionType(elements[i].get());
        if (!currentType->isAssignableTo(elementType.get()) &&
            !elementType->isAssignableTo(currentType.get())) {
            // 类型不兼容，使用联合类型或回退到any
            elementType = Type::createAny();
            break;
        }
    }

    return std::make_unique<ArrayType>(std::move(elementType));
}

std::unique_ptr<Type> TypeChecker::checkObjectLiteralExpression(
    const AST::ObjectLiteralExpression* expr) {
    auto objType = std::make_unique<ObjectType>();

    for (const auto& prop : expr->getProperties()) {
        auto valueType = getExpressionType(prop.value.get());
        objType->addProperty(prop.key, std::move(valueType));
    }

    return objType;
}

std::unique_ptr<Type> TypeChecker::checkCallExpression(
    const AST::CallExpression* expr) {
    auto calleeType = getExpressionType(expr->getCallee());

    // 检查是否是函数类型
    if (auto funcType = dynamic_cast<FunctionType*>(calleeType.get())) {
        const auto& params = funcType->getParamTypes();
        const auto& args = expr->getArguments();

        // 检查参数数量
        if (args.size() < params.size()) {
            addError(TypeError::ErrorKind::TooFewArguments,
                     "Too few arguments in function call", expr->getPosition());
        } else if (args.size() > params.size()) {
            addError(TypeError::ErrorKind::TooManyArguments,
                     "Too many arguments in function call",
                     expr->getPosition());
        }

        // 检查参数类型
        for (size_t i = 0; i < std::min(params.size(), args.size()); i++) {
            auto argType = getExpressionType(args[i].get());
            if (!argType->isAssignableTo(params[i].get())) {
                addError(TypeError::ErrorKind::Incompatible,
                         "Argument " + std::to_string(i + 1) +
                             " type mismatch: expected " +
                             params[i]->toString() + ", got " +
                             argType->toString(),
                         args[i]->getPosition());
            }
        }

        // 返回函数的返回类型
        return funcType->getReturnType()->clone();
    } else {
        addError(TypeError::ErrorKind::NotCallable,
                 "Cannot call value of type " + calleeType->toString(),
                 expr->getCallee()->getPosition());
        return Type::createAny();
    }
}

std::unique_ptr<Type> TypeChecker::checkMemberExpression(
    const AST::MemberExpression* expr) {
    auto objectType = getExpressionType(expr->getObject());

    // 检查是否是对象类型
    if (auto objType = dynamic_cast<ObjectType*>(objectType.get())) {
        // 对于非计算属性
        if (!expr->isComputed()) {
            if (auto propExpr = dynamic_cast<const AST::IdentifierExpression*>(
                    expr->getProperty())) {
                const std::string& propName = propExpr->getName();

                if (objType->hasProperty(propName)) {
                    return objType->getPropertyType(propName)->clone();
                } else {
                    std::cerr << "Type error: Property '" << propName
                              << "' does not exist on object" << std::endl;
                }
            }
        }
        // 对于计算属性，我们无法在静态类型检查时确定属性名
        else {
            // 如果对象类型有索引签名，使用它
            // 这里简化处理，不实现索引签名检查
        }
    } else if (auto arrayType = dynamic_cast<ArrayType*>(objectType.get())) {
        // 处理数组访问，如 arr[0]
        if (expr->isComputed()) {
            auto propType = getExpressionType(expr->getProperty());
            if (auto numType = dynamic_cast<PrimitiveType*>(propType.get())) {
                if (numType->getKind() == PrimitiveType::Kind::Number) {
                    // 返回数组元素类型
                    return arrayType->getElementType()->clone();
                }
            }
        }
    }

    return Type::createAny();
}

std::unique_ptr<Type> TypeChecker::checkBinaryExpression(
    const AST::BinaryExpression* expr) {
    auto leftType = getExpressionType(expr->getLeft());
    auto rightType = getExpressionType(expr->getRight());

    switch (expr->getOperator()) {
        case AST::BinaryExpression::Operator::Add:
            // 加法可以用于数字或字符串
            if ((leftType->equals(Type::createNumber().get()) &&
                 rightType->equals(Type::createNumber().get())) ||
                (leftType->equals(Type::createNumber().get()) &&
                 rightType->equals(Type::createString().get())) ||
                (leftType->equals(Type::createString().get()) &&
                 rightType->equals(Type::createNumber().get())) ||
                (leftType->equals(Type::createString().get()) &&
                 rightType->equals(Type::createString().get()))) {
                return leftType->equals(Type::createString().get()) ||
                               rightType->equals(Type::createString().get())
                           ? Type::createString()
                           : Type::createNumber();
            }
            std::cerr << "Type error: '+' operator not applicable to types"
                      << std::endl;
            return Type::createAny();

        case AST::BinaryExpression::Operator::Subtract:
        case AST::BinaryExpression::Operator::Multiply:
        case AST::BinaryExpression::Operator::Divide:
        case AST::BinaryExpression::Operator::Modulo:
            // 这些操作符只能用于数字
            if (leftType->equals(Type::createNumber().get()) &&
                rightType->equals(Type::createNumber().get())) {
                return Type::createNumber();
            }
            std::cerr << "Type error: Arithmetic operator not applicable to "
                         "non-number types"
                      << std::endl;
            return Type::createAny();

        case AST::BinaryExpression::Operator::Equal:
        case AST::BinaryExpression::Operator::NotEqual:
        case AST::BinaryExpression::Operator::Less:
        case AST::BinaryExpression::Operator::Greater:
        case AST::BinaryExpression::Operator::LessEqual:
        case AST::BinaryExpression::Operator::GreaterEqual:
            // 比较操作符返回布尔值
            return Type::createBoolean();

        case AST::BinaryExpression::Operator::And:
        case AST::BinaryExpression::Operator::Or:
            // 逻辑运算符返回布尔值（简化处理，实际上返回的是操作数类型）
            return Type::createBoolean();

        default:
            return Type::createAny();
    }
}

std::unique_ptr<Type> TypeChecker::checkUnaryExpression(
    const AST::UnaryExpression* expr) {
    auto operandType = getExpressionType(expr->getOperand());

    switch (expr->getOperator()) {
        case AST::UnaryExpression::Operator::Plus:
        case AST::UnaryExpression::Operator::Minus:
            // 一元加减操作符只能用于数字
            if (operandType->equals(Type::createNumber().get())) {
                return Type::createNumber();
            }
            std::cerr << "Type error: Unary '+'/'-' operator not applicable to "
                         "non-number type"
                      << std::endl;
            return Type::createAny();

        case AST::UnaryExpression::Operator::Not:
            // 逻辑非返回布尔值
            return Type::createBoolean();

        case AST::UnaryExpression::Operator::BitwiseNot:
            // 位非操作符只能用于数字
            if (operandType->equals(Type::createNumber().get())) {
                return Type::createNumber();
            }
            std::cerr << "Type error: Bitwise '~' operator not applicable to "
                         "non-number type"
                      << std::endl;
            return Type::createAny();

        case AST::UnaryExpression::Operator::Increment:
        case AST::UnaryExpression::Operator::Decrement:
            // 自增自减只能用于数字
            if (operandType->equals(Type::createNumber().get())) {
                return Type::createNumber();
            }
            std::cerr << "Type error: Increment/decrement operator not "
                         "applicable to non-number type"
                      << std::endl;
            return Type::createAny();

        default:
            return Type::createAny();
    }
}

std::unique_ptr<Type> TypeChecker::checkConditionalExpression(
    const AST::ConditionalExpression* expr) {
    auto conditionType = getExpressionType(expr->getCondition());
    auto boolType = Type::createBoolean();

    if (!conditionType->isAssignableTo(boolType.get())) {
        std::cerr << "Type error: Condition must be assignable to boolean"
                  << std::endl;
    }

    auto consequentType = getExpressionType(expr->getConsequent());
    auto alternateType = getExpressionType(expr->getAlternate());

    // 如果两个分支类型兼容，返回其中一个
    if (consequentType->isAssignableTo(alternateType.get())) {
        return alternateType->clone();
    } else if (alternateType->isAssignableTo(consequentType.get())) {
        return consequentType->clone();
    }

    // 如果类型不兼容，可以返回联合类型
    // 这里简化处理，返回any类型
    return Type::createAny();
}

// 新增: 添加类型错误
void TypeChecker::addError(TypeError::ErrorKind kind,
                           const std::string& message,
                           const AST::Position& position) {
    errors.emplace_back(kind, message, position);
    std::cerr << "Type error at " << position.line << ":" << position.column
              << " - " << message << std::endl;
}

// 新增: 类型推导
std::unique_ptr<Type> TypeChecker::inferTypeFromExpression(
    const AST::Expression* expr) {
    // 基本与getExpressionType相同，但可以进行更复杂的推导
    return getExpressionType(expr);
}

// 新增: 应用类型守卫
void TypeChecker::applyTypeGuard(const std::string& name,
                                 std::unique_ptr<Type> guardedType) {
    // 记录当前作用域中应用了类型守卫的变量
    currentScope->narrowedTypes.insert(name);

    // 更新变量类型
    Type* currentType = lookupSymbol(name);
    if (currentType) {
        // 在当前作用域中替换符号类型
        addSymbol(name, std::move(guardedType));
    }
}

// 新增: 解析类型注解
std::unique_ptr<Type> TypeChecker::resolveTypeAnnotation(
    const AST::TypeAnnotation* typeAnnotation) {
    if (!typeAnnotation)
        return Type::createAny();

    // Using dynamic_cast to determine the actual type of TypeAnnotation
    if (auto basicType =
            dynamic_cast<const AST::BasicTypeAnnotation*>(typeAnnotation)) {
        const std::string& typeName = basicType->getTypeName();
        if (typeName == "number")
            return Type::createNumber();
        else if (typeName == "string")
            return Type::createString();
        else if (typeName == "boolean")
            return Type::createBoolean();
        else if (typeName == "null")
            return Type::createNull();
        else if (typeName == "undefined")
            return Type::createUndefined();
        else if (typeName == "any")
            return Type::createAny();
        else if (typeName == "never")
            return Type::createNever();
        else if (typeName == "unknown")
            return Type::createUnknown();
        else {
            // 尝试查找已定义类型
            Type* type = lookupSymbol(typeName);
            if (type) {
                return type->clone();
            }
            addError(TypeError::ErrorKind::Undefined,
                     "Undefined type: " + typeName, basicType->getPosition());
            return Type::createAny();
        }
    } else if (auto arrayType = dynamic_cast<const AST::ArrayTypeAnnotation*>(
                   typeAnnotation)) {
        auto elemType = resolveTypeAnnotation(arrayType->getElementType());
        return std::make_unique<ArrayType>(std::move(elemType));
    } else if (auto unionType = dynamic_cast<const AST::UnionTypeAnnotation*>(
                   typeAnnotation)) {
        std::vector<std::unique_ptr<Type>> unionTypes;
        for (const auto& type : unionType->getTypes()) {
            unionTypes.push_back(resolveTypeAnnotation(type.get()));
        }
        return std::make_unique<UnionType>(std::move(unionTypes));
    } else if (auto intersectionType =
                   dynamic_cast<const AST::IntersectionTypeAnnotation*>(
                       typeAnnotation)) {
        std::vector<std::unique_ptr<Type>> intersectionTypes;
        for (const auto& type : intersectionType->getTypes()) {
            intersectionTypes.push_back(resolveTypeAnnotation(type.get()));
        }
        return Type::createIntersection(std::move(intersectionTypes));
    } else if (auto genericType =
                   dynamic_cast<const AST::GenericTypeAnnotation*>(
                       typeAnnotation)) {
        std::vector<std::unique_ptr<Type>> typeArgs;
        for (const auto& arg : genericType->getTypeArguments()) {
            typeArgs.push_back(resolveTypeAnnotation(arg.get()));
        }
        return instantiateGenericType(genericType->getBaseType(),
                                      std::move(typeArgs));
    } else if (auto funcType = dynamic_cast<const AST::FunctionTypeAnnotation*>(
                   typeAnnotation)) {
        std::vector<std::unique_ptr<Type>> paramTypes;
        for (const auto& param : funcType->getParameters()) {
            paramTypes.push_back(resolveTypeAnnotation(param.type.get()));
        }
        auto returnType = resolveTypeAnnotation(funcType->getReturnType());
        return std::make_unique<FunctionType>(std::move(paramTypes),
                                              std::move(returnType));
    } else if (auto objType = dynamic_cast<const AST::ObjectTypeAnnotation*>(
                   typeAnnotation)) {
        auto newObjType = std::make_unique<ObjectType>();
        for (const auto& prop : objType->getProperties()) {
            newObjType->addProperty(prop.name,
                                    resolveTypeAnnotation(prop.type.get()));
        }
        return newObjType;
    } else if (auto tupleType = dynamic_cast<const AST::TupleTypeAnnotation*>(
                   typeAnnotation)) {
        // Handle tuple type if needed
        return Type::createAny();  // Placeholder
    }

    // Default case if none of the dynamic casts match
    return Type::createAny();
}

// 新增: 处理泛型实例化
std::unique_ptr<Type> TypeChecker::instantiateGenericType(
    const std::string& genericName,
    const std::vector<std::unique_ptr<Type>>& typeArgs) {
    // 查找基础泛型类型
    auto baseType = lookupSymbol(genericName);
    if (!baseType) {
        addError(TypeError::ErrorKind::Generic,
                 "Cannot find generic type: " + genericName,
                 AST::Position{0, 0, 0});  // 位置信息不可用
        return Type::createAny();
    }

    // 检查类型参数数量
    auto it = genericTypeParams.find(genericName);
    if (it != genericTypeParams.end() && it->second.size() != typeArgs.size()) {
        addError(
            TypeError::ErrorKind::Generic,
            "Wrong number of type arguments for generic type: " + genericName,
            AST::Position{0, 0, 0});
        return Type::createAny();
    }

    // 创建泛型实例
    std::vector<std::unique_ptr<Type>> clonedArgs;
    for (const auto& arg : typeArgs) {
        clonedArgs.push_back(arg->clone());
    }
    return std::make_unique<GenericInstanceType>(genericName,
                                                 std::move(clonedArgs));
}

// 新增: 检查类型断言表达式
void TypeChecker::checkTypeAssertion(
    const AST::TypeAssertionExpression* assertion) {
    auto sourceType = getExpressionType(assertion->getExpression());
    auto targetType = resolveTypeAnnotation(assertion->getTypeAnnotation());

    // 在某些情况下可能需要验证断言的合理性
    bool isValid = true;

    // 例如：不允许将字符串断言为数字
    if (auto sourceStr = dynamic_cast<PrimitiveType*>(sourceType.get())) {
        if (auto targetNum = dynamic_cast<PrimitiveType*>(targetType.get())) {
            if (sourceStr->getKind() == PrimitiveType::Kind::String &&
                targetNum->getKind() == PrimitiveType::Kind::Number) {
                isValid = false;
            }
        }
    }

    if (!isValid) {
        addError(TypeError::ErrorKind::Incompatible,
                 "Invalid type assertion from " + sourceType->toString() +
                     " to " + targetType->toString(),
                 assertion->getPosition());
    }
}

// 新增: 检查类型守卫
void TypeChecker::checkTypeGuard(const AST::Expression* condition) {
    // 检查类似于 typeof x === 'string' 的类型守卫模式
    if (auto binExpr = dynamic_cast<const AST::BinaryExpression*>(condition)) {
        if (binExpr->getOperator() == AST::BinaryExpression::Operator::Equal ||
            binExpr->getOperator() ==
                AST::BinaryExpression::Operator::StrictEqual) {
            // 检查 typeof x === 'string' 模式
            if (auto callExpr = dynamic_cast<const AST::CallExpression*>(
                    binExpr->getLeft())) {
                if (auto callee =
                        dynamic_cast<const AST::IdentifierExpression*>(
                            callExpr->getCallee())) {
                    if (callee->getName() == "typeof" &&
                        callExpr->getArguments().size() == 1) {
                        if (auto strLiteral =
                                dynamic_cast<const AST::LiteralExpression*>(
                                    binExpr->getRight())) {
                            if (strLiteral->getKind() ==
                                AST::LiteralExpression::LiteralKind::String) {
                                // 获取被检查的标识符
                                if (auto idExpr = dynamic_cast<
                                        const AST::IdentifierExpression*>(
                                        callExpr->getArguments()[0].get())) {
                                    const std::string& varName =
                                        idExpr->getName();
                                    const std::string& typeString =
                                        strLiteral->getStringValue();

                                    // 根据字符串字面量确定类型
                                    std::unique_ptr<Type> guardedType;
                                    if (typeString == "string") {
                                        guardedType = Type::createString();
                                    } else if (typeString == "number") {
                                        guardedType = Type::createNumber();
                                    } else if (typeString == "boolean") {
                                        guardedType = Type::createBoolean();
                                    } else if (typeString == "undefined") {
                                        guardedType = Type::createUndefined();
                                    } else if (typeString == "function") {
                                        std::vector<std::unique_ptr<Type>>
                                            paramTypes;
                                        guardedType =
                                            std::make_unique<FunctionType>(
                                                std::move(paramTypes),
                                                Type::createAny());
                                    } else if (typeString == "object") {
                                        guardedType =
                                            std::make_unique<ObjectType>();
                                    } else {
                                        return;  // 不支持的类型字符串
                                    }

                                    // 应用类型守卫
                                    applyTypeGuard(varName,
                                                   std::move(guardedType));
                                }
                            }
                        }
                    }
                }
            }

            // 检查 x instanceof Y 模式
            if (auto instOfExpr =
                    dynamic_cast<const AST::InstanceOfExpression*>(
                        binExpr->getLeft())) {
                if (auto idExpr =
                        dynamic_cast<const AST::IdentifierExpression*>(
                            instOfExpr->getLeft())) {
                    if (auto typeExpr =
                            dynamic_cast<const AST::IdentifierExpression*>(
                                instOfExpr->getRight())) {
                        const std::string& varName = idExpr->getName();
                        const std::string& typeName = typeExpr->getName();

                        // 查找类型
                        Type* classType = lookupSymbol(typeName);
                        if (classType) {
                            applyTypeGuard(varName, classType->clone());
                        }
                    }
                }
            }
        }

        // 检查 x !== null && x !== undefined 模式（非空检查）
        if (binExpr->getOperator() == AST::BinaryExpression::Operator::And) {
            checkNullUndefinedGuard(binExpr);
        }
    }

    // 检查直接的 instanceof 表达式
    if (auto instOfExpr =
            dynamic_cast<const AST::InstanceOfExpression*>(condition)) {
        if (auto idExpr = dynamic_cast<const AST::IdentifierExpression*>(
                instOfExpr->getLeft())) {
            if (auto typeExpr = dynamic_cast<const AST::IdentifierExpression*>(
                    instOfExpr->getRight())) {
                const std::string& varName = idExpr->getName();
                const std::string& typeName = typeExpr->getName();

                // 查找类型
                Type* classType = lookupSymbol(typeName);
                if (classType) {
                    applyTypeGuard(varName, classType->clone());
                }
            }
        }
    }
}

// 辅助函数：检查非空守卫模式
void TypeChecker::checkNullUndefinedGuard(
    const AST::BinaryExpression* andExpr) {
    bool hasNullCheck = false;
    bool hasUndefinedCheck = false;
    std::string guardedVarName;

    // 检查左侧
    if (auto leftBin =
            dynamic_cast<const AST::BinaryExpression*>(andExpr->getLeft())) {
        if (leftBin->getOperator() ==
                AST::BinaryExpression::Operator::NotEqual ||
            leftBin->getOperator() ==
                AST::BinaryExpression::Operator::StrictNotEqual) {
            if (auto idExpr = dynamic_cast<const AST::IdentifierExpression*>(
                    leftBin->getLeft())) {
                guardedVarName = idExpr->getName();

                if (auto nullLiteral =
                        dynamic_cast<const AST::LiteralExpression*>(
                            leftBin->getRight())) {
                    if (nullLiteral->getKind() ==
                        AST::LiteralExpression::LiteralKind::Null) {
                        hasNullCheck = true;
                    } else if (nullLiteral->getKind() ==
                               AST::LiteralExpression::LiteralKind::Undefined) {
                        hasUndefinedCheck = true;
                    }
                }
            }
        }
    }

    // 检查右侧
    if (auto rightBin =
            dynamic_cast<const AST::BinaryExpression*>(andExpr->getRight())) {
        if (rightBin->getOperator() ==
                AST::BinaryExpression::Operator::NotEqual ||
            rightBin->getOperator() ==
                AST::BinaryExpression::Operator::StrictNotEqual) {
            if (auto idExpr = dynamic_cast<const AST::IdentifierExpression*>(
                    rightBin->getLeft())) {
                // 确保左右两边检查的是同一个变量
                if (guardedVarName.empty() ||
                    guardedVarName == idExpr->getName()) {
                    if (guardedVarName.empty()) {
                        guardedVarName = idExpr->getName();
                    }

                    if (auto nullLiteral =
                            dynamic_cast<const AST::LiteralExpression*>(
                                rightBin->getRight())) {
                        if (nullLiteral->getKind() ==
                            AST::LiteralExpression::LiteralKind::Null) {
                            hasNullCheck = true;
                        } else if (nullLiteral->getKind() ==
                                   AST::LiteralExpression::LiteralKind::
                                       Undefined) {
                            hasUndefinedCheck = true;
                        }
                    }
                }
            }
        }
    }

    // 如果同时有null和undefined检查，应用非空类型守卫
    if (!guardedVarName.empty() && hasNullCheck && hasUndefinedCheck) {
        Type* originalType = lookupSymbol(guardedVarName);
        if (originalType) {
            // 创建非空类型（从原始类型中排除null和undefined）
            auto nonNullableType =
                removeNullAndUndefined(originalType->clone());
            applyTypeGuard(guardedVarName, std::move(nonNullableType));
        }
    }
}

// 辅助函数：从类型中移除null和undefined
std::unique_ptr<Type> TypeChecker::removeNullAndUndefined(
    std::unique_ptr<Type> type) {
    if (auto unionType = dynamic_cast<UnionType*>(type.get())) {
        std::vector<std::unique_ptr<Type>> newTypes;
        for (auto& memberType : unionType->getTypes()) {
            if (auto primType =
                    dynamic_cast<PrimitiveType*>(memberType.get())) {
                if (primType->getKind() != PrimitiveType::Kind::Null &&
                    primType->getKind() != PrimitiveType::Kind::Undefined) {
                    newTypes.push_back(memberType->clone());
                }
            } else {
                newTypes.push_back(memberType->clone());
            }
        }

        if (newTypes.empty()) {
            return Type::createNever();  // 所有类型都被排除了
        } else if (newTypes.size() == 1) {
            return std::move(newTypes[0]);  // 只剩一个类型
        } else {
            return std::make_unique<UnionType>(std::move(newTypes));
        }
    }

    // 如果不是联合类型，直接返回
    return type;
}

std::unique_ptr<Type> TypeChecker::checkTypeAssertionExpression(
    const AST::TypeAssertionExpression* expr) {
    // 先执行类型断言检查
    checkTypeAssertion(expr);

    // 返回断言后的类型
    auto targetType = resolveTypeAnnotation(expr->getTypeAnnotation());
    return targetType;
}
}  // namespace tsx
