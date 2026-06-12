# Juice Appendix — Polish Your Terminal Game

Adoptable by any Campaign or Workshop checkpoint. Each checkpoint is
self-contained and tells you which tutorials it works with.

## Checkpoints

### JUICE-01: Color
ANSI escape codes for HP (red), damage (yellow), card names (cyan), healing
(green). Works with any tutorial that prints values.

```cpp
// Color helpers — stdlib only
#define RST "\033[0m"
#define RED "\033[31m"
#define GRN "\033[32m"
#define YLW "\033[33m"
#define CYN "\033[36m"

std::printf(RED "%d" RST " HP\n", hp);
std::printf(YLW "+%d" RST " damage\n", dmg);
```

### JUICE-02: HP bar
Unicode block characters. `██████░░` instead of `6/8 HP`.

```cpp
void printHpBar(int current, int max) {
    int filled = (current * 8 + max - 1) / max;  // 0..8
    for (int i = 0; i < 8; ++i) {
        std::printf("%s", i < filled ? "█" : "░");
    }
    std::printf(" %d/%d\n", current, max);
}
```

### JUICE-03: Clear on turn
Screen reset between turns. Uses stdlib escape codes.

```cpp
std::printf("\033[2J\033[H");  // clear screen, move cursor home
```

### JUICE-04: Pacing
Small delay after receipt printout so the player can read it.

```cpp
#include <thread>
#include <chrono>
// ...
std::this_thread::sleep_for(std::chrono::milliseconds(400));
```

### JUICE-05: FTXUI signpost
When escape codes aren't enough, FTXUI is the next step.

```cpp
// https://github.com/ArthurSonzogni/FTXUI
// Add to CMakeLists.txt:
//   FetchContent_Declare(ftxui ...)
//   target_link_libraries(my_game PRIVATE ftxui::screen)
```

## Going further

- **Menus and choice lists** — termios raw mode for arrow-key selection
- **Sprite art** — ANSI art for the goblin, the player, the boss
- **Animation** — damage numbers that scroll, health bars that tween
- **Beyond terminal** — signposts to SDL (graphical), Unreal plugin path
