#include "abstack/codegen/dockerfile_emitter.hxx"

#include <sstream>

namespace abstack
{

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
            output << "ENTRYPOINT " << *stage.entrypoint << "\n";

        if (stage.cmd.has_value())
            output << "CMD " << *stage.cmd << "\n";

        if (i + 1 < service.stages.size())
            output << "\n";
    }

    return output.str();
}

} // namespace abstack
