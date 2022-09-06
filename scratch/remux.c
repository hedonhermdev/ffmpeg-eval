#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

int main(int argc, char** argv) {
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;

    AVPacket *pkt = NULL;
    const char *in_filename;
    int ret, i;
    int stream_index = 0;
    int *stream_mapping = NULL;

    int stream_mapping_size = 0;

    if (argc < 2) {
        printf("usage: %s input", argv[0]);
        return 1;
    }

    in_filename = argv[1];

    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "could not allocate AVPacket\n");
        return 1;
    }

    ifmt_ctx = malloc(sizeof(AVFormatContext));

    ifmt_ctx->pb = NULL;
}
