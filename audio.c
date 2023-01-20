#include "file.h"
#include "filter.h"
#include "debug_tools.h"

#include "filters/video_display.h"
#include "filters/video_encoder.h"
#include "filters/audio_encoder.h"
#include "filters/audio_resample.h"
#include "filters/ffmpeg_filter_video.h"
#include "filters/ffmpeg_filter_audio.h"
#include "filters/pts.h"
#include "filters/delay2.h"

#include <libavutil/time.h>
#include "video_queue.h"
#include <pthread.h>
// folder library
#include <dirent.h>
#include <unistd.h>
#include <string.h>

int main()
{
    pthread_mutex_init(&mutex, NULL);
    av_log_set_level(AV_LOG_DEBUG);

    char *output = "out";
    char *filetype = "flv";

    file *encoder = file_create(2, output, filetype);
    int samplerate = 44100;
    int sample_fmt = AV_SAMPLE_FMT_FLTP;
    int channels = 2;
    int audio_bitrate = 128000;
    char *audio_encoder = "aac";
    AVDictionary *audio_codec_options = NULL;

    filters_path *path_audio;

    path_audio = filter_encode_audio_create(encoder->codec, encoder->container, encoder->streams, audio_encoder,
                                            sample_fmt, channels, samplerate, audio_bitrate, audio_codec_options);

    filter_path_init(path_audio);

    // INITIALIZE OUTPUT
    if (!(encoder->container->oformat->flags & AVFMT_NOFILE))
    {
        int avopen_ret = avio_open2(&encoder->container->pb, output,
                                    AVIO_FLAG_WRITE, NULL, NULL);
        if (avopen_ret < 0)
        {
            fprintf(stderr, "failed to open stream output context, stream will not work\n");
            exit(1);
        }
    }

    AVDictionary *muxer_opts = NULL;
    if (avformat_write_header(encoder->container, &muxer_opts) < 0)
    {
        printf("an error occurred when opening output file");
        exit(1);
    }

    encoder->paths[0] = filter_pts_create(encoder->streams[0], (AVRational){1, samplerate});
    encoder->paths[0]->next = path_audio;

    filter_path_init(encoder->paths[0]);

    filters_path *frame_free = filter_path_create();
    frame_free->filter_frame = filter_frame_free;
    frame_free->is_init = 1;
    encoder->paths[0] = fp_append(encoder->paths[0], frame_free);

    float t = 0;
    float tincr = 2 * M_PI * 440.0 / samplerate;

    for (int i = 0; i < samplerate / encoder->codec[0]->frame_size * 300; i++)
    {
        AVFrame *frame = av_frame_alloc();
        // sample format, nb_samples, channel_layout
        frame->format = sample_fmt;
        frame->channel_layout = av_get_default_channel_layout(channels);
        frame->nb_samples = encoder->codec[0]->frame_size;
        int res = av_frame_get_buffer(frame, 0);
        if (res != 0)
        {
            printf("Failed filling frame buffer %s\n", av_err2str(res));
        }

        float **data = (float **)frame->data;
        for (int sample = 0; sample < frame->nb_samples; sample++)
        {
            data[0][sample] =0;
            data[1][sample] = (sin(t) * 10000);

            t += tincr;
        }
        filter_path_apply(encoder->paths[0], frame);
    }
}