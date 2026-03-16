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

    while (!check(TokenType::RBrace))
    {
        if (match(TokenType::Use))
        {
            UseStmt use{};
            use.template_name =
                consume(TokenType::Identifier, "Expected template name after `use`").lexeme;

            consume(TokenType::LParen, "Expected `(` after template name");

            if (!check(TokenType::RParen))
            {
                do
                {
                    use.arguments.push_back(parse_value());
                } while (match(TokenType::Comma));
            }

            consume(TokenType::RParen, "Expected `)` after arguments");

            decl.uses.push_back(use);
        }
        else
        {
            throw std::runtime_error("Expected service statement");
        }
    }

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
    {
        if (match(TokenType::From))
        {
            if (!stage.from_image.has_value())
                throw std::runtime_error("Duplicate `from` in stage");

            stage.from_image = parse_value();
        }
        else if (match(TokenType::Workdir))
        {
            if (!stage.workdir.has_value())
                throw std::runtime_error("Duplicate `workdir` in stage");

            stage.workdir = parse_value();
        }
        else if (match(TokenType::Copy))
        {
            CopyStmt copy{};

            if (match(TokenType::From))
                copy.from_stage =
                    consume(TokenType::Identifier, "Expected stage name after `from`").lexeme;

            copy.source = parse_value();
            copy.destination = parse_value();

            stage.copies.push_back(copy);
        }
        else if (match(TokenType::Run))
        {
            stage.run_commands.push_back(parse_value());
        }
        else
        {
            throw std::runtime_error("Expected stage statement");
        }
    }

    consume(TokenType::RBrace, "Expected `}` after stage body");
    return stage;
}

Value Parser::parse_value()
{
    if (match(TokenType::String))
        return StringLiteral{previous().lexeme};

    if (match(TokenType::Identifier))
        return IdentifierRef{previous().lexeme};

    if (match(TokenType::Number))
        return StringLiteral{previous().lexeme};

    throw std::runtime_error("Expected value");
}

namespace
{
void dump_value(const Value& value)
{
    std::visit(
        [](const auto& v) {
            using T = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<T, StringLiteral>)
                std::cout << '"' << v.value << '"';
            else if constexpr (std::is_same_v<T, IdentifierRef>)
                std::cout << v.name;
        },
        value);
}
} // namespace

void Parser::dump_ast(const Ast& ast)
{
    for (const auto& t : ast.templates)
    {
        std::cout << "template " << t.name << "\n";

        for (const auto& p : t.params)
            std::cout << "  param: " << p << "\n";

        for (const auto& s : t.stages)
        {
            std::cout << "  stage: " << s.name << "\n";

            if (s.from_image.has_value())
                std::cout << "    from: ";
            dump_value(*s.from_image);
            std::cout << "\n";

            if (s.workdir.has_value())
            {
                std::cout << "    workdir: ";
                dump_value(*s.workdir);
                std::cout << "\n";
            }

            for (const auto& copy : s.copies)
            {
                if (!copy.from_stage.empty())
                {
                    std::cout << "    copy from " << copy.from_stage << ": ";
                    dump_value(copy.source);
                    std::cout << " -> ";
                    dump_value(copy.destination);
                    std::cout << "\n";
                }
                else
                {
                    std::cout << "    copy: ";
                    dump_value(copy.source);
                    std::cout << " -> ";
                    dump_value(copy.destination);
                    std::cout << "\n";
                }
            }

            for (const auto& cmd : s.run_commands)
            {
                std::cout << "    run: ";
                dump_value(cmd);
                std::cout << "\n";
            }
        }
    }

    for (const auto& s : ast.services)
    {
        std::cout << "service " << s.name << "\n";

        for (const auto& use : s.uses)
        {
            std::cout << "  use: " << use.template_name << "(";

            for (std::size_t i = 0; i < use.arguments.size(); ++i)
            {
                dump_value(use.arguments[i]);

                if (i + 1 < use.arguments.size())
                    std::cout << ", ";
            }

            std::cout << ")\n";
        }
    }
}
