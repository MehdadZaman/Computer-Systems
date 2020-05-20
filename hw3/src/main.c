#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {

    sf_mem_init();

    sf_mem_fini();

    return EXIT_SUCCESS;
}
