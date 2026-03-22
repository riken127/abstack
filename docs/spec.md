# Abstack Language Specification v0.3.0

## 1. Goal
`abstack` is a declarative DSL for defining reusable image build logic (`template`) and deployable service instantiation (`service`) from the same source.

Compilation model:

```text
abstack file.abs -> Dockerfile.<service>* + docker-compose.generated.yml
```

`docker-compose` is treated as a target projection of service declarations, not as an authoring source language.

## 2. Design Principles
1. Declarative-only language (no loops, conditionals, mutable variables).
2. Strong separation of concerns: image build graph vs runtime instantiation.
3. Reuse-first design through template parameters and service instantiation.
4. Deterministic output for reviewability and reproducible generation.
5. Compose syntax should remain a thin runtime projection over Dockerfile-centric semantics.

## 3. Top-Level Model
An `.abs` file contains only:
1. `template` declarations
2. `service` declarations

```abstack
template app_image(name, service_port) {
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

service api {
    use app_image("api", 8080)
    env {
        LOG_LEVEL = "info"
    }
    port "8080:8080"
}
```

## 4. Template vs Service
### Template responsibilities
1. Define build stages via `stage` blocks.
2. Declare image construction instructions (`from`, `copy`, `run`, `workdir`).
3. Declare image defaults (`env`, `expose`, `cmd`, `entrypoint`).
4. Expose parameters for reuse.

### Service responsibilities
1. Instantiate one or more templates with `use`.
2. Overlay runtime behavior (`env`, `expose`, `cmd`, `entrypoint`).
3. Define compose-facing runtime data (`port`, `depends_on`).
4. Produce one concrete build/runtime output unit per service.

## 5. Values and Interpolation
Supported scalar values:
1. `string`
2. `integer`
3. `identifier`

Identifier values are primarily used for template parameter references.

String interpolation is supported inside template strings:

```abstack
run "go build -o app ./cmd/${name}"
```

## 6. Comments
Comments are ignored by the lexer and can appear anywhere whitespace is valid.

Supported forms:
1. `# single-line comment`
2. `// single-line comment`
3. `/* multi-line block comment */`

## 7. Statements
### Stage statements (inside `stage`)
1. `from <value>`
2. `workdir <value>`
3. `copy <value> <value>`
4. `copy from <identifier> <value> <value>`
5. `run <value>`
6. `env { KEY = <value> ... }`
7. `expose <value>`
8. `cmd <command_value>`
9. `entrypoint <command_value>`

### Service statements (inside `service`)
1. `use <template>(args...)`
2. `env { KEY = <value> ... }`
3. `expose <value>`
4. `cmd <command_value>`
5. `entrypoint <command_value>`
6. `port <value>`
7. `depends_on <identifier>`

`command_value` may be:
1. scalar value (`"/app --serve"`)
2. array form (`["/app", "--serve"]`)

## 8. Multi-Use Composition
A service may contain multiple `use` statements.

Current composition behavior:
1. Template instances are lowered in declaration order.
2. Stage names are namespaced when multiple `use` statements exist to avoid collisions.
3. Service runtime overlays (`env`, `expose`, `cmd`, `entrypoint`) apply to the final lowered stage.

Practical implication: `use` order matters because the last lowered stage is the final runtime stage.

## 9. Generation Rules
Pipeline:

```text
DSL -> AST -> semantic validation -> template expansion -> Docker IR -> Dockerfile + Compose
```

Per service output:
1. `Dockerfile.<service>`
2. Compose service block inside `docker-compose.generated.yml`

Runtime overlay projection:
1. Service `env` is appended to final stage `ENV` and projected to compose `environment`.
2. Service `expose` is appended to final stage `EXPOSE`.
3. Service `cmd` / `entrypoint` overrides final stage defaults when present.
4. Service `port` is projected to compose `ports`.
5. If `port` is omitted, service-level `expose` values may project as `p:p` mappings.

## 10. Validation Rules
### Syntax
Compiler rejects malformed declarations/statements (unexpected token, missing delimiter, invalid location).

### Semantics
1. Template names must be unique.
2. Service names must be unique.
3. Template parameters must be unique.
4. Stage names must be unique per template.
5. Each stage must contain exactly one `from`.
6. `copy from <stage>` must reference an existing stage in that template.
7. Services must contain at least one `use`.
8. `use` argument count must match template parameter count.
9. Service command/entrypoint values cannot contain unresolved identifiers.
10. Services cannot `depends_on` themselves.
11. `depends_on` entries must reference declared services.

## 11. Output Layout
Default CLI output:
1. Dockerfiles: `generated/Dockerfile.<service>`
2. Compose: `generated/docker-compose.generated.yml`

These can be customized with:
1. `--out-dir <dir>`
2. `--compose-file <file>`
