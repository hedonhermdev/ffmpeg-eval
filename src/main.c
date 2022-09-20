#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "../user-trampoline/buddy_domain.h"
#include "../user-trampoline/include.h"
#include "../user-trampoline/loader.h"
#include "../user-trampoline/rt.h"

#include "include.h"

#define CHUNKS_BUFFER_DOMAIN 13
#define UNUSED_ARG 0
#define GB1 1024 * 1024 * 1024

int pkey_mprotect(void *addr, size_t len, int prot, int pkey);

int (*_split)(split_data *args) __attribute__((section(".ro_trampoline_data")));;
int (*_extract)(chunk_data *args) __attribute__((section(".ro_trampoline_data")));;

int fsize(FILE *fp) {
  struct stat sb;

  int ret = fstat(fileno(fp), &sb);
  if (ret < 0) {
    perror("fstat");
    return -1;
  }

  return sb.st_size;
}

int main(int argc, char **argv) {
  int ret;
  char *filename;
  FILE *fp;
  uint8_t *video_buffer;
  size_t video_buffer_size;

  /* RT_init(); */
  MPK_init();

  if (argc < 2) {
    fprintf(stderr, "usage: %s <filename>", argv[0]);
    return 1;
  }

  filename = argv[1];

  fp = fopen(filename, "rb");
  if (fp == NULL) {
    perror("fopen");
    return 1;
  }

  video_buffer_size = fsize(fp);
  video_buffer = mmap(NULL, video_buffer_size, PROT_READ | PROT_WRITE,
      MAP_PRIVATE, fileno(fp), 0);
  if (!video_buffer) {
    perror("mmap");
    return 1;
  }
  ret = pkey_mprotect(video_buffer, video_buffer_size, PROT_READ | PROT_WRITE, 13);
  if (ret < 0) {
    perror("pkey_mprotect");
    return 1;
  }

  chunk_data* chunks = domain_alloc(sizeof(chunk_data) * 128, 12);
  uint8_t* chunks_buffer = domain_alloc(GB1, 12);

  split_data *split_args = domain_alloc(sizeof(split_data), 1);
  if (!split_args) {
    fprintf(stderr, "domain_alloc: failed\n");
    return 1;
  }

  split_args->video_buffer = video_buffer;
  split_args->video_buffer_size = video_buffer_size;
  split_args->num_chunks = 5;
  split_args->chunks = chunks;
  split_args->chunks_buffer = chunks_buffer;

  struct elf_domain *dom_a = load_elf_domain("split.so", 1);
  /* struct elf_domain *dom_a = load_elf_domain("add.so", 2); */
  void (*dom_a_buddy_init)(void *start, void *end) =
      find_symbol(dom_a, "buddy_init");
  void *dom_a_heap = domain_alloc(GB1, 1);
  dom_a_buddy_init(dom_a_heap, dom_a_heap + GB1);

  struct elf_domain *dom_b = load_elf_domain("extract.so", 2);
  void (*dom_b_buddy_init)(void *start, void *end) = find_symbol(dom_b, "buddy_init");
  void *dom_b_heap = domain_alloc(GB1, 2);
  dom_b_buddy_init(dom_b_heap, dom_b_heap + GB1);

  _split = find_symbol(dom_a, "split");
  _extract = find_symbol(dom_b, "extract");
  /* int (*_add)(int, int) = find_symbol(dom_a, "add"); */

  extern size_t PKEY_A;
  extern int pkey_l, pkey_m;

  // give domain A access to domains L and M
  PKEY_A = PKEY_A & !( ( 3 << ( pkey_l*2 ) ) );
  PKEY_A = PKEY_A & !( ( 3 << ( pkey_l*2 ) ) );
 
  ret = split(split_args, UNUSED_ARG, UNUSED_ARG, 13);
  /* ret = _split(split_args); */

  for (int i = 0; i < split_args->num_chunks; i++) {
      ret = extract(&split_args->chunks[i], UNUSED_ARG, UNUSED_ARG, 12);
      printf("extract returned: %d\n", ret);
  }

  /* ret = _add(1000, 10233); */

  printf("done\n");

  return 0;
}
