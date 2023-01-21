#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include "debug_tools.h" //DELETE
#include "filter.h"
#include <immintrin.h>
#include "audio_hash.h"
#include <float.h>
uint64_t next_lfsr(int64_t lfsr)
{
    uint64_t bit = ((lfsr >> 63) ^ (lfsr >> 61) ^ (lfsr >> 60) ^ (lfsr >> 31)) & 1;
    lfsr = (lfsr << 1) | bit;
    return lfsr;
}

void filter_audio_hash_init(filters_path *filter_step)
{
    filter_audio_hash *params = filter_step->filter_params;

    params->len = params->nb_samples * av_get_bytes_per_sample(params->fmt);

    params->random_nb = malloc(sizeof(uint8_t) * params->len);

    for (int i = 0; i < params->len; i++)
    {

        params->seed = next_lfsr(params->seed);
        printf("%i\n", (uint8_t)params->seed);
        params->random_nb[i] = (uint8_t)params->seed;
    }
}

void hash_frame_fltp(AVFrame *frame, filter_audio_hash *params)
{
    float **data = (float **)frame->data;
    uint16_t *random_nb = (uint16_t *)params->random_nb;
    for (int i = 0; i < frame->nb_samples; i++)
    {
        for (int ch = 0; ch < frame->channels; ch++)
        {
            int src_index = i;
            if (params->reverse == 1)
                src_index = frame->nb_samples - (i + 1);
            
            int dst_index = random_nb[src_index] % frame->nb_samples;

            float tmp = data[ch][src_index];
            data[ch][src_index] = data[ch][dst_index];
            data[ch][dst_index] = tmp;

            if (src_index < 5)
            {
                printf("data[ch][i] %f -> %f\n", data[ch][i], data[ch][dst_index]);
            }
        }
    }
}

AVFrame *filter_audio_hash_apply(filters_path *filter_step, AVFrame *frame)
{

    filter_audio_hash *params = filter_step->filter_params;
    switch (frame->format)
    {
    case AV_SAMPLE_FMT_FLTP:
        hash_frame_fltp(frame, params);
        break;
    default:
        break;
    }

    return frame;
}

void filter_audio_hash_destroy(filters_path *filter_props)
{
    filter_audio_hash *params = filter_props->filter_params;
    free(params->random_nb);
    free(params);
}

filters_path *filter_audio_hash_create(int64_t seed,
                                       int nb_samples,
                                       int nb_channels,
                                       enum AVSampleFormat fmt,
                                       int reverse)
{
    filters_path *filter_step = filter_path_create();
    filter_step->init = filter_audio_hash_init;
    filter_step->uninit = filter_audio_hash_destroy;
    filter_step->filter_frame = filter_audio_hash_apply;
    filter_step->multiple_output = 0;
    if (reverse)

        filter_step->filter_name = "audio hash reverse";
    else
        filter_step->filter_name = "audio hash";

    filter_audio_hash *filter_params = malloc(sizeof(filter_audio_hash));
    filter_params->reverse = reverse;
    filter_params->seed = seed;
    filter_params->nb_samples = nb_samples;
    filter_params->nb_channels = nb_channels;
    filter_params->fmt = fmt;

    filter_step->filter_params = filter_params;

    return filter_step;
}
