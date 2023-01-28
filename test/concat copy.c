// From array of paths to videos, transcode them to RTMP server
#include "debug_tools.h"
#include "file.h"
#include "filter.h"

#include "filters/video_display.h"
#include "filters/video_encoder.h"
#include "filters/audio_encoder.h"
#include "filters/audio_resample.h"
#include "filters/ffmpeg_filter_video.h"
#include "filters/ffmpeg_filter_audio.h"
#include "filters/pts.h"
#include "filters/delay2.h"
#include "filters/samples_per_frame.h"
#include "filters/audio_hash.h"

#include <libavutil/time.h>
#include "video_queue.h"
#include <pthread.h>
// folder library
#include <dirent.h>
#include <unistd.h>
#include <string.h>

int filters_path_from_files(file *src, file *dst);
int filters_path_from_files_audio(file *src, file *dst);
int filters_path_from_files_video(file *src, file *dst);

int main()
{

    /* -------------------------- CREATE ENCODER -------------------------- */

    pthread_mutex_init(&mutex, NULL);
    // av_log_set_level(AV_LOG_DEBUG);

    // GLOBAL SETTINGS
    char *output[] = {"./outputs/original.flv", "./outputs/hash2.flv", "./outputs/reverse_hash.flv"};
    int nb_output = 3;
    char *filetype = "flv";

    // AUDIO SETTINGS
    int samplerate = 44100;
    int sample_fmt = AV_SAMPLE_FMT_FLTP;
    int channels = 2;
    int audiobitrate = 128000;
    char *audioencoder = "aac";

    AVDictionary *audio_codec_options = NULL;
    // filter hash settings
    int64_t seed = (int64_t)0xafafafafa;
    // filter hash settings
    file **encoder = malloc(sizeof(file *) * nb_output);
    filters_path *paths = NULL;
    for (int i = 0; i < nb_output; i++)
    {

        encoder[i] = file_create(1, output[i], filetype);
        paths = fp_append(paths,
                          filter_encode_audio_create(&encoder[i]->codec[0], encoder[i]->container, &encoder[i]->streams[0], audioencoder,
                                                     sample_fmt, channels, samplerate, audiobitrate, audio_codec_options));
        filter_path_init(paths);

        // INITIALIZE OUTPUT
        if (!(encoder[i]->container->oformat->flags & AVFMT_NOFILE))
        {
            int avopen_ret = avio_open2(&encoder[i]->container->pb, output[i],
                                        AVIO_FLAG_WRITE, NULL, NULL);
            if (avopen_ret < 0)
            {
                fprintf(stderr, "failed to open stream output context, stream will not work\n");
                exit(1);
            }
        }

        AVDictionary *muxer_opts = NULL;
        if (avformat_write_header(encoder[i]->container, &muxer_opts) < 0)
        {
            printf("an error occurred when opening output file");
            exit(1);
        }
        if (i != nb_output - 1)
        {
            paths = fp_append(paths,
                              filter_audio_hash_create(seed, encoder[i]->streams[0]->codecpar->frame_size, channels, AV_SAMPLE_FMT_FLTP, 1));
        }

        filter_path_init(paths);
    }
    paths = fp_append(
        filter_pts_create(encoder[0]->streams[0], (AVRational){1, samplerate}),
        paths);
    filter_path_init(paths);
    encoder[0]->paths[0] = paths;

    fp_print(paths);
    int ret;

    char *audio_input_path = "./outputs/hash.flv";

    file *audio = file_open(audio_input_path);
    if (!audio)
    {
        printf("Can't open audio input %s\n", audio_input_path);
        exit(1);
    }

    ret = file_open_codecs(audio, CODEC_SKIP, NULL);
    if (ret == 1)
    {
        printf("Can't open codec for %s\n", audio_input_path);
        exit(1);
    }
    audio->fl = file_to_frame_list(audio);
    filters_path_from_files(audio, encoder[0]);

    video_queue *new_audio = video_queue_create();
    new_audio->video = audio;
    new_audio->status = VIDEO_QUEUE_STATUS_WAITING;

    vq_run *audio_q = calloc(sizeof(vq_run), 1);
    audio_q->extra_filters = file_match_paths(audio, encoder[0]);

    audio_q->vq = calloc(sizeof(video_queue *), 1);
    audio_q->vq[0] = NULL;
    pthread_t a_thread;

    ret = pthread_create(&a_thread, NULL, video_queue_run, (void *)audio_q);

    *audio_q->vq = video_queue_append(*audio_q->vq, new_audio);

    pthread_join(a_thread, NULL);
    for (int i = 0; i < nb_output; i++)
    {
        av_write_trailer(encoder[i]->container);
    }
}

int filters_path_from_files(file *src, file *dst)
{
    int src_audio_index = file_find_media_type(AVMEDIA_TYPE_AUDIO, src);
    int src_video_index = file_find_media_type(AVMEDIA_TYPE_VIDEO, src);
    if (src_audio_index != -1)
        filters_path_from_files_audio(src, dst);

    if (src_video_index != -1)
        filters_path_from_files_video(src, dst);
    return 0;
}

