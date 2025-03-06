// lexer.cpp
#include "lexer.h"
#include <cctype>
#include <unordered_map>

namespace tsx {

const std::unordered_map<std::string_view, Token::Type> Lexer::keywords = {
    {"let", Token::Type::Let},
    {"const", Token::Type::Const},
    {"var", Token::Type::Var},
    {"function", Token::Type::Function},
    {"class", Token::Type::Class},
    {"interface", Token::Type::Interface},
    {"if", Token::Type::If},
    {"else", Token::Type::Else},
    {"for", Token::Type::For},
    {"while", Token::Type::While},
    {"do", Token::Type::Do},
    {"return", Token::Type::Return},
    {"break", Token::Type::Break},
    {"continue", Token::Type::Continue},
    {"switch", Token::Type::Switch},
    {"case", Token::Type::Case},
    {"default", Token::Type::Default},
    {"try", Token::Type::Try},
    {"catch", Token::Type::Catch},
    {"finally", Token::Type::Finally},
    {"throw", Token::Type::Throw},
    {"typeof", Token::Type::TypeOf},
    {"instanceof", Token::Type::InstanceOf},
    {"in", Token::Type::In},
    {"of", Token::Type::Of},
    {"extends", Token::Type::Extends},
    {"implements", Token::Type::Implements},
    {"super", Token::Type::Super},
    {"this", Token::Type::This},
    {"new", Token::Type::New},
    {"delete", Token::Type::Delete},
    {"void", Token::Type::Void},
    {"null", Token::Type::Null},
    {"undefined", Token::Type::Undefined},
    {"true", Token::Type::True},
    {"false", Token::Type::False},
    {"public", Token::Type::Public},
    {"private", Token::Type::Private},
    {"protected", Token::Type::Protected},
    {"static", Token::Type::Static},
    {"readonly", Token::Type::Readonly},
    {"async", Token::Type::Async},
    {"await", Token::Type::Await},
    {"yield", Token::Type::Yield},
    {"import", Token::Type::Import},
    {"export", Token::Type::Export},
    {"from", Token::Type::From},
    {"as", Token::Type::As},
    {"type", Token::Type::Type},
    {"enum", Token::Type::Enum},
    {"namespace", Token::Type::Namespace},
    {"constructor", Token::Type::Constructor},
    {"get", Token::Type::Get},
    {"set", Token::Type::Set},

    // Type-related keywords
    {"number", Token::Type::Number},
    {"string", Token::Type::String},
    {"boolean", Token::Type::Boolean},
    {"any", Token::Type::Any},
    {"unknown", Token::Type::Unknown},
    {"never", Token::Type::Never},
    {"void", Token::Type::Void},
    {"object", Token::Type::Object},
    {"symbol", Token::Type::Symbol},
    {"bigint", Token::Type::BigInt},
    {"keyof", Token::Type::Keyof},
    {"typeof", Token::Type::Typeof},
};

Token Lexer::nextToken() {
    skipWhitespace();

    start = current;

    if (isAtEnd())
        return makeToken(Token::Type::EOF);

    char c = advance();

    if (std::isalpha(c) || c == '_')
        return scanIdentifier();
    if (std::isdigit(c))
        return scanNumber();

    switch (c) {
        case '(':
            return makeToken(Token::Type::LeftParen);
        case ')':
            return makeToken(Token::Type::RightParen);
        case '{':
            return makeToken(Token::Type::LeftBrace);
        case '}':
            return makeToken(Token::Type::RightBrace);
        case '[':
            return makeToken(Token::Type::LeftBracket);
        case ']':
            return makeToken(Token::Type::RightBracket);
        case ';':
            return makeToken(Token::Type::Semicolon);
        case ',':
            return makeToken(Token::Type::Comma);
        case '.':
            if (match('.')) {
                if (match('.')) {
                    // Handle spread/rest operator
                    // This is a simplified example - actual implementation
                    // would handle this differently
                    return makeToken(Token::Type::Dot);
                }
                return makeToken(Token::Type::Dot);  // double dot (error in TS)
            }
            return makeToken(Token::Type::Dot);

        case '?':
            if (match('.'))
                return makeToken(Token::Type::QuestionDot);
            return makeToken(Token::Type::QuestionMark);

        case ':':
            return makeToken(Token::Type::Colon);

        case '=':
            if (match('=')) {
                if (match('='))
                    return makeToken(Token::Type::EqualEqualEqual);
                return makeToken(Token::Type::EqualEqual);
            } else if (match('>')) {
                return makeToken(Token::Type::Arrow);
            }
            return makeToken(Token::Type::Equal);

        case '!':
            if (match('=')) {
                if (match('='))
                    return makeToken(Token::Type::ExclamationEqualEqual);
                return makeToken(Token::Type::ExclamationEqual);
            }
            return makeToken(Token::Type::ExclamationMark);

        case '<':
            if (match('='))
                return makeToken(Token::Type::LessThanEqual);
            if (match('<')) {
                if (match('='))
                    return makeToken(Token::Type::LessThanLessThanEqual);
                return makeToken(Token::Type::LessThanLessThan);
            }
            return makeToken(Token::Type::LessThan);

        case '>':
            if (match('='))
                return makeToken(Token::Type::GreaterThanEqual);
            if (match('>')) {
                if (match('='))
                    return makeToken(Token::Type::GreaterThanGreaterThanEqual);
                if (match('>')) {
                    if (match('='))
                        return makeToken(
                            Token::Type::
                                GreaterThanGreaterThanGreaterThanEqual);
                    return makeToken(
                        Token::Type::GreaterThanGreaterThanGreaterThan);
                }
                return makeToken(Token::Type::GreaterThanGreaterThan);
            }
            return makeToken(Token::Type::GreaterThan);

        case '+':
            if (match('='))
                return makeToken(Token::Type::PlusEqual);
            if (match('+'))
                return makeToken(Token::Type::PlusPLus);
            return makeToken(Token::Type::Plus);

        case '-':
            if (match('='))
                return makeToken(Token::Type::MinusEqual);
            if (match('-'))
                return makeToken(Token::Type::MinusMinus);
            return makeToken(Token::Type::Minus);

        case '*':
            if (match('='))
                return makeToken(Token::Type::AsteriskEqual);
            return makeToken(Token::Type::Asterisk);

        case '/':
            if (match('/')) {
                // Line comment
                while (peek() != '\n' && !isAtEnd())
                    advance();
                return nextToken();
            }
            if (match('*')) {
                // Block comment
                while (!(peek() == '*' && peekNext() == '/') && !isAtEnd()) {
                    if (peek() == '\n') {
                        line++;
                        column = 0;
                    }
                    advance();
                }

                if (isAtEnd())
                    return errorToken("Unterminated block comment");

                // Consume the closing */
                advance();
                advance();

                return nextToken();
            }
            if (match('='))
                return makeToken(Token::Type::SlashEqual);
            return makeToken(Token::Type::Slash);

        case '%':
            if (match('='))
                return makeToken(Token::Type::PercentEqual);
            return makeToken(Token::Type::Percent);

        case '^':
            if (match('='))
                return makeToken(Token::Type::CaretEqual);
            return makeToken(Token::Type::Caret);

        case '&':
            if (match('&'))
                return makeToken(Token::Type::AmpersandAmpersand);
            if (match('='))
                return makeToken(Token::Type::AmpersandEqual);
            return makeToken(Token::Type::Ampersand);

        case '|':
            if (match('|'))
                return makeToken(Token::Type::PipePipe);
            if (match('='))
                return makeToken(Token::Type::PipeEqual);
            return makeToken(Token::Type::Pipe);

        case '~':
            return makeToken(Token::Type::Tilde);

        case '\'':
        case '"':
            return scanString();

        case '`':
            return scanTemplateString();
    }

    return errorToken("Unexpected character");
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        Token token = nextToken();
        tokens.push_back(token);

        if (token.type == Token::Type::EOF) {
            break;
        }
    }

