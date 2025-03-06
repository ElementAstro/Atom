#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>
#include "../types/types.h"

namespace tsx {
namespace AST {

// Forward declarations
class Expression;
class Statement;
class TypeAnnotation;

// 添加：Position结构体，表示源代码中的具体位置
struct Position {
    size_t line;    // 行号
    size_t column;  // 列号
    size_t offset;  // 文件中的字节偏移量

    Position() : line(0), column(0), offset(0) {}
    Position(size_t l, size_t c, size_t o) : line(l), column(c), offset(o) {}

    std::string toString() const {
        return "line " + std::to_string(line) + ", column " +
               std::to_string(column);
    }
};

// Base AST Node
class Node {
protected:
    Position position;

public:
    virtual ~Node() = default;
    virtual std::string toString() const = 0;

    const Position& getLocation() const { return position; }
    void setLocation(const Position& pos) { position = pos; }
};

// Location information for error reporting
struct SourceLocation {
    size_t start;
    size_t end;
    size_t line;
    size_t column;
};

// 添加：TypeAnnotation基类 - 表示类型注解
class TypeAnnotation : public Node {
protected:
    Position position;

public:
    virtual ~TypeAnnotation() = default;
    void setPosition(const Position& pos) { position = pos; }
    const Position& getPosition() const { return position; }
};

// 添加：基本类型注解（如number, string, boolean等）
class BasicTypeAnnotation : public TypeAnnotation {
private:
    std::string typeName;

public:
    explicit BasicTypeAnnotation(const std::string& name) : typeName(name) {}

    std::string toString() const override { return typeName; }

    const std::string& getTypeName() const { return typeName; }
};

// 添加：数组类型注解（如 string[]）
class ArrayTypeAnnotation : public TypeAnnotation {
private:
    std::unique_ptr<TypeAnnotation> elementType;

public:
    explicit ArrayTypeAnnotation(std::unique_ptr<TypeAnnotation> elemType)
        : elementType(std::move(elemType)) {}

    std::string toString() const override {
        return elementType->toString() + "[]";
    }

    const TypeAnnotation* getElementType() const { return elementType.get(); }
};

// 添加：对象类型注解（如 {name: string, age: number}）
class ObjectTypeAnnotation : public TypeAnnotation {
public:
    struct Property {
        std::string name;
        std::unique_ptr<TypeAnnotation> type;
        bool optional;

        Property(std::string n, std::unique_ptr<TypeAnnotation> t,
                 bool opt = false)
            : name(std::move(n)), type(std::move(t)), optional(opt) {}
    };

private:
    std::vector<Property> properties;

public:
    void addProperty(Property prop) { properties.push_back(std::move(prop)); }

    std::string toString() const override {
        std::string result = "{ ";
        for (size_t i = 0; i < properties.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += properties[i].name;
            if (properties[i].optional)
                result += "?";
            result += ": " + properties[i].type->toString();
        }
        return result + " }";
    }

    const std::vector<Property>& getProperties() const { return properties; }
};

// 添加：函数类型注解（如 (a: number, b: string) => boolean）
class FunctionTypeAnnotation : public TypeAnnotation {
public:
    struct Parameter {
        std::string name;
        std::unique_ptr<TypeAnnotation> type;
        bool optional;
        bool isRest;

        Parameter(std::string n, std::unique_ptr<TypeAnnotation> t,
                  bool opt = false, bool rest = false)
            : name(std::move(n)),
              type(std::move(t)),
              optional(opt),
              isRest(rest) {}
    };

private:
    std::vector<Parameter> parameters;
    std::unique_ptr<TypeAnnotation> returnType;

public:
    explicit FunctionTypeAnnotation(std::unique_ptr<TypeAnnotation> retType)
        : returnType(std::move(retType)) {}

    void addParameter(Parameter param) {
        parameters.push_back(std::move(param));
    }

    std::string toString() const override {
        std::string result = "(";
        for (size_t i = 0; i < parameters.size(); ++i) {
            if (i > 0)
                result += ", ";
            if (parameters[i].isRest)
                result += "...";
            if (!parameters[i].name.empty())
                result += parameters[i].name + ": ";
            result += parameters[i].type->toString();
            if (parameters[i].optional)
                result += "?";
        }
        result += ") => " + returnType->toString();
        return result;
    }

    const std::vector<Parameter>& getParameters() const { return parameters; }
    const TypeAnnotation* getReturnType() const { return returnType.get(); }
};

// 添加：联合类型注解（如 string | number）
class UnionTypeAnnotation : public TypeAnnotation {
private:
    std::vector<std::unique_ptr<TypeAnnotation>> types;

public:
    void addType(std::unique_ptr<TypeAnnotation> type) {
        types.push_back(std::move(type));
    }

