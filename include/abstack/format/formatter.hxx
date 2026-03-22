#pragma once

#include "abstack/ast.hxx"

#include <string>

namespace abstack
{

[[nodiscard]] std::string format_ast(const Ast& ast);

} // namespace abstack
