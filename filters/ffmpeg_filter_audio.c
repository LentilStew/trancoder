#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include "filters/filter.h"
#include <string.h>

#include "ffmpeg_filter_audio.h"
#include <libavfilter/avfilter.h>
void filter_ffmpeg_audio_init(filters_path *filter_step)
{
    ffmpeg_filters_params_audio *params = (ffmpeg_filters_params_audio *)filter_step->filter_params;
    char args[512];
    int ret = 0;

    const AVFilter *abuffersrc = avfilter_get_by_name("abuffer");
    const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");

    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();

    params->filter_graph = avfilter_graph_alloc();

    if (!outputs || !inputs || !params->filter_graph)
    {
        ret = AVERROR(ENOMEM);
        return;
    }

    /*
    if (dec_ctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
        av_channel_layout_default(&dec_ctx->ch_layout, dec_ctx->ch_layout.nb_channels);
*/
    char channel_layout_str[64];
    av_get_channel_layout_string(channel_layout_str, sizeof(channel_layout_str),
                                 params->channels, params->channel_layout);

    ret = snprintf(args, sizeof(args),
                   "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
                   params->time_base.num, params->time_base.den, params->sample_rate,
                   av_get_sample_fmt_name(params->sample_fmt),
                   channel_layout_str);

    printf("%s\n", args);
    /*
    av_get_channel_layout_string();
    av_channel_layout_describe(&dec_ctx->ch_layout, args + ret, sizeof(args) - ret);
*/
    ret = avfilter_graph_create_filter(&params->buffersrc_ctx, abuffersrc, "in",
                                       args, NULL, params->filter_graph);
    if (ret < 0)
    {
        printf("Cannot create audio buffer source\n");
        exit(0);
    }

    /* buffer audio sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&params->buffersink_ctx, abuffersink, "out",
                                       NULL, NULL, params->filter_graph);
    if (ret < 0)
    {
        printf("Cannot create buffer sink\n");
        exit(0);
    }

    ret = av_opt_set_int_list(params->buffersink_ctx, "sample_fmts", params->out_sample_fmts, AV_SAMPLE_FMT_NONE,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        printf("Cannot set output sample format\n");
        exit(0);
    }
    /*
        ret = av_opt_set(params->buffersink_ctx, "channel_layouts", "mono", AV_OPT_SEARCH_CHILDREN);
        if (ret < 0)
        {
            printf("Cannot set output channel layout %s\n", av_err2str(ret));
            exit(0);
        }*/

    ret = av_opt_set_int_list(params->buffersink_ctx, "sample_rates", params->out_sample_rates, -1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        printf("Cannot set output sample rate\n");
        exit(0);
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
    {
        printf("BIG TIME ERROR avfilter_graph_parse_ptr %s\n", av_err2str(ret));
        exit(1);
        return;
    }
    // avfilter_graph_parse2(params->filter_graph,params->filters_descr,&inputs, &outputs)
    if ((ret = avfilter_graph_config(params->filter_graph, NULL)) < 0)
    {
        printf("BIG TIME ERROR avfilter_graph_config %s\n", av_err2str(ret));
        exit(1);

        return;
    }
    /* Print summary of the sink buffer
     * Note: args buffer is reused to store channel layout string */
    const AVFilterLink *outlink;
    outlink = params->buffersink_ctx->inputs[0];
    av_log(NULL, AV_LOG_INFO, "Output: srate:%dHz fmt:%s chlayout:%s\n",
           (int)outlink->sample_rate,
           (char *)av_x_if_null(av_get_sample_fmt_name(outlink->format), "?"),
           args);

    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return;
}

AVFrame *filter_ffmpeg_audio(filters_path *filter_props, AVFrame *frame)
{
    /* push the decoded frame into the filtergraph */

    int res;

    ffmpeg_filters_params_audio *params = filter_props->filter_params;

    if (frame && av_buffersrc_add_frame_flags(params->buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
    {
        printf("Error while feeding the filtergraph\n");

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
        exit(1);
    }

    return new_frame;
}

void filter_ffmpeg_audio_destroy(filters_path *filter_props)
{
    ffmpeg_filters_params_audio *params = filter_props->filter_params;
    avfilter_graph_free(&params->filter_graph);
    free(params->filters_descr);
    free(params);
}

filters_path *filter_ffmpeg_audio_create(char *filters_descr,
                                         AVRational time_base,
                                         int sample_rate,
                                         int *out_sample_rates,
                                         enum AVSampleFormat sample_fmt,
                                         enum AVSampleFormat *out_sample_fmts,
                                         int64_t channel_layout,
                                         int channels)
{
    filters_path *filter_step = filter_path_create();
    filter_step->filter_frame = filter_ffmpeg_audio;
    filter_step->uninit = filter_ffmpeg_audio_destroy;
    filter_step->init = filter_ffmpeg_audio_init;
    filter_step->multiple_output = 1;

    ffmpeg_filters_params_audio *params = calloc(sizeof(ffmpeg_filters_params_audio), 1);
    params->time_base = time_base;
    params->sample_rate = sample_rate;
    params->channels = channels;
    params->out_sample_rates = out_sample_rates;
    params->sample_fmt = sample_fmt;
    params->out_sample_fmts = out_sample_fmts;
    params->channel_layout = channel_layout;
    params->filters_descr = strdup(filters_descr);

    filter_step->filter_params = params;
    return filter_step;
}
