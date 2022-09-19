#include <stdlib.h>
#include <inttypes.h>

typedef struct chunk_data {
    uint8_t chunk_num;
    size_t chunk_size;
    size_t frame_size;
    uint8_t* frame_buf;
} chunk_data;

typedef struct split_data {
    uint8_t *video_buffer;
    size_t video_buffer_size;
    int num_chunks;
    chunk_data* chunks;
    uint8_t* chunks_buffer;
} split_data;

