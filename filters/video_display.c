#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_mouse.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include "filters/filter.h"

#include "video_display.h"

void filter_display_video_init(filters_path *fp)
{
    renderer *params = fp->filter_params;
 int res;
    // attempt to initialize graphics
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("error initializing SDL: %s\n", SDL_GetError());
        return; // return 1;
    }

    params->window = SDL_CreateWindow(params->name,
                                      SDL_WINDOWPOS_CENTERED,
                                      SDL_WINDOWPOS_CENTERED,
                                      params->width, params->height, 0);
    if (!params->window)
    {
        printf("error creating window: %s\n", SDL_GetError());
        SDL_Quit();
        return; // return 1;
    }

    // create a renderer, which sets up the graphics hardware
    Uint32 render_flags = SDL_RENDERER_ACCELERATED;
    params->renderer = SDL_CreateRenderer(params->window, -1, render_flags);
    if (!params->renderer)
    {
        printf("error creating renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(params->window);
        SDL_Quit();
        return; // return 1;
    }
    res = SDL_RenderClear(params->renderer);
    if (res != 0)
    {
        printf("error clearing renderer: %s\n", SDL_GetError());
        SDL_DestroyRenderer(params->renderer);
        SDL_DestroyWindow(params->window);
        SDL_Quit();
        return; // return 1;
    }
    // load the image data into the graphics hardware's memory
    params->texture = SDL_CreateTexture(
        params->renderer,
        params->format,
        SDL_TEXTUREACCESS_STREAMING,
        params->width,
        params->height);
    if (!params->texture)
    {
        printf("error creating texture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(params->renderer);
        SDL_DestroyWindow(params->window);
        SDL_Quit();
        return; // return 1;
    }
    return; // return 0;
}
int frame_nb = 0;
AVFrame *filter_display_video(filters_path *fp, AVFrame *frame)
{

    renderer *params = fp->filter_params;
    if (params->closed)
    {
        return frame;
    }
    int res;
    switch (params->format) // DONE BY COPILOT MAY BE RIGHT MAY BE WRONG
    {
    case SDL_PIXELFORMAT_RGBA32:
        res = SDL_UpdateTexture(params->texture, NULL, frame->data[0], frame->linesize[0]);
        break;
    case SDL_PIXELFORMAT_YV12:
    case SDL_PIXELFORMAT_IYUV:
    case SDL_PIXELFORMAT_YUY2:
    case SDL_PIXELFORMAT_UYVY:
    case SDL_PIXELFORMAT_YVYU:
        res = SDL_UpdateYUVTexture(params->texture, NULL, frame->data[0], frame->linesize[0], frame->data[1], frame->linesize[1], frame->data[2], frame->linesize[2]);
        break;
    case SDL_PIXELFORMAT_NV12:
    case SDL_PIXELFORMAT_NV21:
        res = SDL_UpdateNVTexture(params->texture, NULL, frame->data[0], frame->linesize[0], frame->data[1], frame->linesize[1]);
        break;

    default:
        return frame;
    }

    if (res != 0)
    {
        printf("error updating texture: %s\n", SDL_GetError());
        SDL_DestroyTexture(params->texture);
        SDL_DestroyRenderer(params->renderer);
        SDL_DestroyWindow(params->window);
        SDL_Quit();
        return frame;
    }
    // clear the window
    res = SDL_RenderClear(params->renderer);
    if (res != 0)
    {
        printf("error clearing renderer: %s\n", SDL_GetError());
        SDL_DestroyTexture(params->texture);
        SDL_DestroyRenderer(params->renderer);
        SDL_DestroyWindow(params->window);
        SDL_Quit();
        return frame;
    }
    // draw the image to the window
    res = SDL_RenderCopy(params->renderer, params->texture, NULL, NULL);
    if (res != 0)
    {
        printf("error copying texture: %s\n", SDL_GetError());
        SDL_DestroyTexture(params->texture);
        SDL_DestroyRenderer(params->renderer);
        SDL_DestroyWindow(params->window);
        SDL_Quit();
        return frame;
    }

    SDL_RenderPresent(params->renderer);

    // check if the window is closed
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {

        case SDL_WINDOWEVENT:

            switch (event.window.event)
            {

            case SDL_WINDOWEVENT_CLOSE: // exit game
                params->closed = 1;
                SDL_DestroyTexture(params->texture);
                SDL_DestroyRenderer(params->renderer);
                SDL_Quit();
                break;

            default:
                break;
            }
            break;
        }
    }

    return frame;
}

void filter_display_video_uninit(filters_path *fp)
{
    renderer *params = fp->filter_params;
    if (params->closed)
        return;
    // clean up resources before exiting

    params->closed = 1;

    free(params);

    return; // return 0;
}

filters_path *filter_display_video_create(int width, int height, int format, char *name)
{
    filters_path *filter_step = filter_path_create();
    filter_step->init = filter_display_video_init;
    filter_step->uninit = filter_display_video_uninit;
    filter_step->filter_frame = filter_display_video;

    renderer *r = malloc(sizeof(renderer));
    r->format = format;
    r->width = width;
    r->height = height;
    r->name = name;
    r->closed = 0;
    filter_step->filter_params = r;

    return filter_step;
}
