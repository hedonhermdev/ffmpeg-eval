#include <stdlib.h>
#include <stdio.h>

#include "../include.h"

int extract(chunk_data* args) {
    fprintf(stderr, "args->chunk_num: %d\n", args->chunk_num);

    return 0;
}
