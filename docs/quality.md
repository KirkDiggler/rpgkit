# Quality scorecard

_Last updated: 2026-06-11 (core v1 complete)_

Grades are honest, not aspirational. A = solid and trusted, D = known hazard.

| Component | Grade | Rationale |
|---|---|---|
| Build system (CMake + presets + Makefile) | A | Green on Linux (clang/gcc) and Windows (MSVC) in CI; one command (`make test`) from fresh clone |
| Lint pipeline (clang-format, clang-tidy) | A | `WarningsAsErrors`, `HeaderFilterRegex` covers the header-only library; CI runs exactly what `make pre-commit` runs |
| CI | A | Build/test matrix + lint job, all required |
| `rpg::core` | A- | All four contracts implemented and composed (40 tests incl. the v1 acceptance integration test); minus: Bus snapshot-copy cost unmeasured, no benchmark suite yet |
| Docs | A- | Tutorials 01–03 + how-tos + cookbook, all code CI-built; externally validated by the qwen experiment (a 30B model built a game from them); minus: tutorials 04+ still to come |
