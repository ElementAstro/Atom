// lexer.h
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#undef EOF

namespace tsx {

class Token {
public:
    enum class Type {
        // Keywords
        Let,
        Const,
        Var,
        Function,
        Class,
        Interface,
        If,
        Else,
        For,
        While,
        Do,
        Return,
        Break,
        Continue,
        Switch,
        Case,
        Default,
        Try,
        Catch,
        Finally,
        Throw,
        TypeOf,
        InstanceOf,
        In,
        Of,
        Extends,
        Implements,
        Super,
        This,
        New,
        Delete,
        Void,
        Null,
        Undefined,
        True,
        False,
        Public,
        Private,
        Protected,
        Static,
        Readonly,
        Async,
        Await,
        Yield,
        Import,
        Export,
        From,
        As,
        Type,
        Enum,
        Namespace,
        Constructor,  // 添加 Constructor 关键字
        Get,          // 添加 Get 关键字
        Set,          // 添加 Set 关键字

        // Type-related keywords
        Number,
        String,
        Boolean,
        Any,
        Unknown,
        Never,
        Object,
        Symbol,
        BigInt,
        Keyof,
        Typeof,

        // Punctuation and operators
        LeftParen,
        RightParen,
        LeftBrace,
        RightBrace,
        LeftBracket,
        RightBracket,
        Semicolon,
        Comma,
        Dot,
        QuestionDot,
        Colon,
        QuestionMark,
        Arrow,
        Plus,
        Minus,
        Asterisk,
        Slash,
        Percent,
        Caret,
        Ampersand,
        Pipe,
        Tilde,
        ExclamationMark,
        Equal,
        PlusEqual,
        MinusEqual,
        AsteriskEqual,
        SlashEqual,
        PercentEqual,
        CaretEqual,
        AmpersandEqual,
        PipeEqual,
        EqualEqual,
        ExclamationEqual,
        EqualEqualEqual,
        ExclamationEqualEqual,
        LessThan,
        GreaterThan,
        LessThanEqual,
        GreaterThanEqual,
        LessThanLessThan,
        GreaterThanGreaterThan,
        GreaterThanGreaterThanGreaterThan,
        LessThanLessThanEqual,
        GreaterThanGreaterThanEqual,
        GreaterThanGreaterThanGreaterThanEqual,
        AmpersandAmpersand,
        PipePipe,
        PlusPLus,
        MinusMinus,

        // Literals and identifiers
        NumberLiteral,
        StringLiteral,
        Identifier,
        TemplateString,

        // Special tokens
        EOF,
        Error
    };

    Token(Type type, std::string_view lexeme, size_t line, size_t column)
        : type(type), lexeme(lexeme), line(line), column(column) {}

    Type type;
    std::string_view lexeme;
    size_t line;
    size_t column;

    // For storing literal values
    std::optional<double> numberValue;
    std::optional<std::string> stringValue;
};

class Lexer {
private:
    std::string_view source;
    size_t start = 0;
    size_t current = 0;
    size_t line = 1;
    size_t column = 1;

    static const std::unordered_map<std::string_view, Token::Type> keywords;

public:
    explicit Lexer(std::string_view source) : source(source) {}

    Token nextToken();
    std::vector<Token> tokenize();

private:
    bool isAtEnd() const;
    char advance();
    bool match(char expected);
    char peek() const;
    char peekNext() const;

    void skipWhitespace();
    Token scanIdentifier();
    Token scanNumber();
    Token scanString();
    Token scanTemplateString();
    Token scanOperator();

    Token makeToken(Token::Type type) const;
    Token errorToken(const std::string& message) const;
};

}  // namespace tsx