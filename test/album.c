// From array of paths to videos, transcode them to RTMP server
#include "file.h"
#include "filter.h"
#include "debug_tools.h"
#include "filters/video_display.h"
#include "filters/video_encoder.h"
#include "filters/audio_encoder.h"
#include "filters/pts.h"
#include "filters/delay2.h"
#include <libavutil/time.h>
#include "video_queue.h"
#include <pthread.h>
// folder library
#include <dirent.h>
#include <unistd.h>

// returns paths to all files in folder;if relative is 1 returns complete path from folder, relative 0 returns filenames, returns filecount in success -1 on error
int files_dir_get(char *folder, int relative, char ***out)
{
    // create array of files paths
    DIR *dir;
    struct dirent *ent;

    char **files = calloc(sizeof(char *), 10000); // max 10000 files per folder

    int file_count = 0;

    dir = opendir(folder);
    if (!dir)
    {
        /* could not open directory */
        printf("can't open directory\n");
        return -1;
    }

    /* print all the files and directories within directory */
    while ((ent = readdir(dir)) != NULL)
    {
        if (ent->d_type != DT_REG)
            continue;

        char *path = malloc(sizeof(char) * 4096);
        // print path
        printf("%s\n", ent->d_name);

        if (relative == 1)
        {
            strcpy(path, folder);
            strcat(path, "/");
        }

        strcat(path, ent->d_name);

        files[file_count] = path;

        file_count++;
    }
    closedir(dir);
    out[0] = files;
    return file_count;
}

typedef struct video_queue_appender_args
{
    video_queue *vq;
    char *input_folder;
} video_queue_appender_args;

// append to video_queue file "input.mp4"
void *video_queue_appender(void *_args)
{
    video_queue_appender_args *args = (video_queue_appender_args *)_args;
    video_queue *root = args->vq;
    video_queue *transcoding = root;
    char *folder = args->input_folder;
    int res;
    char **files;
    int file_count = files_dir_get(folder, 1, &files);

    for (int file_index = 0;;)
    {

        video_queue **curr = &transcoding;

        if (file_count == -1)
            printf("can't open %s\n", folder);
        for (int i = 0; i < 3; i++, curr = &(*curr)->next)
        {
            if (*curr != NULL && (*curr)->status != VIDEO_QUEUE_STATUS_UNDEFINED)
                continue;

            if (*curr == NULL)
                *curr = video_queue_create();

            char *curr_file = files[file_index % file_count];
            file_index++;

            file *f = file_open(curr_file);
            if (res != 0)
            {
                printf("ERROR OPENING FILE %s\n", curr_file);
                exit(1);
            }

            res = file_open_codecs(f, NULL, NULL);
            if (res != 0)
            {
                printf("ERROR OPENING CODECS\n");
                exit(1);
            }

            (*curr)->file = f;
            (*curr)->status = VIDEO_QUEUE_STATUS_WAITING;
        }

        if (transcoding->status == VIDEO_QUEUE_STATUS_DONE)
            transcoding = transcoding->next;

        // Sleep for a short time if there are
        // no video files to transcoder
    }
}

typedef struct video_queue_transcoder_args
{
    video_queue *vq;
    file *encoder;
} video_queue_transcoder_args;

void *video_queue_transcoder(void *args)
{
    video_queue_transcoder_args *a = (video_queue_transcoder_args *)args;
    video_queue *vq = a->vq;
    file *encoder = a->encoder;
    file_match_streams(encoder);

    while (1)
    {

        // video_queue_print(a->vq);
        if (vq == NULL || vq->status == VIDEO_QUEUE_STATUS_UNDEFINED)
        {

            // Sleep for a short time if there are no video files to transcoder
            continue;
        }
        // video_queue_print(a->vq);
        if (vq->status != VIDEO_QUEUE_STATUS_WAITING)
        {
            // Skip to the next element in the linked list if this one is not waiting
            vq = vq->next;
            continue;
        }

        // Set the status to transcoding
        vq->status = VIDEO_QUEUE_STATUS_TRANSCODING;

        // Allocate the frame and packet objects
        AVFrame *frame = av_frame_alloc();
        AVPacket *packet = av_packet_alloc();

        // Transcode the video file
        int res;
        while (1)
        {
            res = file_decode_frame(vq->file, frame, packet);
            if (res == 1)
            {
                printf("Error decoding a frame\n");
                break;
            }
            else if (res == 0)
            {

                int frame_codec_type = vq->file->streams[packet->stream_index]->codecpar->codec_type;
                if (frame_codec_type != AVMEDIA_TYPE_AUDIO)
                    continue;

                int encoder_codec_index = file_find_media_type(frame_codec_type, encoder);
                if (encoder_codec_index == -1)
                {
                    printf("ERROR: Could not find encoder codec\n");
                    exit(1);
                }
                frame = filter_path_apply(encoder->paths[encoder_codec_index], frame);
            }
            else if (res == -1)
            {
                printf("File ended\n");
                exit(0);
                break;
            }
        }

        // Free the frame and packet objects
        av_frame_free(&frame);
        av_packet_free(&packet);

        // Set the status to done
        vq->status = VIDEO_QUEUE_STATUS_DONE;
        // Move to the next element in the linked list
        while (vq->next == NULL || vq->next->status == VIDEO_QUEUE_STATUS_UNDEFINED)
        {
            printf("Waiting.. %p\n", vq->next);
        }
        vq = vq->next;
    }
}

