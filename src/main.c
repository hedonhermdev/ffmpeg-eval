#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>


#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

struct buffer_data {
    uint8_t *ptr;
    size_t size;
};

long long fsize(char* path) {
    struct stat sb;

    if (stat(path, &sb) < 0) {
        perror("stat");
        return -1;
    }
    printf("size: %lld", sb.st_size);

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


int main(int argc, char** argv) {
    int ret;
    char* filename;
    FILE* fp;
    long long filesize;
    struct buffer_data bd;
    uint8_t* io_ctx_buffer;
    size_t io_ctx_buffer_size = 4096;
    AVIOContext *io_ctx = NULL;
    AVFormatContext *ifmt_ctx = NULL;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <filename>", argv[0]);
        return 1;
    }

    filename = argv[1];
    filesize = fsize(filename);

    fp = fopen(filename, "rb");

    if (fp == NULL) {
        perror("fopen");
        return 1;
    }

    printf("filesize: %lld\n", filesize);


    uint8_t* video_buffer = (uint8_t*) av_malloc(filesize * sizeof(uint8_t*));
    filesize = fread(video_buffer, 1, filesize, fp);

    bd.ptr = video_buffer;
    bd.size = filesize;


    io_ctx_buffer = av_malloc(io_ctx_buffer_size);
    io_ctx = avio_alloc_context(io_ctx_buffer, io_ctx_buffer_size, 0, &bd, read_packet, NULL, NULL);

    ifmt_ctx = avformat_alloc_context();
    ifmt_ctx->pb = io_ctx;

    ret = avformat_open_input(&ifmt_ctx, NULL, 0, 0);
    if (ret < 0) {
        fprintf(stderr, "failed ot open input");
        return 1;
    }

    // ret = avformat_find_stream_info(ifmt_ctx, NULL);
    // if (ret < 0) {
    //     fprintf(stderr, "failed to find stream info from i/p");
    //     return 1;
    // }

    av_dump_format(ifmt_ctx, 0, filename, 0);

    return 0;
}
