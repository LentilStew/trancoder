#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/avutil.h>
#include "filters/audio_encoder.h"
#include "filters/video_display.h"
#include "filters/video_encoder.h"
#include "filters/edges.h"
#include "filters/swscale.h"
#include "file.h"
#include <time.h>

int main()
{
    // av_log_set_level(AV_LOG_DEBUG);
    file *input = malloc(sizeof(file));
    int res = file_open(input, "./roach.mp4");
    if (res != 0)
    {
        printf("Failed opening input\n");
        return -1;
    }
    res = file_open_codecs(input, "h264_cuvid", NULL);
    if (res != 0)
    {
        printf("Failed opening codecs\n");
        return -1;
    }

    frame_list *fl = file_to_frame_list(input);

    AVStream *video_stream = input->container->streams[file_find_media_type(AVMEDIA_TYPE_VIDEO, input)];

    AVRational frame_rate = av_guess_frame_rate(input->container, video_stream, NULL);
    AVRational sample_aspect_ratio = {1, 1};
    int bitrate = 1000000;

    // CREATE OUTPUT
    char *output_file = "rgb_test";

    // char *output_file = "rtmp://rio.contribute.live-video.net/app/live_402755286_s27jxAKlcKRwSzuyL6OIQeu4yQaQ0G";
    file *output = file_create(1, output_file, "flv");
    if (!output)
    {
        printf("failed creating output\n");
        return 1;
    }

    AVDictionary *codec_options = NULL;

    // av_dict_set(&codec_options, "profile", "main", 0);
    // av_dict_set(&codec_options, "preset", "fast", 0);
    //  av_dict_set(&codec_options, "tune", "zerolatency", 0);
    output->paths[0] = filter_encode_video_create(output->codec[0], output->container, "libx264rgb", video_stream->codecpar->width, video_stream->codecpar->height, AV_PIX_FMT_RGB24, sample_aspect_ratio, av_inv_q(frame_rate), frame_rate, bitrate, codec_options);

    // output->paths[0] = filter_display_video_create(video_stream->codecpar->width, video_stream->codecpar->height, SDL_PIXELFORMAT_RGB24, "roach");
    /*
        //output->paths[0]->next = filter_encode_video_create(output->codec[0], output->container, "h264_nvenc", video_stream->codecpar->width, video_stream->codecpar->height, AV_PIX_FMT_NV12, sample_aspect_ratio, av_inv_q(frame_rate), frame_rate, bitrate, codec_options);
        // INITIALIZE OUTPUT
        if (!(output->container->oformat->flags & AVFMT_NOFILE))
        {
            int avopen_ret = avio_open2(&output->container->pb, output_file,
                                        AVIO_FLAG_WRITE, NULL, NULL);
            if (avopen_ret < 0)
            {
                fprintf(stderr, "failed to open stream output context, stream will not work\n");
                return 1;
            }
        }

        AVDictionary *muxer_opts = NULL;
        if (avformat_write_header(output->container, &muxer_opts) < 0)
        {
            printf("an error occurred when opening output file");
            return 1;
        }

        av_dump_format(output->container, 0, NULL, 1); // STREAM TEST
        */
    filter_path_init(output->paths[0]);
    // ENCODE
    int counter = 0;
    // start time
    clock_t start = clock(), diff;
    frame_list *curr = fl;
    while (1)
    {
        if (counter > 100)
            break;

        counter++;

        // encode no faster than fps
        if (curr->media_type == AVMEDIA_TYPE_VIDEO)
        {
            diff = clock() - start;
            int msec = diff * 1000 / CLOCKS_PER_SEC;
            if (msec < 1000 / av_q2d(frame_rate))
                continue;
            printf("curr->frame %p\n", curr->frame);
            printf("curr->frame %p\n", curr->frame);
            printf("curr->frame %p\n", curr->frame);
            printf("curr->frame %p\n", curr->frame);
            printf("curr->frame %p\n", curr->frame);
            
            printf("msec %d\n", msec);
            start = clock();

            filter_path_apply(output->paths[0], curr->frame);
        }
        curr = curr->next;
    }

    // CLEANUP
    filter_path_apply(output->paths[0], NULL);
    filter_path_uninit(output->paths[0]);
    free(output->paths[0]);
    av_write_trailer(output->container);
    file_free(output);
}