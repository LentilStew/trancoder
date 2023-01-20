#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include "debug_tools.h" //DELETE
#include <libswscale/swscale.h>
#include "filter.h"

typedef struct swscale
{
    struct SwsContext *sws_ctx;
    int in_width;
    int in_height;
    int in_pixel_fmt;
    int out_width;
    int out_height;
    int out_pixel_fmt;
} swscale;

void filter_sws_scale_init(filters_path *filter_step);
void filter_sws_scale_uninit(filters_path *filter_props);

AVFrame *filter_sws_scale(filters_path *filter_props, AVFrame *frame);

filters_path *filter_sws_scale_create(int in_width, int in_height, int in_pixel_fmt, int out_width, int out_height, int out_pixel_fmt);
