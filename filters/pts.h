#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include "debug_tools.h" //DELETE
#include "filter.h"

typedef struct filter_pts_params
{
    AVStream *stream;
    AVRational fps;
    double frame_duration;
    double next_pts;
} filter_pts_params;

void filter_pts_init(filters_path *filter_step);
AVFrame *filter_pts(filters_path *filter_props, AVFrame *frame);

void filter_pts_uninit(filters_path *filter_props);

filters_path *filter_pts_create(AVStream *stream, AVRational fps);
