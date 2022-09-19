#include <libavutil/frame.h>
#include <stdio.h>
#include <libavformat/avformat.h>

int add(int a, int b) {
    AVFrame* frame = av_frame_alloc();
    fprintf(stderr, "inside add\n");
	return a + b;
}
