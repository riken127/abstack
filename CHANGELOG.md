# Changelog

All notable changes to this project are documented in this file.

## [0.6.0] - 2026-03-22

### Added

1. Native Windows CI job in GitHub Actions to validate configure/build/test on `windows-latest`.
2. CLI callback event flow that prints tagged progress/events to screen.
3. Persistent CLI callback log file: `.abstack/logs/abstack-cli.log`.
4. Spinner feedback for wait-heavy non-interactive external command execution in CLI.
5. `ABSTACK_NO_SPINNER=1` environment toggle to disable spinner output.

### Changed

1. CMake presets now use the Ninja generator for cross-platform parity.
2. Docker helper and compose command execution paths use platform-aware quoting and normalized process exit handling.
3. Project version bumped to 0.6.0.
4. Tooling/CLI docs updated with Windows-native workflow and callback/spinner logging guidance.

## [0.5.0] - 2026-03-22

### Added

1. Bundled stdlib profile system (opt-in) linked in CLI generation flow.
2. New CLI stdlib support:
   - `--stdlib-profile <name>` on `build`, `sync`, and generation-enabled `compose`
   - `--list-stdlib-profiles` on `build`, `sync`, and `compose`
   - `abstack stdlib list`
3. Bundled `core-v1` stdlib templates (with `default` alias):
   - `std_v1_go_service`
   - `std_v1_node_service`
   - `std_v1_python_service`
   - `std_v1_static_site`
   - `std_v1_postgres`
   - `std_v1_redis`
4. Stdlib integration coverage in pipeline tests.
5. New stdlib sample: `samples/stdlib_stack.abs`.
6. Release notes for v0.5.0.

### Changed

1. Project version bumped to 0.5.0.
2. CLI help and command reference updated for stdlib profile workflows.
3. Language/spec/docs updated to clarify CLI-linked stdlib behavior.

## [0.4.0] - 2026-03-22

### Added

1. Utility-style CLI command set:
   - `build` for single-file compilation
   - `sync` for regex-driven multi-file composition generation
   - `fmt` for canonical `.abs` formatting and `--check` workflows
   - `docker` for lightweight container ops (`ls`, `inspect`, `logs`, `shell`, `stats`)
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
