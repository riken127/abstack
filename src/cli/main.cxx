#include "abstack/codegen/compose_emitter.hxx"
#include "abstack/codegen/dockerfile_emitter.hxx"
#include "abstack/format/formatter.hxx"
#include "abstack/frontend/lexer.hxx"
#include "abstack/frontend/parser.hxx"
#include "abstack/ir/lowering.hxx"
#include "abstack/semantic/validator.hxx"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef ABSTACK_HAS_CURSES
#include <curses.h>
#endif

namespace
{

struct EmitOptions
{
    std::filesystem::path output_dir = "generated";
    std::optional<std::filesystem::path> compose_path;
    bool clean_output = false;
};

struct BuildResult
{
    std::vector<std::filesystem::path> dockerfiles;
    std::filesystem::path compose_file;
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

[[nodiscard]] std::string consume_option_value(const std::vector<std::string>& args,
                                               std::size_t& index,
                                               const std::string& option)
{
    if (index + 1 >= args.size())
        throw std::runtime_error(option + " requires a value");

    ++index;
    return args[index];
}

[[nodiscard]] abstack::Ast parse_ast_source(const std::string& source,
                                            const std::string& context)
{
    try
    {
        abstack::Lexer lexer(source);
        const auto tokens = lexer.tokenize();

        abstack::Parser parser(tokens);
        return parser.parse();
    }
    catch (const std::exception& error)
    {
        throw std::runtime_error(context + ": " + error.what());
    }
}

[[nodiscard]] abstack::Ast parse_ast_file(const std::filesystem::path& path)
{
    return parse_ast_source(read_file(path), path.string());
}

[[nodiscard]] std::regex compile_regex(const std::string& pattern, const std::string& label)
{
    try
    {
        return std::regex(pattern);
    }
    catch (const std::regex_error& error)
    {
        throw std::runtime_error("Invalid regex for " + label + ": `" + pattern +
                                 "` (" + error.what() + ")");
    }
}

[[nodiscard]] std::optional<std::regex> compile_optional_regex(const std::optional<std::string>& pattern,
                                                               const std::string& label)
{
    if (!pattern.has_value())
        return std::nullopt;

    return compile_regex(*pattern, label);
}

[[nodiscard]] abstack::Ast filter_services(abstack::Ast ast,
                                           const std::optional<std::regex>& service_regex)
{
    if (!service_regex.has_value())
        return ast;

    std::vector<abstack::ServiceDecl> filtered;
    filtered.reserve(ast.services.size());

    for (const auto& service : ast.services)
    {
        bool matches = std::regex_search(service.name, *service_regex);

        if (!matches)
        {
            const std::size_t separator = service.name.rfind("__");
            if (separator != std::string::npos && separator + 2 < service.name.size())
            {
                const std::string_view base_name =
                    std::string_view(service.name).substr(separator + 2);
                matches = std::regex_search(base_name.begin(), base_name.end(), *service_regex);
            }
        }

        if (matches)
            filtered.push_back(service);
    }

    ast.services = std::move(filtered);
    return ast;
}

[[nodiscard]] abstack::Ast merge_asts(std::vector<abstack::Ast> asts)
{
    abstack::Ast merged;

    for (auto& ast : asts)
    {
        std::move(ast.templates.begin(), ast.templates.end(), std::back_inserter(merged.templates));
        std::move(ast.services.begin(), ast.services.end(), std::back_inserter(merged.services));
    }

    return merged;
}

[[nodiscard]] std::uint64_t fnv1a_hash(const std::string_view value)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char c : value)
    {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return hash;
}

[[nodiscard]] std::string sanitize_identifier(std::string_view text)
{
    std::string output;
    output.reserve(text.size() + 8);

    for (const char c : text)
    {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            output.push_back(c);
        else
            output.push_back('_');
    }

    if (output.empty() || (output.front() >= '0' && output.front() <= '9'))
        output.insert(output.begin(), '_');

    return output;
}

[[nodiscard]] abstack::Ast namespace_templates(abstack::Ast ast,
                                               const std::filesystem::path& input_dir,
                                               const std::filesystem::path& source_file)
{
    const std::string relative = std::filesystem::relative(source_file, input_dir).generic_string();
    const std::string prefix =
        sanitize_identifier(relative) + "_" + std::to_string(fnv1a_hash(relative));

    std::unordered_map<std::string, std::string> renamed_templates;
    renamed_templates.reserve(ast.templates.size());

    for (auto& tmpl : ast.templates)
    {
        const std::string original_name = tmpl.name;
        tmpl.name = prefix + "__" + original_name;
        renamed_templates.emplace(original_name, tmpl.name);
    }

    for (auto& service : ast.services)
    {
        for (auto& use : service.uses)
        {
            const auto it = renamed_templates.find(use.template_name);
            if (it != renamed_templates.end())
                use.template_name = it->second;
        }
    }

    std::unordered_map<std::string, std::string> renamed_services;
    renamed_services.reserve(ast.services.size());

    for (auto& service : ast.services)
    {
        const std::string original_name = service.name;
        service.name = prefix + "__" + original_name;
        renamed_services.emplace(original_name, service.name);
    }

    for (auto& service : ast.services)
    {
        for (auto& dependency : service.depends_on)
        {
            const auto it = renamed_services.find(dependency);
            if (it != renamed_services.end())
                dependency = it->second;
        }
    }

    return ast;
}

[[nodiscard]] std::vector<std::filesystem::path> collect_abs_files(const std::filesystem::path& input_dir,
                                                                    const std::regex& file_regex)
{
    if (!std::filesystem::exists(input_dir))
        throw std::runtime_error("Input directory does not exist: " + input_dir.string());

    if (!std::filesystem::is_directory(input_dir))
        throw std::runtime_error("Input path is not a directory: " + input_dir.string());

    std::vector<std::filesystem::path> files;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(input_dir))
    {
        if (!entry.is_regular_file())
            continue;

        const auto relative = std::filesystem::relative(entry.path(), input_dir).generic_string();
        if (std::regex_search(relative, file_regex))
            files.push_back(entry.path());
    }

