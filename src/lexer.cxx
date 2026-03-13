#include "lexel.hxx"

#include <stdexcept>

Lexer::Lexer(const std::string& source) : source(source)
{
}

std::vector<Token> Lexer::tokenize()
{
    std::vector<Token> tokens;

    while (!is_at_end())
    {
        skip_whitespace();

        if (is_at_end())
            break;

        char c = peek();

        if (isalpha(c) || c == '_')
            tokens.push_back(identifier());
        else if (isdigit(c))
            tokens.push_back(number());
        else if (c == '"')
            tokens.push_back(string());
        else
            tokens.push_back(symbol());
    }

    tokens.push_back({TokenType::EndOfFile, "", line});
    return tokens;
}

char Lexer::peek() const
{
    return source[pos];
}

char Lexer::advance()
{
    return source[pos++];
}

bool Lexer::is_at_end() const
{
    return pos >= source.size();
}

void Lexer::skip_whitespace()
{
    while (!is_at_end())
    {
        char c = peek();

        if (c == ' ' || c == '\t' || c == '\r')
        {
            advance();
        }
        else if (c == '\n')
        {
            line++;
            advance();
        }
        else
            break;
    }
}

Token Lexer::identifier()
{
    size_t start = pos;

    while (!is_at_end() && (isalnum(peek()) || peek() == '_'))
        advance();

    std::string text = source.substr(start, pos - start);

    TokenType type = TokenType::Identifier;

    if (text == "template")
        type = TokenType::Template;
    else if (text == "service")
        type = TokenType::Service;
    else if (text == "stage")
        type = TokenType::Stage;
    else if (text == "use")
        type = TokenType::Use;
    else if (text == "from")
        type = TokenType::From;
    else if (text == "run")
        type = TokenType::Run;
    else if (text == "copy")
        type = TokenType::Copy;
    else if (text == "env")
        type = TokenType::Env;
    else if (text == "expose")
        type = TokenType::Expose;
    else if (text == "cmd")
        type = TokenType::Cmd;
    else if (text == "entrypoint")
        type = TokenType::Entrypoint;
    else if (text == "workdir")
        type = TokenType::Workdir;

    return {type, text, line};
}

Token Lexer::number()
{
    size_t start = pos;

    while (!is_at_end() && isdigit(peek()))
        advance();

    std::string text = source.substr(start, pos - start);

    return {TokenType::Number, text, line};
}

Token Lexer::string()
{
    advance();

    size_t start = pos;

    while (!is_at_end() && peek() != '"')
    {
        if (peek() == '\n')
            line++;

        advance();
    }

    std::string text = source.substr(start, pos - start);

    advance();

    return {TokenType::String, text, line};
}

Token Lexer::symbol()
{
    char c = advance();

    switch (c)
    {
    case '{':
        return {TokenType::LBrace, "{", line};
    case '}':
        return {TokenType::RBrace, "}", line};
    case '(':
        return {TokenType::LParen, "(", line};
    case ')':
        return {TokenType::RParen, ")", line};
    case '[':
        return {TokenType::LBracket, "[", line};
    case ']':
        return {TokenType::RBracket, "]", line};
    case ',':
        return {TokenType::Comma, ",", line};
    case '=':
        return {TokenType::Equal, "=", line};
    default:
        throw std::runtime_error("Unexpected character");
    }
}

const char* Lexer::to_string(TokenType type)
{
    switch (type)
    {
    case TokenType::Identifier:
        return "Identifier";
    case TokenType::String:
        return "String";
    case TokenType::Number:
        return "Number";
    case TokenType::LBrace:
        return "LBrace";
    case TokenType::RBrace:
        return "RBrace";
    case TokenType::LParen:
        return "LParen";
    case TokenType::RParen:
        return "RParen";
    case TokenType::LBracket:
        return "LBracket";
    case TokenType::RBracket:
        return "RBracket";
    case TokenType::Comma:
        return "Comma";
    case TokenType::Equal:
        return "Equal";
    case TokenType::Template:
        return "Template";
    case TokenType::Service:
        return "Service";
    case TokenType::Stage:
        return "Stage";
    case TokenType::Use:
        return "Use";
    case TokenType::From:
        return "From";
    case TokenType::Run:
        return "Run";
    case TokenType::Copy:
        return "Copy";
    case TokenType::Env:
        return "Env";
    case TokenType::Expose:
        return "Expose";
    case TokenType::Cmd:
        return "Cmd";
    case TokenType::Entrypoint:
        return "Entrypoint";
    case TokenType::Workdir:
        return "Workdir";
    case TokenType::EndOfFile:
        return "EndOfFile";
    }

    return "Unknown";
}
