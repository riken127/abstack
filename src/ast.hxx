#pragma once

#include <string>
#include <vector>

struct Stage
{
    std::string name;
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
};

struct Ast
{
    std::vector<TemplateDecl> templates;
    std::vector<ServiceDecl> services;
};
