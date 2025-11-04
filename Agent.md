# Agent Guide for ORTEAF

This document gives AI/automation agents a quick reference for working in this repository.

## Project Snapshot
- **Language / Tooling:** C++20, CMake, Ninja, clang/gcc; GoogleTest for unit tests; Doxygen for API docs.
- **Targets:** `orteaf` static library plus helper interface targets `orteaf_user` and `orteaf_extension`.
- **Key Options (root CMakeLists):**
  - `ENABLE_CPU` (default `ON`), `ENABLE_CUDA`, `ENABLE_MPS` toggle runtimes.
  - `ORTEAF_RUNTIME_STATS_LEVEL` (`0/1/2`) with per-component overrides (`ORTEAF_CPU_STATS_LEVEL`, etc., `AUTO` inherits).
  - These values propagate to both the library and tests via `ORTEAF_ENABLE_*` and `ORTEAF_*_STATS_LEVEL` macros.

## Repository Layout
```
orteaf/                # Library sources
  include/orteaf/
    user/              # Public wrappers (PImpl front end)
    extension/         # Extension points (Kernel, Ops, TensorImpl, ModuleImpl)
    internal/          # Core runtime/allocator/diagnostics implementations
      diagnostics/
        error/         # Common error data + throw/fatal helpers
        log/           # Compile-time logging macros and sinks
  src/…                # Mirrors the include tree (user/extension/internal)
tests/                 # Mirrors the include layout for TDD-focused suites
docs/
  developer/design.md  # Architecture + access boundaries
  developer/testing-strategy.md # Checklist-oriented TDD guidance
  developer/environment.md      # Docker/shared environment instructions
  Doxyfile             # Doxygen config (output to docs/api/)
docker/dev/Dockerfile  # Linux dev container with cmake/clang/doxygen
scripts/setup-mps.sh   # Placeholder for macOS Metal setup
```

## Build & Test Workflow
1. Configure from the project root:  
   `cmake -S . -B build -DENABLE_CUDA=ON -DORTEAF_RUNTIME_STATS_LEVEL=1`
2. Build: `cmake --build build`
3. To generate docs (requires Doxygen):  
   `doxygen docs/Doxyfile` → output under `docs/api/html/`.
4. Tests currently require Googletest via FetchContent (network access or vendored dependency). Tests are expected to mirror the `user/extension/internal` structure.

## Documentation Links
- Architecture overview: `docs/developer/design.md`
- Extension guidelines: `docs/developer/extension-guide.md`
- Environment setup (Docker, MPS placeholder, WSL): `docs/developer/environment.md`
- Doxygen configs: `docs/Doxyfile` (core) / `docs/Doxyfile.tests` (tests)
- Testing checklist for TDD: `docs/developer/testing-strategy.md`
- Roadmap / challenge log templates: `docs/roadmap.md`, `docs/challenge-log.md`

## Notes for Agents
- Respect the access layers: user-facing code belongs in `user/`, extension points in `extension/`, internal logic in `internal/`.
- When adding tests, start from the checklist in `testing-strategy.md` and create files under the matching `tests/<layer>/…` directory.
- Do not delete the placeholder translation unit `orteaf/src/internal/placeholder.cpp` until real sources are present (the library needs at least one TU).
- Generated docs (`docs/api/`) are ignored via `.gitignore`; avoid committing them.
- Docker image (`docker/dev`) offers a reproducible environment; CUDA support would require a separate image based on `nvidia/cuda`.

Keep this file updated as new subsystems or workflows are introduced so future agents can ramp up quickly.
