# Standard Library Guide (v0.6.0)

This guide documents the bundled `abstack` stdlib profiles and templates.

## 1. Enable Stdlib in CLI

Stdlib is opt-in and linked before validation/lowering:

```bash
abstack build samples/stdlib_stack.abs --stdlib-profile default --out-dir generated
```

List available profiles:

```bash
abstack stdlib list
```

## 2. Bundled Profiles

1. `core-v1`
2. `default` (alias to `core-v1`)

## 3. `core-v1` Template Reference

### `std_v1_go_service(name, service_port)`

Use for Go service builds with a build+runtime multi-stage template.

### `std_v1_node_service(service_port, start_cmd)`

Use for Node.js services with npm install in a runtime stage.

### `std_v1_python_service(service_port, start_cmd)`

Use for Python services with requirements install in a runtime stage.

### `std_v1_static_site(service_port, site_dir)`

Use for static site delivery with nginx runtime image.

### `std_v1_postgres()`

Runtime-only postgres template with port exposure.

### `std_v1_redis()`

Runtime-only redis template with port exposure.

## 4. Example

```abstack
service db {
    use std_v1_postgres()
    port "5432:5432"
}

service api {
    use std_v1_go_service("api", 8080)
    port "8080:8080"
    depends_on db
}
```

Compile:

```bash
abstack build app.abs --stdlib-profile default --out-dir generated
```

## 5. Compatibility Notes

1. Grammar does not change; stdlib provides additional templates only.
2. No profile is linked implicitly.
3. Template name collisions are validated like normal user templates.
