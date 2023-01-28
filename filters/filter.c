#include <libavformat/avformat.h>
#include "filters/filter.h"


void filter_path_init(filters_path *path)
{

    if (path == NULL)
    {
        return;
    }
    if (path->init && path->is_init != 1)
    {
        path->init(path);
    }
    path->is_init = 1;
    filter_path_init(path->next);
}

void filter_path_uninit(filters_path *path)
{
    if (path == NULL)
    {
        return;
    }

    if (path->uninit && path->is_uninit != 1)
    {
        path->uninit(path);
    }
    path->is_uninit = 1;
    filters_path *next = path->next;

    free(path);
    filter_path_uninit(next);
}

filters_path *fp_append(filters_path *path, filters_path *filter)
{
    if (path == NULL)
    {
        return filter;
    }
    path->next = fp_append(path->next, filter);
    return path;
}
void fp_print(filters_path *path)
{
    if (!path)
    {
        printf("\"null\"\n");
        return;
    }
    printf("\"%s\"->", path->filter_name ? path->filter_name : "null");
    fp_print(path->next);
}
filters_path *filter_path_create()
{
    filters_path *filter_step = calloc(sizeof(filters_path), 1);
    if (filter_step == NULL)
    {
        return NULL;
    }
    filter_step->is_init = 0;
    filter_step->next = NULL;
    filter_step->filter_frame = NULL;
    filter_step->init = NULL;
    filter_step->uninit = NULL;
    filter_step->filter_params = NULL;
    filter_step->filter_name = NULL;
    filter_step->multiple_output = 0;
    filter_step->flushing = 0;
    return filter_step;
}

void fp_flush(filters_path *path)
{
    filters_path *curr = path;
    while (curr)
    {
        curr->flushing = 1;
        filter_path_apply(curr, NULL);
        curr = curr->next;
    }
}

AVFrame *filter_path_apply(filters_path *path, AVFrame *frame)
{
    if (path == NULL)
    {
        return frame;
    }

    if (!path->is_init)
    {
        printf("WARNING filter %s was not initialized\n", path->filter_name ? path->filter_name : "null");
    }
    if (path->filter_frame == NULL)
    {
        printf("WARNING path->filter_frame == NULL\n");
    }
    // printf("%s\n", path->filter_name ? path->filter_name : "null");
    if (path->multiple_output == 1)
    {
        while (1)
        {
            frame = path->filter_frame(path, frame);
            if (frame == NULL)
                break;
            filter_path_apply(path->next, frame);
            frame = NULL;
        }
    }
    else
    {
        frame = path->filter_frame(path, frame);
        if (!frame)
        {
            return NULL;
        }
        return filter_path_apply(path->next, frame);
    }

    return frame;
}

filters_path *fp_copy(filters_path *head)
{
    if (head == NULL)
        return NULL;

    filters_path *new_node = malloc(sizeof(filters_path));

    if (!new_node)
        return NULL;

    *new_node = *head;
    new_node->next = fp_copy(head->next);

    return new_node;
}

// I HATE THIS PART OF THE CODE
AVFrame *filter_frame_free(filters_path *filter_props, AVFrame *frame)
{
    if (frame)
    {
        av_frame_free(&frame);
    }
    return NULL;
}