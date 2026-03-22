#include "abstack/frontend/parser.hxx"

#include <stdexcept>
#include <utility>

namespace abstack
{

Parser::Parser(const std::vector<Token>& tokens) : tokens_(tokens)
{
}

Ast Parser::parse()
{
    Ast ast;

    while (!is_at_end())
    {
        // Top-level declarations are intentionally dispatched by leading keyword so the
        // grammar stays easy to extend without a larger parser table.
        if (match(TokenType::Template))
        {
            ast.templates.push_back(parse_template());
            continue;
        }

        if (match(TokenType::Service))
        {
            ast.services.push_back(parse_service());
            continue;
        }

        fail_here("expected `template` or `service`");
    }

    return ast;
}

bool Parser::is_at_end() const
{
    return peek().type == TokenType::EndOfFile;
}

const Token& Parser::peek() const
{
    return tokens_[current_];
}

const Token& Parser::previous() const
{
    return tokens_[current_ - 1];
}

const Token& Parser::advance()
{
    if (!is_at_end())
        ++current_;

    return previous();
}

bool Parser::check(const TokenType type) const
{
    if (is_at_end())
        return false;

    return peek().type == type;
}

bool Parser::match(const TokenType type)
{
    if (!check(type))
        return false;

    advance();
    return true;
}

const Token& Parser::consume(const TokenType type, const char* message)
{
    if (check(type))
        return advance();

    fail_here(message);
}

TemplateDecl Parser::parse_template()
{
    TemplateDecl decl{};
    decl.name = consume(TokenType::Identifier, "expected template name").lexeme;

    consume(TokenType::LParen, "expected `(` after template name");
    if (!check(TokenType::RParen))
    {
        do
        {
            decl.params.push_back(consume(TokenType::Identifier, "expected parameter name").lexeme);
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RParen, "expected `)` after template parameters");

    consume(TokenType::LBrace, "expected `{` before template body");
    while (!check(TokenType::RBrace))
        decl.stages.push_back(parse_stage());
    consume(TokenType::RBrace, "expected `}` after template body");

    return decl;
}

ServiceDecl Parser::parse_service()
{
    ServiceDecl decl{};
    decl.name = consume(TokenType::Identifier, "expected service name").lexeme;

    consume(TokenType::LBrace, "expected `{` before service body");

    while (!check(TokenType::RBrace))
    {
        // Service bodies are a keyword-led statement list; each branch also enforces the
        // per-statement uniqueness rules that the validator expects later.
        if (match(TokenType::Use))
        {
            decl.uses.push_back(parse_use());
            continue;
        }

        if (match(TokenType::Env))
        {
            parse_env_block(decl.env);
            continue;
        }

        if (match(TokenType::Expose))
        {
            decl.exposes.push_back(parse_value());
            continue;
        }

        if (match(TokenType::Cmd))
        {
            if (decl.cmd.has_value())
                fail_here("duplicate `cmd` in service");

            decl.cmd = parse_command_expr();
            continue;
        }

        if (match(TokenType::Entrypoint))
        {
            if (decl.entrypoint.has_value())
                fail_here("duplicate `entrypoint` in service");

            decl.entrypoint = parse_command_expr();
            continue;
        }

        if (match(TokenType::Port))
        {
            decl.ports.push_back(parse_value());
            continue;
        }

        if (match(TokenType::DependsOn))
        {
            decl.depends_on.push_back(
                consume(TokenType::Identifier, "expected service identifier after `depends_on`")
                    .lexeme);
            continue;
        }

        fail_here("unexpected statement in service body");
    }

    consume(TokenType::RBrace, "expected `}` after service body");
    return decl;
}

Stage Parser::parse_stage()
{
    consume(TokenType::Stage, "expected `stage`");

    Stage stage{};
    stage.name = consume(TokenType::Identifier, "expected stage name").lexeme;

    consume(TokenType::LBrace, "expected `{` before stage body");

    while (!check(TokenType::RBrace))
    {
        // Stage bodies follow the same keyword-dispatch pattern, which keeps the statement
        // ordering flexible while still guarding against duplicate singleton directives.
        if (match(TokenType::From))
        {
            if (stage.from_image.has_value())
                fail_here("duplicate `from` in stage");

            stage.from_image = parse_value();
            continue;
        }

        if (match(TokenType::Workdir))
        {
            if (stage.workdir.has_value())
                fail_here("duplicate `workdir` in stage");

            stage.workdir = parse_value();
            continue;
        }

        if (match(TokenType::Copy))
        {
            CopyStmt copy{};
            if (match(TokenType::From))
            {
                copy.from_stage =
                    consume(TokenType::Identifier, "expected stage name after `from`").lexeme;
            }

            copy.source = parse_value();
            copy.destination = parse_value();
            stage.copies.push_back(std::move(copy));
            continue;
        }

        if (match(TokenType::Run))
        {
            stage.run_commands.push_back(parse_value());
            continue;
        }

        if (match(TokenType::Env))
        {
            parse_env_block(stage.env);
            continue;
        }

        if (match(TokenType::Expose))
        {
            stage.exposes.push_back(parse_value());
            continue;
        }

        if (match(TokenType::Cmd))
        {
            if (stage.cmd.has_value())
                fail_here("duplicate `cmd` in stage");

            stage.cmd = parse_command_expr();
            continue;
        }

        if (match(TokenType::Entrypoint))
        {
            if (stage.entrypoint.has_value())
                fail_here("duplicate `entrypoint` in stage");

            stage.entrypoint = parse_command_expr();
            continue;
        }

        fail_here("unexpected statement in stage body");
    }

    consume(TokenType::RBrace, "expected `}` after stage body");
    return stage;
}

UseStmt Parser::parse_use()
{
    UseStmt use{};
    use.template_name = consume(TokenType::Identifier, "expected template name after `use`").lexeme;

    consume(TokenType::LParen, "expected `(` after template name");
    if (!check(TokenType::RParen))
    {
        do
        {
            use.arguments.push_back(parse_value());
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RParen, "expected `)` after use arguments");

    return use;
}

void Parser::parse_env_block(std::vector<EnvBinding>& env)
{
    consume(TokenType::LBrace, "expected `{` after `env`");

    while (!check(TokenType::RBrace))
    {
        EnvBinding binding{};
        binding.key = consume(TokenType::Identifier, "expected environment key").lexeme;
        consume(TokenType::Equal, "expected `=` after environment key");
        binding.value = parse_value();
        env.push_back(std::move(binding));
    }

    consume(TokenType::RBrace, "expected `}` after env block");
}

Value Parser::parse_value()
{
    if (match(TokenType::String))
        return StringValue{previous().lexeme};

    if (match(TokenType::Number))
        return NumberValue{std::stoll(previous().lexeme)};

    if (match(TokenType::Identifier))
        return IdentifierValue{previous().lexeme};

    fail_here("expected value");
}

CommandExpr Parser::parse_command_expr()
{
    if (!match(TokenType::LBracket))
        return parse_value();

    ArrayLiteral literal{};

    if (!check(TokenType::RBracket))
    {
        do
        {
            literal.items.push_back(parse_value());
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RBracket, "expected `]` after command array");
    return literal;
}

void Parser::fail_here(const std::string& message) const
{
    const Token& token = peek();
    throw std::runtime_error("Parse error at line " + std::to_string(token.line) + ": " +
                             message + " (found " + to_string(token.type) + ")");
}

} // namespace abstack
