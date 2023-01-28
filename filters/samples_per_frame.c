#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include "debug_tools.h" //DELETE
#include "filters/filter.h"

#include "samples_per_frame.h"

void filter_samples_per_frame_init(filters_path *filter_step)
{
    filter_samples_per_frame_params *params = filter_step->filter_params;

    AVFrame *frame = av_frame_alloc();
    frame->nb_samples = params->nb_samples;
    frame->channel_layout = av_get_default_channel_layout(params->nb_channels);
    frame->format = params->fmt;

    int res = av_frame_get_buffer(frame, 0);
    if (res != 0)
    {
        printf("failed allocating frame (filter_samples_per_frame_params )%s\n", av_err2str(res));
        exit(0);
    }

    params->frame = frame;
    params->offset = 0;
    return;
}

AVFrame *filter_samples_per_frame(filters_path *filter_props, AVFrame *frame)
{
    filter_samples_per_frame_params *params = filter_props->filter_params;
    if (frame)
    {

        if (params->in_frame)
            av_frame_free(&params->in_frame);
        params->in_frame = frame;
        params->in_frame_offset = 0;
    }

    while (1)
    {
        int frames_left = params->nb_samples - params->offset;

        if (frames_left > (params->in_frame->nb_samples - params->in_frame_offset))
            frames_left = params->in_frame->nb_samples - params->in_frame_offset;

        av_samples_copy(params->frame->data, params->in_frame->data, params->offset, params->in_frame_offset, frames_left, params->nb_channels, params->fmt);
        params->in_frame_offset += frames_left;
        params->offset += frames_left;

        if (params->nb_samples == params->offset)
        {

            AVFrame *new_frame = params->frame;

            params->frame = NULL;
            params->frame = av_frame_alloc();
            params->frame->nb_samples = params->nb_samples;
            params->frame->channel_layout = av_get_default_channel_layout(params->nb_channels);
            params->frame->format = params->fmt;
            params->offset = 0;
            int res = av_frame_get_buffer(params->frame, 0);
            if (res != 0)
            {
                printf("%s\n", av_err2str(res));
                exit(0);
            }
            return new_frame;
        }

        if (params->in_frame_offset == params->in_frame->nb_samples)
            return NULL;
    }
}

void filter_samples_per_frame_uninit(filters_path *filter_props)
{
    filter_samples_per_frame_params *params = filter_props->filter_params;
    free(params);
}

filters_path *filter_samples_per_frame_create(int nb_channels, int nb_samples, enum AVSampleFormat sample_fmt)
{
    filters_path *filter_step = filter_path_create();
    filter_step->init = filter_samples_per_frame_init;
    filter_step->uninit = filter_samples_per_frame_uninit;
    filter_step->filter_frame = filter_samples_per_frame;
    filter_step->multiple_output = 1;
    filter_step->filter_name = "samples_per_frame";
    printf("%i %i %i\n", nb_channels, nb_samples, sample_fmt);
    filter_samples_per_frame_params *filter_params = malloc(sizeof(filter_samples_per_frame_params));
    filter_params->frame = NULL;
    filter_params->in_frame = NULL;
    filter_params->fmt = sample_fmt;
    filter_params->nb_channels = nb_channels;
    filter_params->nb_samples = nb_samples;

    filter_step->filter_params = filter_params;
    return filter_step;
}
