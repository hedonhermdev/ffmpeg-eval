/*
 * Copyright (c) 2014 Stefano Sabatini
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/**
 * @file
 * libavformat AVIOContext API example.
 *
 * Make libavformat demuxer access media content through a custom
 * AVIOContext read callback.
 * @example doc/examples/avio_reading.c
 */
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdint.h>

const int avio_ctx_buffer_size = 4096;

struct buffer_data {
    uint8_t *ptr;
    size_t size;
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
    /* copy internal buffer data to buf */
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr  += buf_size;
    bd->size -= buf_size;
    return buf_size;
}

int read_buffer(struct buffer_data bd, AVFormatContext* fmt_ctx, uint8_t* avio_ctx_buffer, AVIOContext *avio_ctx)
{
    int ret;

    ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open input\n");
        return -1;
    }
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not find stream information\n");
        return -1;
    }
    av_dump_format(fmt_ctx, 0, "stream", 0);

    return 0;
}

int main(int argc, char **argv) {
    char *input_filename = NULL;
    int ret = 0;
    struct buffer_data bd = { 0 };
    FILE* fp;
    size_t video_buffer_size;
    uint8_t *video_buffer;

    uint8_t *avio_ctx_buffer = NULL;
    AVFormatContext *fmt_ctx = NULL;
    AVIOContext *avio_ctx = NULL;

    if (argc != 2) {
        fprintf(stderr, "usage: %s input_file\n"
                "API example program to show how to read from a custom buffer "
                "accessed through AVIOContext.\n", argv[0]);
        return 1;
    }

    input_filename = argv[1];

    fp = fopen(input_filename, "rb");
    if (fp == NULL) {
        perror("fopen");
        return 1;
    }

    video_buffer_size = fsize(fp);
    video_buffer = mmap(NULL, video_buffer_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fileno(fp), 0);

    bd.ptr  = video_buffer;
    bd.size = video_buffer_size;

    if (!(fmt_ctx = avformat_alloc_context())) {
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

    ret = read_buffer(bd, fmt_ctx, avio_ctx_buffer, avio_ctx);

end:
    avformat_close_input(&fmt_ctx);
    /* note: the internal buffer could have changed, and be != avio_ctx_buffer */
    av_freep(&avio_ctx->buffer);
    av_freep(&avio_ctx);
    if (ret < 0) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        return 1;
    }
    return ret;
}
