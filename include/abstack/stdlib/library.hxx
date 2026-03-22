#pragma once

#include <optional>
#include <string_view>
#include <vector>

namespace abstack
{

struct StdlibProfile
{
    std::string_view name;
    std::string_view description;
};

[[nodiscard]] std::vector<StdlibProfile> stdlib_profiles();
[[nodiscard]] std::optional<std::string_view> stdlib_profile_source(std::string_view profile);

} // namespace abstack