    std::sort(files.begin(), files.end());
    return files;
}

[[nodiscard]] abstack::BuildPlan plan_from_ast(abstack::Ast ast,
                                                const std::optional<std::regex>& service_regex)
{
    abstack::Ast filtered = filter_services(std::move(ast), service_regex);
    if (filtered.services.empty())
        throw std::runtime_error("No services matched selection criteria");

    abstack::validate_ast(filtered);
    return abstack::lower_to_ir(filtered);
}

void clean_generated_outputs(const EmitOptions& options)
{
    if (!options.clean_output)
        return;

    if (!std::filesystem::exists(options.output_dir))
        return;

    for (const auto& entry : std::filesystem::directory_iterator(options.output_dir))
    {
        if (!entry.is_regular_file())
            continue;

        const std::string filename = entry.path().filename().string();
        if (filename.starts_with("Dockerfile."))
            std::filesystem::remove(entry.path());
    }

    const std::filesystem::path default_compose = options.output_dir / "docker-compose.generated.yml";
    if (!options.compose_path.has_value() && std::filesystem::exists(default_compose))
        std::filesystem::remove(default_compose);
}

[[nodiscard]] BuildResult emit_plan(const abstack::BuildPlan& plan,
                                    const EmitOptions& options,
                                    const bool verbose)
{
    clean_generated_outputs(options);
    std::filesystem::create_directories(options.output_dir);

    BuildResult result{};

    for (const auto& service : plan.services)
    {
        const auto dockerfile_path = options.output_dir / service.compose.dockerfile;
        write_file(dockerfile_path, abstack::emit_dockerfile(service));
        result.dockerfiles.push_back(dockerfile_path);

        if (verbose)
            std::cout << "Generated " << dockerfile_path << "\n";
    }

    result.compose_file =
        options.compose_path.value_or(options.output_dir / "docker-compose.generated.yml");
    write_file(result.compose_file, abstack::emit_compose(plan));

    if (verbose)
        std::cout << "Generated " << result.compose_file << "\n";

    return result;
}

[[nodiscard]] abstack::BuildPlan plan_from_file(const std::filesystem::path& input,
                                                const std::optional<std::regex>& service_regex)
{
    return plan_from_ast(parse_ast_file(input), service_regex);
}

[[nodiscard]] abstack::BuildPlan plan_from_sync_dir(const std::filesystem::path& input_dir,
                                                    const std::regex& file_regex,
                                                    const std::optional<std::regex>& service_regex,
                                                    const bool verbose)
{
    const auto files = collect_abs_files(input_dir, file_regex);
    if (files.empty())
    {
        throw std::runtime_error("No .abs files matched in directory: " + input_dir.string());
    }

    std::vector<abstack::Ast> asts;
    asts.reserve(files.size());

    for (const auto& file : files)
    {
        if (verbose)
            std::cout << "Sync source " << file << "\n";

        asts.push_back(namespace_templates(parse_ast_file(file), input_dir, file));
    }

    return plan_from_ast(merge_asts(std::move(asts)), service_regex);
}

