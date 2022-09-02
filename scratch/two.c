#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>

static int width, height;
static enum AVPixelFormat pix_fmt;
static AVStream *video_stream = NULL, *audio_stream = NULL;
static FILE *video_dst_file = NULL;
static uint8_t *video_dst_data[4] = {NULL};
static int video_dst_linesize[4];
static int video_dst_bufsize;

static AVFormatContext *ifmt_ctx;
int frame_idx = 1;

#define INBUF_SIZE 4096

static void open_out_file(int idx) {
    char fname[1024];

    snprintf(fname, sizeof(fname), "out-%d.raw", idx);

    video_dst_file = fopen(fname, "wb");

    if (!video_dst_file) {
        fprintf(stderr, "could not open outfile for writing");
        exit(1);
    }
}


static int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx,
                              AVFormatContext *fmt_ctx, enum AVMediaType type) {
    int ret, stream_index;
    AVStream *st;
    const AVCodec *dec = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file",
                av_get_media_type_string(type));
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];

        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }

        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }

        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type));
            return ret;
        }

        /* Init the decoders */
        if ((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }

    return 0;
}

static int open_in_file(const char *filename) {
    int ret;
    unsigned int i;

    ifmt_ctx = NULL;

    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
        fprintf(stderr, "cannot open input file");
        return ret;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        fprintf(stderr, "cannot find stream info");
        return ret;
    }

    return 0;
}

static void decode_pkt(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt) {
    int ret;

    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending packet for decoding\n");
        exit(1);
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret > 0) {
            fprintf(stderr, "error during decoding\n");
            exit(1);
        }
        av_image_copy(video_dst_data, video_dst_linesize,
            (const uint8_t **)(frame->data), frame->linesize,
            pix_fmt, width, height);
        printf("wrote %lu bytes\n", fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file));
    }
}

int main(int argc, char **argv) {
    const char *filename;
    const char outfilename;
    char *p;
    int num_splits;
    const AVCodec *codec;
    int duration;
    int ret;
    AVFrame *frame;
    AVPacket pkt;
    AVCodecContext *dec_ctx = NULL;
    int split_num = 1;
    int cut_time = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <filename> <num_splits>", argv[0]);
        exit(1);
    }

    filename = argv[1];
    num_splits = strtol(argv[2], &p, 10);
    printf("num_splits: %d\n", num_splits);

    ret = open_in_file(filename);
    if (ret < 0) {
        fprintf(stderr, "failed to open file");
        exit(1);
    }

    av_dump_format(ifmt_ctx, 0, filename, 0);

    codec = avcodec_find_decoder(ifmt_ctx->streams[0]->codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "codec not found\n");
        exit(1);
    }

    frame = av_frame_alloc();

    if (!frame) {
        fprintf(stderr, "could not allocate video frame");
        exit(1);
    }

    int stream_idx = -1;

    if (open_codec_context(&stream_idx, &dec_ctx, ifmt_ctx, AVMEDIA_TYPE_VIDEO) < 0) {
        fprintf(stderr, "couldn't open codec");
        exit(1);
    }


    video_stream = ifmt_ctx->streams[stream_idx];
    width = dec_ctx->width;
    height = dec_ctx->height;
    pix_fmt = dec_ctx->pix_fmt;
    ret = av_image_alloc(video_dst_data, video_dst_linesize, width, height, pix_fmt, 1);
    if (ret < 0) {
        fprintf(stderr, "could not allocate image buffer\n");
        exit(1);
    }
    video_dst_bufsize = ret;

    duration = video_stream->duration;
    cut_time += duration / num_splits;
    open_out_file(split_num++);

    while (1) {
        AVStream *in_stream;
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0) {
            break;
        }
        if (pkt.stream_index == stream_idx) {
            if (pkt.pts <= cut_time) {
                printf("pts: %llu\n", pkt.pts);
                decode_pkt(dec_ctx, frame, &pkt);
            } else {
                cut_time += duration / num_splits;
                open_out_file(split_num++);
                decode_pkt(dec_ctx, frame, &pkt);
            }
        }
    }

    printf("duration: %d\n", duration);

    printf("%d %dx%d\n", pix_fmt, width, height);

    return 0;
}
