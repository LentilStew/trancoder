#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include "filter.h"
#include <libavfilter/avfilter.h>

typedef struct ffmpeg_filters_params_audio
{
    AVRational time_base;
    int sample_rate;
    int *out_sample_rates;
    int64_t channel_layout;
    enum AVSampleFormat sample_fmt;
    enum AVSampleFormat *out_sample_fmts;
    int channels;
    char *filters_descr;

    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;
    AVFilterContext *buffersink_ctx;

} ffmpeg_filters_params_audio;

void filter_ffmpeg_audio_init(filters_path *filter_step);

AVFrame *filter_ffmpeg_audio(filters_path *filter_props, AVFrame *frame)

    ;
void filter_ffmpeg_audio_destroy(filters_path *filter_props);

filters_path *filter_ffmpeg_audio_create(char *filters_descr,
                                         AVRational time_base,
                                         int sample_rate,
                                         int *out_sample_rates,
                                         enum AVSampleFormat sample_fmt,
                                         enum AVSampleFormat *out_sample_fmts,
                                         int64_t channel_layout,
                                         int channels);