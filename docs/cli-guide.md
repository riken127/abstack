# CLI Guide (v0.4.0)

`abstack` now provides a utility-style CLI with subcommands for generation, synchronization, formatting, and compose integration.

## 1. Command Overview

1. `build`: compile one `.abs` file.
2. `sync`: compile many `.abs` files from a directory using regex matching.
3. `fmt`: canonical formatter for `.abs` sources.
4. `docker`: lightweight container helper (`ls`, `inspect`, `logs`, `shell`, `stats`).
5. `compose`: optional wrapper around `docker compose` using generated compose output.
6. `tui`: optional curses UI (if built with TUI support).

Show help:

```bash
abstack help
```

## 2. Build

Compile a single file:

```bash
abstack build samples/unified.abs --out-dir generated
```

Options:

1. `--out-dir <dir>`
2. `--compose-file <file>`
3. `--service-regex <pattern>` to generate only matching services.
4. `--clean` to remove old generated `Dockerfile.*` files before writing.
5. `--dry-run` to validate/lower and print compose output without writing files.

Compatibility mode also works:

```bash
abstack samples/unified.abs --out-dir generated
```

## 3. Sync (Regex-driven generation)

Compile all matching `.abs` files from a directory and merge into one output plan:

```bash
abstack sync \
  --input-dir samples \
  --file-regex '.*\\.abs$' \
  --service-regex '^(api|worker|db)$' \
  --out-dir generated \
  --clean
```

Why this is useful:

1. Dynamic compose generation from many source files.
2. Service slicing by regex (`--service-regex`) for environment-specific output.
3. One-shot sync to keep generated artifacts aligned with sources.
4. Automatic template/service namespacing per source file to avoid name collisions during merge.

## 4. Format

Format one file in place:

```bash
abstack fmt samples/unified.abs
```

Check formatting in CI style:

```bash
abstack fmt samples --file-regex '.*\\.abs$' --check
```

Print formatted output (single file):

```bash
abstack fmt samples/unified.abs --stdout
```

Formatter behavior notes:

1. Produces canonical ordering based on the AST model.
2. Preserves valid syntax/values but not comments (comments are lexer-skipped currently).

## 5. Compose Integration

This is intentionally a thin wrapper, not a full orchestration replacement.

Generate from `.abs` then run compose:

```bash
abstack compose --abs samples/unified.abs -- up -d
```

Use sync mode + compose:

```bash
abstack compose \
  --input-dir samples \
  --file-regex '.*\\.abs$' \
  --service-regex '^api|db$' \
  -- up -d
```

Use existing generated compose without regenerating:

```bash
abstack compose --compose-file generated/docker-compose.generated.yml -- ps
```

## 6. Minimal Docker Ops Helper

The `docker` command group gives a very small built-in operational view without trying to replace full Docker tooling.

List running containers in a table:

```bash
abstack docker ls
```

Include stopped containers and filter by regex:

```bash
abstack docker ls --all --filter 'api|db'
```

Auto-refresh every 2 seconds (minimal htop-like behavior):

```bash
abstack docker ls --watch 2
```

Inspect container metadata:

```bash
abstack docker inspect my-container
```

Tail logs:

```bash
abstack docker logs my-container --tail 200
abstack docker logs my-container --tail 200 --follow
```

Open a shell inside a container:

```bash
abstack docker shell my-container
abstack docker shell my-container --shell bash --user root
```

One-shot stats table:

```bash
abstack docker stats
```

## 7. TUI (Optional)

If built with curses support, launch:

```bash
abstack tui
```

Current TUI actions:

1. `b` build file
2. `f` format file
3. `s` sync directory
4. `q` quit

## 8. Scope Clarification: “Full docker-compose integration”

A full compose lifecycle framework inside abstack would likely be out of scope for the compiler core.

Current compromise (implemented):

1. Abstack remains source-of-truth compiler.
2. `compose` subcommand provides convenient pass-through execution.
3. You still use native Docker Compose behavior directly, with generated files managed by abstack.