    std::string toString() const override {
        std::string result = "";
        for (size_t i = 0; i < types.size(); ++i) {
            if (i > 0)
                result += " | ";
            result += types[i]->toString();
        }
        return result;
    }

    const std::vector<std::unique_ptr<TypeAnnotation>>& getTypes() const {
        return types;
    }
};

// 添加：交叉类型注解（如 T & U）
class IntersectionTypeAnnotation : public TypeAnnotation {
private:
    std::vector<std::unique_ptr<TypeAnnotation>> types;

public:
    void addType(std::unique_ptr<TypeAnnotation> type) {
        types.push_back(std::move(type));
    }

    std::string toString() const override {
        std::string result = "";
        for (size_t i = 0; i < types.size(); ++i) {
            if (i > 0)
                result += " & ";
            result += types[i]->toString();
        }
        return result;
    }

    const std::vector<std::unique_ptr<TypeAnnotation>>& getTypes() const {
        return types;
    }
};

// 添加：泛型类型注解（如 Array<T>）
class GenericTypeAnnotation : public TypeAnnotation {
private:
    std::string baseType;
    std::vector<std::unique_ptr<TypeAnnotation>> typeArguments;

public:
    explicit GenericTypeAnnotation(std::string base)
        : baseType(std::move(base)) {}

    void addTypeArgument(std::unique_ptr<TypeAnnotation> typeArg) {
        typeArguments.push_back(std::move(typeArg));
    }

    std::string toString() const override {
        std::string result = baseType;
        if (!typeArguments.empty()) {
            result += "<";
            for (size_t i = 0; i < typeArguments.size(); ++i) {
                if (i > 0)
                    result += ", ";
                result += typeArguments[i]->toString();
            }
            result += ">";
        }
        return result;
    }

    const std::string& getBaseType() const { return baseType; }
    const std::vector<std::unique_ptr<TypeAnnotation>>& getTypeArguments()
        const {
        return typeArguments;
    }
};

// 添加：元组类型注解（如 [string, number]）
class TupleTypeAnnotation : public TypeAnnotation {
private:
    std::vector<std::unique_ptr<TypeAnnotation>> elementTypes;

public:
    void addElementType(std::unique_ptr<TypeAnnotation> elemType) {
        elementTypes.push_back(std::move(elemType));
    }

    std::string toString() const override {
        std::string result = "[";
        for (size_t i = 0; i < elementTypes.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += elementTypes[i]->toString();
        }
        return result + "]";
    }

    const std::vector<std::unique_ptr<TypeAnnotation>>& getElementTypes()
        const {
        return elementTypes;
    }
};

// Base for all expressions
class Expression : public Node {
protected:
    SourceLocation location;
    std::unique_ptr<Type> type;  // Type information filled by type checker

public:
    virtual ~Expression() = default;
    void setType(std::unique_ptr<Type> t) { type = std::move(t); }
    const Type* getType() const { return type.get(); }
    const SourceLocation& getLocation() const { return location; }
    void setLocation(SourceLocation loc) { location = loc; }
    const Position& getPosition() const { return position; }
    void setPosition(const Position& pos) { position = pos; }
};

// 添加：类型断言表达式
class TypeAssertionExpression : public Expression {
private:
    std::unique_ptr<Expression> expression;
    std::unique_ptr<TypeAnnotation> typeAnnotation;

public:
    TypeAssertionExpression(std::unique_ptr<Expression> expr,
                            std::unique_ptr<TypeAnnotation> typeAnnot)
        : expression(std::move(expr)), typeAnnotation(std::move(typeAnnot)) {}

    std::string toString() const override {
        return "(" + expression->toString() + " as " +
               typeAnnotation->toString() + ")";
    }

    const Expression* getExpression() const { return expression.get(); }
    Expression* getExpressionMutable() { return expression.get(); }
    const TypeAnnotation* getTypeAnnotation() const {
        return typeAnnotation.get();
    }
};

// Literal expressions
class LiteralExpression : public Expression {
public:
    enum class LiteralKind { Number, String, Boolean, Null, Undefined };

    using LiteralValue =
        std::variant<double, std::string, bool, std::nullptr_t, std::monostate>;

private:
    LiteralKind kind;
    LiteralValue value;

public:
    LiteralExpression(LiteralKind k, LiteralValue v)
        : kind(k), value(std::move(v)) {}

    std::string toString() const override {
        switch (kind) {
            case LiteralKind::Number:
                return std::to_string(std::get<double>(value));
            case LiteralKind::String:
                return "\"" + std::get<std::string>(value) + "\"";
            case LiteralKind::Boolean:
                return std::get<bool>(value) ? "true" : "false";
            case LiteralKind::Null:
                return "null";
            case LiteralKind::Undefined:
                return "undefined";
        }
        return "";  // To satisfy compiler
    }