[[nodiscard]] std::string shell_escape(const std::string& input)
{
    if (input.empty())
        return "''";

    std::string escaped = "'";
    for (const char c : input)
    {
        if (c == '\'')
            escaped += "'\"'\"'";
        else
            escaped.push_back(c);
    }
    escaped += "'";

    return escaped;
}

void print_help()
{
    std::cout
        << "abstack CLI\n"
        << "\n"
        << "Commands:\n"
        << "  build <file.abs> [--out-dir <dir>] [--compose-file <file>]\n"
        << "        [--service-regex <pattern>] [--clean] [--dry-run]\n"
        << "\n"
        << "  fmt <file-or-dir> [--file-regex <pattern>] [--check] [--stdout]\n"
        << "\n"
        << "  sync --input-dir <dir> [--file-regex <pattern>]\n"
        << "       [--service-regex <pattern>] [--out-dir <dir>]\n"
        << "       [--compose-file <file>] [--clean]\n"
        << "\n"
        << "  compose [--abs <file.abs> | --input-dir <dir>] [sync/build options]\n"
        << "          [--compose-file <file>] -- <docker compose args...>\n"
        << "\n"
        << "  tui   Launch optional curses UI (if enabled in this build).\n"
        << "\n"
        << "  help  Show this help message.\n"
        << "\n"
        << "Compatibility:\n"
        << "  abstack <file.abs> [options] is treated as build command.\n";
}

int handle_build(const std::vector<std::string>& args)
{
    EmitOptions emit{};
    std::optional<std::string> service_pattern;
    std::optional<std::filesystem::path> input;
    bool dry_run = false;

    for (std::size_t i = 0; i < args.size(); ++i)
    {
        const std::string& arg = args[i];

        if (arg == "--out-dir")
        {
            emit.output_dir = consume_option_value(args, i, arg);
            continue;
        }

        if (arg == "--compose-file")
        {
            emit.compose_path = consume_option_value(args, i, arg);
            continue;
        }

        if (arg == "--service-regex")
        {
            service_pattern = consume_option_value(args, i, arg);
            continue;
        }

        if (arg == "--clean")
        {
            emit.clean_output = true;
            continue;
        }

        if (arg == "--dry-run")
        {
            dry_run = true;
            continue;
        }

        if (!arg.empty() && arg.front() == '-')
            throw std::runtime_error("Unknown option for build: " + arg);

        if (input.has_value())
            throw std::runtime_error("build accepts exactly one input file");

        input = std::filesystem::path(arg);
    }

    if (!input.has_value())
        throw std::runtime_error("build requires an input .abs file");

    const auto service_regex = compile_optional_regex(service_pattern, "--service-regex");
    const auto plan = plan_from_file(*input, service_regex);

    if (dry_run)
    {
        std::cout << "Dry run successful: " << plan.services.size() << " service(s)\n";
        std::cout << abstack::emit_compose(plan);
        return 0;
    }

    [[maybe_unused]] const auto build_result = emit_plan(plan, emit, true);
    return 0;
}

int handle_sync(const std::vector<std::string>& args)
{
    EmitOptions emit{};
    std::optional<std::string> service_pattern;
    std::string file_pattern = R"(.*\.abs$)";
    std::optional<std::filesystem::path> input_dir;

    for (std::size_t i = 0; i < args.size(); ++i)
    {
        const std::string& arg = args[i];

        if (arg == "--input-dir")
        {
            input_dir = consume_option_value(args, i, arg);
            continue;
        }

        if (arg == "--file-regex")
        {
            file_pattern = consume_option_value(args, i, arg);
            continue;
        }

        if (arg == "--service-regex")
        {
            service_pattern = consume_option_value(args, i, arg);
            continue;
        }

        if (arg == "--out-dir")
        {
            emit.output_dir = consume_option_value(args, i, arg);
            continue;
        }

        if (arg == "--compose-file")
        {
            emit.compose_path = consume_option_value(args, i, arg);
            continue;
        }

        if (arg == "--clean")
        {
            emit.clean_output = true;
            continue;
        }

        throw std::runtime_error("Unknown option for sync: " + arg);
    }

    if (!input_dir.has_value())
        throw std::runtime_error("sync requires --input-dir");

    const auto file_regex = compile_regex(file_pattern, "--file-regex");
    const auto service_regex = compile_optional_regex(service_pattern, "--service-regex");

    const auto plan = plan_from_sync_dir(*input_dir, file_regex, service_regex, true);
    [[maybe_unused]] const auto build_result = emit_plan(plan, emit, true);
    return 0;
}

