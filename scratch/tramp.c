#define _GNU_SOURCE 

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>

#include "../user-trampoline/rt.h"
#include "../user-trampoline/loader.h"
#include "../user-trampoline/include.h"

int main() {
    int ret;

    RT_init();
    MPK_init();

    struct elf_domain* dom_a = load_elf_domain("add.so", 1);
    
    void* add_ptr = find_symbol(dom_a, "add");
    int (*add_fn)(int,int) = add_ptr;
    printf("add_ptr: %p\n", add_ptr);
    
    printf("add_res: %d\n", add_fn(34, 35));

    return 0;
}
