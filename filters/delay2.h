#include "filter.h"
#include <time.h>
#include <libavutil/time.h>

typedef struct filter_delay_params
{
    int64_t start_time; // in microseconds, at this time the delay will start, use curr or 0 for instant start
    AVStream *stream;
} filter_delay_params;

AVFrame *filter_delay(filters_path *filter_props, AVFrame *frame);
// in microseconds, at this time the delay will start, use curr or 0 for instant start

filters_path *filter_delay_create(int64_t start_time, AVStream *stream);