int handle_fmt(const std::vector<std::string>& args)
{
    std::optional<std::filesystem::path> target;
    std::string file_pattern = R"(.*\.abs$)";
    bool check = false;
    bool stdout_only = false;

    for (std::size_t i = 0; i < args.size(); ++i)
    {
        const std::string& arg = args[i];

        if (arg == "--file-regex")
        {
            file_pattern = consume_option_value(args, i, arg);
            continue;
        }

        if (arg == "--check")
        {
            check = true;
            continue;
        }

        if (arg == "--stdout")
        {
            stdout_only = true;
            continue;
        }

        if (!arg.empty() && arg.front() == '-')
            throw std::runtime_error("Unknown option for fmt: " + arg);

        if (target.has_value())
            throw std::runtime_error("fmt accepts one file or directory target");

        target = std::filesystem::path(arg);
    }

    if (!target.has_value())
        throw std::runtime_error("fmt requires a file or directory path");

    const auto format_one = [&](const std::filesystem::path& file,
                                const bool allow_stdout) -> bool {
        const std::string original = read_file(file);
        const std::string formatted = abstack::format_ast(parse_ast_file(file));
        const bool changed = (original != formatted);

        if (check)
        {
            if (changed)
                std::cout << "Needs formatting: " << file << "\n";
            return changed;
        }

        if (stdout_only)
        {
            if (!allow_stdout)
                throw std::runtime_error("--stdout is only supported for a single file target");

            std::cout << formatted;
            return changed;
        }

        if (changed)
        {
            write_file(file, formatted);
            std::cout << "Formatted " << file << "\n";
        }

        return changed;
    };

    if (std::filesystem::is_regular_file(*target))
    {
        const bool changed = format_one(*target, true);
        return (check && changed) ? 1 : 0;
    }

    if (!std::filesystem::is_directory(*target))
        throw std::runtime_error("fmt target must be a file or directory");

    const auto file_regex = compile_regex(file_pattern, "--file-regex");
    const auto files = collect_abs_files(*target, file_regex);

    std::size_t changed_count = 0;
    for (const auto& file : files)
    {
        if (format_one(file, false))
            ++changed_count;
    }

    if (files.empty())
        std::cout << "No files matched for formatting\n";

    if (check)
    {
        if (changed_count > 0)
        {
            std::cout << changed_count << " file(s) require formatting\n";
            return 1;
        }

        std::cout << "Formatting check passed\n";
        return 0;
    }

    std::cout << "Formatting completed";
    if (!stdout_only)
        std::cout << ": " << changed_count << " file(s) updated";
    std::cout << "\n";

    return 0;
}

int handle_compose(const std::vector<std::string>& args)
{
    EmitOptions emit{};
    std::optional<std::filesystem::path> abs_input;
    std::optional<std::filesystem::path> sync_input_dir;
    std::optional<std::string> service_pattern;
    std::string file_pattern = R"(.*\.abs$)";
    std::vector<std::string> compose_args;
    bool after_separator = false;

    for (std::size_t i = 0; i < args.size(); ++i)
    {
        const std::string& arg = args[i];

        if (after_separator)
        {
            compose_args.push_back(arg);
            continue;
        }

        if (arg == "--")
        {
            after_separator = true;
            continue;
        }

        if (arg == "--abs")
        {
            abs_input = consume_option_value(args, i, arg);
            continue;
        }

        if (arg == "--input-dir")
        {
            sync_input_dir = consume_option_value(args, i, arg);
            continue;
        }

        if (arg == "--file-regex")
        {
            file_pattern = consume_option_value(args, i, arg);
            continue;
        }

        if (arg == "--service-regex")
        {
            service_pattern = consume_option_value(args, i, arg);
            continue;
        }

        if (arg == "--out-dir")
        {
            emit.output_dir = consume_option_value(args, i, arg);
            continue;
        }

        if (arg == "--compose-file")
        {
            emit.compose_path = consume_option_value(args, i, arg);
            continue;
        }

        if (arg == "--clean")
        {
            emit.clean_output = true;
            continue;
        }

        throw std::runtime_error("Unknown option for compose: " + arg);
    }

    if (compose_args.empty())
    {
        throw std::runtime_error(
            "compose requires docker compose arguments after `--` (example: -- up -d)");
    }

    if (abs_input.has_value() && sync_input_dir.has_value())
        throw std::runtime_error("compose accepts either --abs or --input-dir, not both");

    const auto service_regex = compile_optional_regex(service_pattern, "--service-regex");

    if (abs_input.has_value())
    {
        const auto plan = plan_from_file(*abs_input, service_regex);
        [[maybe_unused]] const auto build_result = emit_plan(plan, emit, true);
    }
    else if (sync_input_dir.has_value())
    {
        const auto file_regex = compile_regex(file_pattern, "--file-regex");
        const auto plan = plan_from_sync_dir(*sync_input_dir, file_regex, service_regex, true);
        [[maybe_unused]] const auto build_result = emit_plan(plan, emit, true);
    }

    const std::filesystem::path compose_path =
        emit.compose_path.value_or(emit.output_dir / "docker-compose.generated.yml");

    if (!std::filesystem::exists(compose_path))
    {
        throw std::runtime_error("Compose file not found: " + compose_path.string() +
                                 ". Generate it first with build/sync or pass --abs/--input-dir.");
    }

    std::string command = "docker compose -f " + shell_escape(compose_path.string());
    for (const auto& value : compose_args)
        command += " " + shell_escape(value);

    std::cout << "Running: " << command << "\n";
    const int status = std::system(command.c_str());
    if (status != 0)
        return status;

    return 0;
}

