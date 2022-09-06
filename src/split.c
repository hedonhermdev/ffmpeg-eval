#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

static AVFormatContext* ifmt_ctx = NULL;

static int open_in_file(const char* filename) {
    int ret;
    unsigned int i;

    ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL);

    if (ret < 0) {
        fprintf(stderr, "cannot open input file\n");
        return ret;
    }

    ret = avformat_find_stream_info(ifmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "cannot find stream info\n");
        return ret;
    }

    return 0;
}
