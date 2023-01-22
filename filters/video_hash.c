#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include "debug_tools.h" //DELETE
#include "filter.h"
#include "video_hash.h"
#include <emmintrin.h>

void filter_video_hash_init(filters_path *filter_step)
{
}

AVFrame *filter_video_hash(filters_path *filter_props, AVFrame *frame)
{
    int real_heights[AV_NUM_DATA_POINTERS] = {0};
    int block_size = 16;

    __m128i src, modifier, result;
    modifier = _mm_set1_epi8(0b11111111);

    switch (frame->format)
    {

    case AV_PIX_FMT_YUV420P:
        real_heights[0] = frame->height;
        real_heights[1] = frame->height / 2;
        real_heights[2] = frame->height / 2;
        break;

    default:
        fprintf(stderr, "Unsupported pixel format %d %s %d", frame->format, __FILE__, __LINE__);
        exit(1);
        break;
    }
    printf("%i\n", frame->linesize[0] * real_heights[0]);
    printf("%i\n", frame->linesize[1] * real_heights[1]);
    printf("%i\n", frame->linesize[2] * real_heights[2]);

    for (int z = 0; z < AV_NUM_DATA_POINTERS; z++)
    {
        for (int x = 0; x < frame->linesize[z] * real_heights[z]; x += block_size)
        {
            src = _mm_load_si128((__m128i *)(frame->data[z] + x));
            result = _mm_xor_si128(modifier, src);
            _mm_storeu_si128((__m128i *)(frame->data[z] + x), result);
        }
    }
    return frame;
}

void filter_video_hash_destroy(filters_path *filter_props) {}

// w_nb_blocks and h_nb_blocks can be 0
filters_path *filter_video_hash_create(uint64_t seed, int reverse,
                                       int w_nb_blocks,
                                       int h_nb_blocks)
{
    filters_path *filter_step = filter_path_create();
    filter_step->init = filter_video_hash_init;
    filter_step->uninit = filter_video_hash_destroy;
    filter_step->filter_frame = filter_video_hash;
    filter_step->multiple_output = 0;
    filter_step->filter_name = "video_hash";
    video_hash *filter_params = malloc(sizeof(video_hash));

    filter_params->seed = seed;
    filter_params->reverse = reverse;
    filter_params->seed = w_nb_blocks;
    filter_params->reverse = h_nb_blocks;

    filter_step->filter_params = filter_params;

    return filter_step;
}