    return tokens;
}

bool Lexer::isAtEnd() const { return current >= source.length(); }

char Lexer::advance() {
    char c = source[current++];
    column++;
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd())
        return false;
    if (source[current] != expected)
        return false;

    current++;
    column++;
    return true;
}

char Lexer::peek() const {
    if (isAtEnd())
        return '\0';
    return source[current];
}

char Lexer::peekNext() const {
    if (current + 1 >= source.length())
        return '\0';
    return source[current + 1];
}

void Lexer::skipWhitespace() {
    while (true) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                advance();
                break;
            case '\n':
                line++;
                column = 0;
                advance();
                break;
            default:
                return;
        }
    }
}

Token Lexer::scanIdentifier() {
    while (std::isalnum(peek()) || peek() == '_')
        advance();

    std::string_view text = source.substr(start, current - start);
    Token::Type type = Token::Type::Identifier;

    auto it = keywords.find(text);
    if (it != keywords.end()) {
        type = it->second;
    }

    return makeToken(type);
}

Token Lexer::scanNumber() {
    while (std::isdigit(peek()))
        advance();

    // Look for a decimal part
    if (peek() == '.' && std::isdigit(peekNext())) {
        // Consume the "."
        advance();

        while (std::isdigit(peek()))
            advance();
    }

    Token token = makeToken(Token::Type::NumberLiteral);
    token.numberValue =
        std::stod(std::string(source.substr(start, current - start)));
    return token;
}

