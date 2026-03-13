#pragma once

#include "token.hxx"
#include <string>
#include <vector>

class Lexer
{
public:
    explicit Lexer(const std::string& source);

    std::vector<Token> tokenize();
    static const char* to_string(TokenType type);
private:
    std::string source;
    size_t pos = 0;
    int line = 1;

    char peek() const;
    char advance();
    bool is_at_end() const;

    void skip_whitespace();

    Token identifier();
    Token string();
    Token number();
    Token symbol();
};