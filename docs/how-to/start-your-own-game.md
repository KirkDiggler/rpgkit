# Recipe: start your own game project

**Goal:** your game in its own directory (and eventually its own repo) —
*not* inside the rpgkit clone. rpgkit is a dependency your project
downloads, like any library.

The tutorials happen inside the rpgkit clone because everything's already
set up there. Your actual game graduates out. Living proof these steps
work: [rpg-demo-game](https://github.com/KirkDiggler/rpg-demo-game) was
created from this page.

## 1. Make the project (four files)

```sh
mkdir -p my-game/src
cd my-game
```

**`CMakeLists.txt`** — the whole dependency story. FetchContent downloads
rpgkit from GitHub at the pinned release tag on first build; no install
step, no package manager:

```cmake
cmake_minimum_required(VERSION 3.25)
project(my_game LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
  rpgkit
  GIT_REPOSITORY https://github.com/KirkDiggler/rpgkit.git
  GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(rpgkit)

add_executable(my_game src/main.cpp)
target_link_libraries(my_game PRIVATE rpg::core)
```

**`Makefile`** — the short commands:

```make
.PHONY: build run clean

build:
	cmake -B build
	cmake --build build -j

run: build
	./build/my_game

clean:
	rm -rf build
```

**`.gitignore`**:

```
build/
compile_commands.json
.cache/
```

**`src/main.cpp`** — start from the tutorial 02 game
([`examples/tutorials/02_game_base/main.cpp`](../../examples/tutorials/02_game_base/main.cpp)
in the rpgkit repo): copy it in, then make it yours.

## 2. Build and run

```sh
make run
```

The first build clones rpgkit into `build/_deps/` (a minute or so); after
that it's cached. You should be playing the goblin fight from tutorial 02 —
except now it's *your* project.

## 3. Editor support

`CMAKE_EXPORT_COMPILE_COMMANDS` is already on. Point clangd at it with a
`.clangd` file in the project root:

```yaml
CompileFlags:
  CompilationDatabase: build
```

## Rules of thumb

- **Pin a tag, never a branch.** `GIT_TAG v0.1.0` means your game builds the
  same way next year. Upgrading rpgkit = changing that one line and
  rebuilding. Tags and what's in them: the
  [releases page](https://github.com/KirkDiggler/rpgkit/releases).
- **Never edit anything under `build/`** — including the rpgkit sources
  FetchContent puts in `build/_deps/`. If rpgkit seems to be missing
  something your game needs, that's an issue to file on rpgkit, not a file
  to edit.
- When the project is more than a toy, `git init` and push it — and if a
  model or agent works in the repo, give it an `AGENTS.md` with the house
  rules ([rpg-demo-game's](https://github.com/KirkDiggler/rpg-demo-game/blob/main/AGENTS.md)
  is a working template).

## Gotchas

- `rpg::core` (with the `::`) is the name you link; it comes from
  FetchContent. If CMake says the target doesn't exist, the
  `FetchContent_MakeAvailable(rpgkit)` line is missing or below your
  `target_link_libraries`.
- Building offline? The first configure needs the network (it clones
  rpgkit); after that, everything's local.
