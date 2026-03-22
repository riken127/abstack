#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace abstack
{

struct DockerCopy
{
    std::optional<std::string> from_stage;
    std::string source;
    std::string destination;
};

struct DockerStage
{
    std::string name;
    std::string from_image;
    std::optional<std::string> workdir;
    std::vector<DockerCopy> copies;
    std::vector<std::string> run_commands;
    std::vector<std::pair<std::string, std::string>> env;
    std::vector<std::string> exposes;
    std::optional<std::string> cmd;
    std::optional<std::string> entrypoint;
};

struct ComposeService
{
    std::string name;
    std::string dockerfile;
    std::vector<std::pair<std::string, std::string>> environment;
    std::vector<std::string> ports;
    std::vector<std::string> depends_on;
    std::optional<std::string> command;
    std::optional<std::string> entrypoint;
};

struct ServiceBuild
{
    std::string service_name;
    std::vector<DockerStage> stages;
    ComposeService compose;
};

struct BuildPlan
{
    std::vector<ServiceBuild> services;
};

} // namespace abstack
