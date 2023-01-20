#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include "filter.h"

typedef struct audio_resample_params
{
    int in_sample_rate;
    int out_sample_rate;
    int64_t in_channel_layout;
    int64_t out_channel_layout;
    int out_frame_nb_sample;

    enum AVSampleFormat in_sample_fmt;
    enum AVSampleFormat out_sample_fmt;
    SwrContext *resample_context;

} audio_resample_params;

void audio_resample_init(filters_path *filter_step);

AVFrame *audio_resample(filters_path *filter_step, AVFrame *frame);

void audio_resample_destroy(filters_path *filter_step);

filters_path *audio_resample_create(int in_sample_rate, int out_sample_rate,
                                    int64_t in_channel_layout, int64_t out_channel_layout,
                                    enum AVSampleFormat in_sample_fmt, enum AVSampleFormat out_sample_fmt);