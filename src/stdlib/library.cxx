#include "abstack/stdlib/library.hxx"

namespace abstack
{

namespace
{
constexpr std::string_view kCoreV1Profile = "core-v1";
constexpr std::string_view kDefaultAlias = "default";

constexpr std::string_view kCoreV1Source = R"(
template std_v1_go_service(name, service_port) {
    stage build {
        from "golang:1.22"
        workdir "/src"
        copy "." "/src"
        run "go build -o app ./cmd/${name}"
    }

    stage runtime {
        from "alpine:3.20"
        copy from build "/src/app" "/app"
        expose service_port
        cmd ["/app"]
    }
}

template std_v1_node_service(service_port, start_cmd) {
    stage runtime {
        from "node:20-alpine"
        workdir "/app"
        copy "." "/app"
        run "npm ci --omit=dev"
        expose service_port
        cmd start_cmd
    }
}

template std_v1_python_service(service_port, start_cmd) {
    stage runtime {
        from "python:3.12-alpine"
        workdir "/app"
        copy "." "/app"
        run "pip install --no-cache-dir -r requirements.txt"
        expose service_port
        cmd start_cmd
    }
}

template std_v1_static_site(service_port, site_dir) {
    stage runtime {
        from "nginx:1.27-alpine"
        copy site_dir "/usr/share/nginx/html"
        expose service_port
        cmd ["nginx", "-g", "daemon off;"]
    }
}

template std_v1_postgres() {
    stage runtime {
        from "postgres:16"
        expose 5432
    }
}

template std_v1_redis() {
    stage runtime {
        from "redis:7-alpine"
        expose 6379
    }
}
)";
} // namespace

std::vector<StdlibProfile> stdlib_profiles()
{
    return {
        StdlibProfile{.name = kCoreV1Profile,
                      .description =
                          "Core templates for go/node/python/static services plus postgres/redis."},
        StdlibProfile{.name = kDefaultAlias, .description = "Alias to `core-v1`."},
    };
}

std::optional<std::string_view> stdlib_profile_source(const std::string_view profile)
{
    if (profile == kCoreV1Profile || profile == kDefaultAlias)
        return kCoreV1Source;

    return std::nullopt;
}

} // namespace abstack
