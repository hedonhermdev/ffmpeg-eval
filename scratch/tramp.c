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

    void* domain_addr = mmap(NULL, 1024*1024, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (domain_addr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    printf("domain_addr: %p\n", domain_addr);
    ret = pkey_mprotect(domain_addr, 1024*1024, PROT_READ | PROT_WRITE, 1);
    
    if (ret < 0) {
        perror("pkey_mprotect");
        exit(1);
    }

    struct elf_domain* dom_a = load_elf_domain("add.so", 1);

    void* add_ptr = find_symbol(dom_a, "_add");

    int (*add_fn)(int,int);
    add_fn = add_ptr;
    printf("%d", add_fn(34, 35));

    return 0;
}
