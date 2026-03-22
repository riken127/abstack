#pragma once

#include "abstack/frontend/token.hxx"

#include <string>
#include <vector>

namespace abstack
{

class Lexer
{
public:
    explicit Lexer(std::string source);

    [[nodiscard]] std::vector<Token> tokenize();

private:
    std::string source_;
    std::size_t pos_ = 0;
    int line_ = 1;

    [[nodiscard]] bool is_at_end() const;
    [[nodiscard]] char peek() const;
    [[nodiscard]] char peek_next() const;
    char advance();

    void skip_whitespace_and_comments();

    [[nodiscard]] Token identifier();
    [[nodiscard]] Token number();
    [[nodiscard]] Token string();
    [[nodiscard]] Token symbol();
};

} // namespace abstack
