#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/avutil.h>
#include "debug_tools.h"
#include "file.h"
#include "filter.h"
#include "video_queue.h"
#include <libavutil/time.h>

video_queue *video_queue_create()
{

    video_queue *queue = malloc(sizeof(video_queue));
    if (queue == NULL)
    {
        return NULL;
    }
    queue->next = NULL;
    queue->status = VIDEO_QUEUE_STATUS_UNDEFINED;

    return queue;
}

void video_queue_print(video_queue *queue)
{
    if (queue == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("%i->", queue->status);
    video_queue_print(queue->next);
}

// free video_queue
void video_queue_free(video_queue *vq)
{
    if (!vq)
        return;

    file_free(vq->video);

    video_queue_free(vq->next);
    free(vq);
}

// append new to end of root
video_queue *video_queue_append(video_queue *root, video_queue *new)
{
    if (root == NULL)
        return new;

    root->next = video_queue_append(root->next, new);
    return root;
}

// TESTS

void *video_queue_run(void *_vq)
{
    vq_run *vq_run = _vq;
    video_queue **vq = vq_run->vq;
    filters_path **extra_filters = vq_run->extra_filters;
    video_queue **curr = vq;
    while (1)
    {
        printf("%p %p\n", curr, curr[0]);

        printf("WAITING FOR NEW INPUT\n");
        av_usleep(1000000);
        if (curr[0] == NULL)
            continue;

        switch (curr[0]->status)
        {
        case VIDEO_QUEUE_STATUS_UNDEFINED:
            continue;
        case VIDEO_QUEUE_STATUS_TRANSCODING:
        case VIDEO_QUEUE_STATUS_DONE:
        case VIDEO_QUEUE_STATUS_FREE:
            curr = &curr[0]->next;
            continue;

        case VIDEO_QUEUE_STATUS_WAITING:

            curr[0]->status = VIDEO_QUEUE_STATUS_TRANSCODING;

            file_stream(curr[0]->video, extra_filters);

            return NULL;
            curr[0]->status = VIDEO_QUEUE_STATUS_DONE;
        }
    }
}