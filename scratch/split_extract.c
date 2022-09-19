#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>

// hold the format info of the input file
AVFormatContext *ifmt_ctx;
AVCodecContext *dec_ctx;

int stream_idx;
AVStream *video_stream;
static int width, height;
enum AVPixelFormat pix_fmt;

static int frame_bufsize;
static int frame_linesize[4];
static uint8_t *frame_buf[4] = {NULL};


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

static int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx) {
    int ret, stream_index;
    AVStream *st;
    const AVCodec *dec = NULL;

    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

    if (ret < 0) {
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];

        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            return AVERROR(EINVAL);
        }

        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            return AVERROR(ENOMEM);
        }

        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
            return ret;
        }

        if ((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0) {
            return ret;
        }
        *stream_idx = stream_index;
    }

    return 0;
}

static int decode_pkt(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt) {
    int ret;

    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending packet for decoding: %d\n", ret);
        exit(1);
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        else if (ret > 0) {
            fprintf(stderr, "error during decoding\n");
            return -ret;
        }

        av_image_copy(frame_buf, frame_linesize, (const uint8_t **)frame->data, frame->linesize, pix_fmt, width, height);
    }

    return 0;
}

// #NOTE: assuming one packet == one frame here 
// this might not be true always
static void make_split(AVPacket* pkt, AVFrame* frame, int n_frames, uint8_t* video_buf) {
    int ret;
    for (int i = 0; i < n_frames; i++) {
        ret = av_read_frame(ifmt_ctx, pkt);
        if (ret < 0) {
            fprintf(stderr, "error reading frame");
            break;
        }
        if (pkt->stream_index == stream_idx) {
            decode_pkt(dec_ctx, frame, pkt);
        }
    }
}

int main(int argc, char** argv) {
    fprintf(stderr, "hello world\n");
    int ret;
    const char* filename;
    char *p;
    int num_splits;
    int nb_frames;
    uint8_t*** splits[num_splits];
    FILE* dst_file;
    char fname[1024];
    AVPacket* pkt;
    AVFrame* frame;

    if (argc < 3) {
        fprintf(stderr, "here\n");
        fprintf(stderr, "usage: %s <filename> <num_splits>", argv[0]);
    }

    filename = argv[1];
    num_splits = strtol(argv[2], &p, 10);

    // 1. open input file
    ret = open_in_file(filename);
    if (ret < 0) {
        fprintf(stderr, "failed to open input file\n");
        exit(1);
    }

    // 2. Dump video information to stderr
    av_dump_format(ifmt_ctx, 0, filename, 0);

    // 3. Find and initialize the appropriate decoder the video stream.
    ret = open_codec_context(&stream_idx, &dec_ctx, ifmt_ctx);
    if (ret < 0) {
        fprintf(stderr, "couldn't open decoder for video stream\n");
        exit(1);
    }

    video_stream = ifmt_ctx->streams[stream_idx];
    width = dec_ctx->width;
    height = dec_ctx->height;
    pix_fmt = dec_ctx->pix_fmt;
    nb_frames = video_stream->nb_frames;

    int frames = nb_frames / num_splits;

    ret = av_image_alloc(frame_buf, frame_linesize, width, height, pix_fmt, 1);
    if (ret < 0) {
        fprintf(stderr, "could not allocate image buffer");
        exit(1);
    }
    frame_bufsize = ret;

    pkt = av_packet_alloc();
    frame = av_frame_alloc();

    for (int i = 0; i < num_splits; i++) {
        snprintf(fname, sizeof(fname), "out-%d.raw", i);
        dst_file = fopen(fname, "wb");

        uint8_t *split_buf, *bufptr;


        split_buf = malloc(frame_bufsize * frames);
        if (!split_buf) {
            fprintf(stderr, "couldn't allocate buffer");
            exit(1);
        }
        bufptr = split_buf;

        for (int f = 0; f < frames; f++) {
            ret = av_read_frame(ifmt_ctx, pkt);
            if (ret < 0) {
                break;
            }

            if (pkt->stream_index == stream_idx) {
                decode_pkt(dec_ctx, frame, pkt);
            }
            memcpy(bufptr, frame_buf[0], frame_bufsize);
            bufptr += frame_bufsize;
        }

        fwrite(split_buf, frame_bufsize*frames, 1, dst_file);
        fclose(dst_file);
    }

    decode_pkt(dec_ctx, frame, NULL);
    avformat_close_input(&ifmt_ctx);
    avcodec_free_context(&dec_ctx);
    av_frame_free(&frame);

    return 0;
}
