#pragma once

#include <string>
#include <vector>

struct CopyStmt
{
    std::string from_stage;
    std::string source;
    std::string destination;
};

struct UseStmt
{
    std::string template_name;
    std::vector<std::string> arguments;
};

struct Stage
{
    std::string name;
    std::string from_image;
    std::string workdir;
    std::vector<CopyStmt> copies;
    std::vector<std::string> run_commands;
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
