#define CPL_IMPL
#include <cpl/cpl.h>

#include "networking.h"
#include "game.h"

int main(void) {
    networking_init();
    game_run();
    networking_close();
}
