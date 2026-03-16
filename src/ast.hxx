#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

struct StringLiteral
{
    std::string value;
};

struct IdentifierRef
{
    std::string name;   
};

using Value = std::variant<StringLiteral, IdentifierRef>;

struct CopyStmt
{
    std::string from_stage;
    Value source;
    Value destination;
};

struct UseStmt
{
    std::string template_name;
    std::vector<Value> arguments;
};

struct Stage
{
    std::string name;
    std::optional<Value> from_image;
    std::optional<Value> workdir;
    std::vector<CopyStmt> copies;
    std::vector<Value> run_commands;
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
};

struct Ast
{
    std::vector<TemplateDecl> templates;
    std::vector<ServiceDecl> services;
};