Token Lexer::scanString() {
    char quote = source[start];  // Either ' or "

    while (peek() != quote && !isAtEnd()) {
        if (peek() == '\n') {
            line++;
            column = 0;
        }

        if (peek() == '\\' && peekNext() == quote) {
            advance();  // Skip the backslash
        }

        advance();
    }

    if (isAtEnd())
        return errorToken("Unterminated string");

    // The closing quote
    advance();

    // Extract the string value without quotes
    std::string_view strValue = source.substr(start + 1, current - start - 2);
    Token token = makeToken(Token::Type::StringLiteral);
    token.stringValue = std::string(strValue);

    return token;
}

Token Lexer::scanTemplateString() {
    // Template strings can span multiple lines and have expressions inside ${}
    while (peek() != '`' && !isAtEnd()) {
        if (peek() == '\n') {
            line++;
            column = 0;
        }

        if (peek() == '\\' && peekNext() == '`') {
            advance();  // Skip the backslash
        }

        if (peek() == '$' && peekNext() == '{') {
            // This is a simplified approach - in a real lexer, we would need
            // to track the template expression state and handle it properly
            advance();  // $
            advance();  // {

            // Simplified: just count braces to find the matching closing }
            int braceCount = 1;
            while (braceCount > 0 && !isAtEnd()) {
                if (peek() == '{')
                    braceCount++;
                else if (peek() == '}')
                    braceCount--;

                if (braceCount > 0)
                    advance();
            }

            if (isAtEnd())
                return errorToken("Unterminated template expression");

            advance();  // Consume the closing }
        } else {
            advance();
        }
    }

    if (isAtEnd())
        return errorToken("Unterminated template string");

    // The closing backtick
    advance();

    // Extract the template string value without backticks
    std::string_view strValue = source.substr(start + 1, current - start - 2);
    Token token = makeToken(Token::Type::TemplateString);
    token.stringValue = std::string(strValue);

    return token;
}

Token Lexer::makeToken(Token::Type type) const {
    return Token(type, source.substr(start, current - start), line,
                 column - (current - start));
}

Token Lexer::errorToken(const std::string& message) const {
    Token token(Token::Type::Error, message, line, column);
    return token;
}

}  // namespace tsx