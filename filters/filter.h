#ifndef FILTER22_H_H
#define FILTER22_H_H
#include <libavformat/avformat.h>

typedef struct filters_path_t
{
    void (*init)(struct filters_path_t *ctx);
    int is_init;
    AVFrame *(*filter_frame)(struct filters_path_t *ctx, AVFrame *frame);

    void (*uninit)(struct filters_path_t *ctx);
    int is_uninit;

    void *filter_params;

    int multiple_output; // 0 no ; 1 yes
    int flushing;

    struct filters_path_t *next;

    const char *filter_name;

} filters_path;

void fp_flush(filters_path *path);

AVFrame *filter_path_apply(filters_path *path, AVFrame *frame);

filters_path *filter_path_create();

void filter_path_init(filters_path *path);

void filter_path_uninit(filters_path *path);
void fp_print(filters_path *path);
filters_path *fp_append(filters_path *path, filters_path *filter);

filters_path *fp_copy(filters_path *head);

// I HATE THIS FUNCTION
AVFrame *filter_frame_free(filters_path *filter_props, AVFrame *frame);

#endif