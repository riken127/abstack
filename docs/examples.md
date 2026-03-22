# Abstack Examples

This document walks through practical `.abs` examples and points to ready-to-run files under `samples/`.

## 1. Quickstart Service

File: `samples/quickstart.abs`

```abstack
template hello_service() {
    stage runtime {
        from "alpine:3.20"
        run "echo hello from abstack"
        cmd ["/bin/sh", "-c", "echo hello"]
    }
}

service hello {
    use hello_service()
    port "8080:8080"
}
```

## 2. API + DB Stack

File: `samples/unified.abs`

Highlights:

1. Reusable Go template with parameter interpolation.
2. Runtime overlays in service blocks.
3. `depends_on` between services.

## 3. Multi-Use Composition

File: `samples/multi_use.abs`

```abstack
template diagnostics() {
    stage diag {
        from "alpine:3.20"
        run "echo diagnostics enabled"
    }
}

template app(name, service_port) {
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
    use diagnostics()
    use app("api", 8080)
    entrypoint ["/app"]
    cmd ["/app", "--serve"]
    port "8080:8080"
}
```

Note: overlay statements apply to the final lowered stage, so `app` is placed last.

## 4. Microservices Example

File: `samples/microservices.abs`

Includes:

1. API service.
2. Worker service.
3. PostgreSQL service.
4. Shared template reuse for build/runtime consistency.

## 5. Stdlib-Based Stack

File: `samples/stdlib_stack.abs`

Highlights:

1. Uses bundled stdlib templates only (no local template declarations).
2. API + postgres + redis stack.
3. Works with explicit stdlib profile linkage (`--stdlib-profile default`).

## 6. Running an Example

```bash
./build/native/abstack samples/microservices.abs --out-dir generated
./build/native/abstack build samples/stdlib_stack.abs --stdlib-profile default --out-dir generated
```

You can then inspect:

1. `generated/Dockerfile.api`
2. `generated/Dockerfile.worker`
3. `generated/Dockerfile.db`
4. `generated/docker-compose.generated.yml`

## 7. Inspecting Outputs Quickly

```bash
sed -n '1,200p' generated/Dockerfile.api
sed -n '1,200p' generated/docker-compose.generated.yml
```

## 8. Example Selection Guidance

1. Start with `quickstart.abs` for syntax basics.
2. Move to `unified.abs` for template/service layering.
3. Use `multi_use.abs` for composition behavior.
4. Use `microservices.abs` as a realistic baseline for project layouts.
5. Use `stdlib_stack.abs` when you want compiler-bundled templates.
