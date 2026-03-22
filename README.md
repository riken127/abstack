# abstack

`abstack` is a declarative DSL compiler that unifies image build definition (`template`) and service runtime instantiation (`service`).

A single `.abs` file compiles into:

1. `Dockerfile.<service>` per service.
2. `docker-compose.generated.yml` with service runtime projection.

## Current Version

v0.6.0

## Why This Model

Instead of authoring Dockerfile and compose separately, abstack treats compose as a projection target over template-driven Docker semantics.

## Quickstart

```bash
cmake --preset native
cmake --build --preset native
./build/native/abstack samples/unified.abs --out-dir generated
```

Run tests:

```bash
ctest --test-dir build/native --output-on-failure
```

## Key Capabilities

1. Parameterized reusable templates.
2. Multi-stage builds.
3. Service runtime overlays (`env`, `expose`, `cmd`, `entrypoint`, `port`, `depends_on`).
4. Multiple template instantiations per service (`use` can appear multiple times).
5. Command/entrypoint scalar and array forms.
6. Optional bundled stdlib profiles (`core-v1` / `default`) for reusable service templates.
7. Native Windows build/test support in CI.
8. CLI callbacks with persistent log registration (`.abstack/logs/abstack-cli.log`) and spinner feedback for wait-heavy shell commands.
9. Utility-grade CLI commands: `build`, `sync`, `fmt`, `docker`, `compose`, `stdlib`, and optional `tui`.

## Documentation

1. [Language spec](docs/spec.md)
2. [Grammar reference](docs/grammar.md)
3. [Language guide](docs/language-guide.md)
4. [Stdlib guide](docs/stdlib.md)
5. [CLI guide](docs/cli-guide.md)
6. [CLI reference](docs/cli-reference.md)
7. [CLI cookbook](docs/cli-cookbook.md)
8. [Examples guide](docs/examples.md)
9. [Tooling/CI guide](docs/tooling.md)
10. [Release notes v0.6.0](docs/releases/v0.6.0.md)
11. [Release notes v0.5.0](docs/releases/v0.5.0.md)
12. [Release notes v0.4.0](docs/releases/v0.4.0.md)
13. [Release notes v0.3.0](docs/releases/v0.3.0.md)
14. [Changelog](CHANGELOG.md)
15. [Contributing](CONTRIBUTING.md)

## Project Layout

- [`include/abstack`](include/abstack): public compiler interfaces
- [`src/frontend`](src/frontend): lexer/parser
- [`src/semantic`](src/semantic): semantic checks
- [`src/ir`](src/ir): lowering and template expansion
- [`src/stdlib`](src/stdlib): bundled stdlib profile definitions
- [`src/codegen`](src/codegen): Dockerfile/Compose emitters
- [`src/format`](src/format): canonical `.abs` formatter
- [`src/cli/main.cxx`](src/cli/main.cxx): CLI entrypoint
- [`tests/pipeline_test.cxx`](tests/pipeline_test.cxx): integration tests
- [`samples`](samples): runnable DSL samples

## LSP

`clangd` is the recommended language server. Configure with:

1. `.clangd`
2. compile database from `cmake --preset native` (`build/native`)
