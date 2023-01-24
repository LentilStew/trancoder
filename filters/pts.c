#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include "debug_tools.h" //DELETE
#include "filter.h"
#include "pts.h"

void filter_pts_init(filters_path *filter_step)
{
    filter_pts_params *params = filter_step->filter_params;

    AVStream *stream = params->stream;

    AVRational fps = params->fps;

    int frames_per_packet = 1;
    AVRational time_base = stream->time_base;

    if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        fps.den = 1;
        fps.num = stream->codecpar->sample_rate;
        frames_per_packet = stream->codecpar->frame_size;
        // For the audio there are 1024 (or 960) frames per packet https://stackoverflow.com/questions/23216103/about-definition-for-terms-of-audio-codec
    }
    params->frame_duration = (double)(av_q2d(av_div_q((AVRational){time_base.den, 1}, fps))) * (double)frames_per_packet;

    printf("\n\ntime_base.den %i\n", time_base.den);
    printf("fps %i/%i\n", fps.num, fps.den);
    printf("params->frame_duration %f\n", params->frame_duration);
    printf("frames_per_packet %i\n", frames_per_packet);

    printf("stream->codecpar->frame_size32 %i\n\n", stream->codecpar->frame_size);
}
int count = 0;

AVFrame *filter_pts(filters_path *filter_props, AVFrame *frame)
{
    filter_pts_params *params = filter_props->filter_params;

    if (!frame)
        return frame;
    /*
    AVStream *stream = params->stream;

    AVRational fps = params->fps;

    // int output_nb_samples = 1024;

    int frames_per_packet = 1;
    AVRational time_base = stream->time_base;
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        fps.den = 1;
        fps.num = stream->codecpar->sample_rate;

        frames_per_packet = frame->nb_samples;
    }
    // params->frame_duration = (double)(av_q2d(av_div_q((AVRational){time_base.den, 1}, fps))) * (double)frames_per_packet;*/
    frame->pkt_duration = params->frame_duration;
    frame->pts = params->next_pts;

    params->next_pts = params->next_pts + params->frame_duration;

    return frame;
}

void filter_pts_uninit(filters_path *filter_props)
{
    filter_pts_params *params = filter_props->filter_params;
    free(params);
}

filters_path *filter_pts_create(AVStream *stream, AVRational fps)
{
    filters_path *filter_step = filter_path_create();
    filter_step->init = filter_pts_init;
    filter_step->uninit = filter_pts_uninit;
    filter_step->filter_frame = filter_pts;
    filter_step->multiple_output = 0;
    filter_step->filter_name = "pts";
    filter_pts_params *filter_params = malloc(sizeof(filter_pts_params));

    filter_params->stream = stream;
    filter_params->fps = fps;
    filter_params->next_pts = 0;

    filter_step->filter_params = filter_params;

    return filter_step;
}
