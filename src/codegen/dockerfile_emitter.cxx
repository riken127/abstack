#include "abstack/codegen/dockerfile_emitter.hxx"

#include <sstream>
#include <type_traits>

namespace abstack
{

namespace
{
[[nodiscard]] std::string json_quote(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');

    for (const char c : value)
    {
        if (c == '"' || c == '\\')
            escaped.push_back('\\');
        escaped.push_back(c);
    }

    escaped.push_back('"');
    return escaped;
}

[[nodiscard]] std::string format_command(const Command& command)
{
    return std::visit(
        [](const auto& value) -> std::string {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::string>)
            {
                return value;
            }
            else
            {
                std::ostringstream output;
                output << "[";
                for (std::size_t i = 0; i < value.size(); ++i)
                {
                    output << json_quote(value[i]);
                    if (i + 1 < value.size())
                        output << ", ";
                }
                output << "]";
                return output.str();
            }
        },
        command);
}
} // namespace

std::string emit_dockerfile(const ServiceBuild& service)
{
    std::ostringstream output;

    for (std::size_t i = 0; i < service.stages.size(); ++i)
    {
        const DockerStage& stage = service.stages[i];

        output << "FROM " << stage.from_image;
        if (!stage.name.empty())
            output << " AS " << stage.name;
        output << "\n";

        if (stage.workdir.has_value())
            output << "WORKDIR " << *stage.workdir << "\n";

        for (const auto& copy : stage.copies)
        {
            output << "COPY ";
            if (copy.from_stage.has_value())
                output << "--from=" << *copy.from_stage << " ";
            output << copy.source << " " << copy.destination << "\n";
        }

        for (const auto& command : stage.run_commands)
            output << "RUN " << command << "\n";

        for (const auto& [key, value] : stage.env)
            output << "ENV " << key << "=" << value << "\n";

        for (const auto& port : stage.exposes)
            output << "EXPOSE " << port << "\n";

        if (stage.entrypoint.has_value())
            output << "ENTRYPOINT " << format_command(*stage.entrypoint) << "\n";

        if (stage.cmd.has_value())
            output << "CMD " << format_command(*stage.cmd) << "\n";

        if (i + 1 < service.stages.size())
            output << "\n";
    }

    return output.str();
}

} // namespace abstack
