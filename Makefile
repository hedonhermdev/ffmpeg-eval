# clang -IFFmpeg/ src/main.c -LFFmpeg/libavutil -LFFmpeg/libavformat -LFFmpeg/libavcodec -lavformat -lavcodec -lavutil
CC=clang
CFLAGS=-g -c -O0
IPATH=-IFFmpeg/
LDPATH=-LFFmpeg/libavutil -LFFmpeg/libavformat -LFFmpeg/libavcodec 
LDLIBS=-lavformat -lavcodec -lavutil

eval: eval.o 
	$(CC) $(IPATH) $(LDPATH) $(LDLIBS) -o $@ -g $^ 

eval.o: src/main.c
	$(CC) $(CFLAGS) -o $@ $^

