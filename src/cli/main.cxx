#include "abstack/codegen/compose_emitter.hxx"
#include "abstack/codegen/dockerfile_emitter.hxx"
#include "abstack/format/formatter.hxx"
#include "abstack/frontend/lexer.hxx"
#include "abstack/frontend/parser.hxx"
#include "abstack/ir/lowering.hxx"
#include "abstack/semantic/validator.hxx"
#include "abstack/stdlib/library.hxx"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

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

struct StdlibOptions
{
    std::optional<std::string> profile;
    bool list_profiles = false;
};

struct BuildResult
{
    std::vector<std::filesystem::path> dockerfiles;
    std::filesystem::path compose_file;
};

struct CommandOutput
{
    int exit_code = 0;
    std::string output;
};

struct DockerContainerRow
{
    std::string id;
    std::string name;
    std::string image;
    std::string status;
    std::string running_for;
    std::string ports;
};

enum class CallbackLevel
{
    Info,
    Error
};

struct CallbackEvent
{
    std::string topic;
    std::string message;
    CallbackLevel level = CallbackLevel::Info;
};

using Callback = std::function<void(const CallbackEvent&)>;

[[nodiscard]] std::string now_timestamp()
{
    const std::time_t now = std::time(nullptr);
    std::tm tm_parts{};
#ifdef _WIN32
    localtime_s(&tm_parts, &now);
#else
    localtime_r(&now, &tm_parts);
#endif

    std::ostringstream out;
    out << std::put_time(&tm_parts, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

[[nodiscard]] std::filesystem::path default_log_path()
{
    return ".abstack/logs/abstack-cli.log";
}

class CallbackRegistry
{
public:
    CallbackRegistry() : callback_(default_callback)
    {
    }

    void emit(const std::string& topic, const std::string& message, const CallbackLevel level)
    {
        const CallbackEvent event{.topic = topic, .message = message, .level = level};

        callback_(event);
        try
        {
            append_log(event);
        }
        catch (const std::exception&)
        {
            // Logging failures should never break primary CLI operations.
        }
    }

private:
    static void default_callback(const CallbackEvent& event)
    {
        std::ostream& stream = (event.level == CallbackLevel::Error) ? std::cerr : std::cout;
        stream << "[" << event.topic << "] " << event.message << "\n";
    }

    void append_log(const CallbackEvent& event)
    {
        std::lock_guard<std::mutex> lock(log_mutex_);

        if (!log_stream_.is_open())
        {
            const std::filesystem::path path = default_log_path();
            std::filesystem::create_directories(path.parent_path());
            log_stream_.open(path, std::ios::app);
        }

        if (!log_stream_.is_open())
            return;

        const std::string level = (event.level == CallbackLevel::Error) ? "ERROR" : "INFO";
        log_stream_ << now_timestamp() << " [" << level << "] [" << event.topic
                    << "] " << event.message << "\n";
        log_stream_.flush();
    }

    Callback callback_;
    std::ofstream log_stream_;
    std::mutex log_mutex_;
};

[[nodiscard]] CallbackRegistry& callback_registry()
{
    static CallbackRegistry registry;
    return registry;
}

void callback_info(const std::string& topic, const std::string& message)
{
    callback_registry().emit(topic, message, CallbackLevel::Info);
}

void callback_error(const std::string& topic, const std::string& message)
{
    callback_registry().emit(topic, message, CallbackLevel::Error);
}

[[nodiscard]] bool spinner_enabled()
{
    if (const char* no_spinner = std::getenv("ABSTACK_NO_SPINNER");
        no_spinner != nullptr && std::string_view(no_spinner) == "1")
    {
        return false;
    }

#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

class Spinner
{
public:
    Spinner(std::string label, const bool enabled) : label_(std::move(label)), enabled_(enabled)
    {
        if (!enabled_)
            return;

        running_.store(true);
        worker_ = std::thread([this]() {
            static constexpr std::array<char, 4> kFrames{'|', '/', '-', '\\'};
            std::size_t frame_index = 0;

            while (running_.load())
            {
                std::cout << "\r" << label_ << " " << kFrames[frame_index % kFrames.size()]
                          << std::flush;
                ++frame_index;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    Spinner(const Spinner&) = delete;
    Spinner& operator=(const Spinner&) = delete;

    ~Spinner()
    {
        finish(true, "");
    }

    void finish(const bool success, const std::string& detail)
    {
        if (!enabled_)
            return;

        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false))
            return;

        if (worker_.joinable())
            worker_.join();

        std::cout << "\r" << label_ << " " << (success ? "done" : "failed");
        if (!detail.empty())
            std::cout << " " << detail;
        std::cout << "          \n";
        std::cout.flush();
    }

private:
    std::string label_;
    bool enabled_ = false;
    std::atomic<bool> running_{false};
    std::thread worker_;
};

[[nodiscard]] int normalize_exit_code(const int status)
{
#ifdef _WIN32
    return status;
#else
    if (status == -1)
        return status;

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    return status;
#endif
}

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

[[nodiscard]] int parse_positive_int(const std::string& value, const std::string& option)
{
    int parsed = 0;
    try
    {
        parsed = std::stoi(value);
    }
    catch (const std::exception&)
    {
        throw std::runtime_error(option + " must be a positive integer");
    }

    if (parsed <= 0)
        throw std::runtime_error(option + " must be a positive integer");

    return parsed;
}

[[nodiscard]] CommandOutput run_captured_command(const std::string& command,
                                                 const std::string& activity_label)
{
    callback_info("exec", "Running command: " + command);
    const std::string wrapped = command + " 2>&1";

#ifdef _WIN32
    FILE* pipe = _popen(wrapped.c_str(), "r");
#else
    FILE* pipe = popen(wrapped.c_str(), "r");
#endif

    if (pipe == nullptr)
        throw std::runtime_error("Failed to execute command: " + command);

    Spinner spinner(activity_label, spinner_enabled());

    std::string output;
    char buffer[512]{};
    while (fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != nullptr)
        output += buffer;

#ifdef _WIN32
    const int status = _pclose(pipe);
#else
    const int status = pclose(pipe);
#endif

    const int exit_code = normalize_exit_code(status);
    spinner.finish(exit_code == 0, "(exit " + std::to_string(exit_code) + ")");

    if (exit_code == 0)
        callback_info("exec", "Command finished successfully");
    else
        callback_error("exec", "Command failed with exit code " + std::to_string(exit_code));

    return CommandOutput{.exit_code = exit_code, .output = std::move(output)};
}

[[nodiscard]] std::string truncate_cell(std::string value, const std::size_t max_width)
{
    if (value.size() <= max_width)
        return value;

    if (max_width <= 3)
        return value.substr(0, max_width);

    value.resize(max_width - 3);
    value += "...";
    return value;
}

[[nodiscard]] std::vector<std::string> split_tab_columns(const std::string& line)
{
    std::vector<std::string> columns;
    std::size_t start = 0;

    while (start <= line.size())
    {
        const std::size_t tab = line.find('\t', start);
        if (tab == std::string::npos)
        {
            columns.push_back(line.substr(start));
            break;
        }

        columns.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }

    return columns;
}

[[nodiscard]] std::vector<DockerContainerRow> parse_docker_ps_rows(const std::string& output)
{
    std::vector<DockerContainerRow> rows;
    std::istringstream input(output);
    std::string line;

    while (std::getline(input, line))
    {
        if (line.empty())
            continue;

        const auto columns = split_tab_columns(line);
        if (columns.size() < 6)
            continue;

        rows.push_back(DockerContainerRow{
            .id = columns[0],
            .name = columns[1],
            .image = columns[2],
            .status = columns[3],
            .running_for = columns[4],
            .ports = columns[5],
        });
    }

    return rows;
}

void print_docker_table(const std::vector<DockerContainerRow>& rows)
{
    constexpr std::size_t kIdWidth = 12;
    constexpr std::size_t kNameWidth = 24;
    constexpr std::size_t kImageWidth = 28;
    constexpr std::size_t kStatusWidth = 24;
    constexpr std::size_t kAgeWidth = 14;
    constexpr std::size_t kPortsWidth = 32;

    std::cout << std::left << std::setw(static_cast<int>(kIdWidth)) << "ID"
              << "  " << std::setw(static_cast<int>(kNameWidth)) << "NAME"
              << "  " << std::setw(static_cast<int>(kImageWidth)) << "IMAGE"
              << "  " << std::setw(static_cast<int>(kStatusWidth)) << "STATUS"
              << "  " << std::setw(static_cast<int>(kAgeWidth)) << "AGE"
              << "  " << "PORTS" << "\n";

    std::cout << std::string(kIdWidth + kNameWidth + kImageWidth + kStatusWidth + kAgeWidth +
                                 kPortsWidth + 10,
                             '-')
              << "\n";

    for (const auto& row : rows)
    {
        std::cout << std::left << std::setw(static_cast<int>(kIdWidth))
                  << truncate_cell(row.id, kIdWidth) << "  "
                  << std::setw(static_cast<int>(kNameWidth))
                  << truncate_cell(row.name, kNameWidth) << "  "
                  << std::setw(static_cast<int>(kImageWidth))
                  << truncate_cell(row.image, kImageWidth) << "  "
                  << std::setw(static_cast<int>(kStatusWidth))
                  << truncate_cell(row.status, kStatusWidth) << "  "
                  << std::setw(static_cast<int>(kAgeWidth))
                  << truncate_cell(row.running_for, kAgeWidth) << "  "
                  << truncate_cell(row.ports, kPortsWidth) << "\n";
    }
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

[[nodiscard]] abstack::Ast link_stdlib(abstack::Ast ast,
                                       const std::optional<std::string>& stdlib_profile)
{
    if (!stdlib_profile.has_value())
        return ast;

    const auto source = abstack::stdlib_profile_source(*stdlib_profile);
    if (!source.has_value())
    {
        throw std::runtime_error("Unknown stdlib profile `" + *stdlib_profile +
                                 "`. Use --list-stdlib-profiles to view available profiles.");
    }

    abstack::Ast stdlib_ast =
        parse_ast_source(std::string(*source), "stdlib profile `" + *stdlib_profile + "`");

    std::vector<abstack::Ast> asts;
    asts.reserve(2);
    asts.push_back(std::move(stdlib_ast));
    asts.push_back(std::move(ast));
    return merge_asts(std::move(asts));
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

// Sync mode merges many .abs files into one plan, so each file gets a stable
// per-path namespace to avoid template/service name collisions after merging.
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
                                                const std::optional<std::string>& stdlib_profile,
                                                const std::optional<std::regex>& service_regex)
{
    return plan_from_ast(link_stdlib(parse_ast_file(input), stdlib_profile), service_regex);
}

[[nodiscard]] abstack::BuildPlan plan_from_sync_dir(const std::filesystem::path& input_dir,
                                                    const std::regex& file_regex,
                                                    const std::optional<std::string>& stdlib_profile,
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

    return plan_from_ast(link_stdlib(merge_asts(std::move(asts)), stdlib_profile), service_regex);
}

void print_stdlib_profiles()
{
    const auto profiles = abstack::stdlib_profiles();

    std::cout << "Bundled stdlib profiles:\n";
    for (const auto& profile : profiles)
        std::cout << "  - " << profile.name << ": " << profile.description << "\n";
}

[[nodiscard]] bool consume_stdlib_option(const std::vector<std::string>& args,
                                         std::size_t& index,
                                         const std::string& arg,
                                         StdlibOptions& stdlib)
{
    if (arg == "--stdlib-profile")
    {
        stdlib.profile = consume_option_value(args, index, arg);
        return true;
    }

    if (arg == "--list-stdlib-profiles")
    {
        stdlib.list_profiles = true;
        return true;
    }

    return false;
}

// Shell-facing helpers build command strings one argument at a time, then
// quote each piece here so container names, users, and compose args stay safe.
[[nodiscard]] std::string shell_escape(const std::string& input)
{
#ifdef _WIN32
    std::string escaped = "\"";
    for (const char c : input)
    {
        if (c == '"')
            escaped += "\\\"";
        else if (c == '%')
            escaped += "%%";
        else
            escaped.push_back(c);
    }
    escaped += "\"";
    return escaped;
#else
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
#endif
}

[[nodiscard]] int run_system_command(const std::string& command,
                                     const std::string& activity_label,
                                     const bool interactive)
{
    callback_info("exec", "Running command: " + command);
    Spinner spinner(activity_label, spinner_enabled() && !interactive);

    const int status = std::system(command.c_str());
    const int exit_code = normalize_exit_code(status);

    spinner.finish(exit_code == 0, "(exit " + std::to_string(exit_code) + ")");
    if (exit_code == 0)
        callback_info("exec", "Command finished successfully");
    else
        callback_error("exec", "Command failed with exit code " + std::to_string(exit_code));

    return exit_code;
}

void print_help()
{
    std::cout
        << "abstack CLI\n"
        << "\n"
        << "Commands:\n"
        << "  build <file.abs> [--out-dir <dir>] [--compose-file <file>]\n"
        << "        [--service-regex <pattern>] [--clean] [--dry-run]\n"
        << "        [--stdlib-profile <name>] [--list-stdlib-profiles]\n"
        << "\n"
        << "  fmt <file-or-dir> [--file-regex <pattern>] [--check] [--stdout]\n"
        << "\n"
        << "  sync --input-dir <dir> [--file-regex <pattern>]\n"
        << "       [--service-regex <pattern>] [--out-dir <dir>]\n"
        << "       [--compose-file <file>] [--clean]\n"
        << "       [--stdlib-profile <name>] [--list-stdlib-profiles]\n"
        << "\n"
        << "  compose [--abs <file.abs> | --input-dir <dir>] [sync/build options]\n"
        << "          [--compose-file <file>] [--stdlib-profile <name>]\n"
        << "          [--list-stdlib-profiles] -- <docker compose args...>\n"
        << "\n"
        << "  docker <ls|inspect|logs|shell|stats> [options]\n"
        << "         Lightweight container operations helper.\n"
        << "\n"
        << "  stdlib [list]  Show bundled stdlib profiles.\n"
        << "\n"
        << "  tui   Launch optional curses UI (if enabled in this build).\n"
        << "\n"
        << "  help  Show this help message.\n"
        << "\n"
        << "Compatibility:\n"
        << "  abstack <file.abs> [options] is treated as build command.\n"
        << "\n"
        << "Observability:\n"
        << "  callback events are logged to .abstack/logs/abstack-cli.log\n"
        << "  set ABSTACK_NO_SPINNER=1 to disable spinner output.\n";
}

void print_docker_help()
{
    std::cout
        << "abstack docker command suite\n"
        << "\n"
        << "Subcommands:\n"
        << "  docker ls [--all] [--filter <regex>] [--watch <seconds>]\n"
        << "  docker inspect <container>\n"
        << "  docker logs <container> [--tail <lines>] [--follow]\n"
        << "  docker shell <container> [--shell <command>] [--user <user>]\n"
        << "  docker stats [--all]\n";
}

// Docker subcommands are intentionally thin wrappers: keep CLI parsing small,
// then delegate to docker itself for the actual behavior.
int handle_docker_ls(const std::vector<std::string>& args)
{
    bool include_all = false;
    int watch_seconds = 0;
    std::optional<std::string> filter_pattern;

    for (std::size_t i = 0; i < args.size(); ++i)
    {
        const std::string& arg = args[i];

        if (arg == "--all")
        {
            include_all = true;
            continue;
        }

        if (arg == "--watch")
        {
            watch_seconds = parse_positive_int(consume_option_value(args, i, arg), arg);
            continue;
        }

        if (arg == "--filter")
        {
            filter_pattern = consume_option_value(args, i, arg);
            continue;
        }

        throw std::runtime_error("Unknown option for docker ls: " + arg);
    }

    const auto filter_regex = compile_optional_regex(filter_pattern, "--filter");

    const std::string format =
        "{{.ID}}\\t{{.Names}}\\t{{.Image}}\\t{{.Status}}\\t{{.RunningFor}}\\t{{.Ports}}";
    std::string command = "docker ps " + std::string(include_all ? "-a " : "") + "--no-trunc ";
    command += "--format " + shell_escape(format);

    do
    {
        const auto result = run_captured_command(command, "Querying docker containers");
        if (result.exit_code != 0)
            throw std::runtime_error("docker ps failed:\n" + result.output);

        auto rows = parse_docker_ps_rows(result.output);
        if (filter_regex.has_value())
        {
            std::vector<DockerContainerRow> filtered;
            filtered.reserve(rows.size());

            for (const auto& row : rows)
            {
                if (std::regex_search(row.id, *filter_regex) ||
                    std::regex_search(row.name, *filter_regex) ||
                    std::regex_search(row.image, *filter_regex) ||
                    std::regex_search(row.status, *filter_regex))
                {
                    filtered.push_back(row);
                }
            }

            rows = std::move(filtered);
        }

        std::sort(rows.begin(),
                  rows.end(),
                  [](const DockerContainerRow& a, const DockerContainerRow& b) {
                      return a.name < b.name;
                  });

        if (watch_seconds > 0)
            std::cout << "\x1b[2J\x1b[H";

        print_docker_table(rows);
        std::cout << "\n" << rows.size() << " container(s) listed\n";

        if (watch_seconds <= 0)
            break;

        std::cout << "Refreshing every " << watch_seconds << " second(s). Ctrl+C to stop.\n";
        std::this_thread::sleep_for(std::chrono::seconds(watch_seconds));
    } while (true);

    return 0;
}

int handle_docker_inspect(const std::vector<std::string>& args)
{
    if (args.size() != 1)
        throw std::runtime_error("docker inspect requires exactly one container identifier");

    const auto result =
        run_captured_command("docker inspect " + shell_escape(args.front()),
                             "Inspecting container");
    if (result.exit_code != 0)
        throw std::runtime_error("docker inspect failed:\n" + result.output);

    std::cout << result.output;
    return 0;
}

int handle_docker_logs(const std::vector<std::string>& args)
{
    std::optional<std::string> container;
    int tail_lines = 200;
    bool follow = false;

    for (std::size_t i = 0; i < args.size(); ++i)
    {
        const std::string& arg = args[i];

        if (arg == "--tail")
        {
            tail_lines = parse_positive_int(consume_option_value(args, i, arg), arg);
            continue;
        }

        if (arg == "--follow")
        {
            follow = true;
            continue;
        }

        if (!arg.empty() && arg.front() == '-')
            throw std::runtime_error("Unknown option for docker logs: " + arg);

        if (container.has_value())
            throw std::runtime_error("docker logs accepts exactly one container identifier");

        container = arg;
    }

    if (!container.has_value())
        throw std::runtime_error("docker logs requires a container identifier");

    std::string command = "docker logs --tail " + std::to_string(tail_lines) + " ";
    if (follow)
        command += "--follow ";

    command += shell_escape(*container);
    return run_system_command(command, "Streaming docker logs", follow);
}

int handle_docker_shell(const std::vector<std::string>& args)
{
    std::optional<std::string> container;
    std::string shell = "sh";
    std::optional<std::string> user;

    for (std::size_t i = 0; i < args.size(); ++i)
    {
        const std::string& arg = args[i];

        if (arg == "--shell")
        {
            shell = consume_option_value(args, i, arg);
            continue;
        }

        if (arg == "--user")
        {
            user = consume_option_value(args, i, arg);
            continue;
        }

        if (!arg.empty() && arg.front() == '-')
            throw std::runtime_error("Unknown option for docker shell: " + arg);

        if (container.has_value())
            throw std::runtime_error("docker shell accepts exactly one container identifier");

        container = arg;
    }

    if (!container.has_value())
        throw std::runtime_error("docker shell requires a container identifier");

    std::string command = "docker exec -it ";
    if (user.has_value())
        command += "--user " + shell_escape(*user) + " ";

    command += shell_escape(*container) + " " + shell_escape(shell);
    return run_system_command(command, "Opening container shell", true);
}

int handle_docker_stats(const std::vector<std::string>& args)
{
    bool include_all = false;

    for (const auto& arg : args)
    {
        if (arg == "--all")
        {
            include_all = true;
            continue;
        }

        throw std::runtime_error("Unknown option for docker stats: " + arg);
    }

    std::string command = "docker stats --no-stream ";
    if (include_all)
        command += "--all";

    return run_system_command(command, "Collecting docker stats", false);
}

int handle_docker(const std::vector<std::string>& args)
{
    callback_info("docker", "Starting docker helper command");

    if (args.empty() || args.front() == "help" || args.front() == "--help" || args.front() == "-h")
    {
        print_docker_help();
        return 0;
    }

    const std::string& command = args.front();
    const std::vector<std::string> subargs(args.begin() + 1, args.end());

    if (command == "ls")
        return handle_docker_ls(subargs);

    if (command == "inspect")
        return handle_docker_inspect(subargs);

    if (command == "logs")
        return handle_docker_logs(subargs);

    if (command == "shell")
        return handle_docker_shell(subargs);

    if (command == "stats")
        return handle_docker_stats(subargs);

    throw std::runtime_error("Unknown docker subcommand: " + command);
}

int handle_build(const std::vector<std::string>& args)
{
    callback_info("build", "Starting build command");
    EmitOptions emit{};
    StdlibOptions stdlib{};
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

        if (consume_stdlib_option(args, i, arg, stdlib))
            continue;

        if (!arg.empty() && arg.front() == '-')
            throw std::runtime_error("Unknown option for build: " + arg);

        if (input.has_value())
            throw std::runtime_error("build accepts exactly one input file");

        input = std::filesystem::path(arg);
    }

    if (stdlib.list_profiles)
    {
        print_stdlib_profiles();
        callback_info("build", "Listed stdlib profiles");
        return 0;
    }

    if (!input.has_value())
        throw std::runtime_error("build requires an input .abs file");

    const auto service_regex = compile_optional_regex(service_pattern, "--service-regex");
    const auto plan = plan_from_file(*input, stdlib.profile, service_regex);

    if (dry_run)
    {
        std::cout << "Dry run successful: " << plan.services.size() << " service(s)\n";
        std::cout << abstack::emit_compose(plan);
        callback_info("build", "Build dry-run completed");
        return 0;
    }

    [[maybe_unused]] const auto build_result = emit_plan(plan, emit, true);
    callback_info("build", "Build command completed");
    return 0;
}

int handle_sync(const std::vector<std::string>& args)
{
    callback_info("sync", "Starting sync command");
    EmitOptions emit{};
    StdlibOptions stdlib{};
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

        if (consume_stdlib_option(args, i, arg, stdlib))
            continue;

        throw std::runtime_error("Unknown option for sync: " + arg);
    }

    if (stdlib.list_profiles)
    {
        print_stdlib_profiles();
        callback_info("sync", "Listed stdlib profiles");
        return 0;
    }

    if (!input_dir.has_value())
        throw std::runtime_error("sync requires --input-dir");

    const auto file_regex = compile_regex(file_pattern, "--file-regex");
    const auto service_regex = compile_optional_regex(service_pattern, "--service-regex");

    const auto plan = plan_from_sync_dir(*input_dir, file_regex, stdlib.profile, service_regex, true);
    [[maybe_unused]] const auto build_result = emit_plan(plan, emit, true);
    callback_info("sync", "Sync command completed");
    return 0;
}

int handle_fmt(const std::vector<std::string>& args)
{
    callback_info("fmt", "Starting format command");
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

    callback_info("fmt", "Format command completed");
    return 0;
}

// Compose is a two-stage command: first materialize the requested build/sync
// inputs, then forward the remaining argv tail to `docker compose`.
int handle_compose(const std::vector<std::string>& args)
{
    callback_info("compose", "Starting compose command");
    EmitOptions emit{};
    StdlibOptions stdlib{};
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
            // Everything after `--` is passed through unchanged to docker compose.
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

        if (consume_stdlib_option(args, i, arg, stdlib))
            continue;

        throw std::runtime_error("Unknown option for compose: " + arg);
    }

    if (stdlib.list_profiles)
    {
        print_stdlib_profiles();
        callback_info("compose", "Listed stdlib profiles");
        return 0;
    }

    if (compose_args.empty())
    {
        throw std::runtime_error(
            "compose requires docker compose arguments after `--` (example: -- up -d)");
    }

    if (abs_input.has_value() && sync_input_dir.has_value())
        throw std::runtime_error("compose accepts either --abs or --input-dir, not both");

    if (stdlib.profile.has_value() && !abs_input.has_value() && !sync_input_dir.has_value())
    {
        throw std::runtime_error(
            "--stdlib-profile requires --abs or --input-dir in compose mode");
    }

    const auto service_regex = compile_optional_regex(service_pattern, "--service-regex");

    if (abs_input.has_value())
    {
        const auto plan = plan_from_file(*abs_input, stdlib.profile, service_regex);
        [[maybe_unused]] const auto build_result = emit_plan(plan, emit, true);
    }
    else if (sync_input_dir.has_value())
    {
        const auto file_regex = compile_regex(file_pattern, "--file-regex");
        const auto plan =
            plan_from_sync_dir(*sync_input_dir, file_regex, stdlib.profile, service_regex, true);
        [[maybe_unused]] const auto build_result = emit_plan(plan, emit, true);
    }

    const std::filesystem::path compose_path =
        emit.compose_path.value_or(emit.output_dir / "docker-compose.generated.yml");

    if (!std::filesystem::exists(compose_path))
    {
        throw std::runtime_error("Compose file not found: " + compose_path.string() +
                                 ". Generate it first with build/sync or pass --abs/--input-dir.");
    }

    // Compose is assembled as a shell command only after each argument has
    // been quoted, so the generated file path and passthrough args stay intact.
    std::string command = "docker compose -f " + shell_escape(compose_path.string());
    for (const auto& value : compose_args)
        command += " " + shell_escape(value);

    std::cout << "Running: " << command << "\n";
    const int status = run_system_command(command, "Running docker compose", true);
    if (status != 0)
        return status;

    callback_info("compose", "Compose command completed");
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
    // The TUI is optional at build time; the command remains available, but the
    // implementation is only compiled when curses support is present.
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
                    const auto plan = plan_from_file(input, std::nullopt, std::nullopt);
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
                        plan_from_sync_dir(input_dir, file_regex, std::nullopt, std::nullopt, false);
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
    // Keep the command wired up even in non-TUI builds so the user gets a clear
    // runtime message instead of an accidental silent no-op.
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

        if (command == "docker")
            return handle_docker(subargs);

        if (command == "stdlib")
        {
            if (subargs.empty() || subargs.front() == "list" || subargs.front() == "--help" ||
                subargs.front() == "-h")
            {
                print_stdlib_profiles();
                callback_info("stdlib", "Listed bundled stdlib profiles");
                return 0;
            }

            throw std::runtime_error("Unknown stdlib subcommand: " + subargs.front());
        }

        if (command == "tui")
            return handle_tui();

        // Backward compatibility: treat first positional argument as build input.
        return handle_build(args);
    }
    catch (const std::exception& error)
    {
        callback_error("fatal", error.what());
        return 1;
    }
}
