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
    float *random_nb = (float *)params->random_nb;

    for (int i = 0; i < params->nb_samples; i++)
    {
        params->seed = next_lfsr(params->seed);
        random_nb[i] = (float)(params->seed % 1000000) / 1000000.0;
        /*        printf("the random number %f\n", (float)params->seed);
                printf("the random number (the value that should be behind the comma) %ld\n", params->seed % 1000000);
                printf("the value I get %f\n", random_nb[i]);
                printf("the value I want %f\n", (float)(params->seed % 1000000) / 1000000.0);*/
        // printf("%f\n", random_nb[i]);
    }
}

AVFrame *filter_audio_hash_apply(filters_path *filter_step, AVFrame *frame)
{

    filter_audio_hash *params = filter_step->filter_params;

    // int frame_len = frame->nb_samples * av_get_bytes_per_sample(params->fmt);

    float **data = (float **)frame->data;
    float *random_nb = (float *)params->random_nb;
    for (int i = 0; i < frame->nb_samples; i++)
    {
        for (int ch = 0; ch < frame->channels; ch++)
        {
            float reandom_nb = fabsf(random_nb[i]);

            if (params->reverse == 1)
                reandom_nb = 1 / reandom_nb;
            
            data[ch][i] *= reandom_nb;
            if (i < 5)
            {

                printf("%i) %f * %f", params->reverse, data[ch][i], reandom_nb);
                printf(" = %f\n", data[ch][i]);
            }
        }
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
