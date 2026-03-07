# OrbisTTY

OrbisTTY is a simple terminal-style text renderer for **PlayStation 4 homebrew** that provides a `printf`-like interface using **sceVideoOut** and **FreeType**.

**Note:** OrbisTTY was only tested with the provided Inconsolata-Regular.ttf!

## Features

- `printf`-style text output
- Automatic line scrolling
- FreeType font rendering
- Double buffered rendering
- Lightweight and easy to integrate

## Example

```cpp
#include "OrbisTTY.h"

#define printf(...) OrbisTTY::orbis_printf(__VA_ARGS__)

int main() {
    if (!OrbisTTY::init("/app0/assets/fonts/Inconsolata-Regular.ttf"))
        return 1;

    for (int i = 0; ; i++)
        printf("Homebrew counter %d", i);

    return 0;
}
