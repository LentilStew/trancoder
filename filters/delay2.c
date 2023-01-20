#include "delay2.h"

AVFrame *filter_delay(filters_path *filter_props, AVFrame *frame)
{
    filter_delay_params *delay = filter_props->filter_params;
    if (!frame)
        return frame;
    // convert frame->pts to real time
    int64_t frame_time = frame->pts * av_q2d(delay->stream->time_base) * 1000000;

    int64_t time_left = frame_time - (av_gettime() - delay->start_time);
    //printf("msec %li \n", time_left);
    av_usleep(time_left > 0 ? time_left : 0);

    return frame;
}

void filter_delay_destroy(filters_path *fp)
{
    filter_delay_params *params = fp->filter_params;
    free(params);
}
// in microseconds, at this time the delay will start, use curr or 0 for instant start
filters_path *filter_delay_create(int64_t start_time, AVStream *stream)
{

    filters_path *filter_props = filter_path_create();
    filter_props->filter_name = "delay";

    filter_delay_params *delay_params = malloc(sizeof(filter_delay_params));
    delay_params->start_time = start_time;
    delay_params->stream = stream;

    filter_props->filter_frame = filter_delay;
    filter_props->filter_params = delay_params;

    return filter_props;
}