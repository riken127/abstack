#include "abstack/frontend/lexer.hxx"

#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace abstack
{

namespace
{
[[nodiscard]] std::runtime_error make_lex_error(const int line, const std::string& message)
{
    return std::runtime_error("Lex error at line " + std::to_string(line) + ": " + message);
}
} // namespace

Lexer::Lexer(std::string source) : source_(std::move(source))
{
}

std::vector<Token> Lexer::tokenize()
{
    std::vector<Token> tokens;

    while (!is_at_end())
    {
        skip_whitespace_and_comments();
        if (is_at_end())
            break;

        const char c = peek();
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
        {
            tokens.push_back(identifier());
        }
        else if (std::isdigit(static_cast<unsigned char>(c)))
        {
            tokens.push_back(number());
        }
        else if (c == '"')
        {
            tokens.push_back(string());
        }
        else
        {
            tokens.push_back(symbol());
        }
    }

    tokens.push_back(Token{TokenType::EndOfFile, "", line_});
    return tokens;
}

bool Lexer::is_at_end() const
{
    return pos_ >= source_.size();
}

char Lexer::peek() const
{
    return source_[pos_];
}

char Lexer::peek_next() const
{
    if (pos_ + 1 >= source_.size())
        return '\0';

    return source_[pos_ + 1];
}

char Lexer::advance()
{
    return source_[pos_++];
}

void Lexer::skip_whitespace_and_comments()
{
    while (!is_at_end())
    {
        const char c = peek();

        if (c == ' ' || c == '\t' || c == '\r')
        {
            advance();
            continue;
        }

        if (c == '\n')
        {
            ++line_;
            advance();
            continue;
        }

        if (c == '#')
        {
            while (!is_at_end() && peek() != '\n')
                advance();
            continue;
        }

        break;
    }
}

Token Lexer::identifier()
{
    const std::size_t start = pos_;
    while (!is_at_end())
    {
        const char c = peek();
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
            break;
        advance();
    }

    const std::string text = source_.substr(start, pos_ - start);

    static const std::unordered_map<std::string, TokenType> kKeywords{
        {"template", TokenType::Template},   {"service", TokenType::Service},
        {"stage", TokenType::Stage},         {"use", TokenType::Use},
        {"from", TokenType::From},           {"run", TokenType::Run},
        {"copy", TokenType::Copy},           {"env", TokenType::Env},
        {"expose", TokenType::Expose},       {"cmd", TokenType::Cmd},
        {"entrypoint", TokenType::Entrypoint},
        {"workdir", TokenType::Workdir},     {"port", TokenType::Port},
        {"depends_on", TokenType::DependsOn},
    };

    if (const auto it = kKeywords.find(text); it != kKeywords.end())
        return Token{it->second, text, line_};

    return Token{TokenType::Identifier, text, line_};
}

Token Lexer::number()
{
    const std::size_t start = pos_;
    while (!is_at_end() && std::isdigit(static_cast<unsigned char>(peek())))
        advance();

    return Token{TokenType::Number, source_.substr(start, pos_ - start), line_};
}

Token Lexer::string()
{
    advance();

    std::string value;
    while (!is_at_end())
    {
        const char c = advance();

        if (c == '"')
            return Token{TokenType::String, value, line_};

        if (c == '\\')
        {
            if (is_at_end())
                throw make_lex_error(line_, "unterminated escape sequence");

            const char escaped = advance();
            switch (escaped)
            {
            case 'n':
                value.push_back('\n');
                break;
            case 't':
                value.push_back('\t');
                break;
            case '"':
                value.push_back('"');
                break;
            case '\\':
                value.push_back('\\');
                break;
            default:
                throw make_lex_error(line_, "unsupported escape sequence");
            }
            continue;
        }

        if (c == '\n')
            ++line_;

        value.push_back(c);
    }

    throw make_lex_error(line_, "unterminated string literal");
}

Token Lexer::symbol()
{
    const char c = advance();

    switch (c)
    {
    case '{':
        return Token{TokenType::LBrace, "{", line_};
    case '}':
        return Token{TokenType::RBrace, "}", line_};
    case '(':
        return Token{TokenType::LParen, "(", line_};
    case ')':
        return Token{TokenType::RParen, ")", line_};
    case '[':
        return Token{TokenType::LBracket, "[", line_};
    case ']':
        return Token{TokenType::RBracket, "]", line_};
    case ',':
        return Token{TokenType::Comma, ",", line_};
    case '=':
        return Token{TokenType::Equal, "=", line_};
    default:
        throw make_lex_error(line_, std::string("unexpected character `") + c + "`");
    }
}

} // namespace abstack
