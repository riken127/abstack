#include "abstack/ir/lowering.hxx"

#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace abstack
{

namespace
{
[[nodiscard]] std::string stringify_value(const Value& value)
{
    return std::visit(
        [](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, StringValue>)
            {
                return v.value;
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

[[nodiscard]] std::string interpolate_template_string(
    const std::string& input,
    const std::unordered_map<std::string, Value>& bindings,
    const std::string& template_name)
{
    std::string output;
    output.reserve(input.size());

    for (std::size_t i = 0; i < input.size(); ++i)
    {
        if (input[i] != '$' || i + 1 >= input.size() || input[i + 1] != '{')
        {
            output.push_back(input[i]);
            continue;
        }

        const std::size_t close = input.find('}', i + 2);
        if (close == std::string::npos)
        {
            throw std::runtime_error("Internal error: unterminated interpolation in template `" +
                                     template_name + "`");
        }

        const std::string key = input.substr(i + 2, close - (i + 2));
        const auto it = bindings.find(key);
        if (it == bindings.end())
        {
            throw std::runtime_error("Internal error: unknown interpolation key `" + key +
                                     "` in template `" + template_name + "`");
        }

        output += stringify_value(it->second);
        i = close;
    }

    return output;
}

[[nodiscard]] std::string resolve_template_value(
    const Value& value,
    const std::unordered_map<std::string, Value>& bindings,
    const std::string& template_name)
{
    if (const auto* string_value = std::get_if<StringValue>(&value))
    {
        return interpolate_template_string(string_value->value, bindings, template_name);
    }

    if (const auto* identifier = std::get_if<IdentifierValue>(&value))
    {
        const auto it = bindings.find(identifier->name);
        if (it == bindings.end())
        {
            throw std::runtime_error("Internal error: unresolved template parameter `" +
                                     identifier->name + "` in template `" + template_name + "`");
        }

        return stringify_value(it->second);
    }

    return stringify_value(value);
}

[[nodiscard]] Command resolve_template_command(
    const CommandExpr& command,
    const std::unordered_map<std::string, Value>& bindings,
    const std::string& template_name)
{
    return std::visit(
        [&](const auto& expr) -> Command {
            using T = std::decay_t<decltype(expr)>;
            if constexpr (std::is_same_v<T, Value>)
            {
                return resolve_template_value(expr, bindings, template_name);
            }
            else
            {
                std::vector<std::string> values;
                values.reserve(expr.items.size());
                for (const auto& item : expr.items)
                    values.push_back(resolve_template_value(item, bindings, template_name));
                return values;
            }
        },
        command);
}

[[nodiscard]] Command resolve_service_command(const CommandExpr& command)
{
    return std::visit(
        [](const auto& expr) -> Command {
            using T = std::decay_t<decltype(expr)>;
            if constexpr (std::is_same_v<T, Value>)
            {
                return stringify_value(expr);
            }
            else
            {
                std::vector<std::string> values;
                values.reserve(expr.items.size());
                for (const auto& item : expr.items)
                    values.push_back(stringify_value(item));
                return values;
            }
        },
        command);
}

[[nodiscard]] std::string normalize_port_mapping(const std::string& value)
{
    if (value.find(':') != std::string::npos)
        return value;

    return value + ":" + value;
}
} // namespace

BuildPlan lower_to_ir(const Ast& ast)
{
    std::unordered_map<std::string, const TemplateDecl*> templates;
    for (const auto& tmpl : ast.templates)
        templates.emplace(tmpl.name, &tmpl);

    BuildPlan plan{};

    for (const auto& service : ast.services)
    {
        if (service.uses.empty())
        {
            throw std::runtime_error("Internal error: service `" + service.name +
                                     "` has no template instantiations during lowering");
        }

        ServiceBuild build{};
        build.service_name = service.name;
        build.compose.name = service.name;
        build.compose.dockerfile = "Dockerfile." + service.name;

        for (std::size_t use_index = 0; use_index < service.uses.size(); ++use_index)
        {
            const UseStmt& use = service.uses[use_index];
            const auto tmpl_it = templates.find(use.template_name);
            if (tmpl_it == templates.end())
            {
                throw std::runtime_error("Internal error: missing template `" + use.template_name +
                                         "` during lowering");
            }

            const TemplateDecl& tmpl = *tmpl_it->second;

            std::unordered_map<std::string, Value> bindings;
            for (std::size_t i = 0; i < tmpl.params.size(); ++i)
                bindings.emplace(tmpl.params[i], use.arguments[i]);

            std::unordered_map<std::string, std::string> stage_name_map;
            // Each template instantiation gets a private stage namespace when a service
            // uses the template more than once; single-use templates keep their original
            // names for readability in generated output.
            for (const auto& stage : tmpl.stages)
            {
                if (service.uses.size() == 1)
                {
                    stage_name_map.emplace(stage.name, stage.name);
                }
                else
                {
                    stage_name_map.emplace(stage.name,
                                           "u" + std::to_string(use_index) + "_" + tmpl.name +
                                               "_" + stage.name);
                }
            }

            for (const auto& stage : tmpl.stages)
            {
                DockerStage lowered_stage{};
                lowered_stage.name = stage_name_map.at(stage.name);
                lowered_stage.from_image =
                    resolve_template_value(*stage.from_image, bindings, tmpl.name);

                if (stage.workdir.has_value())
                {
                    lowered_stage.workdir =
                        resolve_template_value(*stage.workdir, bindings, tmpl.name);
                }

                lowered_stage.copies.reserve(stage.copies.size());
                for (const auto& copy : stage.copies)
                {
                    std::optional<std::string> from_stage;
                    if (copy.from_stage.has_value())
                        from_stage = stage_name_map.at(*copy.from_stage);

                    lowered_stage.copies.push_back(
                        DockerCopy{.from_stage = from_stage,
                                   .source = resolve_template_value(
                                       copy.source, bindings, tmpl.name),
                                   .destination = resolve_template_value(
                                       copy.destination, bindings, tmpl.name)});
                }

                lowered_stage.run_commands.reserve(stage.run_commands.size());
                for (const auto& command : stage.run_commands)
                {
                    lowered_stage.run_commands.push_back(
                        resolve_template_value(command, bindings, tmpl.name));
                }

                lowered_stage.env.reserve(stage.env.size());
                for (const auto& binding : stage.env)
                {
                    lowered_stage.env.emplace_back(
                        binding.key,
                        resolve_template_value(binding.value, bindings, tmpl.name));
                }

                lowered_stage.exposes.reserve(stage.exposes.size());
                for (const auto& expose : stage.exposes)
                {
                    lowered_stage.exposes.push_back(
                        resolve_template_value(expose, bindings, tmpl.name));
                }

                if (stage.cmd.has_value())
                {
                    lowered_stage.cmd = resolve_template_command(
                        *stage.cmd, bindings, tmpl.name);
                }

                if (stage.entrypoint.has_value())
                {
                    lowered_stage.entrypoint = resolve_template_command(
                        *stage.entrypoint, bindings, tmpl.name);
                }

                build.stages.push_back(std::move(lowered_stage));
            }
        }

        // Service-level fields intentionally overlay the last lowered stage, matching the
        // DSL's "final stage then service overrides" model for compose-facing settings.
        DockerStage& final_stage = build.stages.back();

        for (const auto& binding : service.env)
        {
            const std::string value = stringify_value(binding.value);
            final_stage.env.emplace_back(binding.key, value);
            build.compose.environment.emplace_back(binding.key, value);
        }

        for (const auto& expose : service.exposes)
            final_stage.exposes.push_back(stringify_value(expose));

        if (service.cmd.has_value())
        {
            Command cmd = resolve_service_command(*service.cmd);
            final_stage.cmd = cmd;
            build.compose.command = cmd;
        }

        if (service.entrypoint.has_value())
        {
            Command entrypoint = resolve_service_command(*service.entrypoint);
            final_stage.entrypoint = entrypoint;
            build.compose.entrypoint = entrypoint;
        }

        for (const auto& port : service.ports)
            build.compose.ports.push_back(normalize_port_mapping(stringify_value(port)));

        if (build.compose.ports.empty())
        {
            for (const auto& expose : service.exposes)
            {
                build.compose.ports.push_back(
                    normalize_port_mapping(stringify_value(expose)));
            }
        }

        build.compose.depends_on = service.depends_on;

        plan.services.push_back(std::move(build));
    }

    return plan;
}

} // namespace abstack
