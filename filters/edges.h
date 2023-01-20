#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include "filter.h"

void filter_edges_init(filters_path *filter_step);

AVFrame *filter_edges(filters_path *filter_props, AVFrame *frame);

filters_path *filter_edges_create();