    LiteralKind getKind() const { return kind; }
    const LiteralValue& getValue() const { return value; }

    std::string getStringValue() const {
        if (kind == LiteralKind::String) {
            return std::get<std::string>(value);
        }
        return "";
    }
};

// Identifier reference
class IdentifierExpression : public Expression {
private:
    std::string name;

public:
    explicit IdentifierExpression(std::string n) : name(std::move(n)) {}

    std::string toString() const override { return name; }

    const std::string& getName() const { return name; }
};

// Binary operations
class BinaryExpression : public Expression {
public:
    enum class Operator {
        Add,
        Subtract,
        Multiply,
        Divide,
        Modulo,
        Equal,
        NotEqual,
        Less,
        LessEqual,
        Greater,
        GreaterEqual,
        And,
        Or,
        BitwiseAnd,
        BitwiseOr,
        BitwiseXor,
        LeftShift,
        RightShift,
        UnsignedRightShift,
        StrictEqual,
        StrictNotEqual
    };

private:
    Operator op;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;

public:
    BinaryExpression(Operator op, std::unique_ptr<Expression> l,
                     std::unique_ptr<Expression> r)
        : op(op), left(std::move(l)), right(std::move(r)) {}

    std::string toString() const override {
        std::string opStr;
        switch (op) {
            case Operator::Add:
                opStr = "+";
                break;
            case Operator::Subtract:
                opStr = "-";
                break;
            case Operator::Multiply:
                opStr = "*";
                break;
            case Operator::Divide:
                opStr = "/";
                break;
            case Operator::Modulo:
                opStr = "%";
                break;
            case Operator::Equal:
                opStr = "==";
                break;
            case Operator::NotEqual:
                opStr = "!=";
                break;
            case Operator::Less:
                opStr = "<";
                break;
            case Operator::LessEqual:
                opStr = "<=";
                break;
            case Operator::Greater:
                opStr = ">";
                break;
            case Operator::GreaterEqual:
                opStr = ">=";
                break;
            case Operator::And:
                opStr = "&&";
                break;
            case Operator::Or:
                opStr = "||";
                break;
            case Operator::BitwiseAnd:
                opStr = "&";
                break;
            case Operator::BitwiseOr:
                opStr = "|";
                break;
            case Operator::BitwiseXor:
                opStr = "^";
                break;
            case Operator::LeftShift:
                opStr = "<<";
                break;
            case Operator::RightShift:
                opStr = ">>";
                break;
            case Operator::UnsignedRightShift:
                opStr = ">>>";
                break;
            case Operator::StrictEqual:
                opStr = "===";
                break;
            case Operator::StrictNotEqual:
                opStr = "!==";
                break;
        }
        return "(" + left->toString() + " " + opStr + " " + right->toString() +
               ")";
    }

    Operator getOperator() const { return op; }
    const Expression* getLeft() const { return left.get(); }
    const Expression* getRight() const { return right.get(); }
    Expression* getLeftMutable() { return left.get(); }
    Expression* getRightMutable() { return right.get(); }
};

// Unary operations
class UnaryExpression : public Expression {
public:
    enum class Operator {
        Minus,
        Plus,
        Not,
        BitwiseNot,
        Increment,
        Decrement,
        TypeOf,
        Delete
    };

    enum class Prefix { Yes, No };

private:
    Operator op;
    std::unique_ptr<Expression> operand;
    Prefix prefix;

public:
    UnaryExpression(Operator o, std::unique_ptr<Expression> e,
                    Prefix p = Prefix::Yes)
        : op(o), operand(std::move(e)), prefix(p) {}

    std::string toString() const override {
        std::string opStr;
        switch (op) {
            case Operator::Minus:
                opStr = "-";
                break;
            case Operator::Plus:
                opStr = "+";
                break;
            case Operator::Not:
                opStr = "!";
                break;
            case Operator::BitwiseNot:
                opStr = "~";
                break;
            case Operator::Increment:
                opStr = "++";
                break;
            case Operator::Decrement:
                opStr = "--";
                break;
            case Operator::TypeOf:
                opStr = "typeof ";
                break;
            case Operator::Delete:
                opStr = "delete ";
                break;
        }

        if (prefix == Prefix::Yes) {
            return opStr + operand->toString();
        } else {
            return operand->toString() + opStr;
        }
    }

    Operator getOperator() const { return op; }
    const Expression* getOperand() const { return operand.get(); }
    Expression* getOperandMutable() { return operand.get(); }
    Prefix isPrefix() const { return prefix; }
};

// Array literal ([1, 2, 3])
class ArrayLiteralExpression : public Expression {
private:
    std::vector<std::unique_ptr<Expression>> elements;

public:
    explicit ArrayLiteralExpression(
        std::vector<std::unique_ptr<Expression>> elems)
        : elements(std::move(elems)) {}

