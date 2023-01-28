#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_mouse.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>

#include "file.h"
#include "filters/filter.h"


typedef struct renderer
{
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Window *window;
    char *name;
    int closed;
    int format;
    int width;
    int height;
} renderer;

void filter_display_video_init(filters_path *fp);
void filter_display_video_uninit(filters_path *fp);
AVFrame *filter_display_video(filters_path *fp, AVFrame *frame);
filters_path *filter_display_video_create(int width, int height, int format, char *name);

#endif