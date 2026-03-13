#pragma once

#include "ast.hxx"
#include "token.hxx"

#include <vector>

class Parser
{
public:
    explicit Parser(const std::vector<Token>& tokens);

    Ast parse();
private:
    const std::vector<Token>& tokens;
    size_t current = 0;

    bool is_at_end() const;
    const Token& peek() const;
    const Token& previous() const;
    const Token& advance();

    bool check(TokenType type) const;
    bool match(TokenType type);

    const Token& consume(TokenType type, const char* message);

    TemplateDecl parse_template();
    ServiceDecl parse_service();
    Stage parse_stage();
};
