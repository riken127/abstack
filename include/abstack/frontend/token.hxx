#pragma once

#include <string>

namespace abstack
{

enum class TokenType
{
    Identifier,
    String,
    Number,

    LBrace,
    RBrace,
    LParen,
    RParen,
    LBracket,
    RBracket,

    Comma,
    Equal,

    Template,
    Service,
    Stage,
    Use,
    From,
    Run,
    Copy,
    Env,
    Expose,
    Cmd,
    Entrypoint,
    Workdir,
    Port,
    DependsOn,

    EndOfFile
};

struct Token
{
    TokenType type;
    std::string lexeme;
    int line = 1;
};

[[nodiscard]] const char* to_string(TokenType type);

} // namespace abstack
