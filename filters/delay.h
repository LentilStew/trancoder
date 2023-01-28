#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include "debug_tools.h" //DELETE
#include "filters/filter.h"


typedef struct delay_frame
{
    double fps;
    time_t start_time;
    int frame_count;
    int frame_duration;
    AVRational time_base;
} delay_frame;

AVFrame *filter_delay(filters_path *filter_props, AVFrame *frame);

void filter_delay_init(filters_path *filter_props);
filters_path *filter_delay_create(double fps);