// From array of paths to videos, transcode them to RTMP server
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
    file *encoder;

    pthread_mutex_init(&mutex, NULL);
    // av_log_set_level(AV_LOG_DEBUG);

    // GLOBAL SETTINGS
    char *output = "out";
    char input[32768] = "audio:./video_inputs/song_yt.mkv";

    char *filetype = "flv";

    // VIDEO SETTINGS
    int width = 116;
    int height = 116;
    AVRational fps = (AVRational){25, 1};
    int bitrate = 1000000;
    int pixelFormat = AV_PIX_FMT_YUV420P;
    AVRational sample_aspect_ratio = (AVRational){1, 1};
    AVRational timebase = av_inv_q(fps);
    char *videoencoder = "libx264";

    AVDictionary *video_codec_options = NULL;
    av_dict_set(&video_codec_options, "profile", "main", 0);
    av_dict_set(&video_codec_options, "preset", "fast", 0);
    av_dict_set(&video_codec_options, "tune", "zerolatency", 0);

    // AUDIO SETTINGS
    int samplerate = 48000;
    int channels = 2;
    int audiobitrate = 128000;
    char *audioencoder = "aac";
    int sample_fmt = AV_SAMPLE_FMT_FLTP;
    AVDictionary *audio_codec_options = NULL;
    encoder = file_create(2, output, filetype);

    filters_path *paths[2];
    paths[0] = filter_encode_video_create(&encoder->codec[0], encoder->container, &encoder->streams[0], videoencoder,
                                          width, height, pixelFormat, sample_aspect_ratio, timebase, fps, bitrate, video_codec_options);
    paths[1] = filter_encode_audio_create(&encoder->codec[1], encoder->container, &encoder->streams[1], audioencoder,
                                          sample_fmt, channels, samplerate, audiobitrate, audio_codec_options);

    filter_path_init(paths[0]);
    filter_path_init(paths[1]);

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
    int ret;

    int64_t start_time = av_gettime();
    encoder->paths[1] = filter_pts_create(encoder->streams[1], (AVRational){1, samplerate}); // since it's audio the fps doesn't matter (it's already on the stream)
    // encoder->paths[1]->next = filter_delay_create(start_time, encoder->streams[1]);
    encoder->paths[1]->next = paths[1];

    encoder->paths[0] = filter_pts_create(encoder->streams[0], encoder->codec[0]->framerate);
    // encoder->paths[0]->next = filter_delay_create(start_time, encoder->streams[0]);
    encoder->paths[0]->next = paths[0];

    filter_path_init(encoder->paths[0]);
    filter_path_init(encoder->paths[1]);

    char *video_input_path = "./video_inputs/roach.mp4";

    file *video = file_open(video_input_path);
    if (!video)
    {
        printf("Can't open video input %s\n", video_input_path);
        exit(1);
    }

    ret = file_open_codecs(video, NULL, CODEC_SKIP);
    if (ret == 1)
    {
        printf("Can't open codec for %s\n", video_input_path);
        exit(1);
    }

    filters_path_from_files(video, encoder);
    frame_list *v_fl = file_to_frame_list(video);
    // frame_list_circular(v_fl);

    video_queue *new_video = video_queue_create();
    new_video->tag = FILE_FRAME_LIST;
    new_video->video.fl = v_fl;
    new_video->status = VIDEO_QUEUE_STATUS_WAITING;

    vq_run *video_q = calloc(sizeof(vq_run), 1);
    video_q->encoder = encoder;
    video_q->vq = calloc(sizeof(video_queue *), 1);
    video_q->vq[0] = NULL;
    pthread_t v_thread;
    ret = pthread_create(&v_thread, NULL, video_queue_run, (void *)video_q);

    vq_run *audio_q = calloc(sizeof(vq_run), 1);
    audio_q->encoder = encoder;
    audio_q->vq = calloc(sizeof(video_queue *), 1);
    audio_q->vq[0] = NULL;
    pthread_t a_thread;
    ret = pthread_create(&a_thread, NULL, video_queue_run, (void *)audio_q);

    *video_q->vq = video_queue_append(*video_q->vq, new_video);

    char *file_path;
    while (1)
    {
        video_queue **curr_q = NULL;
        /*
        if (!fgets(input, sizeof(input), stdin))
            continue;
        */

        if ((file_path = strchr(input, ':')) == NULL)
            continue;

        file_path++; // path to file
        char *video_decoder = CODEC_SKIP;
        char *audio_decoder = CODEC_SKIP;

        if (strncmp(input, "audio", 5) == 0)
        {
            audio_decoder = NULL;
            curr_q = audio_q->vq;
        }

        if (strncmp(input, "video", 5) == 0)
        {
            video_decoder = NULL;
            curr_q = video_q->vq;
        }

        file *new_file = file_open(file_path);
        if (!new_file)
        {
            printf("Can't open video input %s\n", new_file->filename);
            exit(1);
        }

        ret = file_open_codecs(new_file, video_decoder, audio_decoder);
        if (ret == 1)
        {
            printf("Can't open codec for %s\n", new_file->filename);
            exit(1);
        }

        filters_path_from_files(new_file, encoder);

        int audio_index = file_find_media_type(AVMEDIA_TYPE_AUDIO, new_file);
        int audio_index_encoder = file_find_media_type(AVMEDIA_TYPE_AUDIO, encoder);

        if (audio_index != -1 && audio_index_encoder != -1)
        {
            AVCodecContext *dst_audio_codec = encoder->codec[audio_index_encoder];
            new_file->paths[audio_index] = fp_append(new_file->paths[audio_index],
                                                     filter_audio_hash_create(0x5163513,
                                                                              dst_audio_codec->frame_size,
                                                                              dst_audio_codec->channels,
                                                                              dst_audio_codec->sample_fmt));
            filter_path_init(new_file->paths[audio_index]);
        }

        video_queue *new_file_queue = video_queue_create();
        new_file_queue->tag = FILE_DECODER;
        new_file_queue->video.f = new_file;
        new_file_queue->status = VIDEO_QUEUE_STATUS_WAITING;

        *curr_q = video_queue_append(*curr_q, new_file_queue);
        break;
    }
    pthread_join(v_thread, NULL);
    pthread_join(a_thread, NULL);

    av_write_trailer(encoder->container);
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

    /* ====================== AUDIO PATH ======================*/
    // int audio_filter_desc_size = 1024;
    // char *audio_filter_desc = calloc(sizeof(char), audio_filter_desc_size);
    // int audio_filter_desc_len = 0;

    int src_audio_index = file_find_media_type(AVMEDIA_TYPE_AUDIO, src);
    int dst_audio_index = file_find_media_type(AVMEDIA_TYPE_AUDIO, dst);
    if (src_audio_index == -1 || dst_audio_index == -1)
    {
        printf("NO AUDIO STREAM in %i or %i\n", src_audio_index, dst_audio_index);
    }

    AVCodecContext *src_audio_codec = src->codec[src_audio_index];
    AVCodecContext *dst_audio_codec = dst->codec[dst_audio_index];
    /*
    printf("sample_rate=%i != sample_rate=%i\n",
           src_audio_codec->sample_rate, dst_audio_codec->sample_rate);

    if (src_audio_codec->sample_rate != dst_audio_codec->sample_rate)
    {
        audio_filter_desc_len += snprintf(audio_filter_desc + audio_filter_desc_len,
                                          audio_filter_desc_size - audio_filter_desc_len,
                                          "aresample=%i",
                                          dst_audio_codec->sample_rate);
    }

    printf("src_audio_codec->sample_fmt=%i != dst_audio_codec->sample_fmt=%i\n",
           src_audio_codec->sample_fmt, dst_audio_codec->sample_fmt);

    if (src_audio_codec->sample_fmt != dst_audio_codec->sample_fmt)
    {
        audio_filter_desc_len += snprintf(audio_filter_desc + audio_filter_desc_len,
                                          audio_filter_desc_size - audio_filter_desc_len,
                                          ",sample_fmt=%s",
                                          av_get_sample_fmt_name(dst_audio_codec->sample_fmt));
    }

    printf("src_audio_codec->channel_layout=%li != dst_audio_codec->channel_layout=%li\n",
           src_audio_codec->channel_layout, dst_audio_codec->channel_layout);

    if (src_audio_codec->channel_layout != dst_audio_codec->channel_layout)
    {
        audio_filter_desc_len += snprintf(audio_filter_desc + audio_filter_desc_len,
                                          audio_filter_desc_size - audio_filter_desc_len,
                                          ",channel_layout=%s",
                                          av_get_channel_name(dst_audio_codec->channel_layout));
    }

    if (audio_filter_desc_len != 0)
    {
        enum AVSampleFormat out_sample_fmt[] = {dst_audio_codec->sample_fmt, AV_SAMPLE_FMT_NONE};
        int out_sample_rate[] = {dst_audio_codec->sample_rate, -1};

        src->paths[src_audio_index] = fp_append(src->paths[src_audio_index],
                                                filter_ffmpeg_audio_create(audio_filter_desc, src->streams[src_audio_index]->time_base,
                                                                           src_audio_codec->sample_rate, out_sample_rate,
                                                                           src_audio_codec->sample_fmt,
                                                                           out_sample_fmt,
                                                                           src_audio_codec->channel_layout,
                                                                           src_audio_codec->channels));
    }*/
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
    // free(audio_filter_desc);

    filter_path_init(src->paths[src_audio_index]);
    return 0;
}