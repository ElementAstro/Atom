// parser.cpp
#include "parser.h"
#include <iostream>
#include <stdexcept>

namespace tsx {

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens) {
    // Register prefix parsers
    registerPrefix(Token::Type::Identifier,
                   [this] { return parseIdentifier(); });
    registerPrefix(Token::Type::NumberLiteral,
                   [this] { return parseNumericLiteral(); });
    registerPrefix(Token::Type::StringLiteral,
                   [this] { return parseStringLiteral(); });
    registerPrefix(Token::Type::True, [this] { return parseBooleanLiteral(); });
    registerPrefix(Token::Type::False,
                   [this] { return parseBooleanLiteral(); });
    registerPrefix(Token::Type::Null, [this] { return parseNullLiteral(); });
    registerPrefix(Token::Type::Undefined,
                   [this] { return parseUndefinedLiteral(); });
    registerPrefix(Token::Type::LeftParen,
                   [this] { return parseParenthesizedExpression(); });
    registerPrefix(Token::Type::LeftBracket,
                   [this] { return parseArrayLiteral(); });
    registerPrefix(Token::Type::LeftBrace,
                   [this] { return parseObjectLiteral(); });
    registerPrefix(Token::Type::Plus,
                   [this] { return parseUnaryExpression(); });
    registerPrefix(Token::Type::Minus,
                   [this] { return parseUnaryExpression(); });
    registerPrefix(Token::Type::ExclamationMark,
                   [this] { return parseUnaryExpression(); });
    registerPrefix(Token::Type::Tilde,
                   [this] { return parseUnaryExpression(); });
    registerPrefix(Token::Type::PlusPLus,
                   [this] { return parseUnaryExpression(); });
    registerPrefix(Token::Type::MinusMinus,
                   [this] { return parseUnaryExpression(); });

    // Register infix parsers with precedences
    registerInfix(
        Token::Type::Plus,
        [this](auto left) { return parseBinaryExpression(std::move(left)); },
        Precedence::Additive);
    registerInfix(
        Token::Type::Minus,
        [this](auto left) { return parseBinaryExpression(std::move(left)); },
        Precedence::Additive);
    registerInfix(
        Token::Type::Asterisk,
        [this](auto left) { return parseBinaryExpression(std::move(left)); },
        Precedence::Multiplicative);
    registerInfix(
        Token::Type::Slash,
        [this](auto left) { return parseBinaryExpression(std::move(left)); },
        Precedence::Multiplicative);
    registerInfix(
        Token::Type::Percent,
        [this](auto left) { return parseBinaryExpression(std::move(left)); },
        Precedence::Multiplicative);
    registerInfix(
        Token::Type::EqualEqual,
        [this](auto left) { return parseBinaryExpression(std::move(left)); },
        Precedence::Equality);
    registerInfix(
        Token::Type::ExclamationEqual,
        [this](auto left) { return parseBinaryExpression(std::move(left)); },
        Precedence::Equality);
    registerInfix(
        Token::Type::EqualEqualEqual,
        [this](auto left) { return parseBinaryExpression(std::move(left)); },
        Precedence::Equality);
    registerInfix(
        Token::Type::ExclamationEqualEqual,
        [this](auto left) { return parseBinaryExpression(std::move(left)); },
        Precedence::Equality);
    registerInfix(
        Token::Type::LessThan,
        [this](auto left) { return parseBinaryExpression(std::move(left)); },
        Precedence::Relational);
    registerInfix(
        Token::Type::GreaterThan,
        [this](auto left) { return parseBinaryExpression(std::move(left)); },
        Precedence::Relational);
    registerInfix(
        Token::Type::LessThanEqual,
        [this](auto left) { return parseBinaryExpression(std::move(left)); },
        Precedence::Relational);
    registerInfix(
        Token::Type::GreaterThanEqual,
        [this](auto left) { return parseBinaryExpression(std::move(left)); },
        Precedence::Relational);
    registerInfix(
        Token::Type::AmpersandAmpersand,
        [this](auto left) { return parseBinaryExpression(std::move(left)); },
        Precedence::LogicalAnd);
    registerInfix(
        Token::Type::PipePipe,
        [this](auto left) { return parseBinaryExpression(std::move(left)); },
        Precedence::LogicalOr);
    registerInfix(
        Token::Type::LeftParen,
        [this](auto left) { return parseCallExpression(std::move(left)); },
        Precedence::Call);
    registerInfix(
        Token::Type::Dot,
        [this](auto left) { return parseMemberExpression(std::move(left)); },
        Precedence::Call);
    registerInfix(
        Token::Type::LeftBracket,
        [this](auto left) { return parseMemberExpression(std::move(left)); },
        Precedence::Call);
    registerInfix(
        Token::Type::QuestionMark,
        [this](auto left) {
            // Conditional/ternary expression
            auto condition = std::move(left);
            auto consequent = parseExpression();
            consume(Token::Type::Colon,
                    "Expected ':' in conditional expression");
            auto alternate = parseExpression(Precedence::Conditional);

            auto expr = std::make_unique<AST::ConditionalExpression>(
                std::move(condition), std::move(consequent),
                std::move(alternate));
            return expr;
        },
        Precedence::Conditional);
}

