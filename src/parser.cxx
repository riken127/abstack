#include "parser.hxx"

#include "token.hxx"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens)
{
}

bool Parser::is_at_end() const
{
    return peek().type == TokenType::EndOfFile;
}

const Token& Parser::peek() const
{
    return tokens[current];
}

const Token& Parser::previous() const
{
    return tokens[current - 1];
}

const Token& Parser::advance()
{
    if (!is_at_end())
        current++;

    return previous();
}

bool Parser::check(TokenType type) const
{
    if (is_at_end())
        return false;

    return peek().type == type;
}

bool Parser::match(TokenType type)
{
    if (!check(type))
        return false;

    advance();
    return true;
}

const Token& Parser::consume(TokenType type, const char* message)
{
    if (check(type))
        return advance();

    throw std::runtime_error(message);
}

Ast Parser::parse()
{
    Ast ast;

    std::unordered_map<TokenType, std::function<void()>> handlers{
        {TokenType::Template,
         [&]() {
             advance();
             ast.templates.push_back(parse_template());
         }},
        {TokenType::Service,
         [&]() {
             advance();
             ast.services.push_back(parse_service());
         }},
        {TokenType::From,
         [&]() {
             advance();
             auto image = consume(TokenType::String, "Expected image");
         }},
    };

    while (!is_at_end())
    {
        auto it = handlers.find(peek().type);

        if (it == handlers.end())
            throw std::runtime_error("Expected template or service");

        it->second();
    }

    return ast;
}

TemplateDecl Parser::parse_template()
{
    TemplateDecl decl{};

    decl.name = consume(TokenType::Identifier, "Expected template name").lexeme;

    consume(TokenType::LParen, "Expected `(` after template name");

    if (!check(TokenType::RParen))
    {
        do
        {
            decl.params.push_back(consume(TokenType::Identifier, "Expected parameter name").lexeme);
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RParen, "Expected `)` after parameters");
    consume(TokenType::LBrace, "Expected `{` before template body");

    while (!check(TokenType::RBrace))
        decl.stages.push_back(parse_stage());

    consume(TokenType::RBrace, "Expected `}` after template body");

    return decl;
}

ServiceDecl Parser::parse_service()
{
    ServiceDecl decl{};
    decl.name = consume(TokenType::Identifier, "Expected service name").lexeme;
    consume(TokenType::LBrace, "Expected `{` before service body");
    consume(TokenType::RBrace, "Expected `}` after service body");
    return decl;
}

Stage Parser::parse_stage()
{
    consume(TokenType::Stage, "Expected `stage`");

    Stage stage{};
    stage.name = consume(TokenType::Identifier, "Expected stage name").lexeme;

    consume(TokenType::LBrace, "Expected `{` before stage body");

    while (!check(TokenType::RBrace))
        advance();

    consume(TokenType::RBrace, "Expected `}` after stage body");

    return stage;
}

void dump_ast(const Ast& ast)
{
    for (const auto& t : ast.templates)
    {
        std::cout << "template " << t.name << "\n";

        for (const auto& p : t.params)
            std::cout << "  param: " << p << "\n";

        for (const auto& s : t.stages)
            std::cout << "  stage: " << s.name << "\n";
    }

    for (const auto& s : ast.services)
    {
        std::cout << "service " << s.name << "\n";
    }
}
