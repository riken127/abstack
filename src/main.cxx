#include <iostream>

#include "lexel.hxx"
#include "parser.hxx"

int main(int argc, char** argv)
{
    const std::string src = R"(

cmd ["/app"]

)";

    Lexer lexer{src};

    auto tokens = lexer.tokenize();

    Parser parser{tokens};
    auto ast = parser.parse();

    parser.dump_ast(ast);

    return 0;
}
