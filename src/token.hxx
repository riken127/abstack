#pragma once

#include <string>

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

    EndOfFile
};

struct Token
{
    TokenType type;
    std::string lexeme;
    int line;
};