    std::string toString() const override {
        std::string result = "[";
        for (size_t i = 0; i < elements.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += elements[i]->toString();
        }
        return result + "]";
    }

    const std::vector<std::unique_ptr<Expression>>& getElements() const {
        return elements;
    }
};

// Object literal ({x: 1, y: "hello"})
class ObjectLiteralExpression : public Expression {
public:
    struct Property {
        std::string key;
        std::unique_ptr<Expression> value;
    };

private:
    std::vector<Property> properties;

public:
    explicit ObjectLiteralExpression(std::vector<Property> props)
        : properties(std::move(props)) {}

    std::string toString() const override {
        std::string result = "{";
        for (size_t i = 0; i < properties.size(); ++i) {
            if (i > 0)
                result += ", ";
            result +=
                properties[i].key + ": " + properties[i].value->toString();
        }
        return result + "}";
    }

    const std::vector<Property>& getProperties() const { return properties; }
};

// Member expression (obj.prop or obj["prop"])
class MemberExpression : public Expression {
private:
    std::unique_ptr<Expression> object;
    std::unique_ptr<Expression> property;
    bool computed;  // true for obj["prop"], false for obj.prop

public:
    MemberExpression(std::unique_ptr<Expression> obj,
                     std::unique_ptr<Expression> prop, bool comp)
        : object(std::move(obj)), property(std::move(prop)), computed(comp) {}

    std::string toString() const override {
        if (computed) {
            return object->toString() + "[" + property->toString() + "]";
        } else {
            return object->toString() + "." + property->toString();
        }
    }

    const Expression* getObject() const { return object.get(); }
    const Expression* getProperty() const { return property.get(); }
    bool isComputed() const { return computed; }
};

// Call expression (func(arg1, arg2))
class CallExpression : public Expression {
private:
    std::unique_ptr<Expression> callee;
    std::vector<std::unique_ptr<Expression>> arguments;

public:
    CallExpression(std::unique_ptr<Expression> callee,
                   std::vector<std::unique_ptr<Expression>> args)
        : callee(std::move(callee)), arguments(std::move(args)) {}

    std::string toString() const override {
        std::string result = callee->toString() + "(";
        for (size_t i = 0; i < arguments.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += arguments[i]->toString();
        }
        return result + ")";
    }

    const Expression* getCallee() const { return callee.get(); }
    const std::vector<std::unique_ptr<Expression>>& getArguments() const {
        return arguments;
    }
};

// Conditional/Ternary expression (cond ? consequent : alternate)
class ConditionalExpression : public Expression {
private:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Expression> consequent;
    std::unique_ptr<Expression> alternate;

public:
    ConditionalExpression(std::unique_ptr<Expression> cond,
                          std::unique_ptr<Expression> cons,
                          std::unique_ptr<Expression> alt)
        : condition(std::move(cond)),
          consequent(std::move(cons)),
          alternate(std::move(alt)) {}

    std::string toString() const override {
        return condition->toString() + " ? " + consequent->toString() + " : " +
               alternate->toString();
    }

    const Expression* getCondition() const { return condition.get(); }
    const Expression* getConsequent() const { return consequent.get(); }
    const Expression* getAlternate() const { return alternate.get(); }
};

// Base Statement class
class Statement : public Node {
protected:
    SourceLocation location;

public:
    virtual ~Statement() = default;
    const SourceLocation& getLocation() const { return location; }
    void setLocation(SourceLocation loc) { location = loc; }
};

// Expression statement (standalone expression)
class ExpressionStatement : public Statement {
private:
    std::unique_ptr<Expression> expression;

public:
    explicit ExpressionStatement(std::unique_ptr<Expression> expr)
        : expression(std::move(expr)) {}

    std::string toString() const override {
        return expression->toString() + ";";
    }

    const Expression* getExpression() const { return expression.get(); }
    Expression* getExpressionMutable() { return expression.get(); }
};

// Variable declaration
class VariableDeclaration : public Statement {
public:
    enum class Kind { Var, Let, Const };

    struct Declarator {
        std::string name;
        std::unique_ptr<Expression> initializer;
        std::unique_ptr<Type> typeAnnotation;
    };

private:
    Kind kind;
    std::vector<Declarator> declarations;

public:
    explicit VariableDeclaration(Kind k) : kind(k) {}

    void addDeclarator(std::string name,
                       std::unique_ptr<Expression> init = nullptr,
                       std::unique_ptr<Type> typeAnnot = nullptr) {
        declarations.push_back(
            {std::move(name), std::move(init), std::move(typeAnnot)});
    }

