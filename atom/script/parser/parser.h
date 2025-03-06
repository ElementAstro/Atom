// parser.h
#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include "../ast/ast.h"
#include "../lexer/lexer.h"
#include "../types/types.h"

namespace tsx {

class Parser {
private:
    std::vector<Token> tokens;
    size_t current = 0;

    // Precedence levels for expressions
    enum class Precedence {
        None,
        Assignment,      // =, +=, -=, etc.
        Conditional,     // ?:
        LogicalOr,       // ||
        LogicalAnd,      // &&
        BitwiseOr,       // |
        BitwiseXor,      // ^
        BitwiseAnd,      // &
        Equality,        // ==, !=, ===, !==
        Relational,      // <, >, <=, >=
        Shift,           // <<, >>, >>>
        Additive,        // +, -
        Multiplicative,  // *, /, %
        Unary,           // !, ~, +, -, ++, -- (prefix)
        Postfix,         // ++, -- (postfix)
        Call,            // (), [], .
        Primary
    };

    // Function type for parsing prefix expressions
    using PrefixParseFn = std::function<std::unique_ptr<AST::Expression>()>;

    // Function type for parsing infix expressions
    using InfixParseFn = std::function<std::unique_ptr<AST::Expression>(
        std::unique_ptr<AST::Expression>)>;

    // Maps for storing parsing functions
    std::unordered_map<Token::Type, PrefixParseFn> prefixParseFns;
    std::unordered_map<Token::Type, InfixParseFn> infixParseFns;
    std::unordered_map<Token::Type, Precedence> precedences;

public:
    explicit Parser(const std::vector<Token>& tokens);

    std::unique_ptr<AST::Program> parse();

private:
    // Register parsing functions
    void registerPrefix(Token::Type type, PrefixParseFn fn);
    void registerInfix(Token::Type type, InfixParseFn fn,
                       Precedence precedence);

    // Helper methods
    bool match(Token::Type type);
    bool check(Token::Type type) const;
    Token advance();
    Token peek() const;
    Token peekNext() const;  // 添加了peekNext方法的声明
    Token previous() const;
    bool isAtEnd() const;
    Token consume(Token::Type type, const std::string& message);
    void synchronize();

    // Get precedence of current token
    Precedence getCurrentPrecedence() const;

    // Parse expressions
    std::unique_ptr<AST::Expression> parseExpression(
        Precedence precedence = Precedence::Assignment);
    std::unique_ptr<AST::Expression> parsePrefixExpression();
    std::unique_ptr<AST::Expression> parseInfixExpression(
        std::unique_ptr<AST::Expression> left);

    // Expression parsing functions
    std::unique_ptr<AST::Expression> parseIdentifier();
    std::unique_ptr<AST::Expression> parseNumericLiteral();
    std::unique_ptr<AST::Expression> parseStringLiteral();
    std::unique_ptr<AST::Expression> parseBooleanLiteral();
    std::unique_ptr<AST::Expression> parseNullLiteral();
    std::unique_ptr<AST::Expression> parseUndefinedLiteral();
    std::unique_ptr<AST::Expression> parseParenthesizedExpression();
    std::unique_ptr<AST::Expression> parseArrayLiteral();
    std::unique_ptr<AST::Expression> parseObjectLiteral();
    std::unique_ptr<AST::Expression> parseUnaryExpression();
    std::unique_ptr<AST::Expression> parseBinaryExpression(
        std::unique_ptr<AST::Expression> left);
    std::unique_ptr<AST::Expression> parseCallExpression(
        std::unique_ptr<AST::Expression> callee);
    std::unique_ptr<AST::Expression> parseMemberExpression(
        std::unique_ptr<AST::Expression> object);

    // Statement parsing
    std::unique_ptr<AST::Statement> parseStatement();
    std::unique_ptr<AST::ExpressionStatement> parseExpressionStatement();
    std::unique_ptr<AST::BlockStatement> parseBlockStatement();
    std::unique_ptr<AST::VariableDeclaration> parseVariableDeclaration();
    std::unique_ptr<AST::IfStatement> parseIfStatement();
    std::unique_ptr<AST::FunctionDeclaration> parseFunctionDeclaration();
    std::unique_ptr<AST::ClassDeclaration> parseClassDeclaration();
    std::unique_ptr<AST::InterfaceDeclaration> parseInterfaceDeclaration();

    // Type parsing
    std::unique_ptr<Type> parseType();
};

}  // namespace tsx