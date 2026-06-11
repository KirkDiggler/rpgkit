# Tutorial 01 — Getting started

**Goal:** rpgkit on your machine, built, tested, and an example running.
Time: ~15 minutes, most of it waiting for installs.

**You need:** a Linux machine (or a Mac, or Windows with WSL) and a
terminal. Commands below are typed into the terminal; lines starting with
`#` are comments — don't type those.

## 1. Install the tools

```sh
sudo apt-get install -y git cmake ninja-build clang clangd clang-format clang-tidy
```

This is the C++ toolchain: `cmake` + `ninja` build the code, `clang`
compiles it, and the `clang-*` tools keep it tidy. You'll never call most
of these directly — a `Makefile` wraps them into short commands.

(Mac: `brew install cmake ninja llvm` covers the same ground.)

## 2. Get the code

```sh
git clone https://github.com/KirkDiggler/rpgkit.git
cd rpgkit
```

A quick map of what you just downloaded:

```
core/        the engine — the rules-pipeline machinery (you won't edit this)
examples/    small runnable programs, one idea each
docs/        guides like this one; docs/how-to/ has copy-paste recipes
Makefile     the short commands: make build, make test
```

## 3. Build it and prove it works

```sh
make test
```

The first run downloads and builds the test framework, then compiles
everything and runs the engine's tests — takes a minute or two. Success
looks like this at the end:

```
100% tests passed, 0 tests failed out of 22
```

If you see that line, your setup is complete and correct. (Numbers may be
bigger by the time you read this — what matters is `0 tests failed`.)

## 4. Run your first example

```sh
./build/debug/examples/chain_basics/chain_basics
```

You should see a sword swing resolve, with a receipt:

```
Strike! base damage 5
  rage (effects): 5 -> 7
  bless (effects): 7 -> 10
  vulnerable (final): 10 -> 20
final damage: 20
```

Run it twice. The **bless** line changes — it rolls a real 4-sided die
every run. The other lines never change. That receipt is rpgkit's signature
move, and tutorial 02 puts it in a game you can actually play.

## 5. Make a change and see it (the whole loop in one minute)

Open `examples/chain_basics/main.cpp` in any text editor. Find the rage
line:

```cpp
mustBeOk(damage.add("effects", "rage", [](int dmg) { return dmg + 2; }));
```

Change `+ 2` to `+ 10`. Save. Then:

```sh
make build
./build/debug/examples/chain_basics/chain_basics
```

The rage line on the receipt now adds 10. **That cycle — edit, `make
build`, run — is all programming is.** Everything in the next tutorials is
that loop, repeated.

Put it back to `+ 2` when you're done (or don't — it's your copy).

## If something goes wrong

- `make: command not found` → install `make`: `sudo apt-get install -y make`
- Errors mentioning `ninja` → step 1 didn't finish; rerun it.
- Anything else: copy the last ~10 lines of the error and ask your friendly
  neighborhood Claude — or open an issue on the repo. Both work.

**Next:** [Tutorial 02 — your first terminal game](02-your-first-terminal-game.md)