    std::string toString() const override {
        std::string kindStr;
        switch (kind) {
            case Kind::Var:
                kindStr = "var";
                break;
            case Kind::Let:
                kindStr = "let";
                break;
            case Kind::Const:
                kindStr = "const";
                break;
        }

        std::string result = kindStr + " ";
        for (size_t i = 0; i < declarations.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += declarations[i].name;

            if (declarations[i].typeAnnotation) {
                result += ": " + declarations[i].typeAnnotation->toString();
            }

            if (declarations[i].initializer) {
                result += " = " + declarations[i].initializer->toString();
            }
        }

        return result + ";";
    }

    Kind getKind() const { return kind; }
    const std::vector<Declarator>& getDeclarations() const {
        return declarations;
    }
};

// Block statement (compound statement)
class BlockStatement : public Statement {
private:
    std::vector<std::unique_ptr<Statement>> statements;

public:
    void addStatement(std::unique_ptr<Statement> stmt) {
        statements.push_back(std::move(stmt));
    }

    std::string toString() const override {
        std::string result = "{\n";
        for (const auto& stmt : statements) {
            result += "  " + stmt->toString() + "\n";
        }
        return result + "}";
    }

    const std::vector<std::unique_ptr<Statement>>& getStatements() const {
        return statements;
    }
};

// If statement
class IfStatement : public Statement {
private:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> thenBranch;
    std::unique_ptr<Statement> elseBranch;

public:
    IfStatement(std::unique_ptr<Expression> cond,
                std::unique_ptr<Statement> thenStmt,
                std::unique_ptr<Statement> elseStmt = nullptr)
        : condition(std::move(cond)),
          thenBranch(std::move(thenStmt)),
          elseBranch(std::move(elseStmt)) {}

    std::string toString() const override {
        std::string result =
            "if (" + condition->toString() + ") " + thenBranch->toString();
        if (elseBranch) {
            result += " else " + elseBranch->toString();
        }
        return result;
    }

    const Expression* getCondition() const { return condition.get(); }
    const Statement* getThenBranch() const { return thenBranch.get(); }
    const Statement* getElseBranch() const { return elseBranch.get(); }
    Expression* getConditionMutable() { return condition.get(); }
    Statement* getThenBranchMutable() { return thenBranch.get(); }
    Statement* getElseBranchMutable() { return elseBranch.get(); }
};

// Function declaration
class FunctionDeclaration : public Statement {
public:
    struct Parameter {
        std::string name;
        std::unique_ptr<Type> typeAnnotation;
        std::unique_ptr<Expression> defaultValue;
        bool isRest = false;
    };

private:
    std::string name;
    std::vector<Parameter> parameters;
    std::unique_ptr<Type> returnType;
    std::unique_ptr<BlockStatement> body;
    bool isAsync = false;
    bool isGenerator = false;
    std::vector<std::unique_ptr<GenericTypeParameter>> typeParameters;

public:
    explicit FunctionDeclaration(std::string n) : name(std::move(n)) {}

    void addParameter(std::string name,
                      std::unique_ptr<Type> typeAnnot = nullptr,
                      std::unique_ptr<Expression> defaultVal = nullptr,
                      bool isRest = false) {
        parameters.push_back({std::move(name), std::move(typeAnnot),
                              std::move(defaultVal), isRest});
    }

    void setReturnType(std::unique_ptr<Type> type) {
        returnType = std::move(type);
    }

    void setBody(std::unique_ptr<BlockStatement> b) { body = std::move(b); }

    void setIsAsync(bool async) { isAsync = async; }

    void setIsGenerator(bool generator) { isGenerator = generator; }

    void addTypeParameter(std::unique_ptr<GenericTypeParameter> param) {
        typeParameters.push_back(std::move(param));
    }

    std::string toString() const override {
        std::string result = "";

        if (isAsync)
            result += "async ";
        result += "function";
        if (isGenerator)
            result += "*";
        result += " " + name;

        // Type parameters
        if (!typeParameters.empty()) {
            result += "<";
            for (size_t i = 0; i < typeParameters.size(); ++i) {
                if (i > 0)
                    result += ", ";
                result += typeParameters[i]->toString();
            }
            result += ">";
        }

        result += "(";
        for (size_t i = 0; i < parameters.size(); ++i) {
            if (i > 0)
                result += ", ";
            if (parameters[i].isRest)
                result += "...";
            result += parameters[i].name;

            if (parameters[i].typeAnnotation) {
                result += ": " + parameters[i].typeAnnotation->toString();
            }

            if (parameters[i].defaultValue) {
                result += " = " + parameters[i].defaultValue->toString();
            }
        }
        result += ")";

        if (returnType) {
            result += ": " + returnType->toString();
        }

        result += " " + body->toString();
        return result;
    }

