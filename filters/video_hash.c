#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>

#include "debug_tools.h" //DELETE
#include "filter.h"
#include "video_hash.h"
#include <emmintrin.h>
#include <immintrin.h>



void
filter_video_hash_init(filters_path *filter_props, AVFrame *frame)
{
    video_hash *params = filter_props->filter_params;
    params->seed = (uint64_t)0x45354;
    params->block_height = 32;
    params->block_width = 32;
    switch (frame->format)
    {

    case AV_PIX_FMT_YUV420P:
        params->real_heights[0] = frame->height;
        params->real_heights[1] = frame->height / 2;
        params->real_heights[2] = frame->height / 2;
        break;

    default:
        fprintf(stderr, "Unsupported pixel format %d %s %d", frame->format, __FILE__, __LINE__);
        exit(1);
    }

    params->nb_blocks_height = params->real_heights[0] / params->block_height;
    params->nb_blocks_width = frame->linesize[0] / params->block_width;

    params->nb_blocks = params->nb_blocks_width * params->nb_blocks_height;
    params->block_swap = malloc(sizeof(int) * params->nb_blocks);
    params->block_modifier = malloc(sizeof(char) * params->nb_blocks);

    for (int i = 0; i < params->nb_blocks; i++)
    {
        params->block_swap[i] = i;
    }

    // shuffle
    for (int i = 0; i < params->nb_blocks; i++)
    {
        params->seed = next_lfsr(params->seed);
        int tmp = params->block_swap[i];
        params->block_swap[i] = params->block_swap[params->seed % params->nb_blocks];
        params->block_swap[params->seed % params->nb_blocks] = tmp;
    }

    for (int i = 0; i < params->nb_blocks; i++)
    {
        params->seed = next_lfsr(params->seed);
        params->block_modifier[i] = (uint8_t)params->seed;
    }

    return;
}

AVFrame *filter_video_hash(filters_path *filter_props, AVFrame *frame)
{

    video_hash *params = filter_props->filter_params;
    if (params->first == 0)
    {
        filter_video_hash_init(filter_props, frame);
        params->first++;
    }

    __m256i src, dst, src_modifier, dst_modifier, src_result, dst_result;

    for (int z = 0; z < 1; z++)
    {

        if (frame->linesize[z] == 0)
            continue;
        int a = 0;

        for (int block = 0; block < params->nb_blocks_height * params->nb_blocks_width; block += 2)
        {
            int src_block = params->block_swap[block];
            int dst_block = params->block_swap[block + 1];
            src_block = a;
            dst_block = a + 1;
            a += 2;

            int src_y_padding = (src_block / params->nb_blocks_width) * frame->linesize[z] * params->block_height;
            int dst_y_padding = (dst_block / params->nb_blocks_width) * frame->linesize[z] * params->block_height;

            int src_x_padding = (src_block % params->nb_blocks_width) * params->block_width;
            int dst_x_padding = (dst_block % params->nb_blocks_width) * params->block_width;

            uint8_t *src_block_start = frame->data[z] + src_x_padding + src_y_padding;
            uint8_t *dst_block_start = frame->data[z] + dst_x_padding + dst_y_padding;

            if (src_x_padding + params->block_width > frame->linesize[z] || dst_x_padding + params->block_width > frame->linesize[z])
                continue;
            if ((src_block / params->nb_blocks_width) + params->block_height >= params->real_heights[z] || (dst_block / params->nb_blocks_width) + params->block_height >= params->real_heights[z])
                continue;

            if (params->reverse)
            {
                src_modifier = _mm256_set1_epi8(params->block_swap[block]);
                dst_modifier = _mm256_set1_epi8(params->block_swap[block + 1]);
            }
            else
            {
                src_modifier = _mm256_set1_epi8(params->block_swap[block + 1]);
                dst_modifier = _mm256_set1_epi8(params->block_swap[block]);
            }

            for (int y = 0; y < params->block_height; y++)
            {

                uint8_t *src_block_line_start = (frame->linesize[z] * y) + src_block_start;
                uint8_t *dst_block_line_start = (frame->linesize[z] * y) + dst_block_start;

                src = _mm256_load_si256((__m256i *)src_block_line_start);
                dst = _mm256_load_si256((__m256i *)dst_block_line_start);

                src_result = _mm256_xor_si256(src_modifier, src);
                dst_result = _mm256_xor_si256(dst_modifier, dst);

                _mm256_store_si256((__m256i *)src_block_line_start, dst_result);
                _mm256_store_si256((__m256i *)dst_block_line_start, src_result);
            }
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
    filter_step->init = NULL;
    filter_step->uninit = filter_video_hash_destroy;
    filter_step->filter_frame = filter_video_hash;
    filter_step->multiple_output = 0;
    filter_step->filter_name = "video_hash";
    filter_step->is_init = 1;

    video_hash *filter_params = malloc(sizeof(video_hash));

    filter_params->first = 0;
    filter_params->reverse = reverse;
    filter_step->filter_params = filter_params;

    return filter_step;
}