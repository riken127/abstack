# Changelog

All notable changes to this project are documented in this file.

## [0.4.0] - 2026-03-22

### Added

1. Utility-style CLI command set:
   - `build` for single-file compilation
   - `sync` for regex-driven multi-file composition generation
   - `fmt` for canonical `.abs` formatting and `--check` workflows
   - `compose` as docker compose passthrough with optional pre-generation
2. Automatic per-file namespacing during `sync` merges to avoid template/service collisions.
3. Optional curses-backed TUI command (`abstack tui`) when built with TUI support.
4. Core formatter module (`abstack::format_ast`) for AST-to-canonical-source rendering.
5. New CLI documentation:
   - `docs/cli-guide.md`
6. Optional vcpkg manifest feature `tui` for curses dependency provisioning.

### Changed

1. Project version bumped to 0.4.0.
2. Build system now detects and enables TUI support when curses is available.
3. Pipeline tests now validate formatter output and reparse correctness.

### Notes

1. Compose integration is intentionally a thin wrapper around native `docker compose` behavior.
2. Formatter currently normalizes structure based on AST semantics and does not preserve comments.

## [0.3.0] - 2026-03-22

### Added

1. Multi-`use` support per service with deterministic stage namespacing.
2. Array syntax support for `cmd` and `entrypoint` (`["arg0", "arg1", ...]`).
3. Command-aware emission:
   - Dockerfile JSON-array command emission.
   - Compose YAML sequence command/entrypoint emission.
4. Additional sample programs:
   - `samples/quickstart.abs`
   - `samples/multi_use.abs`
   - `samples/microservices.abs`
5. Expanded documentation:
   - language guide
   - examples guide
   - updated specification and grammar

### Changed

1. Semantic validation now allows multiple `use` statements in services.
2. `depends_on` validation now requires referenced services to exist.
3. Project version bumped to 0.3.0.

### Quality

1. Integration tests expanded for multi-use, array command emission, and dependency validation.
2. Build and test pipeline remains green under native preset.
