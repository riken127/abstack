#pragma once

#include "abstack/ir/model.hxx"

#include <string>

namespace abstack
{

[[nodiscard]] std::string emit_dockerfile(const ServiceBuild& service);

} // namespace abstack
