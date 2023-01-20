#ifndef DEBUG22_H_
#define DEBUG22_H_

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/avutil.h>

typedef struct frame_list
{
    AVFrame *frame;
    enum AVMediaType media_type;
    struct frame_list *next;
    int index;
} frame_list;

frame_list *frame_list_create();

int save_gray_frame(AVFrame *frame, char *file_name);

AVFrame *frame_black_get(int width, int height, int pix_fmt);

frame_list *frame_list_circular(frame_list *fl);

void frame_list_free(frame_list *fl);

void frame_list_print(frame_list *fl);

AVFrame *frame_copy(AVFrame *frame, enum AVMediaType type);

#endif