# Dev setup

## Linux

```bash
sudo apt install cmake ninja-build clang clangd clang-format clang-tidy
git clone https://github.com/KirkDiggler/rpgkit.git
cd rpgkit
make test
```

`make test` configures with the `debug` preset (Ninja, `-Wall -Wextra -Werror`),
builds, and runs ctest. The first configure downloads GoogleTest via FetchContent,
so it needs network access once.

## Make verbs

| Verb | What it does |
|---|---|
| `make configure` | `cmake --preset debug` — generates `build/debug/` incl. `compile_commands.json` |
| `make build` | Build the debug preset |
| `make test` | Build + `ctest --preset debug --output-on-failure` |
| `make fmt` | `clang-format -i` over all `.hpp`/`.cpp` |
| `make fmt-check` | Format check, fails on drift |
| `make tidy` | `run-clang-tidy` over the compilation database |
| `make lint` | `fmt-check` + `tidy` |
| `make pre-commit` | `lint` + `test` — **run before every push**; CI enforces the same |
| `make clean` | Remove `build/` |

## Windsurf (clangd)

1. Install the **clangd** extension. If the Microsoft C/C++ extension is
   installed, disable its IntelliSense for this workspace — two language servers
   fight over the same buffers.
2. Run `make configure` once so `build/debug/compile_commands.json` exists.
3. The checked-in `.clangd` file points clangd at it:

   ```yaml
   CompileFlags:
     CompilationDatabase: build/debug
   ```

4. Verify: open `core/include/rpg/core/version.hpp` and go-to-definition on
   `kVersion` from the test file. If clangd complains about missing headers,
   re-run `make configure`.

## Coming from Go

| Go world | C++ world here |
|---|---|
| gopls | **clangd** — same role (LSP); needs `compile_commands.json` instead of `go.mod` to know the build |
| go module | **CMake target** — `rpg::core` is the unit you depend on and link; the monorepo is many targets, like a multi-module workspace |
| `go get` | **FetchContent** — declares a dependency by git tag, downloaded and built inside your build tree at configure time |
| testify suite | **gtest fixture** — `TEST_F(MyFixture, Name)` with `SetUp()`/`TearDown()` instead of `s.SetupTest()`; `make test` ≈ `go test ./...` |