int filters_path_from_files_video(file *src, file *dst)
{

    file_match_streams(src);
    file_match_streams(dst);
    int video_filter_desc_size = 1024;
    char *video_filter_desc = calloc(sizeof(char), video_filter_desc_size);
    int video_filter_desc_len = 0;

    int src_video_index = file_find_media_type(AVMEDIA_TYPE_VIDEO, src);
    int dst_video_index = file_find_media_type(AVMEDIA_TYPE_VIDEO, dst);
    if (src_video_index == -1 || dst_video_index == -1)
    {
        printf("NO VIDEO STREAM in %i or %i\n", src_video_index, dst_video_index);
    }

    AVCodecContext *src_video_codec = src->codec[src_video_index];
    AVCodecContext *dst_video_codec = dst->codec[dst_video_index];

    printf("scale=%i:%i != scale=%i:%i\n",
           dst_video_codec->width, dst_video_codec->height, src_video_codec->width, src_video_codec->height);

    if (src_video_codec->width != dst_video_codec->width || src_video_codec->height != dst_video_codec->height)
    {
        video_filter_desc_len += snprintf(video_filter_desc + video_filter_desc_len,
                                          video_filter_desc_size - video_filter_desc_len,
                                          "scale=%i:%i",
                                          dst_video_codec->width, dst_video_codec->height);
    }

    printf("format=%s != format=%s\n",
           av_get_pix_fmt_name(dst_video_codec->pix_fmt), av_get_pix_fmt_name(src_video_codec->pix_fmt));

    if (src_video_codec->pix_fmt != dst_video_codec->pix_fmt)
    {
        video_filter_desc_len += snprintf(video_filter_desc + video_filter_desc_len,
                                          video_filter_desc_size - video_filter_desc_len,
                                          ",format=%s",
                                          av_get_pix_fmt_name(dst_video_codec->pix_fmt));
    }

    AVRational src_frame_rate = av_guess_frame_rate(src->container, src->streams[src_video_index], NULL);

    printf("fps=%i != fps=%i\n",
           (int)av_q2d(dst->codec[dst_video_index]->framerate), (int)av_q2d(src_frame_rate));

    if ((int)av_q2d(src_frame_rate) != (int)av_q2d(dst->codec[dst_video_index]->framerate))
    {
        video_filter_desc_len += snprintf(video_filter_desc + video_filter_desc_len,
                                          video_filter_desc_size - video_filter_desc_len,
                                          ",fps=%i",
                                          (int)av_q2d(dst->codec[dst_video_index]->framerate));
    }

    if (video_filter_desc_len != 0)
    {
        enum AVPixelFormat out_pix_fmt[] = {src_video_codec->pix_fmt, AV_PIX_FMT_NONE};
        src->paths[src_video_index] = fp_append(src->paths[src_video_index],
                                                filter_ffmpeg_video_create(video_filter_desc, src->streams[src_video_index]->time_base,
                                                                           src_video_codec->pix_fmt, out_pix_fmt,
                                                                           src_video_codec->width, src_video_codec->height,
                                                                           (AVRational){1, 1}));
    }

    filter_path_init(src->paths[src_video_index]);
    free(video_filter_desc);

    return 0;
}

int filters_path_from_files_audio(file *src, file *dst)
{
    file_match_streams(src);
    file_match_streams(dst);

    int src_audio_index = file_find_media_type(AVMEDIA_TYPE_AUDIO, src);
    int dst_audio_index = file_find_media_type(AVMEDIA_TYPE_AUDIO, dst);
    if (src_audio_index == -1 || dst_audio_index == -1)
    {
        printf("NO AUDIO STREAM in SRC %i or DST %i\n", src_audio_index, dst_audio_index);
    }

    AVCodecContext *src_audio_codec = src->codec[src_audio_index];
    AVCodecContext *dst_audio_codec = dst->codec[dst_audio_index];

    if (src_audio_codec->sample_rate != dst_audio_codec->sample_rate ||
        src_audio_codec->sample_fmt != dst_audio_codec->sample_fmt ||
        src_audio_codec->channel_layout != dst_audio_codec->channel_layout)
    {
        src->paths[src_audio_index] = fp_append(src->paths[src_audio_index],
                                                audio_resample_create(src_audio_codec->sample_rate, dst_audio_codec->sample_rate,
                                                                      src_audio_codec->channel_layout, dst_audio_codec->channel_layout,
                                                                      src_audio_codec->sample_fmt, dst_audio_codec->sample_fmt));
    }

    src->paths[src_audio_index] = fp_append(src->paths[src_audio_index],
                                            filter_samples_per_frame_create(dst_audio_codec->channels, dst_audio_codec->frame_size, dst_audio_codec->sample_fmt));

    filter_path_init(src->paths[src_audio_index]);
    return 0;
}