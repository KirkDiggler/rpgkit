# rpgkit

Composable combat-systems toolkit for game builders — a C++ implementation of
[rpg-toolkit](https://github.com/KirkDiggler/rpg-toolkit)'s design.

rpgkit is four small, system-agnostic concepts that compose into *any*
triggered-effect combat system:

- a **Bus** that routes events,
- a **Chain** that collects and applies modifiers in a declared stage order,
- an **Action** that fires (the verb),
- an **Effect** that subscribes and modifies (the persistent listener).

D&D 5e is just a rulebook on top of those four. So is a co-op combat
deck-builder. rpgkit ships the components; the game builder assembles them.
Built to be embedded — Unreal (native) and, via a C ABI, Unity.

## Status

Early — not yet usable. The repo is being stood up; `rpg::core` v1 (Bus,
Chain, Topic, Action, Effect) is in progress.

## Design

The architecture and the decisions behind it live in
[rpg-project/ideas/portable-combat-core](https://github.com/KirkDiggler/rpg-project/tree/main/ideas/portable-combat-core).

## License

[MIT](LICENSE)
