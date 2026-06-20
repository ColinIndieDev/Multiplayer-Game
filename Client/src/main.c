#define CPL_IMPLEMENTATION
#include <cpl/cpl.h>

#include <cpstd/cpvec.h>

#include "networking.h"
#include "game.h"

int main(void) {
    networking_init();
    game_run();
    networking_close();
}
