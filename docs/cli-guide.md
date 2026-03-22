# CLI Guide (v0.6.0)

This guide is the practical "how to operate `abstack`" document for local development, CI pipelines, and lightweight Docker runtime workflows.

## 1. Before You Start

Build the binary:

```bash
cmake --preset native
cmake --build --preset native
```

The resulting executable is typically:

```text
./build/native/abstack
```

Runtime dependencies by command family:

1. `build`, `sync`, `fmt`: no Docker dependency.
2. `compose`, `docker`: require Docker CLI and daemon access.
3. `tui`: requires build with curses support (`ABSTACK_ENABLE_TUI=ON`).
4. CLI callbacks log to `.abstack/logs/abstack-cli.log`.

## 2. Mental Model

`abstack` is compiler-first:

1. Input is `.abs` source.
2. Output is generated artifacts (`Dockerfile.<service>`, compose YAML).
3. Runtime helpers (`compose`, `docker`) are optional wrappers, not orchestration replacements.

When to use each command:

1. `build`: one source file.
2. `sync`: many source files from a directory tree with regex selection.
3. `fmt`: canonical source formatting.
4. `compose`: optionally generate, then run `docker compose`.
5. `docker`: minimal container inspection/operations helper.
6. `stdlib`: list bundled stdlib profiles.
7. `tui`: minimal interactive entrypoint for `build`/`fmt`/`sync`.

## 3. Command Conventions

1. Parsing is strict: unknown options fail immediately.
2. Regex arguments use C++ regex syntax.
3. Paths may be relative or absolute.
4. Errors are surfaced as non-zero exit with explicit messages.
5. Backward compatibility is preserved: `abstack <file.abs> ...` maps to `abstack build <file.abs> ...`.
6. Stdlib linkage is explicit and opt-in through `--stdlib-profile`.
7. Spinner feedback is shown for wait-heavy non-interactive shell calls (disable with `ABSTACK_NO_SPINNER=1`).

## 4. Windows Native Support

`abstack` is built and tested natively on Windows in CI.

Practical notes:

1. Presets use Ninja for cross-platform consistency.
2. Docker helper commands use Windows-safe shell quoting.
3. Optional TUI may remain unavailable if curses support is not present.

## 5. Generated Output Model

Default generation behavior:

1. Dockerfiles are emitted under `generated/` as `Dockerfile.<service>`.
2. Compose is emitted as `generated/docker-compose.generated.yml`.

Path controls:

1. `--out-dir <dir>` changes Dockerfile output base and default compose location.
2. `--compose-file <file>` sets explicit compose output path.

Cleanup behavior:

1. `--clean` removes existing `Dockerfile.*` in `--out-dir`.
2. `--clean` also removes the default compose output file in `--out-dir` when `--compose-file` is not explicitly set.

## 6. `build`: Compile One `.abs` File

Syntax:

```bash
abstack build <file.abs> [options]
```

Options:

1. `--out-dir <dir>`
2. `--compose-file <file>`
3. `--service-regex <pattern>`
4. `--clean`
5. `--dry-run`
6. `--stdlib-profile <name>`
7. `--list-stdlib-profiles`

Typical usage:

```bash
abstack build samples/unified.abs --out-dir generated --clean
```

Selective service generation:

```bash
abstack build samples/unified.abs --service-regex '^(api|db)$'
```

Validation-only plus compose preview:

```bash
abstack build samples/unified.abs --dry-run
abstack build samples/stdlib_stack.abs --stdlib-profile default --out-dir generated --clean
```

`--dry-run` writes no files and prints compose output to stdout after successful parse/validate/lower.

## 7. `sync`: Regex-Driven Multi-File Compilation

Syntax:

```bash
abstack sync --input-dir <dir> [options]
```

Options:

1. `--input-dir <dir>` (required)
2. `--file-regex <pattern>` (default `.*\\.abs$`)
3. `--service-regex <pattern>`
4. `--out-dir <dir>`
5. `--compose-file <file>`
6. `--clean`
7. `--stdlib-profile <name>`
8. `--list-stdlib-profiles`

Processing behavior:

1. Files are discovered recursively and sorted.
2. Each source file is namespaced to avoid template/service name collisions.
3. `depends_on` entries inside each file are remapped to the namespaced service IDs.
4. Service filtering matches both namespaced and original base service names.

Example:

```bash
abstack sync \
  --input-dir samples \
  --file-regex '.*\\.abs$' \
  --service-regex '^(api|worker|db)$' \
  --out-dir generated \
  --clean
```

## 8. Stdlib Profiles (Opt-In)

The bundled stdlib is linked before semantic validation/lowering only when explicitly requested.

List available profiles:

```bash
abstack stdlib list
abstack build --list-stdlib-profiles
```

Enable stdlib in generation commands:

```bash
abstack build samples/stdlib_stack.abs --stdlib-profile default --out-dir generated
abstack sync --input-dir samples --stdlib-profile core-v1 --out-dir generated
```

