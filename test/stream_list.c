// From array of paths to videos, transcode them to RTMP server
#include "file.h"
#include "filter.h"
#include "filters/delay.h"
#include "filters/video_display.h"
#include "filters/video_encoder.h"
#include "filters/audio_encoder.h"
#include "filters/pts.h"
#include <libavutil/time.h>
#include "video_queue.h"
#include <pthread.h>

// append to video_queue file "input.mp4"
void *video_queue_appender(void *args)
{
    video_queue *root = (video_queue *)args;
    video_queue *transcoding = root;

    while (1)
    {

        video_queue **curr = &transcoding;

        char *curr_file = "./video_inputs/output.mp4";
        int res;
        for (int i = 0; i < 3; i++, curr = &(*curr)->next)
        {
            if (*curr != NULL && (*curr)->status != VIDEO_QUEUE_STATUS_UNDEFINED)
                continue;

            if (*curr == NULL)
                *curr = video_queue_create();

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
        av_usleep(1000000);
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

    while (1)
    {
        // video_queue_print(a->vq);
        if (vq == NULL || vq->status == VIDEO_QUEUE_STATUS_UNDEFINED)
        {

            // Sleep for a short time if there are no video files to transcoder
            continue;
        }
        video_queue_print(a->vq);
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
                frame = filter_path_apply(encoder->paths[packet->stream_index], frame);
            }
            else if (res == -1)
            {
                printf("File ended\n");
                break;
            }
        }

        // Free the frame and packet objects
        av_frame_free(&frame);
        av_packet_free(&packet);

        // Set the status to done
        vq->status = VIDEO_QUEUE_STATUS_DONE;

        // Move to the next element in the linked list
        vq = vq->next;
    }
}

int main()
{
    file *encoder;

    // GLOBAL SETTINGS
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
    int samplerate = 48000;
    int channels = 2;
    int audiobitrate = 128000;
    char *audioencoder = "libmp3lame";

    AVDictionary *audio_codec_options = NULL;
    // av_dict_set(&audio_codec_options, "profile", "aac_low", 0);

    encoder = file_create(2, rtmp, filetype);

    filters_path *paths[2];
    paths[0] = filter_encode_video_create(&encoder->codec[0], encoder->container, &encoder->streams[0], videoencoder, width, height, pixelFormat, sampleaspectratio, timebase, fps, bitrate, video_codec_options);
    paths[1] = filter_encode_audio_create(&encoder->codec[1], encoder->container, &encoder->streams[1], audioencoder, channels, samplerate, audiobitrate, audio_codec_options);

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

    encoder->paths[0] = filter_pts_create(encoder->streams[0], encoder->codec[0]->framerate);
    encoder->paths[1] = filter_pts_create(encoder->streams[1], (AVRational){1, 48000}); // since it's audio the fps doesn't matter (it's already on the stream)
    encoder->paths[0]->next = paths[0];
    encoder->paths[1]->next = paths[1];

    filter_path_init(encoder->paths[0]);
    filter_path_init(encoder->paths[1]);

    av_dump_format(encoder->container, 0, NULL, 1); // STREAM TEST

    video_queue *vq = video_queue_create();

    video_queue_transcoder_args *args = malloc(sizeof(video_queue_transcoder_args));
    args->vq = vq;
    args->encoder = encoder;

    pthread_t th_appender, th_transcoder;
    if (pthread_create(&th_appender, NULL, video_queue_appender, vq) != 0)
    {
        printf("ERROR CREATING APPENDER NEW THREAD\n");
        exit(1);
    }

    if (pthread_create(&th_transcoder, NULL, video_queue_transcoder, args) != 0)
    {
        printf("ERROR CREATING TRANSCODER NEW THREAD\n");
        exit(1);
    }

    pthread_join(th_appender, NULL);
    pthread_join(th_transcoder, NULL);

    free(args);
    file_free(encoder);
    video_queue_free(vq);
}
