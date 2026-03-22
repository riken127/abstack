#pragma once

#include "abstack/ir/model.hxx"

#include <string>

namespace abstack
{

[[nodiscard]] std::string emit_compose(const BuildPlan& plan);

} // namespace abstack
