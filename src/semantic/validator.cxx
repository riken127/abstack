#include "abstack/semantic/validator.hxx"

#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace abstack
{

namespace
{
void validate_value_in_template(const Value& value,
                                const std::unordered_set<std::string>& param_names,
                                const std::string& template_name)
{
    if (const auto* identifier = std::get_if<IdentifierValue>(&value))
    {
        if (!param_names.contains(identifier->name))
        {
            throw std::runtime_error("Unknown parameter `" + identifier->name +
                                     "` in template `" + template_name + "`");
        }
    }
}

void validate_template(const TemplateDecl& tmpl)
{
    // Template validation establishes the structural invariants that lowering assumes:
    // unique params/stages, a concrete base image, and only in-template references.
    if (tmpl.stages.empty())
        throw std::runtime_error("Template `" + tmpl.name + "` must declare at least one stage");

    std::unordered_set<std::string> param_names;
    for (const auto& param : tmpl.params)
    {
        if (!param_names.insert(param).second)
        {
            throw std::runtime_error("Duplicate parameter `" + param + "` in template `" +
                                     tmpl.name + "`");
        }
    }

    std::unordered_set<std::string> stage_names;
    for (const auto& stage : tmpl.stages)
    {
        if (!stage_names.insert(stage.name).second)
        {
            throw std::runtime_error("Duplicate stage `" + stage.name + "` in template `" +
                                     tmpl.name + "`");
        }

        if (!stage.from_image.has_value())
        {
            throw std::runtime_error("Stage `" + stage.name + "` in template `" + tmpl.name +
                                     "` must contain `from`");
        }

        validate_value_in_template(*stage.from_image, param_names, tmpl.name);

        if (stage.workdir.has_value())
            validate_value_in_template(*stage.workdir, param_names, tmpl.name);

        for (const auto& run : stage.run_commands)
            validate_value_in_template(run, param_names, tmpl.name);

        for (const auto& expose : stage.exposes)
            validate_value_in_template(expose, param_names, tmpl.name);

        if (stage.cmd.has_value())
        {
            std::visit(
                [&](const auto& expr) {
                    using T = std::decay_t<decltype(expr)>;
                    if constexpr (std::is_same_v<T, Value>)
                    {
                        validate_value_in_template(expr, param_names, tmpl.name);
                    }
                    else
                    {
                        for (const auto& item : expr.items)
                            validate_value_in_template(item, param_names, tmpl.name);
                    }
                },
                *stage.cmd);
        }

        if (stage.entrypoint.has_value())
        {
            std::visit(
                [&](const auto& expr) {
                    using T = std::decay_t<decltype(expr)>;
                    if constexpr (std::is_same_v<T, Value>)
                    {
                        validate_value_in_template(expr, param_names, tmpl.name);
                    }
                    else
                    {
                        for (const auto& item : expr.items)
                            validate_value_in_template(item, param_names, tmpl.name);
                    }
                },
                *stage.entrypoint);
        }

        for (const auto& binding : stage.env)
            validate_value_in_template(binding.value, param_names, tmpl.name);

        for (const auto& copy : stage.copies)
        {
            if (copy.from_stage.has_value() && !stage_names.contains(*copy.from_stage))
            {
                throw std::runtime_error("Stage `" + stage.name + "` in template `" + tmpl.name +
                                         "` copies from unknown stage `" + *copy.from_stage +
                                         "`");
            }

            validate_value_in_template(copy.source, param_names, tmpl.name);
            validate_value_in_template(copy.destination, param_names, tmpl.name);
        }
    }
}

void validate_service_value(const Value& value, const std::string& service_name)
{
    if (const auto* identifier = std::get_if<IdentifierValue>(&value))
    {
        throw std::runtime_error("Service `" + service_name +
                                 "` contains unresolved identifier value `" + identifier->name +
                                 "`; use string or number literals");
    }
}

void validate_service_command(const CommandExpr& value, const std::string& service_name)
{
    std::visit(
        [&](const auto& expr) {
            using T = std::decay_t<decltype(expr)>;
            if constexpr (std::is_same_v<T, Value>)
            {
                validate_service_value(expr, service_name);
            }
            else
            {
                for (const auto& item : expr.items)
                    validate_service_value(item, service_name);
            }
        },
        value);
}
} // namespace

void validate_ast(const Ast& ast)
{
    // Resolve template arity first so uses are checked against a complete catalog, then
    // do a second pass for dependency targets to allow forward references between services.
    std::unordered_map<std::string, std::size_t> template_arity;

    for (const auto& tmpl : ast.templates)
    {
        if (template_arity.contains(tmpl.name))
            throw std::runtime_error("Duplicate template `" + tmpl.name + "`");

        template_arity.emplace(tmpl.name, tmpl.params.size());
        validate_template(tmpl);
    }

    std::unordered_set<std::string> service_names;
    for (const auto& service : ast.services)
    {
        if (!service_names.insert(service.name).second)
            throw std::runtime_error("Duplicate service `" + service.name + "`");

        if (service.uses.empty())
            throw std::runtime_error("Service `" + service.name + "` must use one template");

        for (const auto& use : service.uses)
        {
            const auto it = template_arity.find(use.template_name);
            if (it == template_arity.end())
            {
                throw std::runtime_error("Service `" + service.name + "` uses unknown template `" +
                                         use.template_name + "`");
            }

            if (use.arguments.size() != it->second)
            {
                throw std::runtime_error(
                    "Service `" + service.name + "` uses template `" + use.template_name +
                    "` with wrong number of arguments: expected " + std::to_string(it->second) +
                    ", got " + std::to_string(use.arguments.size()));
            }

            for (const auto& arg : use.arguments)
                validate_service_value(arg, service.name);
        }

        for (const auto& binding : service.env)
            validate_service_value(binding.value, service.name);

        for (const auto& expose : service.exposes)
            validate_service_value(expose, service.name);

        if (service.cmd.has_value())
            validate_service_command(*service.cmd, service.name);

        if (service.entrypoint.has_value())
            validate_service_command(*service.entrypoint, service.name);

        for (const auto& port : service.ports)
            validate_service_value(port, service.name);

        std::unordered_set<std::string> deps;
        for (const auto& dependency : service.depends_on)
        {
            if (dependency == service.name)
            {
                throw std::runtime_error("Service `" + service.name + "` cannot depend on itself");
            }

            if (!deps.insert(dependency).second)
            {
                throw std::runtime_error("Service `" + service.name +
                                         "` has duplicate dependency `" + dependency + "`");
            }
        }
    }

    for (const auto& service : ast.services)
    {
        for (const auto& dependency : service.depends_on)
        {
            if (!service_names.contains(dependency))
            {
                throw std::runtime_error("Service `" + service.name +
                                         "` depends on unknown service `" + dependency + "`");
            }
        }
    }
}

} // namespace abstack
