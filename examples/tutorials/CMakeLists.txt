# Tutorial checkpoints — each directory is the complete, runnable state of
# the game at the end of one tutorial in docs/tutorials/. CI builds them so
# the tutorials can never silently rot.
add_subdirectory(02_game_base)
add_subdirectory(03_cards_energy)
add_subdirectory(04_block)
add_subdirectory(05_monster_brain)
add_subdirectory(06_bus)
