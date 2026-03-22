#include "abstack/codegen/compose_emitter.hxx"
#include "abstack/codegen/dockerfile_emitter.hxx"
#include "abstack/frontend/lexer.hxx"
#include "abstack/frontend/parser.hxx"
#include "abstack/ir/lowering.hxx"
#include "abstack/semantic/validator.hxx"

#include <cassert>
#include <stdexcept>
#include <string>

namespace
{

void assert_contains(const std::string& text, const std::string& needle)
{
    if (text.find(needle) == std::string::npos)
    {
        throw std::runtime_error("Expected output to contain: " + needle + "\nActual:\n" + text);
    }
}

void test_pipeline_generation()
{
    const std::string source = R"(
template service_image(name, service_port) {
    stage build {
        from "golang:1.22"
        workdir "/src"
        copy "." "/src"
        run "go build -o app ./cmd/${name}"
    }

    stage runtime {
        from "alpine:3.20"
        copy from build "/src/app" "/app"
        expose service_port
        cmd "/app"
    }
}

template postgres() {
    stage runtime {
        from "postgres:16"
        expose 5432
    }
}

service db {
    use postgres()
}

service api {
    use service_image("api", 8080)
    env {
        LOG_LEVEL = "info"
    }
    expose 9090
    port "8080:9090"
    cmd "/app --serve"
    depends_on db
}
)";

    abstack::Lexer lexer(source);
    const auto tokens = lexer.tokenize();
    abstack::Parser parser(tokens);

    const abstack::Ast ast = parser.parse();
    abstack::validate_ast(ast);

    const abstack::BuildPlan plan = abstack::lower_to_ir(ast);
    assert(plan.services.size() == 2);

    const auto& api = plan.services[1];
    const std::string dockerfile = abstack::emit_dockerfile(api);
    assert_contains(dockerfile, "FROM golang:1.22 AS build");
    assert_contains(dockerfile, "FROM alpine:3.20 AS runtime");
    assert_contains(dockerfile, "EXPOSE 8080");
    assert_contains(dockerfile, "EXPOSE 9090");
    assert_contains(dockerfile, "ENV LOG_LEVEL=info");
    assert_contains(dockerfile, "CMD /app --serve");

    const std::string compose = abstack::emit_compose(plan);
    assert_contains(compose, "api:");
    assert_contains(compose, "dockerfile: \"Dockerfile.api\"");
    assert_contains(compose, "depends_on:");
    assert_contains(compose, "\"db\"");
    assert_contains(compose, "\"8080:9090\"");
    assert_contains(compose, "LOG_LEVEL: \"info\"");
}

void test_semantic_rejects_multiple_use_statements()
{
    const std::string source = R"(
template base() {
    stage runtime {
        from "alpine"
    }
}

service app {
    use base()
    use base()
}
)";

    abstack::Lexer lexer(source);
    const auto tokens = lexer.tokenize();
    abstack::Parser parser(tokens);
    const abstack::Ast ast = parser.parse();

    bool threw = false;
    try
    {
        abstack::validate_ast(ast);
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    assert(threw);
}

} // namespace

int main()
{
    test_pipeline_generation();
    test_semantic_rejects_multiple_use_statements();
    return 0;
}
