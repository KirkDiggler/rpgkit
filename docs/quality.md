# Quality scorecard

_Last updated: 2026-06-21 (v0.2.0 — customer-pressure alignment)_

Grades are honest, not aspirational. A = solid and trusted, D = known hazard.

| Component | Grade | Rationale |
|---|---|---|
| Build system (CMake + presets + Makefile) | A | Green on Linux (clang/gcc) and Windows (MSVC) in CI; one command (`make test`) from fresh clone |
| Lint pipeline (clang-format, clang-tidy) | A | `WarningsAsErrors`, `HeaderFilterRegex` covers the header-only library; CI runs exactly what `make pre-commit` runs |
| CI | A | Build/test matrix + lint job, all required |
| `rpg::core` | A- | All four contracts implemented and composed (40 tests incl. the v1 acceptance integration test); minus: Bus snapshot-copy cost unmeasured, no benchmark suite yet |
| Docs | A- | Tutorials 01–08, workshop tracks, and how-tos are live with example code in CI; externally validated by the demo-game experiment and now grounded by the rpgkit-ue customer-pressure board; minus: UE pressure has not yet been promoted into concrete core design/issues |
