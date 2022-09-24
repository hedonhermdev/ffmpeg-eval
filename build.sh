#!/usr/bin/env bash

set -eu pipefail
set +x

rm -rf a.out split.so extract.so functions

nasm -felf64 -o functions src/functions.S

gcc -g -O1 src/main.c  user-trampoline/rt.c user-trampoline/loader.c functions -T user-trampoline/ls.ld -IFFmpeg

musl/obj/musl-gcc -g -O1  -static -shared -o extract.so src/extract/extract.c user-trampoline/buddy_domain.c FFmpeg/libavformat/libavformat.a FFmpeg/libavcodec/libavcodec.a FFmpeg/libavutil/libavutil.a -IFFmpeg/

musl/obj/musl-gcc -g -O1  -static -shared -o split.so src/split/split.c user-trampoline/buddy_domain.c FFmpeg/libavformat/libavformat.a FFmpeg/libavcodec/libavcodec.a FFmpeg/libavutil/libavutil.a -IFFmpeg/
