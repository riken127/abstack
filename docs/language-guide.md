# Abstack Language Guide (v0.4.0)

This guide explains how to write `.abs` files, compile them, and reason about generated Dockerfile and Compose outputs.

## 1. Mental Model

Abstack has two top-level concepts:

1. `template`: reusable image build logic.
2. `service`: concrete deployment instantiation.

Think of templates as Dockerfile fragments with parameters, and services as runtime projections that materialize actual deployable units.

## 2. Build the Compiler

```bash
cmake --preset native
cmake --build --preset native
```

Run tests:

```bash
ctest --test-dir build/native --output-on-failure
```

## 3. Compile an Abstack File

```bash
./build/native/abstack samples/unified.abs --out-dir generated
```

By default it writes:

1. `generated/Dockerfile.<service>` for each service.
2. `generated/docker-compose.generated.yml` with all service blocks.

Optional compose path override:

```bash
./build/native/abstack samples/unified.abs \
  --out-dir generated \
  --compose-file generated/compose.custom.yml
```

## 4. Core Syntax

### Template declaration

```abstack
template go_service(name, service_port) {
    stage build {
        from "golang:1.22"
        run "go build -o app ./cmd/${name}"
    }

    stage runtime {
        from "alpine:3.20"
        copy from build "/src/app" "/app"
        expose service_port
        cmd ["/app"]
    }
}
```

### Service declaration

```abstack
service api {
    use go_service("api", 8080)
    env {
        LOG_LEVEL = "info"
    }
    port "8080:8080"
    depends_on db
}
```

## 5. Statements Reference

### Stage statements

1. `from <value>`: base image.
2. `workdir <value>`: stage working directory.
3. `copy <src> <dst>`: file copy.
4. `copy from <stage> <src> <dst>`: cross-stage copy.
5. `run <value>`: shell command.
6. `env { KEY = <value> ... }`: environment defaults.
7. `expose <value>`: image-level port hint.
8. `cmd <command_value>`: default command.
9. `entrypoint <command_value>`: default entrypoint.

### Service statements

1. `use <template>(...)`: instantiate template.
2. `env { KEY = <value> ... }`: runtime env overlay.
3. `expose <value>`: runtime expose overlay.
4. `cmd <command_value>`: runtime command override.
5. `entrypoint <command_value>`: runtime entrypoint override.
6. `port <value>`: compose port mapping.
7. `depends_on <identifier>`: compose dependency.

## 6. Command Value Forms

`cmd` and `entrypoint` accept:

1. Scalar form:

```abstack
cmd "/app --serve"
```

2. Array form (recommended for exact argv behavior):

```abstack
cmd ["/app", "--serve"]
entrypoint ["/bin/sh", "-c"]
```

Array form is emitted as:

1. JSON array in Dockerfile (`CMD ["...", "..."]`)
2. YAML sequence in compose (`command: - "..."`)

## 7. Multi-Use Composition

Services can use multiple templates:

```abstack
service api {
    use diagnostics()
    use go_service("api", 8080)
}
```

Rules to remember:

1. Uses are lowered in order.
2. Stage names are namespaced when more than one `use` exists.
3. Runtime overlays apply to the final lowered stage.

Because of rule 3, place the runtime/base template last if you want overlays on that image.

## 8. Validation Rules

The compiler validates:

1. Unique template/service names.
2. Unique stage names per template.
3. `from` presence in each stage.
4. Valid `copy from` stage references.
5. Correct `use` argument count.
6. No self-dependencies in `depends_on`.
7. `depends_on` targets must exist.
8. No unresolved identifier values in service runtime values.

## 9. Recommended Authoring Workflow

1. Start with one template and one service.
2. Add parameters and interpolation.
3. Extract cross-cutting behavior into additional templates.
4. Compose with multiple `use` statements.
5. Confirm generated outputs and keep them reviewable in CI.

## 10. Troubleshooting

### Parse error near command array

Make sure arrays are comma-separated and closed:

```abstack
cmd ["/app", "--serve"]
```

### Unknown dependency

Every `depends_on` target must be a declared `service` in the same file.

### Unexpected final stage behavior with multi-use

Overlay statements apply to the final lowered stage. Reorder `use` statements so your intended runtime template is last.
