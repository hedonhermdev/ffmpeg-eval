#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

static AVFormatContext *ifmt_ctx;

#define INBUF_SIZE 4096

static void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize,
                     char *filename)
{
    FILE *f;
    int i;
 
    f = fopen(filename,"wb");
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}
 
static void decode_pkt(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt,
                   const char *filename)
{
    char buf[1024];
    int ret;
 
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        exit(1);
    }
 
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            exit(1);
        }
 
        printf("saving frame %3d\n", dec_ctx->frame_number);
        fflush(stdout);
 
        /* the picture is allocated by the decoder. no need to
           free it */
        snprintf(buf, sizeof(buf), "%s-%d.pgm", filename, dec_ctx->frame_number);
        pgm_save(frame->data[0], frame->linesize[0],
                 frame->width, frame->height, buf);
    }
}

static int open_codec_context(int *stream_idx,
                              AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
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

static int open_in_file(const char* filename) { 
    int ret;
    unsigned int i;

    ifmt_ctx = NULL;

    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
        fprintf(stderr, "cannot open input file");
        return ret;
    }

    int a = 100;
    printf("%d\n", a);

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        fprintf(stderr, "cannot find stream info"); 
        return ret;
    }

    return 0;
}

int main(int argc, char** argv) {
    const char *filename, *outfilename;
    const AVCodec *codec;
    AVCodecContext *dec_ctx = NULL;
    int ret;
    AVFrame *frame;
    AVPacket pkt;

    if (argc < 3) {
        fprintf(stderr, "provide input and output filenames");
        exit(1);
    }

    filename = argv[1];
    outfilename = argv[2];

    ret = open_in_file(filename);
    if (ret < 0) {
        fprintf(stderr, "failed to open input file");
        exit(1);
    }

    av_dump_format(ifmt_ctx, 0, filename, 0);

    codec = avcodec_find_decoder(ifmt_ctx->streams[0]->codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }

    int stream_idx = -1;

    if (open_codec_context(&stream_idx, &dec_ctx, ifmt_ctx, AVMEDIA_TYPE_VIDEO) < 0) {
        exit(1);
    }

    while (1) {
        AVStream *in_stream;
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0) {
            break;
        }

        if (pkt.stream_index == 0) {
            decode_pkt(dec_ctx, frame, &pkt, outfilename);
        }
    }

    decode_pkt(dec_ctx, frame, NULL, outfilename);
    avcodec_free_context(&dec_ctx);
    av_frame_free(&frame);
    return 0;
}
