#include "libavcodec/codec_par.h"
#include "libavcodec/packet.h"
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "../src/include.h"

unsigned long long rdtsc() {
    unsigned hi, lo;
    __asm__ volatile("rdtsc" : "=a" (lo), "=d" (hi));
    return ((unsigned long long)lo | (unsigned long long)hi << 32);
}

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

  split_data *split_args = malloc(sizeof(split_data));
  if (!split_args) {
    perror("malloc");
    return 1;
  }

  split_args->video_buffer = video_buffer;
  split_args->video_buffer_size = video_buffer_size;
  split_args->num_chunks = 3;
  split_args->chunks = malloc(sizeof(chunk_data) * 3);
  split_args->shared_allocator = &malloc;

  void *handle = dlopen("split.so", RTLD_NOW);
  if (handle == NULL) {
    fprintf(stderr, "dlopen: %s\n", dlerror());
    return 1;
  }
  void *handle2 = dlopen("extract.so", RTLD_NOW);
  if (handle2 == NULL) {
    fprintf(stderr, "dlopen: %s\n", dlerror());
    return 1;
  }

  int (*_split)(split_data *) = dlsym(handle, "split");
  if (_split == NULL) {
    fprintf(stderr, "dlsym: %s\n", dlerror());
    return 1;
  }

  int (*_extract)(chunk_data *) = dlsym(handle2, "extract");
  if (_extract == NULL) {
    fprintf(stderr, "dlsym: %s\n", dlerror());
    return 1;
  }

  size_t start = rdtsc();
  int split_ret = _split(split_args);

  for (int i = 0; i < split_args->num_chunks; i++) {
    ret = _extract(&split_args->chunks[i]);
    if (ret != 0) {
        break;
    }
  }

  size_t cycles = rdtsc() - start;

  printf("cycles: %lu\n", cycles);
  printf("_split returned: %d\n", ret);
  printf("extract returned: %d\n", ret);

  return 0;
}
