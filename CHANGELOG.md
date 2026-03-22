# Changelog

All notable changes to this project are documented in this file.

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