std::unique_ptr<AST::Program> Parser::parse() {
    auto program = std::make_unique<AST::Program>();

    try {
        while (!isAtEnd()) {
            try {
                program->addStatement(parseStatement());
            } catch (const std::runtime_error& e) {
                std::cerr << "Error: " << e.what() << std::endl;
                synchronize();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal error during parsing: " << e.what() << std::endl;
    }

    return program;
}

void Parser::registerPrefix(Token::Type type, PrefixParseFn fn) {
    prefixParseFns[type] = std::move(fn);
}

void Parser::registerInfix(Token::Type type, InfixParseFn fn,
                           Precedence precedence) {
    infixParseFns[type] = std::move(fn);
    precedences[type] = precedence;
}

bool Parser::match(Token::Type type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(Token::Type type) const {
    if (isAtEnd())
        return false;
    return peek().type == type;
}

Token Parser::advance() {
    if (!isAtEnd())
        current++;
    return previous();
}

Token Parser::peek() const { return tokens[current]; }

Token Parser::previous() const { return tokens[current - 1]; }

bool Parser::isAtEnd() const { return peek().type == Token::Type::EOF; }

Token Parser::consume(Token::Type type, const std::string& message) {
    if (check(type))
        return advance();

    throw std::runtime_error(message + " (found " + std::string(peek().lexeme) +
                             " at line " + std::to_string(peek().line) +
                             ", column " + std::to_string(peek().column) + ")");
}

void Parser::synchronize() {
    advance();

    while (!isAtEnd()) {
        if (previous().type == Token::Type::Semicolon)
            return;

        switch (peek().type) {
            case Token::Type::Class:
            case Token::Type::Function:
            case Token::Type::Let:
            case Token::Type::Const:
            case Token::Type::Var:
            case Token::Type::For:
            case Token::Type::If:
            case Token::Type::While:
            case Token::Type::Return:
                return;
            default:
                break;
        }

        advance();
    }
}

Parser::Precedence Parser::getCurrentPrecedence() const {
    if (isAtEnd())
        return Precedence::None;

    auto it = precedences.find(peek().type);
    if (it != precedences.end()) {
        return it->second;
    }

    return Precedence::None;
}

std::unique_ptr<AST::Expression> Parser::parseExpression(
    Precedence precedence) {
    auto prefixFn = prefixParseFns.find(peek().type);
    if (prefixFn == prefixParseFns.end()) {
        throw std::runtime_error("Expected expression, got " +
                                 std::string(peek().lexeme));
    }

    auto leftExp = prefixFn->second();

    while (!isAtEnd() && precedence < getCurrentPrecedence()) {
        auto infixFn = infixParseFns.find(peek().type);
        if (infixFn == infixParseFns.end())
            break;

        leftExp = infixFn->second(std::move(leftExp));
    }

    return leftExp;
}

std::unique_ptr<AST::Expression> Parser::parseIdentifier() {
    auto token = advance();
    return std::make_unique<AST::IdentifierExpression>(
        std::string(token.lexeme));
}

std::unique_ptr<AST::Expression> Parser::parseNumericLiteral() {
    auto token = advance();
    auto expr = std::make_unique<AST::LiteralExpression>(
        AST::LiteralExpression::LiteralKind::Number,
        token.numberValue.value_or(0.0));
    return expr;
}

std::unique_ptr<AST::Expression> Parser::parseStringLiteral() {
    auto token = advance();
    auto expr = std::make_unique<AST::LiteralExpression>(
        AST::LiteralExpression::LiteralKind::String,
        token.stringValue.value_or(""));
    return expr;
}

std::unique_ptr<AST::Expression> Parser::parseBooleanLiteral() {
    bool value = peek().type == Token::Type::True;
    advance();
    auto expr = std::make_unique<AST::LiteralExpression>(
        AST::LiteralExpression::LiteralKind::Boolean, value);
    return expr;
}

std::unique_ptr<AST::Expression> Parser::parseNullLiteral() {
    advance();
    auto expr = std::make_unique<AST::LiteralExpression>(
        AST::LiteralExpression::LiteralKind::Null, nullptr);
    return expr;
}

std::unique_ptr<AST::Expression> Parser::parseUndefinedLiteral() {
    advance();
    auto expr = std::make_unique<AST::LiteralExpression>(
        AST::LiteralExpression::LiteralKind::Undefined, std::monostate{});
    return expr;
}

std::unique_ptr<AST::Expression> Parser::parseParenthesizedExpression() {
    advance();  // Consume (
    auto expr = parseExpression();
    consume(Token::Type::RightParen, "Expected ')' after expression");
    return expr;
}

std::unique_ptr<AST::Expression> Parser::parseArrayLiteral() {
    advance();  // Consume [

    std::vector<std::unique_ptr<AST::Expression>> elements;

    if (!check(Token::Type::RightBracket)) {
        do {
            if (check(Token::Type::RightBracket))
                break;
            elements.push_back(parseExpression());
        } while (match(Token::Type::Comma));
    }

    consume(Token::Type::RightBracket, "Expected ']' after array elements");

    auto expr =
        std::make_unique<AST::ArrayLiteralExpression>(std::move(elements));
    return expr;
}

std::unique_ptr<AST::Expression> Parser::parseObjectLiteral() {
    advance();  // Consume {

    std::vector<AST::ObjectLiteralExpression::Property> properties;

    if (!check(Token::Type::RightBrace)) {
        do {
            if (check(Token::Type::RightBrace))
                break;

            // Parse key
            std::string key;
            if (check(Token::Type::Identifier)) {
                key = std::string(advance().lexeme);
            } else if (check(Token::Type::StringLiteral)) {
                key = advance().stringValue.value_or("");
            } else {
                throw std::runtime_error("Expected property name");
            }

            // Parse shorthand or full property
            std::unique_ptr<AST::Expression> value;
            if (match(Token::Type::Colon)) {
                value = parseExpression();
            } else {
                // Shorthand property: { name } -> { name: name }
                value = std::make_unique<AST::IdentifierExpression>(key);
            }

            properties.push_back({key, std::move(value)});
        } while (match(Token::Type::Comma));
    }

    consume(Token::Type::RightBrace, "Expected '}' after object properties");

    auto expr =
        std::make_unique<AST::ObjectLiteralExpression>(std::move(properties));
    return expr;
}

std::unique_ptr<AST::Expression> Parser::parseUnaryExpression() {
    AST::UnaryExpression::Operator op;

    switch (peek().type) {
        case Token::Type::Plus:
            op = AST::UnaryExpression::Operator::Plus;
            break;
        case Token::Type::Minus:
            op = AST::UnaryExpression::Operator::Minus;
            break;
        case Token::Type::ExclamationMark:
            op = AST::UnaryExpression::Operator::Not;
            break;
        case Token::Type::Tilde:
            op = AST::UnaryExpression::Operator::BitwiseNot;
            break;
        case Token::Type::PlusPLus:
            op = AST::UnaryExpression::Operator::Increment;
            break;
        case Token::Type::MinusMinus:
            op = AST::UnaryExpression::Operator::Decrement;
            break;
        default:
            throw std::runtime_error("Unexpected unary operator");
    }

    advance();
    auto operand = parseExpression(Precedence::Unary);

    return std::make_unique<AST::UnaryExpression>(op, std::move(operand));
}

std::unique_ptr<AST::Expression> Parser::parseBinaryExpression(
    std::unique_ptr<AST::Expression> left) {
    AST::BinaryExpression::Operator op;

    switch (peek().type) {
        case Token::Type::Plus:
            op = AST::BinaryExpression::Operator::Add;
            break;
        case Token::Type::Minus:
            op = AST::BinaryExpression::Operator::Subtract;
            break;
        case Token::Type::Asterisk:
            op = AST::BinaryExpression::Operator::Multiply;
            break;
        case Token::Type::Slash:
            op = AST::BinaryExpression::Operator::Divide;
            break;
        case Token::Type::Percent:
            op = AST::BinaryExpression::Operator::Modulo;
            break;
        case Token::Type::EqualEqual:
        case Token::Type::EqualEqualEqual:
            op = AST::BinaryExpression::Operator::Equal;
            break;
        case Token::Type::ExclamationEqual:
        case Token::Type::ExclamationEqualEqual:
            op = AST::BinaryExpression::Operator::NotEqual;
            break;
        case Token::Type::LessThan:
            op = AST::BinaryExpression::Operator::Less;
            break;
        case Token::Type::GreaterThan:
            op = AST::BinaryExpression::Operator::Greater;
            break;
        case Token::Type::LessThanEqual:
            op = AST::BinaryExpression::Operator::LessEqual;
            break;
        case Token::Type::GreaterThanEqual:
            op = AST::BinaryExpression::Operator::GreaterEqual;
            break;
        case Token::Type::AmpersandAmpersand:
            op = AST::BinaryExpression::Operator::And;
            break;
        case Token::Type::PipePipe:
            op = AST::BinaryExpression::Operator::Or;
            break;
        default:
            throw std::runtime_error("Unexpected binary operator");
    }

    Token opToken = advance();
    Precedence precedence = precedences[opToken.type];

    auto right = parseExpression(
        static_cast<Precedence>(static_cast<int>(precedence) + 1));

    return std::make_unique<AST::BinaryExpression>(op, std::move(left),
                                                   std::move(right));
}

std::unique_ptr<AST::Expression> Parser::parseCallExpression(
    std::unique_ptr<AST::Expression> callee) {
    advance();  // Consume (

    std::vector<std::unique_ptr<AST::Expression>> args;

    if (!check(Token::Type::RightParen)) {
        do {
            args.push_back(parseExpression());
        } while (match(Token::Type::Comma));
    }

    consume(Token::Type::RightParen, "Expected ')' after function arguments");

    return std::make_unique<AST::CallExpression>(std::move(callee),
                                                 std::move(args));
}

std::unique_ptr<AST::Expression> Parser::parseMemberExpression(
    std::unique_ptr<AST::Expression> object) {
    if (match(Token::Type::Dot)) {
        // Property access: obj.prop
        auto property = consume(Token::Type::Identifier,
                                "Expected property name after '.'");
        return std::make_unique<AST::MemberExpression>(
            std::move(object),
            std::make_unique<AST::IdentifierExpression>(
                std::string(property.lexeme)),
            false  // Not computed
        );
    } else if (previous().type == Token::Type::LeftBracket) {
        // Computed property access: obj[expr]
        auto property = parseExpression();
        consume(Token::Type::RightBracket,
                "Expected ']' after computed property");
        return std::make_unique<AST::MemberExpression>(std::move(object),
                                                       std::move(property),
                                                       true  // Computed
        );
    } else {
        throw std::runtime_error("Unexpected token in member expression");
    }
}

std::unique_ptr<AST::Statement> Parser::parseStatement() {
    if (match(Token::Type::Let) || match(Token::Type::Const) ||
        match(Token::Type::Var)) {
        return parseVariableDeclaration();
    } else if (match(Token::Type::Function)) {
        return parseFunctionDeclaration();
    } else if (match(Token::Type::Class)) {
        return parseClassDeclaration();
    } else if (match(Token::Type::Interface)) {
        return parseInterfaceDeclaration();
    } else if (match(Token::Type::If)) {
        return parseIfStatement();
    } else if (match(Token::Type::LeftBrace)) {
        return parseBlockStatement();
    } else {
        return parseExpressionStatement();
    }
}

std::unique_ptr<AST::ExpressionStatement> Parser::parseExpressionStatement() {
    auto expr = parseExpression();
    consume(Token::Type::Semicolon, "Expected ';' after expression");
    return std::make_unique<AST::ExpressionStatement>(std::move(expr));
}

std::unique_ptr<AST::BlockStatement> Parser::parseBlockStatement() {
    auto block = std::make_unique<AST::BlockStatement>();

    while (!check(Token::Type::RightBrace) && !isAtEnd()) {
        block->addStatement(parseStatement());
    }

    consume(Token::Type::RightBrace, "Expected '}' after block");
    return block;
}

std::unique_ptr<AST::VariableDeclaration> Parser::parseVariableDeclaration() {
    AST::VariableDeclaration::Kind kind;

    if (previous().type == Token::Type::Let) {
        kind = AST::VariableDeclaration::Kind::Let;
    } else if (previous().type == Token::Type::Const) {
        kind = AST::VariableDeclaration::Kind::Const;
    } else {
        kind = AST::VariableDeclaration::Kind::Var;
    }

    auto declaration = std::make_unique<AST::VariableDeclaration>(kind);

    do {
        std::string name = std::string(
            consume(Token::Type::Identifier, "Expected variable name").lexeme);

        // Type annotation
        std::unique_ptr<Type> typeAnnotation;
        if (match(Token::Type::Colon)) {
            typeAnnotation = parseType();
        }

        // Initializer
        std::unique_ptr<AST::Expression> initializer;
        if (match(Token::Type::Equal)) {
            initializer = parseExpression();
        }

        declaration->addDeclarator(std::move(name), std::move(initializer),
                                   std::move(typeAnnotation));
    } while (match(Token::Type::Comma));

    consume(Token::Type::Semicolon, "Expected ';' after variable declaration");
    return declaration;
}

std::unique_ptr<AST::IfStatement> Parser::parseIfStatement() {
    consume(Token::Type::LeftParen, "Expected '(' after 'if'");
    auto condition = parseExpression();
    consume(Token::Type::RightParen, "Expected ')' after if condition");

    auto thenBranch = parseStatement();
    std::unique_ptr<AST::Statement> elseBranch;

    if (match(Token::Type::Else)) {
        elseBranch = parseStatement();
    }

    return std::make_unique<AST::IfStatement>(
        std::move(condition), std::move(thenBranch), std::move(elseBranch));
}

std::unique_ptr<AST::FunctionDeclaration> Parser::parseFunctionDeclaration() {
    bool isAsync = false;
    bool isGenerator = false;

    if (previous().type == Token::Type::Async) {
        isAsync = true;
        consume(Token::Type::Function, "Expected 'function' after 'async'");
    }

    if (match(Token::Type::Asterisk)) {
        isGenerator = true;
    }

    std::string name = std::string(
        consume(Token::Type::Identifier, "Expected function name").lexeme);
    auto function = std::make_unique<AST::FunctionDeclaration>(std::move(name));

    function->setIsAsync(isAsync);
    function->setIsGenerator(isGenerator);

    // Type parameters
    if (match(Token::Type::LessThan)) {
        do {
            std::string paramName = std::string(
                consume(Token::Type::Identifier, "Expected type parameter name")
                    .lexeme);

            std::unique_ptr<Type> constraint;
            if (match(Token::Type::Extends)) {
                constraint = parseType();
            }

            function->addTypeParameter(std::make_unique<GenericTypeParameter>(
                std::move(paramName), std::move(constraint)));
        } while (match(Token::Type::Comma));

        consume(Token::Type::GreaterThan, "Expected '>' after type parameters");
    }

    consume(Token::Type::LeftParen, "Expected '(' after function name");

    // Parameters
    if (!check(Token::Type::RightParen)) {
        do {
            bool isRest = match(Token::Type::Dot) && match(Token::Type::Dot) &&
                          match(Token::Type::Dot);

            std::string paramName = std::string(
                consume(Token::Type::Identifier, "Expected parameter name")
                    .lexeme);

            std::unique_ptr<Type> paramType;
            if (match(Token::Type::Colon)) {
                paramType = parseType();
            }

            std::unique_ptr<AST::Expression> defaultValue;
            if (match(Token::Type::Equal)) {
                defaultValue = parseExpression();
            }

            function->addParameter(std::move(paramName), std::move(paramType),
                                   std::move(defaultValue), isRest);
        } while (match(Token::Type::Comma));
    }

    consume(Token::Type::RightParen, "Expected ')' after parameters");

    // Return type
    if (match(Token::Type::Colon)) {
        function->setReturnType(parseType());
    }

    consume(Token::Type::LeftBrace, "Expected '{' before function body");
    function->setBody(parseBlockStatement());

    return function;
}

std::unique_ptr<AST::ClassDeclaration> Parser::parseClassDeclaration() {
    std::string name = std::string(
        consume(Token::Type::Identifier, "Expected class name").lexeme);
    auto classDecl = std::make_unique<AST::ClassDeclaration>(std::move(name));

    // Type parameters
    if (match(Token::Type::LessThan)) {
        do {
            std::string paramName = std::string(
                consume(Token::Type::Identifier, "Expected type parameter name")
                    .lexeme);

            std::unique_ptr<Type> constraint;
            if (match(Token::Type::Extends)) {
                constraint = parseType();
            }

            classDecl->addTypeParameter(std::make_unique<GenericTypeParameter>(
                std::move(paramName), std::move(constraint)));
        } while (match(Token::Type::Comma));

        consume(Token::Type::GreaterThan, "Expected '>' after type parameters");
    }

    // Inheritance
    if (match(Token::Type::Extends)) {
        classDecl->setBaseClass(std::string(
            consume(Token::Type::Identifier, "Expected base class name")
                .lexeme));
    }

    // Interfaces
    if (match(Token::Type::Implements)) {
        do {
            classDecl->addImplements(std::string(
                consume(Token::Type::Identifier, "Expected interface name")
                    .lexeme));
        } while (match(Token::Type::Comma));
    }

    consume(Token::Type::LeftBrace, "Expected '{' before class body");

    // Class body
    while (!check(Token::Type::RightBrace) && !isAtEnd()) {
        AST::ClassDeclaration::Member member;

        // Visibility
        if (match(Token::Type::Public)) {
            member.visibility = AST::ClassDeclaration::Visibility::Public;
        } else if (match(Token::Type::Private)) {
            member.visibility = AST::ClassDeclaration::Visibility::Private;
        } else if (match(Token::Type::Protected)) {
            member.visibility = AST::ClassDeclaration::Visibility::Protected;
        } else {
            member.visibility =
                AST::ClassDeclaration::Visibility::Public;  // Default
        }

        // Static
        member.isStatic = match(Token::Type::Static);

        // Readonly
        member.isReadonly = match(Token::Type::Readonly);

        // Member kind
        if (check(Token::Type::Constructor)) {
            member.kind = AST::ClassDeclaration::MemberKind::Constructor;
            advance();  // consume 'constructor'

            // Parse constructor parameters and body like a function
            auto methodDecl =
                std::make_unique<AST::FunctionDeclaration>("constructor");

            consume(Token::Type::LeftParen, "Expected '(' after constructor");

            // Parameters
            if (!check(Token::Type::RightParen)) {
                do {
                    std::string paramName =
                        std::string(consume(Token::Type::Identifier,
                                            "Expected parameter name")
                                        .lexeme);

                    std::unique_ptr<Type> paramType;
                    if (match(Token::Type::Colon)) {
                        paramType = parseType();
                    }

                    std::unique_ptr<AST::Expression> defaultValue;
                    if (match(Token::Type::Equal)) {
                        defaultValue = parseExpression();
                    }

                    methodDecl->addParameter(std::move(paramName),
                                             std::move(paramType),
                                             std::move(defaultValue));
                } while (match(Token::Type::Comma));
            }

            consume(Token::Type::RightParen,
                    "Expected ')' after constructor parameters");

            consume(Token::Type::LeftBrace,
                    "Expected '{' before constructor body");
            methodDecl->setBody(parseBlockStatement());

            member.methodDecl = std::move(methodDecl);
        } else if (match(Token::Type::Get)) {
            member.kind = AST::ClassDeclaration::MemberKind::GetAccessor;
            member.name = std::string(
                consume(Token::Type::Identifier, "Expected accessor name")
                    .lexeme);

            auto methodDecl =
                std::make_unique<AST::FunctionDeclaration>(member.name);

            consume(Token::Type::LeftParen, "Expected '(' after getter name");
            consume(Token::Type::RightParen,
                    "Expected ')' after getter parameters");

            // Return type
            if (match(Token::Type::Colon)) {
                methodDecl->setReturnType(parseType());
            }

            consume(Token::Type::LeftBrace, "Expected '{' before getter body");
            methodDecl->setBody(parseBlockStatement());

            member.methodDecl = std::move(methodDecl);
        } else if (match(Token::Type::Set)) {
            member.kind = AST::ClassDeclaration::MemberKind::SetAccessor;
            member.name = std::string(
                consume(Token::Type::Identifier, "Expected accessor name")
                    .lexeme);

            auto methodDecl =
                std::make_unique<AST::FunctionDeclaration>(member.name);

            consume(Token::Type::LeftParen, "Expected '(' after setter name");
            std::string paramName = std::string(
                consume(Token::Type::Identifier, "Expected parameter name")
                    .lexeme);

            std::unique_ptr<Type> paramType;
            if (match(Token::Type::Colon)) {
                paramType = parseType();
            }

            methodDecl->addParameter(std::move(paramName),
                                     std::move(paramType));

            consume(Token::Type::RightParen,
                    "Expected ')' after setter parameter");

            consume(Token::Type::LeftBrace, "Expected '{' before setter body");
            methodDecl->setBody(parseBlockStatement());

            member.methodDecl = std::move(methodDecl);
        } else if (check(Token::Type::Identifier)) {
            member.name = std::string(advance().lexeme);

            if (match(Token::Type::LeftParen)) {
                // Method
                member.kind = AST::ClassDeclaration::MemberKind::Method;

                auto methodDecl =
                    std::make_unique<AST::FunctionDeclaration>(member.name);

                // Parameters
                if (!check(Token::Type::RightParen)) {
                    do {
                        std::string paramName =
                            std::string(consume(Token::Type::Identifier,
                                                "Expected parameter name")
                                            .lexeme);

                        std::unique_ptr<Type> paramType;
                        if (match(Token::Type::Colon)) {
                            paramType = parseType();
                        }

                        std::unique_ptr<AST::Expression> defaultValue;
                        if (match(Token::Type::Equal)) {
                            defaultValue = parseExpression();
                        }

                        methodDecl->addParameter(std::move(paramName),
                                                 std::move(paramType),
                                                 std::move(defaultValue));
                    } while (match(Token::Type::Comma));
                }

                consume(Token::Type::RightParen,
                        "Expected ')' after method parameters");

                // Return type
                if (match(Token::Type::Colon)) {
                    methodDecl->setReturnType(parseType());
                }

                consume(Token::Type::LeftBrace,
                        "Expected '{' before method body");
                methodDecl->setBody(parseBlockStatement());

                member.methodDecl = std::move(methodDecl);
            } else {
                // Property
                member.kind = AST::ClassDeclaration::MemberKind::Property;

                // Type annotation
                if (match(Token::Type::Colon)) {
                    member.propertyType = parseType();
                }

                // Initializer
                if (match(Token::Type::Equal)) {
                    member.initializer = parseExpression();
                }

                consume(Token::Type::Semicolon, "Expected ';' after property");
            }
        } else {
            throw std::runtime_error("Expected class member");
        }

        classDecl->addMember(std::move(member));
    }

    consume(Token::Type::RightBrace, "Expected '}' after class body");

    return classDecl;
}

std::unique_ptr<AST::InterfaceDeclaration> Parser::parseInterfaceDeclaration() {
    std::string name = std::string(
        consume(Token::Type::Identifier, "Expected interface name").lexeme);
    auto interfaceDecl =
        std::make_unique<AST::InterfaceDeclaration>(std::move(name));

    // Type parameters
    if (match(Token::Type::LessThan)) {
        do {
            std::string paramName = std::string(
                consume(Token::Type::Identifier, "Expected type parameter name")
                    .lexeme);

            std::unique_ptr<Type> constraint;
            if (match(Token::Type::Extends)) {
                constraint = parseType();
            }

            interfaceDecl->addTypeParameter(
                std::make_unique<GenericTypeParameter>(std::move(paramName),
                                                       std::move(constraint)));
        } while (match(Token::Type::Comma));

        consume(Token::Type::GreaterThan, "Expected '>' after type parameters");
    }

    // Extends
    if (match(Token::Type::Extends)) {
        do {
            interfaceDecl->addExtends(std::string(
                consume(Token::Type::Identifier, "Expected interface name")
                    .lexeme));
        } while (match(Token::Type::Comma));
    }

    consume(Token::Type::LeftBrace, "Expected '{' before interface body");

    // Interface body
    while (!check(Token::Type::RightBrace) && !isAtEnd()) {
        bool readonly = match(Token::Type::Readonly);
        std::string memberName = std::string(
            consume(Token::Type::Identifier, "Expected member name").lexeme);
        bool optional = match(Token::Type::QuestionMark);

        if (match(Token::Type::LeftParen)) {
            // Method signature
            AST::InterfaceDeclaration::Method method;
            method.name = memberName;
            method.optional = optional;

            // Parameters
            if (!check(Token::Type::RightParen)) {
                do {
                    AST::FunctionDeclaration::Parameter param;

                    param.isRest = match(Token::Type::Dot) &&
                                   match(Token::Type::Dot) &&
                                   match(Token::Type::Dot);
                    param.name = std::string(consume(Token::Type::Identifier,
                                                     "Expected parameter name")
                                                 .lexeme);

                    if (match(Token::Type::QuestionMark)) {
                        // Parameter is optional
                    }

                    if (match(Token::Type::Colon)) {
                        param.typeAnnotation = parseType();
                    }

                    method.parameters.push_back(std::move(param));
                } while (match(Token::Type::Comma));
            }

            consume(Token::Type::RightParen,
                    "Expected ')' after method parameters");

            // Return type
            if (match(Token::Type::Colon)) {
                method.returnType = parseType();
            }

            consume(Token::Type::Semicolon,
                    "Expected ';' after method signature");

            interfaceDecl->addMethod(std::move(method));
        } else {
            // Property signature
            AST::InterfaceDeclaration::Property property;
            property.name = memberName;
            property.optional = optional;
            property.readonly = readonly;

            consume(Token::Type::Colon, "Expected ':' after property name");
            property.type = parseType();

            consume(Token::Type::Semicolon, "Expected ';' after property type");

            interfaceDecl->addProperty(std::move(property));
        }
    }

    consume(Token::Type::RightBrace, "Expected '}' after interface body");

    return interfaceDecl;
}

std::unique_ptr<Type> Parser::parseType() {
    if (match(Token::Type::Number)) {
        return Type::createNumber();
    } else if (match(Token::Type::String)) {
        return Type::createString();
    } else if (match(Token::Type::Boolean)) {
        return Type::createBoolean();
    } else if (match(Token::Type::Null)) {
        return Type::createNull();
    } else if (match(Token::Type::Undefined)) {
        return Type::createUndefined();
    } else if (match(Token::Type::Any)) {
        return Type::createAny();
    } else if (match(Token::Type::Identifier)) {
        // Named type (could be a type parameter or a user-defined type)
        std::string name = std::string(previous().lexeme);

        // Handle generics like Array<T>
        if (match(Token::Type::LessThan)) {
            std::vector<std::unique_ptr<Type>> typeArgs;

            do {
                typeArgs.push_back(parseType());
            } while (match(Token::Type::Comma));

            consume(Token::Type::GreaterThan,
                    "Expected '>' after type arguments");

            // Special case for arrays
            if (name == "Array" && typeArgs.size() == 1) {
                return std::make_unique<ArrayType>(std::move(typeArgs[0]));
            }

            // For other generic types, we'd need a more sophisticated system
            // This is a simplified implementation
            return std::make_unique<GenericTypeParameter>(std::move(name));
        }

        // Simple named type
        return std::make_unique<GenericTypeParameter>(std::move(name));
    } else if (match(Token::Type::LeftBracket)) {
        // Tuple type: [T, U, ...]
        std::vector<std::unique_ptr<Type>> elementTypes;

        if (!check(Token::Type::RightBracket)) {
            do {
                elementTypes.push_back(parseType());
            } while (match(Token::Type::Comma));
        }

        consume(Token::Type::RightBracket, "Expected ']' after tuple types");

        // This is simplified - a proper implementation would have a TupleType
        // class
        return std::make_unique<ArrayType>(Type::createAny());
    } else if (match(Token::Type::LeftBrace)) {
        // Object type: { x: number, y: string }
        auto objType = std::make_unique<ObjectType>();

        if (!check(Token::Type::RightBrace)) {
            do {
                std::string propName = std::string(
                    consume(Token::Type::Identifier, "Expected property name")
                        .lexeme);
                bool optional = match(Token::Type::QuestionMark);

                consume(Token::Type::Colon, "Expected ':' after property name");

                auto propType = parseType();
                objType->addProperty(propName, std::move(propType));
            } while (match(Token::Type::Comma) ||
                     match(Token::Type::Semicolon));
        }

        consume(Token::Type::RightBrace, "Expected '}' after object type");

        return objType;
    } else if (match(Token::Type::LeftParen)) {
        // Function type: (x: number, y: string) => boolean
        std::vector<std::unique_ptr<Type>> paramTypes;

        if (!check(Token::Type::RightParen)) {
            do {
                // Optional parameter name (ignored in type checking)
                if (check(Token::Type::Identifier) &&
                    peekNext().type == Token::Type::Colon) {
                    advance();  // Skip parameter name
                    consume(Token::Type::Colon,
                            "Expected ':' after parameter name");
                }

                paramTypes.push_back(parseType());
            } while (match(Token::Type::Comma));
        }

        consume(Token::Type::RightParen,
                "Expected ')' after function parameters");

        consume(Token::Type::Arrow, "Expected '=>' after function parameters");

        auto returnType = parseType();

        return std::make_unique<FunctionType>(std::move(paramTypes),
                                              std::move(returnType));
    } else {
        throw std::runtime_error("Expected type");
    }
}

Token Parser::peekNext() const {
    if (current + 1 >= tokens.size())
        return Token(Token::Type::EOF, "", 0, 0);
    return tokens[current + 1];
}

}  // namespace tsx