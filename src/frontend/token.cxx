#include "abstack/frontend/token.hxx"

namespace abstack
{

const char* to_string(const TokenType type)
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
    case TokenType::Port:
        return "Port";
    case TokenType::DependsOn:
        return "DependsOn";
    case TokenType::EndOfFile:
        return "EndOfFile";
    }

    return "Unknown";
}

} // namespace abstack
