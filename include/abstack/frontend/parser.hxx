#pragma once

#include "abstack/ast.hxx"
#include "abstack/frontend/token.hxx"

#include <vector>

namespace abstack
{

class Parser
{
public:
    explicit Parser(const std::vector<Token>& tokens);

    [[nodiscard]] Ast parse();

private:
    const std::vector<Token>& tokens_;
    std::size_t current_ = 0;

    [[nodiscard]] bool is_at_end() const;
    [[nodiscard]] const Token& peek() const;
    [[nodiscard]] const Token& previous() const;
    const Token& advance();

    [[nodiscard]] bool check(TokenType type) const;
    bool match(TokenType type);
    const Token& consume(TokenType type, const char* message);

    [[nodiscard]] TemplateDecl parse_template();
    [[nodiscard]] ServiceDecl parse_service();
    [[nodiscard]] Stage parse_stage();
    [[nodiscard]] UseStmt parse_use();

    void parse_env_block(std::vector<EnvBinding>& env);
    [[nodiscard]] Value parse_value();
    [[nodiscard]] CommandExpr parse_command_expr();

    [[noreturn]] void fail_here(const std::string& message) const;
};

} // namespace abstack
