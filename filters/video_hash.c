#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>

#include "debug_tools.h" //DELETE
#include "filter.h"
#include "video_hash.h"
#include <emmintrin.h>
#include <immintrin.h>

void filter_video_hash_init(filters_path *filter_props, AVFrame *frame)
{
    video_hash *params = filter_props->filter_params;
    params->seed = (uint64_t)0x45354;
    params->block_height = 32;
    params->block_width = 32;
    block planes[AV_NUM_DATA_POINTERS];

    switch (frame->format)
    {

    case AV_PIX_FMT_YUV420P:
        planes[0].real_heights = frame->height;
        planes[1].real_heights = frame->height / 2;
        planes[2].real_heights = frame->height / 2;
        break;
    case AV_PIX_FMT_RGB24:
    case AV_PIX_FMT_BGR24:
        planes[0].real_heights = frame->height;
        planes[1].real_heights = frame->height;
        planes[2].real_heights = frame->height;
        break;
    default:
        fprintf(stderr, "Unsupported pixel format %d %s %d", frame->format, __FILE__, __LINE__);
        exit(1);
    }

    for (int z = 0; z < AV_NUM_DATA_POINTERS; z++)
    {
        planes[z].nb_blocks_height = planes[z].real_heights / params->block_height;
        planes[z].nb_blocks_width = frame->linesize[z] / params->block_width;

        planes[z].nb_blocks = planes[z].nb_blocks_width * planes[z].nb_blocks_height;
        planes[z].block_swap = malloc(sizeof(int) * planes[z].nb_blocks);
        planes[z].block_modifier = malloc(sizeof(char) * planes[z].nb_blocks);

        for (int i = 0; i < planes[z].nb_blocks; i++)
        {
            planes[z].block_swap[i] = i;
        }

        // shuffle
        for (int i = 0; i < planes[z].nb_blocks; i++)
        {
            params->seed = next_lfsr(params->seed);
            int tmp = planes[z].block_swap[i];
            planes[z].block_swap[i] = planes[z].block_swap[params->seed % planes[z].nb_blocks];
            planes[z].block_swap[params->seed % planes[z].nb_blocks] = tmp;
        }

        for (int i = 0; i < planes[z].nb_blocks; i++)
        {
            params->seed = next_lfsr(params->seed);
            planes[z].block_modifier[i] = (uint8_t)params->seed;
        }
        params->planes[z] = planes[z];
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

    for (int z = 0; z < AV_NUM_DATA_POINTERS; z++)
    {
        if (frame->linesize[z] == 0)
            continue;
        block plane = params->planes[z];
        for (int block = 0; block < plane.nb_blocks_height * plane.nb_blocks_width; block += 2)
        {
            int src_block = plane.block_swap[block];
            int dst_block = plane.block_swap[block + 1];

            int src_y_padding = (src_block / plane.nb_blocks_width) * frame->linesize[z] * params->block_height;
            int dst_y_padding = (dst_block / plane.nb_blocks_width) * frame->linesize[z] * params->block_height;

            int src_x_padding = (src_block % plane.nb_blocks_width) * params->block_width;
            int dst_x_padding = (dst_block % plane.nb_blocks_width) * params->block_width;

            uint8_t *src_block_start = frame->data[z] + src_x_padding + src_y_padding;
            uint8_t *dst_block_start = frame->data[z] + dst_x_padding + dst_y_padding;

            if (params->reverse)
            {
                src_modifier = _mm256_set1_epi8(plane.block_swap[block]);
                dst_modifier = _mm256_set1_epi8(plane.block_swap[block + 1]);
            }
            else
            {
                src_modifier = _mm256_set1_epi8(plane.block_swap[block + 1]);
                dst_modifier = _mm256_set1_epi8(plane.block_swap[block]);
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