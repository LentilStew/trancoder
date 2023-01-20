#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include "filter.h"
#include "libavutil/imgutils.h"
#include <malloc.h>  

#define IPOW2(n) n *n

uint8_t SQRTS2[IPOW2(255)];

#define CHARSQRT(n) (n >= 0xFE00) ? 0xFF : SQRTS2[n]

void filter_edges_init(filters_path *filter_step)
{
    for (int i = 0; i < IPOW2(255); i++)
        SQRTS2[i] = round(sqrt(i));
}

AVFrame *filter_edges(filters_path *filter_props, AVFrame *frame)
{
    if (!frame)
        return NULL;

    AVFrame *new_frame = av_frame_alloc();
    av_frame_copy_props(new_frame, frame);

    new_frame->width = frame->width;
    new_frame->height = frame->height;
    new_frame->format = frame->format;
    new_frame->pts = frame->pts;
    new_frame->pkt_dts = frame->pkt_dts;
    new_frame->pkt_duration = frame->pkt_duration;
    new_frame->pkt_pos = frame->pkt_pos;
    new_frame->pkt_size = frame->pkt_size;
    new_frame->best_effort_timestamp = frame->best_effort_timestamp;

    int res = av_frame_get_buffer(new_frame, 0);
    if (res < 0)
    {
        fprintf(stderr, "Could not allocate the video frame data %d     %s %d", res, __FILE__, __LINE__);
        return NULL;
    }

    int real_heights[AV_NUM_DATA_POINTERS] = {0};

    switch (frame->format)
    {

    case AV_PIX_FMT_YUV420P:
        real_heights[0] = frame->height;
        real_heights[1] = frame->height / 2;
        real_heights[2] = frame->height / 2;
        memset(new_frame->data[1], 128, frame->linesize[1] * real_heights[1]);
        memset(new_frame->data[2], 128, frame->linesize[2] * real_heights[2]);
        real_heights[1] = 0;
        real_heights[2] = 0;
        break;

    default:
        fprintf(stderr, "Unsupported pixel format %d %s %d", frame->format, __FILE__, __LINE__);
        exit(1);
        break;
    }

    int Gx, Gy;
    for (int z = 0; z < AV_NUM_DATA_POINTERS; z++)
    {
        uint8_t *src = frame->data[z];
        uint8_t *dst = new_frame->data[z];

        for (int y = 0; y < real_heights[z]; y++)
        {
            for (int x = 0; x < frame->linesize[z]; x++)
            {
                src = frame->data[z] + y * frame->linesize[z] + x;
                dst = new_frame->data[z] + y * frame->linesize[z] + x;

                if (x == 0 || y == 0 || x == frame->linesize[z] - 1 || y == frame->height - 1)
                {
                    continue;
                }

                Gx = (src[-frame->linesize[z] - 1] + 2 * src[-1] + src[frame->linesize[z] - 1]) -
                     (src[-frame->linesize[z] + 1] + 2 * src[1] + src[frame->linesize[z] + 1]);

                Gy = (src[-frame->linesize[z] - 1] + 2 * src[-frame->linesize[z]] + src[-frame->linesize[z] + 1]) -
                     (src[frame->linesize[z] - 1] + 2 * src[frame->linesize[z]] + src[frame->linesize[z] + 1]);

                dst[0] = CHARSQRT(IPOW2(Gx) + IPOW2(Gy));
            }
        }
    }

    av_frame_free(&frame);

    return new_frame;
}

filters_path *filter_edges_create()
{
    filters_path *filter_step = filter_path_create();
    filter_step->init = filter_edges_init;
    filter_step->filter_frame = filter_edges;
    return filter_step;
}