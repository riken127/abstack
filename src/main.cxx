#include <iostream>

#include "lexel.hxx"
#include "parser.hxx"

int main(int argc, char** argv)
{
    const std::string src = R"(

template go_service(name, port) {
    stage build {
        from "golang:1.22"
        workdir "/src"
        run "go build"
        run "go test ./..."
    }
}

service api {}

)";

    Lexer lexer{src};

    auto tokens = lexer.tokenize();

    Parser parser{tokens};
    auto ast = parser.parse();

    parser.dump_ast(ast);

    return 0;
}