    const std::string& getName() const { return name; }
    const std::vector<Parameter>& getParameters() const { return parameters; }
    const Type* getReturnType() const { return returnType.get(); }
    const BlockStatement* getBody() const { return body.get(); }
    BlockStatement* getBodyMutable() { return body.get(); }
    bool getIsAsync() const { return isAsync; }
    bool getIsGenerator() const { return isGenerator; }
    const std::vector<std::unique_ptr<GenericTypeParameter>>&
    getTypeParameters() const {
        return typeParameters;
    }
};

// Class declaration
class ClassDeclaration : public Statement {
public:
    enum class MemberKind {
        Constructor,
        Method,
        Property,
        GetAccessor,
        SetAccessor
    };
    enum class Visibility { Public, Private, Protected };

    struct Member {
        MemberKind kind;
        Visibility visibility;
        std::string name;
        bool isStatic;
        bool isReadonly;

        // For methods and accessors
        std::unique_ptr<FunctionDeclaration> methodDecl;

        // For properties
        std::unique_ptr<Type> propertyType;
        std::unique_ptr<Expression> initializer;
    };

private:
    std::string name;
    std::string baseClassName;
    std::vector<std::string> implementsInterfaces;
    std::vector<Member> members;
    std::vector<std::unique_ptr<GenericTypeParameter>> typeParameters;

public:
    explicit ClassDeclaration(std::string n) : name(std::move(n)) {}

    void setBaseClass(std::string base) { baseClassName = std::move(base); }

    void addImplements(std::string interface) {
        implementsInterfaces.push_back(std::move(interface));
    }

    void addMember(Member member) { members.push_back(std::move(member)); }

    void addTypeParameter(std::unique_ptr<GenericTypeParameter> param) {
        typeParameters.push_back(std::move(param));
    }

    std::string toString() const override {
        std::string result = "class " + name;

        // Type parameters
        if (!typeParameters.empty()) {
            result += "<";
            for (size_t i = 0; i < typeParameters.size(); ++i) {
                if (i > 0)
                    result += ", ";
                result += typeParameters[i]->toString();
            }
            result += ">";
        }

        if (!baseClassName.empty()) {
            result += " extends " + baseClassName;
        }

        if (!implementsInterfaces.empty()) {
            result += " implements ";
            for (size_t i = 0; i < implementsInterfaces.size(); ++i) {
                if (i > 0)
                    result += ", ";
                result += implementsInterfaces[i];
            }
        }

        result += " {\n";

        for (const auto& member : members) {
            result += "  ";

            // Visibility
            switch (member.visibility) {
                case Visibility::Public:
                    result += "public ";
                    break;
                case Visibility::Private:
                    result += "private ";
                    break;
                case Visibility::Protected:
                    result += "protected ";
                    break;
            }

            if (member.isStatic)
                result += "static ";
            if (member.isReadonly)
                result += "readonly ";

            switch (member.kind) {
                case MemberKind::Constructor:
                    result += "constructor" +
                              member.methodDecl->toString().substr(
                                  member.methodDecl->getName().length() +
                                  8);  // Remove "function name"
                    break;

                case MemberKind::Method:
                    result += member.methodDecl->toString().substr(
                        9);  // Remove "function "
                    break;

                case MemberKind::Property:
                    result += member.name;
                    if (member.propertyType) {
                        result += ": " + member.propertyType->toString();
                    }
                    if (member.initializer) {
                        result += " = " + member.initializer->toString();
                    }
                    result += ";";
                    break;

                case MemberKind::GetAccessor:
                    result += "get " + member.name + "() ";
                    if (member.methodDecl->getReturnType()) {
                        result +=
                            ": " +
                            member.methodDecl->getReturnType()->toString() +
                            " ";
                    }
                    result += member.methodDecl->getBody()->toString();
                    break;

                case MemberKind::SetAccessor:
                    result += "set " + member.name + "(";
                    if (!member.methodDecl->getParameters().empty()) {
                        const auto& param =
                            member.methodDecl->getParameters()[0];
                        result += param.name;
                        if (param.typeAnnotation) {
                            result += ": " + param.typeAnnotation->toString();
                        }
                    }
                    result += ") " + member.methodDecl->getBody()->toString();
                    break;
            }

            result += "\n";
        }

        result += "}";
        return result;
    }

    const std::string& getName() const { return name; }
    const std::string& getBaseClassName() const { return baseClassName; }
    const std::vector<std::string>& getImplements() const {
        return implementsInterfaces;
    }
    const std::vector<Member>& getMembers() const { return members; }
    const std::vector<std::unique_ptr<GenericTypeParameter>>&
    getTypeParameters() const {
        return typeParameters;
    }
};

// Interface declaration
class InterfaceDeclaration : public Statement {
public:
    struct Property {
        std::string name;
        std::unique_ptr<Type> type;
        bool optional;
        bool readonly;
    };