int main()
{
    file *encoder;

    pthread_mutex_init(&mutex, NULL);

    // GLOBAL SETTINGS
    // char *rtmp = "rtmp://rio.contribute.live-video.net/app/live_402755286_s27jxAKlcKRwSzuyL6OIQeu4yQaQ0G";
    char *rtmp = "rtmp://rio.contribute.live-video.net/app/live_402755286_s27jxAKlcKRwSzuyL6OIQeu4yQaQ0G";

    char *filetype = "flv";

    // VIDEO SETTINGS
    int width = 1280;
    int height = 720;
    AVRational fps = (AVRational){25, 1};
    int bitrate = 1000000;
    int pixelFormat = AV_PIX_FMT_YUV420P;
    AVRational sampleaspectratio = (AVRational){1, 1};
    AVRational timebase = av_inv_q(fps);
    char *videoencoder = "libx264";

    AVDictionary *video_codec_options = NULL;
    av_dict_set(&video_codec_options, "profile", "main", 0);
    av_dict_set(&video_codec_options, "preset", "fast", 0);
    av_dict_set(&video_codec_options, "tune", "zerolatency", 0);

    // AUDIO SETTINGS
    int samplerate = 44100;
    int channels = 2;
    int audiobitrate = 128000;
    char *audioencoder = "libmp3lame";

    AVDictionary *audio_codec_options = NULL;
    // av_dict_set(&audio_codec_options, "profile", "aac_low", 0);

    encoder = file_create(2, rtmp, filetype);

    filters_path *paths[2];
    paths[0] = filter_encode_video_create(&encoder->codec[0], encoder->container, &encoder->streams[0], videoencoder,
                                          width, height, pixelFormat, sampleaspectratio, timebase, fps, bitrate, video_codec_options);
    paths[1] = filter_encode_audio_create(&encoder->codec[1], encoder->container, &encoder->streams[1], audioencoder,
                                          channels, samplerate, audiobitrate, audio_codec_options);

    filter_path_init(paths[0]);
    filter_path_init(paths[1]);
    // INITIALIZE OUTPUT
    if (!(encoder->container->oformat->flags & AVFMT_NOFILE))
    {
        int avopen_ret = avio_open2(&encoder->container->pb, rtmp,
                                    AVIO_FLAG_WRITE, NULL, NULL);
        if (avopen_ret < 0)
        {
            fprintf(stderr, "failed to open stream output context, stream will not work\n");
            return 1;
        }
    }

    AVDictionary *muxer_opts = NULL;
    if (avformat_write_header(encoder->container, &muxer_opts) < 0)
    {
        printf("an error occurred when opening output file");
        return 1;
    }

    int64_t start_time = av_gettime();
    encoder->paths[0] = filter_pts_create(encoder->streams[0], encoder->codec[0]->framerate);
    encoder->paths[1] = filter_pts_create(encoder->streams[1], (AVRational){1, 44100}); // since it's audio the fps doesn't matter (it's already on the stream)
    encoder->paths[0]->next = filter_delay_create(start_time, encoder->streams[0]);
    encoder->paths[1]->next = filter_delay_create(start_time, encoder->streams[1]);
    encoder->paths[0]->next->next = paths[0];
    encoder->paths[1]->next->next = paths[1];

    filter_path_init(encoder->paths[0]);
    filter_path_init(encoder->paths[1]);

    video_queue *vq = video_queue_create();

    video_queue_appender_args *args_appender = malloc(sizeof(video_queue_appender_args));
    args_appender->vq = vq;
    args_appender->input_folder = "./audio";

    pthread_t th_appender, th_transcoder;
    if (pthread_create(&th_appender, NULL, video_queue_appender, args_appender) != 0)
    {
        printf("ERROR CREATING APPENDER NEW THREAD\n");
        exit(1);
    }

    video_queue_transcoder_args *args_transcoder = malloc(sizeof(video_queue_transcoder_args));
    args_transcoder->vq = vq;
    args_transcoder->encoder = encoder;

    if (pthread_create(&th_transcoder, NULL, video_queue_transcoder, args_transcoder) != 0)
    {
        printf("ERROR CREATING TRANSCODER NEW THREAD\n");
        exit(1);
    }

    char *video_input = "./video_inputs/roach.mp4";
    int res;

    file *video_file = file_open(video_input);
    if (res != 0)
    {
        printf("Failed opening input\n");
        return -1;
    }

    res = file_open_codecs(video_file, NULL, NULL);
    if (res != 0)
    {
        printf("Failed opening codecs\n");
        return -1;
    }

    frame_list *fl = file_to_frame_list(video_file);
    frame_list_circular(fl);

    int video_encoder_index = file_find_media_type(AVMEDIA_TYPE_VIDEO, encoder);
    while (fl != NULL)
    {
        if (fl->media_type == AVMEDIA_TYPE_VIDEO)
        {
            filter_path_apply(encoder->paths[video_encoder_index], fl->frame);
        }
        fl = fl->next;
    }

    pthread_mutex_destroy(&mutex);

    pthread_join(th_appender, NULL);
    pthread_join(th_transcoder, NULL);

    av_write_trailer(encoder->container);

    free(args_transcoder);
    file_free(encoder);
    video_queue_free(vq);
}
