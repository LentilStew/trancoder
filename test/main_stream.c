#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/avutil.h>
#include "filters/audio_encoder.h"
#include "filters/video_encoder.h"
#include "filters/edges.h"
#include "file.h"
#include <time.h>

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

int main()
{
    //av_log_set_level(AV_LOG_DEBUG);
    int width = 1920;
    int height = 1080;
    int pix_fmt = AV_PIX_FMT_YUV420P;
    int fps = 25;
    int bitrate = 1000000;
    AVRational frame_rate = {fps, 1};
    AVRational time_base = {1, fps};
    AVRational sample_aspect_ratio = {1, 1};

    // CREATE OUTPUT
    char *output_file = "rtmp://rio.contribute.live-video.net/app/live_402755286_s27jxAKlcKRwSzuyL6OIQeu4yQaQ0G";
    file *output = file_create(1, output_file, "flv");
    if (!output)
    {
        printf("failed creating output\n");
        return 1;
    }

    AVDictionary *codec_options = NULL;

    av_dict_set(&codec_options, "profile", "main", 0);
    av_dict_set(&codec_options, "preset", "fast", 0);
    av_dict_set(&codec_options, "tune", "zerolatency", 0);

    output->paths[0] = filter_encode_video_create(output->codec[0], output->container, "libx264", width, height, pix_fmt, sample_aspect_ratio, time_base, frame_rate, bitrate, codec_options);
    filter_path_init(output->paths[0]);

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
    // ENCODE

    // start time
    clock_t start = clock(), diff;

    while (1)
    {
        // encode no faster than fps
        diff = clock() - start;
        int msec = diff * 1000 / CLOCKS_PER_SEC;
        if (msec < 1000 / fps)
            continue;

        start = clock();

        printf("dif %d\n", msec);

        AVFrame *frame = frame_black_get(width, height, pix_fmt);
        frame = filter_path_apply(output->paths[0], frame);
        av_frame_free(&frame);
    }

    // CLEANUP
    filter_path_apply(output->paths[0], NULL);
    filter_path_uninit(output->paths[0]);
    free(output->paths[0]);
    av_write_trailer(output->container);
    file_free(output);
}