Current bundled profile aliases:

1. `core-v1`
2. `default` (alias to `core-v1`)

## 9. `fmt`: Canonical Formatting

Syntax:

```bash
abstack fmt <file-or-dir> [options]
```

Options:

1. `--file-regex <pattern>` (directory mode only, default `.*\\.abs$`)
2. `--check`
3. `--stdout` (single-file mode only)

Behavior:

1. Formatting is semantic/canonical (AST-based), not token-preserving.
2. Comments are currently not preserved.
3. In `--check` mode, exit status is `1` if any file requires formatting.

Examples:

```bash
abstack fmt samples/unified.abs
abstack fmt samples --file-regex '.*\\.abs$' --check
abstack fmt samples/unified.abs --stdout
```

## 10. `compose`: Generate + Run Compose

Syntax:

```bash
abstack compose [generation options] -- <docker compose args...>
```

Generation choices:

1. `--abs <file.abs>` for single-file generation.
2. `--input-dir <dir>` plus sync options for multi-file generation.
3. Omit both to run against an existing compose file.
4. `--stdlib-profile <name>` is supported when generation inputs are present.

Important rules:

1. `--` is required before compose arguments.
2. `--abs` and `--input-dir` are mutually exclusive.
3. If generation options are omitted, compose file must already exist.
4. `--stdlib-profile` requires `--abs` or `--input-dir` in compose mode.

Examples:

```bash
abstack compose --abs samples/unified.abs -- up -d
abstack compose --abs samples/stdlib_stack.abs --stdlib-profile default -- up -d
abstack compose --compose-file generated/docker-compose.generated.yml -- ps
abstack compose --input-dir samples --file-regex '.*\\.abs$' -- down
```

## 11. `docker`: Minimal Runtime Helper

Goal: provide lightweight container operations directly in `abstack` while staying close to native Docker behavior.

Subcommands:

1. `abstack docker ls [--all] [--filter <regex>] [--watch <seconds>]`
2. `abstack docker inspect <container>`
3. `abstack docker logs <container> [--tail <lines>] [--follow]`
4. `abstack docker shell <container> [--shell <command>] [--user <user>]`
5. `abstack docker stats [--all]`

Examples:

```bash
abstack docker ls --all --filter 'api|db'
abstack docker ls --watch 2
abstack docker inspect api-container
abstack docker logs api-container --tail 300 --follow
abstack docker shell api-container --shell bash --user root
abstack docker stats --all
```

Scope boundaries:

1. This is not a replacement for Docker Desktop or a full orchestration UI.
2. It is intended for fast terminal-native inspection and debugging loops.

## 12. `tui`: Optional Curses UI

Run:

```bash
abstack tui
```

Availability:

1. Present only when built with curses support.
2. Otherwise exits with a clear enablement hint.

Current key actions:

1. `b`: build one file
2. `f`: format one file
3. `s`: sync one directory
4. `q`: quit

## 13. Regex Tips

Common patterns:

1. All abs files: `.*\\.abs$`
2. Only under a domain folder: `^services/payments/.*\\.abs$`
3. Core service names: `^(api|worker|db)$`
4. Match suffixed variants: `api(-.*)?$`

Remember:

1. Escape `\` for shell when needed.
2. Test patterns on small scopes first.
3. `sync` file matching is evaluated against source paths relative to `--input-dir`.

## 14. CI Patterns

Minimal validation pipeline:

```bash
abstack fmt samples --file-regex '.*\\.abs$' --check
abstack build samples/unified.abs --out-dir generated --clean
abstack build samples/stdlib_stack.abs --stdlib-profile default --out-dir generated --clean
```

Monorepo-style generation:

```bash
abstack sync --input-dir samples --file-regex '.*\\.abs$' --out-dir generated --clean
```

Compose smoke workflow (when Docker is available in CI):

```bash
abstack compose --abs samples/unified.abs -- config
```

## 15. Failure Modes and Diagnostics

1. Invalid regex: command reports which option/pattern failed.
2. Missing required args: command reports missing option/value.
3. No service match: check `--service-regex` and namespacing assumptions.
4. Missing compose file: generate via `build`/`sync` or provide `--compose-file`.
5. Docker access errors: confirm daemon availability and user permissions.
6. Unknown stdlib profile: run `abstack stdlib list` or `--list-stdlib-profiles`.
7. Shell command visibility and callback history: inspect `.abstack/logs/abstack-cli.log`.

## 16. Quick Decision Guide

1. "I changed one file and want artifacts now": `build`.
2. "I have many `.abs` files and want one merged output": `sync`.
3. "I want bundled reusable templates without local declarations": `--stdlib-profile`.
4. "I need style consistency checks": `fmt --check`.
5. "I want compose lifecycle from generated artifacts": `compose`.
6. "I need fast container inspection/logs/shell": `docker`.
