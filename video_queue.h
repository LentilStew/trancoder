#ifndef VIDEOQ22_H_
#define VIDEOQ22_H_

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/avutil.h>
#include "file.h"
#include "filters/filter.h"
#include "debug_tools.h"

enum video_queue_status
{
    VIDEO_QUEUE_STATUS_UNDEFINED = 0,
    VIDEO_QUEUE_STATUS_WAITING,
    VIDEO_QUEUE_STATUS_TRANSCODING,
    VIDEO_QUEUE_STATUS_DONE,
    VIDEO_QUEUE_STATUS_FREE,
    VIDEO_QUEUE_STATUS_END
};

typedef struct video_queue
{

    file *video;

    struct video_queue *next;
    enum video_queue_status status;
} video_queue;

typedef struct vq_run
{
    video_queue **vq;
    filters_path **extra_filters;
} vq_run;

video_queue *video_queue_create();

void video_queue_print(video_queue *queue);

// free video_queue
void video_queue_free(video_queue *vq);

// append new to end of root
video_queue *video_queue_append(video_queue *root, video_queue *new);

void *video_queue_run(void *_vq);

// TESTS
void _video_queue_append_test();
#endif /*VIDEOQ_H_*/