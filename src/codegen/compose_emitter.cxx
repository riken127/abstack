#include "abstack/codegen/compose_emitter.hxx"

#include <sstream>

namespace abstack
{

namespace
{
[[nodiscard]] std::string yaml_quote(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size() + 4);
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
} // namespace

std::string emit_compose(const BuildPlan& plan)
{
    std::ostringstream output;

    output << "version: \"3.9\"\n";
    output << "services:\n";

    for (const auto& service : plan.services)
    {
        output << "  " << service.compose.name << ":\n";
        output << "    build:\n";
        output << "      context: .\n";
        output << "      dockerfile: " << yaml_quote(service.compose.dockerfile) << "\n";

        if (!service.compose.ports.empty())
        {
            output << "    ports:\n";
            for (const auto& port : service.compose.ports)
                output << "      - " << yaml_quote(port) << "\n";
        }

        if (!service.compose.environment.empty())
        {
            output << "    environment:\n";
            for (const auto& [key, value] : service.compose.environment)
            {
                output << "      " << key << ": " << yaml_quote(value) << "\n";
            }
        }

        if (!service.compose.depends_on.empty())
        {
            output << "    depends_on:\n";
            for (const auto& dependency : service.compose.depends_on)
            {
                output << "      - " << yaml_quote(dependency) << "\n";
            }
        }

        if (service.compose.entrypoint.has_value())
        {
            output << "    entrypoint: " << yaml_quote(*service.compose.entrypoint) << "\n";
        }

        if (service.compose.command.has_value())
            output << "    command: " << yaml_quote(*service.compose.command) << "\n";
    }

    return output.str();
}

} // namespace abstack
