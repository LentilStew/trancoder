#include "filters/filter.h"

#include <time.h>
#include "delay.h"
AVFrame *filter_delay(filters_path *filter_props, AVFrame *frame)
{
    delay_frame *delay = filter_props->filter_params;

    clock_t diff;

    diff = clock() - delay->start_time;
    int msec = diff * 1000 / CLOCKS_PER_SEC;

    while (msec < 1000 / delay->fps)
    {
        diff = clock() - delay->start_time;
        msec = diff * 1000 / CLOCKS_PER_SEC;
    }
    printf("msec %li %i\n", diff, msec);
    delay->start_time = clock();

    return frame;
}

void filter_delay_init(filters_path *filter_props)
{
    delay_frame *delay = filter_props->filter_params;
    delay->start_time = time(NULL);
}

filters_path *filter_delay_create(double fps)
{
    filters_path *filter_props = filter_path_create();
    filter_props->filter_frame = filter_delay;
    delay_frame *delay = malloc(sizeof(delay_frame));

    delay->fps = fps;
    delay->start_time = -1;
    filter_props->filter_params = delay;

    return filter_props;
}
