#include "libavcodec/packet.h"
#include "libavutil/frame.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>

#include "split.h"

struct buffer_data {
    uint8_t *ptr;
    size_t left;
};

int stream_idx;
AVStream *video_stream;
static int width, height;
enum AVPixelFormat pix_fmt;

static int frame_bufsize;
static int frame_linesize[4];
static uint8_t *frame_buf[4] = {NULL};

static int read_pkt(void *opaque, uint8_t *buf, int buf_size)
{
    struct buffer_data *bd = (struct buffer_data *)opaque;
    buf_size = FFMIN(buf_size, bd->left);

    if (!buf_size)
        return AVERROR_EOF;

    /* copy internal buffer data to buf */
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr  += buf_size;
    bd->left -= buf_size;

    return buf_size;
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

int read_buffer(struct buffer_data bd, AVFormatContext* fmt_ctx, uint8_t* avio_ctx_buffer, AVIOContext *avio_ctx)
{
    int ret;

    ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open input: %d\n", AVERROR(ret));
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

int split(split_data *args) {
    int ret = 0;
    AVFormatContext *fmt_ctx = NULL;
    AVIOContext *avio_ctx = NULL;
    AVCodecContext *dec_ctx = NULL;
    AVPacket *pkt;
    AVFrame *frame;

    uint8_t *buffer = NULL;
    size_t buffer_size = 0;
    uint8_t *avio_ctx_buffer = NULL;
    size_t avio_ctx_buffer_size = 4096;
    int nb_frames, frames;

    struct buffer_data bd = { args->video_buffer, args->video_buffer_size };

    if (args->num_chunks < 1) {
        return -1;
    }

    fmt_ctx = avformat_alloc_context();
    if (fmt_ctx == NULL) {
        fprintf(stderr, "could not allocate fmt_ctx");
        ret = -1;
        goto end;
    }
    avio_ctx_buffer = av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
            0, &bd, &read_pkt, NULL, NULL);
    if (!avio_ctx) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    fmt_ctx->pb = avio_ctx;

    ret = read_buffer(bd, fmt_ctx, avio_ctx_buffer, avio_ctx);
    if (ret < 0) {
        fprintf(stderr, "read_buf: failed\n");
        return 1;
    }

    ret = open_codec_context(&stream_idx, &dec_ctx, fmt_ctx);
    if (ret < 0) {
        fprintf(stderr, "failed to open codec context\n");
        goto end;
    }

    video_stream = fmt_ctx->streams[stream_idx];
    width = dec_ctx->width;
    height = dec_ctx->height;
    pix_fmt = dec_ctx->pix_fmt;
    nb_frames = video_stream->nb_frames;
    frames = nb_frames / args->num_chunks;

    pkt = av_packet_alloc();
    frame = av_frame_alloc();

    uint8_t* chunks_buffer_ptr = args->chunks_buffer;

    uint8_t *split_buf, *bufptr;
    split_buf = args->chunks_buffer;
    bufptr = split_buf;

    
    /* for (int i = 0; i < args->num_chunks; i++) { */
    /*     for (int f = 0; f < frames; f++) { */
    /*         ret = av_read_frame(fmt_ctx, pkt); */
    /*         if (ret < 0) { */
    /*             break; */
    /*         } */

    /*         if (pkt->stream_index == stream_idx) { */
    /*             decode_pkt(dec_ctx, frame, pkt); */
    /*         } */
    /*         memcpy(bufptr, frame_buf[0], frame_bufsize); */
    /*         bufptr += frame_bufsize; */
    /*     } */

    /*     args->chunks[i].chunk_num = i; */
    /*     args->chunks[i].chunk_size = bufptr - split_buf; */
    /*     args->chunks[i].frame_buf = split_buf; */
    /*     args->chunks[i].frame_size = frame_bufsize; */

    /*     split_buf = bufptr; */
    /* } */
end:
    if (dec_ctx) {
        decode_pkt(dec_ctx, frame, NULL);
        avcodec_free_context(&dec_ctx);
    }
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
    }
    if (avio_ctx) {
        av_freep(&avio_ctx->buffer);
        avio_context_free(&avio_ctx);
    }

    if (ret < 0) return 1;

    return ret;
}
