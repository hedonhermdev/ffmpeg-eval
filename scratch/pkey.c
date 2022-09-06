#define _GNU_SOURCE
#include <sys/mman.h>

int main() {
        pkey_mprotect(0x0, 4096, 0, 1);
}