    struct Method {
        std::string name;
        std::vector<FunctionDeclaration::Parameter> parameters;
        std::unique_ptr<Type> returnType;
        bool optional;
        std::vector<std::unique_ptr<GenericTypeParameter>> typeParameters;
    };

private:
    std::string name;
    std::vector<std::string> extendsInterfaces;
    std::vector<Property> properties;
    std::vector<Method> methods;
    std::vector<std::unique_ptr<GenericTypeParameter>> typeParameters;

public:
    explicit InterfaceDeclaration(std::string n) : name(std::move(n)) {}

    void addExtends(std::string interface) {
        extendsInterfaces.push_back(std::move(interface));
    }

    void addProperty(Property prop) { properties.push_back(std::move(prop)); }

    void addMethod(Method method) { methods.push_back(std::move(method)); }

    void addTypeParameter(std::unique_ptr<GenericTypeParameter> param) {
        typeParameters.push_back(std::move(param));
    }

    std::string toString() const override {
        std::string result = "interface " + name;

        // Type parameters
        if (!typeParameters.empty()) {
            result += "<";
            for (size_t i = 0; i < typeParameters.size(); ++i) {
                if (i > 0)
                    result += ", ";
                result += typeParameters[i]->toString();
            }
            result += ">";
        }

        if (!extendsInterfaces.empty()) {
            result += " extends ";
            for (size_t i = 0; i < extendsInterfaces.size(); ++i) {
                if (i > 0)
                    result += ", ";
                result += extendsInterfaces[i];
            }
        }

        result += " {\n";

        // Properties
        for (const auto& prop : properties) {
            result += "  ";
            if (prop.readonly)
                result += "readonly ";
            result += prop.name;
            if (prop.optional)
                result += "?";
            result += ": " + prop.type->toString() + ";\n";
        }

        // Methods
        for (const auto& method : methods) {
            result += "  " + method.name;
            if (method.optional)
                result += "?";

            // Type parameters
            if (!method.typeParameters.empty()) {
                result += "<";
                for (size_t i = 0; i < method.typeParameters.size(); ++i) {
                    if (i > 0)
                        result += ", ";
                    result += method.typeParameters[i]->toString();
                }
                result += ">";
            }

            result += "(";
            for (size_t i = 0; i < method.parameters.size(); ++i) {
                if (i > 0)
                    result += ", ";
                const auto& param = method.parameters[i];
                if (param.isRest)
                    result += "...";
                result += param.name;

                if (param.typeAnnotation) {
                    result += ": " + param.typeAnnotation->toString();
                }

                if (param.defaultValue) {
                    result += " = " + param.defaultValue->toString();
                }
            }
            result += ")";

            if (method.returnType) {
                result += ": " + method.returnType->toString();
            }

            result += ";\n";
        }

        result += "}";
        return result;
    }

    const std::string& getName() const { return name; }
    const std::vector<std::string>& getExtends() const {
        return extendsInterfaces;
    }
    const std::vector<Property>& getProperties() const { return properties; }
    const std::vector<Method>& getMethods() const { return methods; }
    const std::vector<std::unique_ptr<GenericTypeParameter>>&
    getTypeParameters() const {
        return typeParameters;
    }
};

// Program (top-level node)
class Program : public Node {
private:
    std::vector<std::unique_ptr<Statement>> statements;

public:
    void addStatement(std::unique_ptr<Statement> stmt) {
        statements.push_back(std::move(stmt));
    }

    std::string toString() const override {
        std::string result;
        for (const auto& stmt : statements) {
            result += stmt->toString() + "\n";
        }
        return result;
    }

    const std::vector<std::unique_ptr<Statement>>& getStatements() const {
        return statements;
    }
};

// 添加 ReturnStatement 类定义
class ReturnStatement : public Statement {
private:
    std::unique_ptr<Expression> value;

public:
    explicit ReturnStatement(std::unique_ptr<Expression> val = nullptr)
        : value(std::move(val)) {}

    std::string toString() const override {
        if (value) {
            return "return " + value->toString() + ";";
        }
        return "return;";
    }

    const Expression* getValue() const { return value.get(); }
    Expression* getValueMutable() { return value.get(); }
};

class InstanceOfExpression : public Expression {
private:
    std::unique_ptr<Expression> left;   // 要检查的对象
    std::unique_ptr<Expression> right;  // 类构造函数

public:
    InstanceOfExpression(std::unique_ptr<Expression> l,
                         std::unique_ptr<Expression> r)
        : left(std::move(l)), right(std::move(r)) {}

    std::string toString() const override {
        return "(" + left->toString() + " instanceof " + right->toString() +
               ")";
    }

