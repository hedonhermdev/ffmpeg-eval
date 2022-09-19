#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "../src/split/split.h"

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

  chunk_data* chunks = malloc(sizeof(chunk_data) * 128);
  uint8_t* chunks_buffer = malloc(1024 * 1024 * 1024);

  split_args->video_buffer = video_buffer;
  split_args->video_buffer_size = video_buffer_size;
  split_args->num_chunks = 5;
  split_args->chunks = chunks;
  split_args->chunks_buffer = chunks_buffer;

  void *handle = dlopen("split.so", RTLD_NOW);
  if (handle == NULL) {
    fprintf(stderr, "dlopen: %s\n", dlerror());
    return 1;
  }

  int (*_split)(split_data *) = dlsym(handle, "split");
  if (_split == NULL) {
    fprintf(stderr, "dlsym: %s\n", dlerror());
    return 1;
  }

  ret = _split(split_args);
  printf("_split returned: %d\n", ret);

  return 0;
}
