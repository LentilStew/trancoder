#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include "debug_tools.h" //DELETE
#include <libswscale/swscale.h>
#include "filters/filter.h"

#include "swscale.h"

void filter_sws_scale_init(filters_path *filter_step)
{
    swscale *filter_params = filter_step->filter_params;
    filter_params->sws_ctx = sws_getContext(
        filter_params->in_width,
        filter_params->in_height,
        filter_params->in_pixel_fmt,
        filter_params->out_width,
        filter_params->out_height,
        filter_params->out_pixel_fmt,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL);
}

AVFrame *filter_sws_scale(filters_path *filter_props, AVFrame *frame)
{
    swscale *filter_params = filter_props->filter_params;
    AVFrame *out_frame = av_frame_alloc();
    out_frame->width = filter_params->out_width;
    out_frame->height = filter_params->out_height;
    out_frame->format = filter_params->out_pixel_fmt;
    av_frame_get_buffer(out_frame, 32);
    sws_scale(filter_params->sws_ctx, (const uint8_t *const *)frame->data, frame->linesize, 0, frame->height, out_frame->data, out_frame->linesize);
    av_frame_free(&frame);
    return out_frame;
}
void filter_sws_scale_uninit(filters_path *filter_props)
{
    swscale *filter_params = filter_props->filter_params;
    sws_freeContext(filter_params->sws_ctx);
    free(filter_params);
}

filters_path *filter_sws_scale_create(int in_width, int in_height, int in_pixel_fmt, int out_width, int out_height, int out_pixel_fmt)
{
    filters_path *filter_step = filter_path_create();
    filter_step->init = filter_sws_scale_init;
    filter_step->uninit = filter_sws_scale_uninit;
    filter_step->filter_frame = filter_sws_scale;
    filter_step->multiple_output = 0;

    swscale *filter_params = malloc(sizeof(swscale));
    printf("in_width %i\n", in_width);
    printf("in_height %i\n", in_height);
    printf("in_pixel_fmt %i\n", in_pixel_fmt);
    printf("out_width %i\n", out_width);
    printf("out_height %i\n", out_height);
    printf("out_pixel_fmt %i\n", out_pixel_fmt);
    
    
    filter_params->in_width = in_width;
    filter_params->in_height = in_height;
    filter_params->in_pixel_fmt = in_pixel_fmt;
    filter_params->out_width = out_width;
    filter_params->out_height = out_height;
    filter_params->out_pixel_fmt = out_pixel_fmt;
    filter_step->filter_params = filter_params;
    return filter_step;
}
