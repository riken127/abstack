#include "abstack/codegen/compose_emitter.hxx"
#include "abstack/codegen/dockerfile_emitter.hxx"
#include "abstack/frontend/lexer.hxx"
#include "abstack/frontend/parser.hxx"
#include "abstack/ir/lowering.hxx"
#include "abstack/semantic/validator.hxx"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>

namespace
{

struct CliOptions
{
    std::filesystem::path input;
    std::filesystem::path output_dir = "generated";
    std::optional<std::filesystem::path> compose_path;
};

[[nodiscard]] std::string read_file(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Failed to open input file: " + path.string());

    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void write_file(const std::filesystem::path& path, const std::string& content)
{
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path());

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("Failed to write file: " + path.string());

    output << content;
}

[[nodiscard]] CliOptions parse_args(const int argc, char** argv)
{
    CliOptions options{};

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--out-dir")
        {
            if (i + 1 >= argc)
                throw std::runtime_error("--out-dir requires a path argument");
            options.output_dir = argv[++i];
            continue;
        }

        if (arg == "--compose-file")
        {
            if (i + 1 >= argc)
                throw std::runtime_error("--compose-file requires a path argument");
            options.compose_path = std::filesystem::path(argv[++i]);
            continue;
        }

        if (!arg.empty() && arg.front() == '-')
            throw std::runtime_error("Unknown option: " + arg);

        if (!options.input.empty())
            throw std::runtime_error("Only one input file is supported");

        options.input = arg;
    }

    if (options.input.empty())
    {
        throw std::runtime_error(
            "Usage: abstack <file.abs> [--out-dir <dir>] [--compose-file <file>] ");
    }

    return options;
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const CliOptions options = parse_args(argc, argv);

        const std::string source = read_file(options.input);

        abstack::Lexer lexer(source);
        const auto tokens = lexer.tokenize();

        abstack::Parser parser(tokens);
        const abstack::Ast ast = parser.parse();
        abstack::validate_ast(ast);

        const abstack::BuildPlan plan = abstack::lower_to_ir(ast);

        std::filesystem::create_directories(options.output_dir);

        for (const auto& service : plan.services)
        {
            const auto dockerfile_path = options.output_dir / service.compose.dockerfile;
            write_file(dockerfile_path, abstack::emit_dockerfile(service));
            std::cout << "Generated " << dockerfile_path << "\n";
        }

        const std::filesystem::path compose_path =
            options.compose_path.value_or(options.output_dir / "docker-compose.generated.yml");
        write_file(compose_path, abstack::emit_compose(plan));

        std::cout << "Generated " << compose_path << "\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
