#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace abstack
{

struct StringValue
{
    std::string value;
};

struct NumberValue
{
    std::int64_t value;
};

struct IdentifierValue
{
    std::string name;
};

using Value = std::variant<StringValue, NumberValue, IdentifierValue>;

struct EnvBinding
{
    std::string key;
    Value value;
};

struct CopyStmt
{
    std::optional<std::string> from_stage;
    Value source;
    Value destination;
};

struct Stage
{
    std::string name;
    std::optional<Value> from_image;
    std::optional<Value> workdir;
    std::vector<CopyStmt> copies;
    std::vector<Value> run_commands;
    std::vector<EnvBinding> env;
    std::vector<Value> exposes;
    std::optional<Value> cmd;
    std::optional<Value> entrypoint;
};

struct UseStmt
{
    std::string template_name;
    std::vector<Value> arguments;
};

struct TemplateDecl
{
    std::string name;
    std::vector<std::string> params;
    std::vector<Stage> stages;
};

struct ServiceDecl
{
    std::string name;
    std::vector<UseStmt> uses;
    std::vector<EnvBinding> env;
    std::vector<Value> exposes;
    std::optional<Value> cmd;
    std::optional<Value> entrypoint;
    std::vector<Value> ports;
    std::vector<std::string> depends_on;
};

struct Ast
{
    std::vector<TemplateDecl> templates;
    std::vector<ServiceDecl> services;
};

} // namespace abstack
