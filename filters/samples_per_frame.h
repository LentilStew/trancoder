#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include "debug_tools.h" //DELETE
#include "filters/filter.h"

typedef struct filter_samples_per_frame_params
{
    AVFrame *in_frame;
    int in_frame_offset;

    AVFrame *frame;
    int offset;

    int nb_samples;
    int nb_channels;
    enum AVSampleFormat fmt;
} filter_samples_per_frame_params;

void filter_samples_per_frame_init(filters_path *filter_step);

AVFrame *filter_samples_per_frame(filters_path *filter_props, AVFrame *frame);

void filter_samples_per_frame_uninit(filters_path *filter_props);

filters_path *filter_samples_per_frame_create(int nb_channels, int nb_samples, enum AVSampleFormat sample_fmt);