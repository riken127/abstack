#include "semantic.hxx"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace
{
void validate_value(const Value& value,
                    const std::unordered_set<std::string>& param_names,
                    const std::string& template_name)
{
    if (const auto* ref = std::get_if<IdentifierRef>(&value))
    {
        if (!param_names.contains(ref->name))
        {
            throw std::runtime_error("Unknown parameter `" + ref->name + "` in template `" +
                                     template_name + "`");
        }
    }
}

void validate_template(const TemplateDecl& tmpl)
{
    if (tmpl.stages.empty())
        throw std::runtime_error("Template `" + tmpl.name + "` must declare at least one stage");

    std::unordered_set<std::string> param_names;
    for (const auto& param : tmpl.params)
    {
        if (!param_names.insert(param).second)
            throw std::runtime_error("Duplicate parameter `" + param + "` in template `" +
                                     tmpl.name + "`");
    }

    std::unordered_set<std::string> stage_names;
    for (const auto& stage : tmpl.stages)
    {
        if (!stage_names.insert(stage.name).second)
            throw std::runtime_error("Duplicate stage `" + stage.name + "` in template `" +
                                     tmpl.name + "`");
    }

    for (const auto& stage : tmpl.stages)
    {
        if (!stage.from_image.has_value())
            throw std::runtime_error("Stage `" + stage.name + "` in template `" + tmpl.name +
                                     "` must contain `from`");

        validate_value(*stage.from_image, param_names, tmpl.name);

        if (stage.workdir.has_value())
            validate_value(*stage.workdir, param_names, tmpl.name);

        for (const auto& cmd : stage.run_commands)
            validate_value(cmd, param_names, tmpl.name);

        for (const auto& copy : stage.copies)
        {
            if (!copy.from_stage.empty() && !stage_names.contains(copy.from_stage))
            {
                throw std::runtime_error("Stage `" + stage.name + "` in template `" + tmpl.name +
                                         "` copies from unknown stage `" + copy.from_stage + "`");
            }

            validate_value(copy.source, param_names, tmpl.name);
            validate_value(copy.destination, param_names, tmpl.name);
        }
    }
}
} // namespace

void validate_ast(const Ast& ast)
{
    std::unordered_map<std::string, std::size_t> template_arity;
    std::unordered_set<std::string> service_names;

    for (const auto& tmpl : ast.templates)
    {
        if (template_arity.contains(tmpl.name))
            throw std::runtime_error("Duplicate template `" + tmpl.name + "`");

        template_arity[tmpl.name] = tmpl.params.size();
        validate_template(tmpl);
    }

    for (const auto& service : ast.services)
    {
        if (!service_names.insert(service.name).second)
            throw std::runtime_error("Duplicate service `" + service.name + "`");

        if (service.uses.empty())
            throw std::runtime_error("Service `" + service.name +
                                     "` must use at least one template");

        for (const auto& use : service.uses)
        {
            auto it = template_arity.find(use.template_name);
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
        }
    }
}
