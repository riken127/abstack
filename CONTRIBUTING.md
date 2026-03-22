# Contributing

Thank you for helping improve `abstack`. The goal is to keep the project small, readable, and easy to extend.

## Development Loop

1. Configure the native build:
   `cmake --preset native`
2. Build it:
   `cmake --build --preset native`
3. Run the test harness:
   `ctest --test-dir build/native --output-on-failure`

If you want to experiment with vcpkg-backed dependencies later, use the `vcpkg` preset after setting `VCPKG_ROOT`.

## Style

- Formatting follows `.clang-format`.
- Editor indentation and line endings follow `.editorconfig`.
- Static analysis is guided by `.clang-tidy`.
- `clangd` is the recommended LSP because it understands the project compile database and the C++20 standard out of the box.
- Prefer high-signal comments for non-obvious behavior (invariants, edge cases, generation semantics, security/shell boundaries).
- Avoid line-by-line narration; if code is self-evident, skip the comment.

## Commits

Use conventional commits with a short, clear body when the change needs context. A good pattern is:

- `feat(tooling): add CMake presets`
- `docs(tooling): describe clangd and formatting workflow`
- `ci: add ubuntu ctest`

Keep the subject line imperative and the body focused on the why, not just the what.
