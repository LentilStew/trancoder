#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include "debug_tools.h" //DELETE
#include "filter.h"
#include <emmintrin.h>
typedef struct block
{

    int real_heights;
    int nb_blocks_width;
    int nb_blocks_height;
    int nb_blocks;
    int *block_swap;
    uint8_t *block_modifier;

} block;
typedef struct video_hash
{
    uint64_t seed;


    int block_height;
    int block_width;
    int first;
    block planes[AV_NUM_DATA_POINTERS];
    int reverse;

} video_hash;


AVFrame *filter_video_hash(filters_path *filter_props, AVFrame *frame);

void filter_video_hash_destroy(filters_path *filter_props);

// w_nb_blocks and h_nb_blocks can be 0
filters_path *filter_video_hash_create(uint64_t seed, int reverse,
                                       int w_nb_blocks,
                                       int h_nb_blocks);