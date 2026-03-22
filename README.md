# abstack

`abstack` is a declarative DSL compiler that unifies image build definition (`template`) and service runtime instantiation (`service`).

A single `.abs` file compiles into:

1. `Dockerfile.<service>` per service.
2. `docker-compose.generated.yml` with service runtime projection.

## Current Version

v0.5.0

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
7. Utility-grade CLI commands: `build`, `sync`, `fmt`, `docker`, `compose`, `stdlib`, and optional `tui`.

## Documentation

1. [Language spec](/home/riken/programming/abstack/docs/spec.md)
2. [Grammar reference](/home/riken/programming/abstack/docs/grammar.md)
3. [Language guide](/home/riken/programming/abstack/docs/language-guide.md)
4. [Stdlib guide](/home/riken/programming/abstack/docs/stdlib.md)
5. [CLI guide](/home/riken/programming/abstack/docs/cli-guide.md)
6. [CLI reference](/home/riken/programming/abstack/docs/cli-reference.md)
7. [CLI cookbook](/home/riken/programming/abstack/docs/cli-cookbook.md)
8. [Examples guide](/home/riken/programming/abstack/docs/examples.md)
9. [Tooling/CI guide](/home/riken/programming/abstack/docs/tooling.md)
10. [Release notes v0.5.0](/home/riken/programming/abstack/docs/releases/v0.5.0.md)
11. [Release notes v0.4.0](/home/riken/programming/abstack/docs/releases/v0.4.0.md)
12. [Release notes v0.3.0](/home/riken/programming/abstack/docs/releases/v0.3.0.md)
13. [Changelog](/home/riken/programming/abstack/CHANGELOG.md)
14. [Contributing](/home/riken/programming/abstack/CONTRIBUTING.md)

## Project Layout

- [`include/abstack`](/home/riken/programming/abstack/include/abstack): public compiler interfaces
- [`src/frontend`](/home/riken/programming/abstack/src/frontend): lexer/parser
- [`src/semantic`](/home/riken/programming/abstack/src/semantic): semantic checks
- [`src/ir`](/home/riken/programming/abstack/src/ir): lowering and template expansion
- [`src/stdlib`](/home/riken/programming/abstack/src/stdlib): bundled stdlib profile definitions
- [`src/codegen`](/home/riken/programming/abstack/src/codegen): Dockerfile/Compose emitters
- [`src/format`](/home/riken/programming/abstack/src/format): canonical `.abs` formatter
- [`src/cli/main.cxx`](/home/riken/programming/abstack/src/cli/main.cxx): CLI entrypoint
- [`tests/pipeline_test.cxx`](/home/riken/programming/abstack/tests/pipeline_test.cxx): integration tests
- [`samples`](/home/riken/programming/abstack/samples): runnable DSL samples

## LSP

`clangd` is the recommended language server. Configure with:

1. `.clangd`
2. compile database from `cmake --preset native` (`build/native`)
