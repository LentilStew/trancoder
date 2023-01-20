
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/avutil.h>
#include "filter.h"
#include "debug_tools.h"
#include <malloc.h>


int i = 0;
int save_gray_frame(AVFrame *frame, char *file_name)
{

    FILE *f;
    char buf[1024];

    snprintf(buf, sizeof(buf), "./ad/frame%d.pgm", i++);

    if (!file_name)
        file_name = buf;

    f = fopen(file_name, "w");
    if (f == NULL)
    {
        return -1;
    }
    fprintf(f, "P5 %d %d 255 ", frame->width, frame->height);

    fwrite(frame->data[0], 1, frame->width * frame->height, f);
    fclose(f);
    printf("path %s\n", file_name);
    return 0;
}

AVFrame *frame_black_get(int width, int height, int pix_fmt)
{
    AVFrame *frame = av_frame_alloc();
    if (!frame)
        return NULL;

    frame->format = pix_fmt;
    frame->width = width;
    frame->height = height;

    av_frame_get_buffer(frame, 0);
    av_frame_make_writable(frame);

    memset(frame->data[0], 0, frame->width * frame->height);
    memset(frame->data[1], 128, frame->width * frame->height / 4);
    memset(frame->data[2], 128, frame->width * frame->height / 4);

    return frame;
}

AVFrame *frame_copy(AVFrame *frame, enum AVMediaType type)
{
    if (!frame)
        return NULL;
    AVFrame *copy = av_frame_alloc();
    if (!copy)
        return NULL;

    // Set the width and height of the new frame to be the same as the original
    if (type == AVMEDIA_TYPE_VIDEO)
    {
        copy->width = frame->width;
        copy->height = frame->height;
        copy->format = frame->format;
    }
    else if (type == AVMEDIA_TYPE_AUDIO)
    {
        copy->sample_rate = frame->sample_rate;
        copy->channel_layout = frame->channel_layout;
        copy->format = frame->format;
        copy->nb_samples = frame->nb_samples;
    }
    else
    {
        av_frame_free(&copy);
        return NULL;
    }
    int res;
    // Allocate memory for the new frame
    if ((res = av_frame_get_buffer(copy, 0)) < 0)
    {
        printf("error getting buffer %s\n", av_err2str(res));
        av_frame_free(&copy);
        return NULL;
    }

    // Copy the data from the original frame to the new frame
    if (av_frame_copy(copy, frame) < 0)
    {
        av_frame_free(&copy);
        return NULL;
    }

    return copy;
}

// video linked list

frame_list *frame_list_create()
{
    frame_list *fl = malloc(sizeof(frame_list));
    fl->frame = NULL;
    fl->next = NULL;
    fl->media_type = AVMEDIA_TYPE_UNKNOWN;
    return fl;
}
/*
frame_list *_file_to_frame_list(file *f, AVPacket *packet, filters_path **paths)
{
    if (!f || !packet)
        return NULL;

    AVFrame *frame = av_frame_alloc();

    int res = file_decode_frame(f, frame, packet);
    switch (res)
    {
    case 0:

        if (f->nb_streams > packet->stream_index)
        {
            filter_path_apply(f->paths[packet->stream_index], frame);
        }
        _file_to_frame_list(f, packet, paths);

    case 1:
        printf("Error decoding a frame\n");
        return NULL;

    case -1:
        return NULL;
    }
    return NULL;
}*/



frame_list *frame_list_circular(frame_list *fl)
{
    frame_list *curr = fl;
    while (curr->next)
    {
        if (curr->next == fl)
            return fl;

        curr = curr->next;
    }
    curr->next = fl;
    return fl;
}

void frame_list_free(frame_list *fl)
{
    if (fl->next)
        frame_list_free(fl->next);
    av_frame_free(&fl->frame);
    free(fl);
}
