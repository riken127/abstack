# CLI Reference (v0.4.0)

This is a command-by-command reference for `abstack`.

See also:

1. [CLI guide](/home/riken/programming/abstack/docs/cli-guide.md) for workflow explanations.
2. [CLI cookbook](/home/riken/programming/abstack/docs/cli-cookbook.md) for recipe-style examples.

## 1. Top-Level

```text
abstack <command> [options]
```

Commands:

1. `help`
2. `build`
3. `sync`
4. `fmt`
5. `compose`
6. `docker`
7. `tui`

Backward-compatible mode:

```text
abstack <file.abs> [build-options]
```

Equivalent to:

```text
abstack build <file.abs> [build-options]
```

## 2. `build`

```text
abstack build <file.abs> [--out-dir <dir>] [--compose-file <file>]
             [--service-regex <pattern>] [--clean] [--dry-run]
```

Options:

1. `--out-dir <dir>`: output directory (`generated` by default).
2. `--compose-file <file>`: custom compose output path.
3. `--service-regex <pattern>`: include only matching services.
4. `--clean`: remove stale generated artifacts before writing.
5. `--dry-run`: print compose output and skip file writes.

Exit behavior:

1. `0` on success.
2. non-zero on parse/semantic/lowering/emission errors.

## 3. `sync`

```text
abstack sync --input-dir <dir> [--file-regex <pattern>]
             [--service-regex <pattern>] [--out-dir <dir>]
             [--compose-file <file>] [--clean]
```

Options:

1. `--input-dir <dir>`: required recursive source root.
2. `--file-regex <pattern>`: source file matcher (default `.*\\.abs$`).
3. `--service-regex <pattern>`: service selector.
4. `--out-dir <dir>`: output directory.
5. `--compose-file <file>`: custom compose output path.
6. `--clean`: remove stale generated artifacts before writing.

Behavior:

1. Merges all selected `.abs` files into one plan.
2. Namespaces template/service IDs by source file.
3. Remaps in-file dependencies to namespaced IDs.

## 4. `fmt`

```text
abstack fmt <file-or-dir> [--file-regex <pattern>] [--check] [--stdout]
```

Options:

1. `--file-regex <pattern>`: directory mode selector (default `.*\\.abs$`).
2. `--check`: report drift and return non-zero if formatting is needed.
3. `--stdout`: print formatted output (single-file mode only).

Exit behavior:

1. `0` when formatting is clean/successful.
2. `1` for `--check` when at least one file needs formatting.
3. non-zero on parse errors or invalid options.

## 5. `compose`

```text
abstack compose [--abs <file.abs> | --input-dir <dir>] [sync/build options]
                [--compose-file <file>] -- <docker compose args...>
```

Generation options:

1. `--abs <file.abs>`: generate from one file first.
2. `--input-dir <dir>` + sync options: generate from many files first.
3. `--file-regex <pattern>`
4. `--service-regex <pattern>`
5. `--out-dir <dir>`
6. `--compose-file <file>`
7. `--clean`

Important:

1. `--` separator is required before compose arguments.
2. If generation flags are omitted, compose file must already exist.

## 6. `docker`

```text
abstack docker <subcommand> [options]
```

### `docker ls`

```text
abstack docker ls [--all] [--filter <regex>] [--watch <seconds>]
```

Options:

1. `--all`: include stopped containers.
2. `--filter <regex>`: regex on ID/name/image/status.
3. `--watch <seconds>`: refresh table continuously.

### `docker inspect`

```text
abstack docker inspect <container>
```

### `docker logs`

```text
abstack docker logs <container> [--tail <lines>] [--follow]
```

Options:

1. `--tail <lines>`: default `200`.
2. `--follow`: stream logs.

### `docker shell`

```text
abstack docker shell <container> [--shell <command>] [--user <user>]
```

Options:

1. `--shell <command>`: default `sh`.
2. `--user <user>`: run command as specified user.

### `docker stats`

```text
abstack docker stats [--all]
```

## 7. `tui`

```text
abstack tui
```

Requirements:

1. Binary must be built with curses support.
2. If not available, command exits with a clear error message.

## 8. Troubleshooting Quick Map

1. `Invalid regex`: check escaping/anchors in `--file-regex` and `--service-regex`.
2. `No services matched selection criteria`: widen service regex or inspect namespacing.
3. `Compose file not found`: run `build`/`sync`, or pass explicit `--compose-file`.
4. Docker daemon permission errors: ensure Docker socket/daemon access for current user.