#ifdef ABSTACK_HAS_CURSES
[[nodiscard]] std::string tui_prompt(const std::string& label)
{
    echo();
    curs_set(1);

    char buffer[512]{};

    move(LINES - 4, 0);
    clrtoeol();
    mvprintw(LINES - 4, 0, "%s", label.c_str());

    move(LINES - 3, 0);
    clrtoeol();
    getnstr(buffer, 511);

    noecho();
    curs_set(0);
    return std::string(buffer);
}
#endif

int handle_tui()
{
#ifdef ABSTACK_HAS_CURSES
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    std::string status = "Ready";
    bool running = true;

    while (running)
    {
        clear();
        mvprintw(0, 0, "abstack TUI");
        mvprintw(2, 0, "b: build file");
        mvprintw(3, 0, "f: format file");
        mvprintw(4, 0, "s: sync directory");
        mvprintw(5, 0, "q: quit");
        mvprintw(7, 0, "Status: %s", status.c_str());
        refresh();

        const int key = getch();

        try
        {
            if (key == 'q')
            {
                running = false;
                continue;
            }

            if (key == 'b')
            {
                const std::string input = tui_prompt("Build input file (.abs):");
                if (!input.empty())
                {
                    const auto plan = plan_from_file(input, std::nullopt);
                    const auto result = emit_plan(plan, EmitOptions{}, false);
                    status = "Built compose: " + result.compose_file.string();
                }
                continue;
            }

            if (key == 'f')
            {
                const std::string input = tui_prompt("Format file (.abs):");
                if (!input.empty())
                {
                    const std::filesystem::path file = input;
                    write_file(file, abstack::format_ast(parse_ast_file(file)));
                    status = "Formatted " + file.string();
                }
                continue;
            }

            if (key == 's')
            {
                const std::string input_dir = tui_prompt("Sync input directory:");
                if (!input_dir.empty())
                {
                    const std::string file_pattern = tui_prompt("File regex (empty for .*\\.abs$):");
                    const std::regex file_regex = compile_regex(
                        file_pattern.empty() ? R"(.*\.abs$)" : file_pattern,
                        "tui sync file regex");

                    const auto plan =
                        plan_from_sync_dir(input_dir, file_regex, std::nullopt, false);
                    const auto result = emit_plan(plan, EmitOptions{}, false);
                    status = "Synced compose: " + result.compose_file.string();
                }
                continue;
            }

            status = "Unknown key";
        }
        catch (const std::exception& error)
        {
            status = error.what();
        }
    }

    endwin();
    return 0;
#else
    throw std::runtime_error(
        "This build does not include TUI support. Reconfigure with ABSTACK_ENABLE_TUI=ON and install curses.");
#endif
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const std::vector<std::string> args(argv + 1, argv + argc);

        if (args.empty())
        {
            print_help();
            return 0;
        }

        const std::string& command = args.front();
        const std::vector<std::string> subargs(args.begin() + 1, args.end());

        if (command == "help" || command == "--help" || command == "-h")
        {
            print_help();
            return 0;
        }

        if (command == "build")
            return handle_build(subargs);

        if (command == "fmt")
            return handle_fmt(subargs);

        if (command == "sync")
            return handle_sync(subargs);

        if (command == "compose")
            return handle_compose(subargs);

        if (command == "tui")
            return handle_tui();

        // Backward compatibility: treat first positional argument as build input.
        return handle_build(args);
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
