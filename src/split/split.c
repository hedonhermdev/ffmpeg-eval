#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>

#include "../include.h"
#include "libavcodec/codec_par.h"

struct buffer_data {
    uint8_t *ptr;
    size_t left;
};

void* (*shared_allocator)(size_t n);

static int copy_packet(AVPacket* dst, AVPacket *src) {
    int ret;

    ret = av_packet_copy_props(dst, src);
    if (ret < 0)
        return ret;

    dst->data = src->data;

    dst->size = src->size;

    uint8_t* data = shared_allocator(src->size + 64);
    if (!data) {
        return -1;
    }
    memset(data, 0, src->size + 64);
    memcpy(data, src->data, src->size);
    dst->buf = av_buffer_create(data, dst->size + 64, av_buffer_default_free, NULL, 0);
    if (!dst->buf) {
        abort();
    }

    dst->data = dst->buf->data;

    return 0;
}

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

static int read_buffer(struct buffer_data bd, AVFormatContext* fmt_ctx, uint8_t* avio_ctx_buffer, AVIOContext *avio_ctx)
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


    return 0;
}

int split(split_data *args) {
    int ret = 0;
    AVFormatContext *fmt_ctx = NULL;
    AVIOContext *avio_ctx = NULL;
    AVCodecContext *dec_ctx = NULL;
    AVPacket *pkt;
    AVFrame *frame;
    uint8_t *avio_ctx_buffer;
    size_t avio_ctx_buffer_size = 4096;

    int nb_frames, frames;

    if (args->num_chunks < 1) {
        return -1;
    }

    struct buffer_data bd = { args->video_buffer, args->video_buffer_size };

    fmt_ctx = avformat_alloc_context();
    if (fmt_ctx == NULL) {
        fprintf(stderr, "could not allocate fmt_ctx");
        return -1;
    }

    avio_ctx_buffer = av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        return AVERROR(ENOMEM);
    }
    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
            0, &bd, &read_pkt, NULL, NULL);
    if (!avio_ctx) {
        return AVERROR(ENOMEM);
    }
    fmt_ctx->pb = avio_ctx;

    ret = read_buffer(bd, fmt_ctx, avio_ctx_buffer, avio_ctx);
    if (ret < 0) {
        fprintf(stderr, "read_buf: failed\n");
        return -1;
    }

    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ret < 0) {
        goto end;
    }
    int stream_index = ret;

    shared_allocator = args->shared_allocator;
    
    pkt = av_packet_alloc();
    frame = av_frame_alloc();

    AVCodecParameters *codecpar, *shared_codecpar;
    codecpar = fmt_ctx->streams[stream_index]->codecpar;
    shared_codecpar = shared_allocator(sizeof(AVCodecParameters));
    memcpy(shared_codecpar, codecpar, sizeof(AVCodecParameters));
    shared_codecpar->extradata = shared_allocator(codecpar->extradata_size);
    memcpy(shared_codecpar->extradata, codecpar->extradata, codecpar->extradata_size);



    for (int i = 0; i < args->num_chunks; i++) {
        int num_pkts = 500;
        int pkt_idx = 0;

        AVPacket* pkts = shared_allocator(num_pkts * sizeof(AVPacket));
        memset(pkts, 0, num_pkts * sizeof(AVPacket));
        while (pkt_idx < num_pkts) {
            ret = av_read_frame(fmt_ctx, pkt);
            if (ret < 0) {
                break;
            }
            if (pkt->stream_index == stream_index) {
                copy_packet(&pkts[pkt_idx], pkt);
                pkt_idx += 1;
            }
        }
        args->chunks[i].chunk_num = i;
        args->chunks[i].codecpar = shared_codecpar;
        args->chunks[i].num_pkts = pkt_idx;
        args->chunks[i].pkts = pkts;
    }

end:
    if (ret < 0) return 1;

    return 0;
}
