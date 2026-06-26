#define CPL_IMPL
#include <cpl/cpl.h>

#include "network.h"

int main(void) {
    network_init();
    network_run();
    network_destroy();
}