    const Expression* getLeft() const { return left.get(); }
    const Expression* getRight() const { return right.get(); }
    Expression* getLeftMutable() { return left.get(); }
    Expression* getRightMutable() { return right.get(); }
};

// 严格相等表达式 (a === b, a !== b)
class StrictEqualExpression : public Expression {
public:
    enum class Mode { Equal, NotEqual };

private:
    Mode mode;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;

public:
    StrictEqualExpression(Mode m, std::unique_ptr<Expression> l,
                          std::unique_ptr<Expression> r)
        : mode(m), left(std::move(l)), right(std::move(r)) {}

    std::string toString() const override {
        std::string opStr = (mode == Mode::Equal) ? "===" : "!==";
        return "(" + left->toString() + " " + opStr + " " + right->toString() +
               ")";
    }

    Mode getMode() const { return mode; }
    const Expression* getLeft() const { return left.get(); }
    const Expression* getRight() const { return right.get(); }
    Expression* getLeftMutable() { return left.get(); }
    Expression* getRightMutable() { return right.get(); }
};

class FunctionExpression : public Expression {
public:
    struct Parameter {
        std::string name;
        std::unique_ptr<Type> typeAnnotation;
        std::unique_ptr<Expression> defaultValue;
        bool isRest = false;
    };

    enum class Kind { Normal, Arrow };

private:
    std::optional<std::string> name;  // 可选名称（匿名函数可能没有名称）
    std::vector<Parameter> parameters;
    std::unique_ptr<Type> returnType;
    std::unique_ptr<BlockStatement> body;
    bool isAsync = false;
    bool isGenerator = false;
    Kind kind = Kind::Normal;
    std::vector<std::unique_ptr<GenericTypeParameter>> typeParameters;

public:
    explicit FunctionExpression(std::optional<std::string> n = std::nullopt,
                                Kind k = Kind::Normal)
        : name(std::move(n)), kind(k) {}

    void addParameter(std::string name,
                      std::unique_ptr<Type> typeAnnot = nullptr,
                      std::unique_ptr<Expression> defaultVal = nullptr,
                      bool isRest = false) {
        parameters.push_back({std::move(name), std::move(typeAnnot),
                              std::move(defaultVal), isRest});
    }

    void setReturnType(std::unique_ptr<Type> type) {
        returnType = std::move(type);
    }

    void setBody(std::unique_ptr<BlockStatement> b) { body = std::move(b); }

    void setIsAsync(bool async) { isAsync = async; }

    void setIsGenerator(bool generator) { isGenerator = generator; }

    void addTypeParameter(std::unique_ptr<GenericTypeParameter> param) {
        typeParameters.push_back(std::move(param));
    }

    std::string toString() const override {
        std::string result = "";

        if (isAsync)
            result += "async ";

        if (kind == Kind::Normal) {
            result += "function";

            if (isGenerator)
                result += "*";

            if (name)
                result += " " + *name;
        }

        // 类型参数
        if (!typeParameters.empty() && kind == Kind::Normal) {
            result += "<";
            for (size_t i = 0; i < typeParameters.size(); ++i) {
                if (i > 0)
                    result += ", ";
                result += typeParameters[i]->toString();
            }
            result += ">";
        }

        result += "(";
        for (size_t i = 0; i < parameters.size(); ++i) {
            if (i > 0)
                result += ", ";
            if (parameters[i].isRest)
                result += "...";
            result += parameters[i].name;

            if (parameters[i].typeAnnotation) {
                result += ": " + parameters[i].typeAnnotation->toString();
            }

            if (parameters[i].defaultValue) {
                result += " = " + parameters[i].defaultValue->toString();
            }
        }
        result += ")";

        if (returnType) {
            result += ": " + returnType->toString();
        }

        if (kind == Kind::Arrow) {
            result += " => ";
            // 箭头函数可以有简写体或块体
            if (body && body->getStatements().size() == 1) {
                // 如果只有一条语句，看是否为返回语句
                auto* retStmt = dynamic_cast<const ReturnStatement*>(
                    body->getStatements()[0].get());
                if (retStmt && retStmt->getValue()) {
                    return result + retStmt->getValue()->toString();
                }
            }
        } else {
            result += " ";
        }

        if (body) {
            result += body->toString();
        }

        return result;
    }

    const std::optional<std::string>& getName() const { return name; }
    const std::vector<Parameter>& getParameters() const { return parameters; }
    const Type* getReturnType() const { return returnType.get(); }
    const BlockStatement* getBody() const { return body.get(); }
    BlockStatement* getBodyMutable() { return body.get(); }
    bool getIsAsync() const { return isAsync; }
    bool getIsGenerator() const { return isGenerator; }
    Kind getKind() const { return kind; }
    const std::vector<std::unique_ptr<GenericTypeParameter>>&
    getTypeParameters() const {
        return typeParameters;
    }
};

}  // namespace AST
}  // namespace tsx