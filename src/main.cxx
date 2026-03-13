#include <iostream>

#include "lexel.hxx"

int main(int argc, char** argv)
{
    const std::string src = R"(

cmd ["/app"]

)";
    Lexer lexer{src};

    for (const auto& t : lexer.tokenize())
        std::cout << t.lexeme << " (" << lexer.to_string(t.type) << ")\n";

    return 0;
}
