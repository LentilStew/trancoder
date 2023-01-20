#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include "filter.h"
#include "ffmpeg_filter_video.h"

void filter_ffmpeg_video_init(filters_path *filter_step)
{
    ffmpeg_filters_params *params = (ffmpeg_filters_params *)filter_step->filter_params;
    char args[512];
    int ret = 0;

    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");

    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    params->filter_graph = avfilter_graph_alloc();

    if (!outputs || !inputs || !params->filter_graph)
    {
        ret = AVERROR(ENOMEM);
        return;
    }

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             params->in_width, params->in_height, params->in_pix_fmt,
             params->time_base.num, params->time_base.den,
             params->sample_aspect_ratio.num, params->sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&params->buffersrc_ctx, buffersrc, "in",
                                       args, NULL, params->filter_graph);
    if (ret < 0)
    {
        printf("Cannot create buffer source\n");
        return;
    }

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&params->buffersink_ctx, buffersink, "out",
                                       NULL, NULL, params->filter_graph);
    if (ret < 0)
    {
        printf("Cannot create buffer sink\n");
        return;
    }

    ret = av_opt_set_int_list(params->buffersink_ctx, "pix_fmts", params->out_pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        printf("Cannot set output pixel format\n");
        return;
    }

    outputs->name = av_strdup("in");
    outputs->filter_ctx = params->buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = params->buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if ((ret = avfilter_graph_parse_ptr(params->filter_graph, params->filters_descr,
                                        &inputs, &outputs, NULL)) < 0)
        return;

    if ((ret = avfilter_graph_config(params->filter_graph, NULL)) < 0)
        return;

    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return;
}

AVFrame *filter_ffmpeg_video(filters_path *filter_props, AVFrame *frame)
{

    /* push the decoded frame into the filtergraph */

    int res;

    ffmpeg_filters_params *params = filter_props->filter_params;

    if (frame && av_buffersrc_add_frame_flags(params->buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");

        return NULL;
    }
    else if (filter_props->flushing)
    {
        res = av_buffersrc_add_frame_flags(params->buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
    }

    if (frame)
    {
        av_frame_free(&frame);
    }

    AVFrame *new_frame = av_frame_alloc();

    res = av_buffersink_get_frame(params->buffersink_ctx, new_frame);

    if (res == AVERROR(EAGAIN) || res == AVERROR_EOF)
    {
        av_frame_free(&new_frame);
        return NULL;
    }
    if (res < 0)
    {
        av_frame_free(&new_frame);
        return NULL;
    }

    return new_frame;
}

void filter_ffmpeg_video_destroy(filters_path *filter_props)
{
    ffmpeg_filters_params *params = filter_props->filter_params;
    avfilter_graph_free(&params->filter_graph);
    free(params->filters_descr);
    free(params);
}

filters_path *filter_ffmpeg_video_create(char *filters_descr, AVRational time_base, int in_pix_fmt,
                                         int *out_pix_fmts, int in_width, int in_height, AVRational sample_aspect_ratio)
{
    filters_path *filter_step = filter_path_create();
    filter_step->filter_frame = filter_ffmpeg_video;
    filter_step->uninit = filter_ffmpeg_video_destroy;
    filter_step->init = filter_ffmpeg_video_init;
    filter_step->multiple_output = 1;

    ffmpeg_filters_params *params = calloc(sizeof(ffmpeg_filters_params), 1);
    params->in_height = in_height;
    params->in_width = in_width;
    params->in_pix_fmt = in_pix_fmt;
    params->out_pix_fmts = out_pix_fmts;
    params->sample_aspect_ratio = sample_aspect_ratio;
    params->time_base = time_base;
    params->filters_descr = strdup(filters_descr);

    filter_step->filter_params = params;
    return filter_step;
}
