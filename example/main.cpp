#include <stdio.h>
#include "OrbisTTY.h"

#define printf(...) OrbisTTY::orbis_printf(__VA_ARGS__)

int main() {
    if (!OrbisTTY::init("/app0/assets/fonts/Inconsolata-Regular.ttf"))
        return 1;

    for (int index = 0; ; index++)
        printf("This PS4 application counts to infinitie! %d", index);

    return 0;
}