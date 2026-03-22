# CLI Cookbook (v0.4.0)

This cookbook provides copy/paste command recipes for common `abstack` workflows.

## 1. First Compile Loop

Build one file and clean previous generated outputs:

```bash
abstack build samples/unified.abs --out-dir generated --clean
```

Expected result:

1. `generated/Dockerfile.<service>` files.
2. `generated/docker-compose.generated.yml`.

## 2. Preview Compose Without Writing Files

Use dry-run when iterating on semantics and service wiring:

```bash
abstack build samples/unified.abs --dry-run
```

Use this in code review to inspect projected compose changes quickly.

## 3. Generate Only a Service Subset

Compile only selected services from one source:

```bash
abstack build samples/unified.abs --service-regex '^(api|db)$' --clean
```

Useful for partial local debugging or focused CI smoke stages.

## 4. Merge Many `.abs` Files from a Directory Tree

Compile recursively with path filtering:

```bash
abstack sync \
  --input-dir samples \
  --file-regex '.*\\.abs$' \
  --out-dir generated \
  --clean
```

Tip:

1. `--file-regex` is matched against paths relative to `--input-dir`.
2. Keep folders predictable so regex patterns stay simple.

## 5. Multi-File + Service Filter

Generate from many files, but only include matching service names:

```bash
abstack sync \
  --input-dir samples \
  --file-regex '.*\\.abs$' \
  --service-regex '^(api|worker|db)$' \
  --out-dir generated
```

`sync` supports matching both base service names and namespaced IDs.

## 6. Format One File

```bash
abstack fmt samples/unified.abs
```

## 7. Format Check in CI

```bash
abstack fmt samples --file-regex '.*\\.abs$' --check
```

Non-zero exit means one or more files need formatting.

## 8. Print Formatted Output to Stdout

```bash
abstack fmt samples/unified.abs --stdout
```

Useful for editor integration or pre-commit preview.

## 9. Run Compose Lifecycle with Generation

Generate from one source and bring services up:

```bash
abstack compose --abs samples/unified.abs -- up -d
```

List running services from generated compose:

```bash
abstack compose --compose-file generated/docker-compose.generated.yml -- ps
```

Bring them down:

```bash
abstack compose --compose-file generated/docker-compose.generated.yml -- down
```

## 10. Compose Validation in CI

Check generated compose validity without starting containers:

```bash
abstack compose --abs samples/unified.abs -- config
```

## 11. Minimal Container Dashboard

List active containers:

```bash
abstack docker ls
```

Watch refresh every 2 seconds:

```bash
abstack docker ls --watch 2
```

Show all containers and filter by regex:

```bash
abstack docker ls --all --filter 'api|db|worker'
```

## 12. Investigate a Running Container

Inspect full metadata:

```bash
abstack docker inspect api-container
```

Tail and follow logs:

```bash
abstack docker logs api-container --tail 200 --follow
```

Open shell as root:

```bash
abstack docker shell api-container --shell bash --user root
```

Snapshot resource usage:

```bash
abstack docker stats --all
```

## 13. Optional TUI Mode

Launch:

```bash
abstack tui
```

Keys:

1. `b`: build file
2. `f`: format file
3. `s`: sync directory
4. `q`: quit

If unavailable, rebuild with `ABSTACK_ENABLE_TUI=ON`.

## 14. Regex Snippet Bank

File regex:

1. All sources: `.*\\.abs$`
2. Payments subtree: `^payments/.*\\.abs$`
3. Exclude by convention (positive match): `^(core|services)/.*\\.abs$`

Service regex:

1. Exactly api and db: `^(api|db)$`
2. All workers: `worker`
3. Prefix family: `^billing-`

## 15. Suggested Make Targets

```make
fmt-check:
	abstack fmt samples --file-regex '.*\\.abs$$' --check

gen:
	abstack sync --input-dir samples --file-regex '.*\\.abs$$' --out-dir generated --clean

up:
	abstack compose --compose-file generated/docker-compose.generated.yml -- up -d

down:
	abstack compose --compose-file generated/docker-compose.generated.yml -- down
```
