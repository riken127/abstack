#include "abstack/codegen/compose_emitter.hxx"
#include "abstack/codegen/dockerfile_emitter.hxx"
#include "abstack/format/formatter.hxx"
#include "abstack/frontend/lexer.hxx"
#include "abstack/frontend/parser.hxx"
#include "abstack/ir/lowering.hxx"
#include "abstack/semantic/validator.hxx"
#include "abstack/stdlib/library.hxx"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{

void assert_contains(const std::string& text, const std::string& needle)
{
    if (text.find(needle) == std::string::npos)
    {
        throw std::runtime_error("Expected output to contain: " + needle + "\nActual:\n" + text);
    }
}

abstack::Ast parse_source(const std::string& source)
{
    abstack::Lexer lexer(source);
    const auto tokens = lexer.tokenize();
    abstack::Parser parser(tokens);
    return parser.parse();
}

abstack::Ast merge_asts(abstack::Ast left, abstack::Ast right)
{
    abstack::Ast merged{};

    merged.templates = std::move(left.templates);
    merged.services = std::move(left.services);

    std::move(right.templates.begin(), right.templates.end(), std::back_inserter(merged.templates));
    std::move(right.services.begin(), right.services.end(), std::back_inserter(merged.services));

    return merged;
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

void test_multi_use_and_array_commands()
{
    const std::string source = R"(
template app(name) {
    stage build {
        from "golang:1.22"
        run "go build -o app ./cmd/${name}"
    }

    stage runtime {
        from "alpine:3.20"
        copy from build "/src/app" "/app"
        expose 8080
        cmd ["/app", "--serve"]
    }
}

template diagnostics() {
    stage tools {
        from "alpine:3.20"
        run "echo diagnostics"
    }
}

service app {
    use app("api")
    use diagnostics()
    entrypoint ["/bin/sh", "-c"]
    cmd ["/app", "--prod"]
    port "8080:8080"
}
)";

    abstack::Lexer lexer(source);
    const auto tokens = lexer.tokenize();
    abstack::Parser parser(tokens);
    const abstack::Ast ast = parser.parse();
    abstack::validate_ast(ast);

    const abstack::BuildPlan plan = abstack::lower_to_ir(ast);
    assert(plan.services.size() == 1);

    const std::string dockerfile = abstack::emit_dockerfile(plan.services.front());
    assert_contains(dockerfile, "AS u0_app_build");
    assert_contains(dockerfile, "AS u1_diagnostics_tools");
    assert_contains(dockerfile, "ENTRYPOINT [\"/bin/sh\", \"-c\"]");
    assert_contains(dockerfile, "CMD [\"/app\", \"--prod\"]");

    const std::string compose = abstack::emit_compose(plan);
    assert_contains(compose, "entrypoint:");
    assert_contains(compose, "- \"/bin/sh\"");
    assert_contains(compose, "- \"-c\"");
    assert_contains(compose, "command:");
    assert_contains(compose, "- \"/app\"");
    assert_contains(compose, "- \"--prod\"");
}

void test_semantic_rejects_unknown_dependency()
{
    const std::string source = R"(
template base() {
    stage runtime {
        from "alpine:3.20"
    }
}

service app {
    use base()
    depends_on missing
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

void test_formatter_outputs_canonical_abs()
{
    const std::string source = R"(
service api {
    depends_on db
    use app("api")
    cmd ["/app", "--serve"]
}

template app(name) {
    stage runtime {
        from "alpine:3.20"
        cmd ["/app", "--serve"]
        run "echo ${name}"
    }
}

service db {
    use db_base()
}

template db_base() {
    stage runtime {
        from "postgres:16"
    }
}
)";

    abstack::Lexer lexer(source);
    const auto tokens = lexer.tokenize();
    abstack::Parser parser(tokens);
    const abstack::Ast ast = parser.parse();
    const std::string formatted = abstack::format_ast(ast);

    assert_contains(formatted, "template app(name) {");
    assert_contains(formatted, "cmd [\"/app\", \"--serve\"]");
    assert_contains(formatted, "service api {");
    assert_contains(formatted, "depends_on db");

    abstack::Lexer formatted_lexer(formatted);
    const auto formatted_tokens = formatted_lexer.tokenize();
    abstack::Parser formatted_parser(formatted_tokens);
    const abstack::Ast reparsed = formatted_parser.parse();
    abstack::validate_ast(reparsed);
}

void test_comments_supported()
{
    const std::string source = R"(
// Single-line comment before declarations.
template base() {
    stage runtime {
        from "alpine:3.20" // Trailing single-line comment.
        run "echo hello"
        # Hash-style comment remains valid.
        /* Multi-line comment
           inside stage body. */
    }
}

/* Multi-line comment between declarations. */
service db {
    use base()
}

service api {
    use base()
    env {
        // Inline docs for env values.
        LOG_LEVEL = "info"
        /* Inline block comment before next binding. */
        MODE = "prod"
    }
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

    const std::string compose = abstack::emit_compose(plan);
    assert_contains(compose, "api:");
    assert_contains(compose, "db:");
    assert_contains(compose, "\"db\"");
}

void test_unterminated_block_comment_rejected()
{
    const std::string source = R"(
template broken() {
    stage runtime {
        from "alpine:3.20"
        /* Unterminated comment
    }
}
)";

    bool threw_expected_error = false;
    try
    {
        abstack::Lexer lexer(source);
        (void)lexer.tokenize();
    }
    catch (const std::runtime_error& error)
    {
        threw_expected_error =
            std::string(error.what()).find("unterminated block comment") != std::string::npos;
    }

    assert(threw_expected_error);
}

void test_stdlib_profiles_available()
{
    const auto profiles = abstack::stdlib_profiles();
    assert(!profiles.empty());
    assert(abstack::stdlib_profile_source("core-v1").has_value());
    assert(abstack::stdlib_profile_source("default").has_value());
    assert(!abstack::stdlib_profile_source("unknown-profile").has_value());
}

void test_stdlib_templates_can_be_linked_with_user_source()
{
    const auto stdlib_source = abstack::stdlib_profile_source("default");
    assert(stdlib_source.has_value());

    const std::string user_source = R"(
service db {
    use std_v1_postgres()
}

service api {
    use std_v1_go_service("api", 8080)
    port "8080:8080"
    depends_on db
}
)";

    const abstack::Ast merged = merge_asts(parse_source(std::string(*stdlib_source)),
                                           parse_source(user_source));

    abstack::validate_ast(merged);
    const abstack::BuildPlan plan = abstack::lower_to_ir(merged);
    assert(plan.services.size() == 2);

    const std::string compose = abstack::emit_compose(plan);
    assert_contains(compose, "api:");
    assert_contains(compose, "db:");
    assert_contains(compose, "\"8080:8080\"");
}

} // namespace

int main()
{
    test_pipeline_generation();
    test_multi_use_and_array_commands();
    test_semantic_rejects_unknown_dependency();
    test_formatter_outputs_canonical_abs();
    test_comments_supported();
    test_unterminated_block_comment_rejected();
    test_stdlib_profiles_available();
    test_stdlib_templates_can_be_linked_with_user_source();
    return 0;
}
