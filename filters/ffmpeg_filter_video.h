#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "filter.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/avfilter.h>

#include <libavutil/opt.h>
typedef struct ffmpeg_filters_params
{
    AVRational time_base;
    int in_pix_fmt;
    int *out_pix_fmts; // AV_PIX_FMT_NONE terminated

    int in_width;
    int in_height;
    AVRational sample_aspect_ratio;
     char *filters_descr;
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;

    AVFilterGraph *filter_graph;

} ffmpeg_filters_params;

void filter_ffmpeg_video_init(filters_path *filter_step);

AVFrame *filter_ffmpeg_video(filters_path *filter_props, AVFrame *frame);

void filter_ffmpeg_video_destroy(filters_path *filter_props);

filters_path *filter_ffmpeg_video_create(char *filters_descr, AVRational time_base, int in_pix_fmt,
                                         int *out_pix_fmts, int in_width, int in_height, AVRational sample_aspect_ratio);