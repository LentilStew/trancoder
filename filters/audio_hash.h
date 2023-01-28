#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include "debug_tools.h" //DELETE
#include "filters/filter.h"

#include <immintrin.h>

typedef struct filter_audio_hash
{
    uint8_t *random_nb;
    int len;
    int64_t seed;
    int reverse;
    int nb_samples;
    int nb_channels;
    enum AVSampleFormat fmt;

} filter_audio_hash;

void filter_audio_hash_init(filters_path *filter_step);

AVFrame *filter_audio_hash_apply(filters_path *filter_step, AVFrame *frame);

void filter_audio_hash_destroy(filters_path *filter_props);

// reverse 0 don't reverse, 1 reverse
filters_path *filter_audio_hash_create(int64_t seed,
                                       int nb_samples,
                                       int nb_channels,
                                       enum AVSampleFormat fmt,
                                       int reverse);