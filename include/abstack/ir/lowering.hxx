#pragma once

#include "abstack/ast.hxx"
#include "abstack/ir/model.hxx"

namespace abstack
{

[[nodiscard]] BuildPlan lower_to_ir(const Ast& ast);

} // namespace abstack
