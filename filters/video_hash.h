#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include "debug_tools.h" //DELETE
#include "filter.h"
#include <emmintrin.h>

typedef struct video_hash
{
    uint8_t *random_numbers; // size of
    int reverse;
    uint64_t seed;

    int first;
} video_hash;

void filter_video_hash_init(filters_path *filter_step);

AVFrame *filter_video_hash(filters_path *filter_props, AVFrame *frame);

void filter_video_hash_destroy(filters_path *filter_props);

// w_nb_blocks and h_nb_blocks can be 0
filters_path *filter_video_hash_create(uint64_t seed, int reverse,
                                       int w_nb_blocks,
                                       int h_nb_blocks);