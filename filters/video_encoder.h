#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <pthread.h>
#include "filters/filter.h"

typedef struct filter_video_encode_params
{
    AVCodecContext **cod_ctx;
    AVFormatContext *container;
    const char *encoder;
    int width;
    int height;
    int pix_fmt;
    int frames;
    int packets;
    double frame_duration;
    AVRational time_base;
    AVRational framerate;
    AVRational sample_aspect_ratio;
    AVRational frame_rate;
    int index;
    int bit_rate;
    int nb_frames;
    int buffer_size;
    AVPacket *packet;
    AVStream **stream;
    AVDictionary *codec_options;
    int delay;
    time_t start_time;
    pthread_mutex_t *mutex;
} filter_video_encode_params;

void filter_encode_video_init(filters_path *filter_step);

void filter_encode_video_uninit(filters_path *filter_props);

AVFrame *filter_encode_video(filters_path *filter_props, AVFrame *frame);

filters_path *filter_encode_video_create(AVCodecContext **cod_ctx,
                                         AVFormatContext *container,
                                         AVStream **stream,
                                         const char *encoder,
                                         int width,
                                         int height,
                                         enum AVPixelFormat pix_fmt,
                                         AVRational sample_aspect_ratio,
                                         AVRational time_base,
                                         AVRational framerate,
                                         int bit_rate,
                                         AVDictionary *codec_options,
                                         pthread_mutex_t *mutex);