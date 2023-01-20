#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/avutil.h>
#include "filters/audio_encoder.h"
#include "filters/video_display.h"
#include "filters/video_encoder.h"
#include "filters/edges.h"
#include "filters/delay.h"
#include "filters/swscale.h"
#include "file.h"
#include <time.h>

int main()
{
    char *file_name = "../video_inputs/roach.mp4";
    file *input = malloc(sizeof(file));
    int res;

    res = file_open(input, file_name);
    if (res != 0)
    {
        printf("Failed opening input\n");
        return -1;
    }

    res = file_open_codecs(input, NULL, NULL);
    if (res != 0)
    {
        printf("Failed opening codecs\n");
        return -1;
    }

    AVStream *video_stream = input->container->streams[file_find_media_type(AVMEDIA_TYPE_VIDEO, input)];

    if (video_stream->codecpar->format != AV_PIX_FMT_YUV420P)
        input->paths[video_stream->index] = filter_append(input->paths[video_stream->index],
                                                          filter_sws_scale_create(video_stream->codecpar->width,
                                                                                  video_stream->codecpar->height,
                                                                                  video_stream->codecpar->format,
                                                                                  video_stream->codecpar->width,
                                                                                  video_stream->codecpar->height,
                                                                                  AV_PIX_FMT_YUV420P));
    filter_path_init(input->paths[video_stream->index]);

    frame_list *fl = file_to_frame_list(input);
    frame_list_circular(fl);

    filters_path *display_video = filter_display_video_create(video_stream->codecpar->width, video_stream->codecpar->height, SDL_PIXELFORMAT_IYUV, "ROACH");

    double framerate = av_q2d(av_guess_frame_rate(input->container, video_stream, NULL));

    display_video = filter_append(display_video,
                                  filter_delay_create(framerate));

    filter_path_init(display_video);

    while (fl != NULL)
    {
        if (fl->media_type == AVMEDIA_TYPE_VIDEO)
            filter_path_apply(display_video, fl->frame);

        fl = fl->next;
    }
}
