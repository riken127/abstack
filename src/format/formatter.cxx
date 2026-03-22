#include "abstack/format/formatter.hxx"

#include <sstream>
#include <string>
#include <type_traits>

namespace abstack
{

namespace
{
[[nodiscard]] std::string escape_string(const std::string& input)
{
    std::string escaped;
    escaped.reserve(input.size() + 2);

    for (const char c : input)
    {
        switch (c)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(c);
            break;
        }
    }

    return escaped;
}

[[nodiscard]] std::string format_value(const Value& value)
{
    return std::visit(
        [](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, StringValue>)
            {
                return '"' + escape_string(v.value) + '"';
            }
            else if constexpr (std::is_same_v<T, NumberValue>)
            {
                return std::to_string(v.value);
            }
            else
            {
                return v.name;
            }
        },
        value);
}

[[nodiscard]] std::string format_command_expr(const CommandExpr& command)
{
    return std::visit(
        [](const auto& expr) -> std::string {
            using T = std::decay_t<decltype(expr)>;
            if constexpr (std::is_same_v<T, Value>)
            {
                return format_value(expr);
            }
            else
            {
                std::ostringstream output;
                output << "[";
                for (std::size_t i = 0; i < expr.items.size(); ++i)
                {
                    output << format_value(expr.items[i]);
                    if (i + 1 < expr.items.size())
                        output << ", ";
                }
                output << "]";
                return output.str();
            }
        },
        command);
}

void append_line(std::ostringstream& output, const int indent, const std::string& content)
{
    output << std::string(static_cast<std::size_t>(indent * 4), ' ') << content << "\n";
}

void format_env_block(std::ostringstream& output, const int indent, const std::vector<EnvBinding>& env)
{
    if (env.empty())
        return;

    append_line(output, indent, "env {");
    for (const auto& binding : env)
    {
        append_line(output,
                    indent + 1,
                    binding.key + " = " + format_value(binding.value));
    }
    append_line(output, indent, "}");
}

void format_stage(std::ostringstream& output, const Stage& stage)
{
    append_line(output, 1, "stage " + stage.name + " {");

    if (stage.from_image.has_value())
        append_line(output, 2, "from " + format_value(*stage.from_image));

    if (stage.workdir.has_value())
        append_line(output, 2, "workdir " + format_value(*stage.workdir));

    for (const auto& copy : stage.copies)
    {
        std::string statement = "copy ";
        if (copy.from_stage.has_value())
            statement += "from " + *copy.from_stage + " ";

        statement += format_value(copy.source) + " " + format_value(copy.destination);
        append_line(output, 2, statement);
    }

    for (const auto& command : stage.run_commands)
        append_line(output, 2, "run " + format_value(command));

    format_env_block(output, 2, stage.env);

    for (const auto& expose : stage.exposes)
        append_line(output, 2, "expose " + format_value(expose));

    if (stage.entrypoint.has_value())
        append_line(output, 2, "entrypoint " + format_command_expr(*stage.entrypoint));

    if (stage.cmd.has_value())
        append_line(output, 2, "cmd " + format_command_expr(*stage.cmd));

    append_line(output, 1, "}");
}

void format_template(std::ostringstream& output, const TemplateDecl& tmpl)
{
    output << "template " << tmpl.name << "(";
    for (std::size_t i = 0; i < tmpl.params.size(); ++i)
    {
        output << tmpl.params[i];
        if (i + 1 < tmpl.params.size())
            output << ", ";
    }
    output << ") {\n";

    for (std::size_t i = 0; i < tmpl.stages.size(); ++i)
    {
        format_stage(output, tmpl.stages[i]);
        if (i + 1 < tmpl.stages.size())
            output << "\n";
    }

    output << "}\n";
}

void format_service(std::ostringstream& output, const ServiceDecl& service)
{
    output << "service " << service.name << " {\n";

    for (const auto& use : service.uses)
    {
        output << "    use " << use.template_name << "(";
        for (std::size_t i = 0; i < use.arguments.size(); ++i)
        {
            output << format_value(use.arguments[i]);
            if (i + 1 < use.arguments.size())
                output << ", ";
        }
        output << ")\n";
    }

    format_env_block(output, 1, service.env);

    for (const auto& expose : service.exposes)
        append_line(output, 1, "expose " + format_value(expose));

    if (service.entrypoint.has_value())
        append_line(output, 1, "entrypoint " + format_command_expr(*service.entrypoint));

    if (service.cmd.has_value())
        append_line(output, 1, "cmd " + format_command_expr(*service.cmd));

    for (const auto& port : service.ports)
        append_line(output, 1, "port " + format_value(port));

    for (const auto& dependency : service.depends_on)
        append_line(output, 1, "depends_on " + dependency);

    output << "}\n";
}
} // namespace

std::string format_ast(const Ast& ast)
{
    std::ostringstream output;

    bool first = true;

    for (const auto& tmpl : ast.templates)
    {
        if (!first)
            output << "\n";

        format_template(output, tmpl);
        first = false;
    }

    for (const auto& service : ast.services)
    {
        if (!first)
            output << "\n";

        format_service(output, service);
        first = false;
    }

    return output.str();
}

} // namespace abstack
