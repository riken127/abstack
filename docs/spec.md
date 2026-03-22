# Abstack Language Specification v0.2.0

## 1. Goal
`abstack` is a declarative DSL for defining reusable container build logic and service runtime instantiation in one source of truth.

Compilation model:

```text
abstack file.abs -> Dockerfile.<service>* + docker-compose.generated.yml
```

In v0.2, `docker-compose` is treated as a projection of `service`, not as an independent authoring syntax.

## 2. Design Principles
1. Declarative only: no loops, conditionals, or mutable state.
2. Clear separation: templates define image build behavior, services define deployment/runtime instantiation.
3. Reuse-first: one template can be reused by different services with different arguments.
4. Deterministic output: generation should be stable and diff-friendly.

## 3. Source Model
A file contains only:
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
        cmd "/app"
    }
}

service api {
    use app_image("api", 8080)
    env {
        LOG_LEVEL = "info"
    }
    port "8080:8080"
    depends_on db
}
```

## 4. Core Distinction
### Template
`template` defines reusable build graph(s) through stage declarations.

Responsibilities:
1. Build stages (`stage`, `from`, `copy`, `run`, `workdir`)
2. Image defaults (`env`, `expose`, `cmd`, `entrypoint`)
3. Parameterized reuse

### Service
`service` instantiates a template and provides runtime/deployment overlays.

Responsibilities:
1. Instantiate build logic with `use`
2. Add runtime env/port/dependency configuration
3. Override runtime command/entrypoint when needed
4. Project runtime config into compose output

## 5. Values and Parameters
Supported value forms:
1. `string`
2. `integer`
3. `identifier` (primarily for template parameter references)

String interpolation inside template strings is supported via `${param}`.

Example:

```abstack
run "go build -o app ./cmd/${name}"
```

## 6. Statements
### Template / Stage Statements
Inside `stage`:
1. `from <value>`
2. `workdir <value>`
3. `copy <value> <value>`
4. `copy from <identifier> <value> <value>`
5. `run <value>`
6. `env { KEY = <value> ... }`
7. `expose <value>`
8. `cmd <value>`
9. `entrypoint <value>`

### Service Statements
Inside `service`:
1. `use <template>(args...)`
2. `env { KEY = <value> ... }`
3. `expose <value>`
4. `cmd <value>`
5. `entrypoint <value>`
6. `port <value>`
7. `depends_on <identifier>`

## 7. Generation Rules
Pipeline:

```text
DSL -> AST -> semantic validation -> template expansion -> Docker IR -> Dockerfile + Compose
```

Per service:
1. Generate `Dockerfile.<service>`
2. Generate compose service entry in `docker-compose.generated.yml`

Runtime overlay behavior:
1. Service `env` is appended to final stage `ENV` and projected to compose `environment`.
2. Service `expose` is appended to final stage `EXPOSE`.
3. Service `cmd` / `entrypoint` overrides final stage defaults when present.
4. Service `port` is projected to compose `ports`.
5. If no explicit `port` exists, service-level `expose` values may be projected as `p:p`.

## 8. Validation Rules
### Syntax
Compiler must reject malformed declarations/statements (unexpected token, missing delimiter, invalid statement position).

### Semantics
1. Template names must be unique.
2. Service names must be unique.
3. Template parameters must be unique.
4. Stage names must be unique per template.
5. Each stage must contain exactly one `from`.
6. `copy from <stage>` must reference an existing stage.
7. Each service must contain exactly one `use` (current v0.2 implementation rule).
8. `use` argument count must match template parameter count.
9. Services cannot `depends_on` themselves.

## 9. Output Layout
Current compiler default:
1. Dockerfiles: `generated/Dockerfile.<service>` (or custom `--out-dir`)
2. Compose: `generated/docker-compose.generated.yml` (or custom `--compose-file`)
