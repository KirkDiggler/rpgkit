# Quality scorecard

_Last updated: 2026-06-10 (PR 1)_

Grades are honest, not aspirational. A = solid and trusted, D = known hazard.

| Component | Grade | Rationale |
|---|---|---|
| Build system (CMake + presets + Makefile) | A | Green on Linux (clang/gcc) and Windows (MSVC) in CI; one command (`make test`) from fresh clone |
| Lint pipeline (clang-format, clang-tidy) | A | `WarningsAsErrors`, `HeaderFilterRegex` covers the header-only library; CI runs exactly what `make pre-commit` runs |
| CI | A | Build/test matrix + lint job, all required |
| `rpg::core` | N/A | Only `version.hpp` exists; graded when behavior lands (PRs 2–4) |
| Docs | B | Living docs scaffolded with real content; architecture doc summarizes a design that code doesn't implement yet |
