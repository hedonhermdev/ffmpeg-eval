#define _GNU_SOURCE
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

struct buffer_data {
    uint8_t *ptr;
    size_t size; ///< size left in the buffer
};

int fsize(FILE* fp) {
  struct stat sb;

  int ret = fstat(fileno(fp), &sb);
  if (ret < 0) {
    perror("fstat");
    return -1;
  }

  return sb.st_size;
}

static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    struct buffer_data *bd = (struct buffer_data *)opaque;
    buf_size = FFMIN(buf_size, bd->size);
    printf("ptr:%p size:%zu\n", bd->ptr, bd->size);
    /* copy internal buffer data to buf */
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr  += buf_size;
    bd->size -= buf_size;
    return buf_size;
}

AVFormatContext* read_buffer(struct buffer_data bd)
{
    int ret;
    AVFormatContext *fmt_ctx = NULL;
    AVIOContext *avio_ctx = NULL;
    uint8_t *buffer = NULL, *avio_ctx_buffer = NULL;
    size_t buffer_size, avio_ctx_buffer_size = 4096;

    if (!(fmt_ctx = avformat_alloc_context())) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    avio_ctx_buffer = av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    avio_ctx_buffer = av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
                                  0, &bd, &read_packet, NULL, NULL);
    if (!avio_ctx) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    fmt_ctx->pb = avio_ctx;
    ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open input\n");
        goto end;
    }
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not find stream information\n");
        goto end;
    }
    av_dump_format(fmt_ctx, 0, "stream", 0);
end:
    avformat_close_input(&fmt_ctx);
    /* note: the internal buffer could have changed, and be != avio_ctx_buffer */
    av_freep(&avio_ctx->buffer);
    av_freep(&avio_ctx);
    av_file_unmap(buffer, buffer_size);
    if (ret < 0) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        return NULL;
    }
    return fmt_ctx;
}

int main(int argc, char** argv) {
  int ret;
  char *filename;
  FILE *fp;
  uint8_t* video_buffer;
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
  video_buffer = mmap(NULL, video_buffer_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, fileno(fp), 0);

    struct buffer_data bd = { 0 };

    bd.ptr = video_buffer;
    bd.size = video_buffer_size;

    AVFormatContext* fmt_ctx = read_buffer(bd);
    if (!fmt_ctx) {
        fprintf(stderr, "failed to read input\n");
    }

    return 0;

}
