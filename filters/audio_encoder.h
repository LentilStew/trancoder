#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include "filters/filter.h"

#include "file.h"

#include <time.h>

typedef struct filter_audio_encode_params
{
    AVCodecContext **cod_ctx;
    AVFormatContext *container;
    const char *encoder;
    int channels;
    int sample_rate;
    int bit_rate;
    AVPacket *packet;
    int frames;
    int packets;
    double frame_duration;
    int sample_fmt;
    AVStream **stream;
    AVDictionary *codec_options;
    int delay;
    time_t start_time;
    pthread_mutex_t *mutex;
} filter_audio_encode_params;

void filter_encode_audio_init(filters_path *filter_step);

void filter_encode_audio_uninit(filters_path *filter_props);

AVFrame *filter_encode_audio(filters_path *filter_props, AVFrame *frame);

filters_path *filter_encode_audio_create(AVCodecContext **cod_ctx,
                                         AVFormatContext *container,
                                         AVStream **stream,
                                         const char *encoder,
                                         int sample_fmt,
                                         int channels,
                                         int sample_rate,
                                         int bit_rate,
                                         AVDictionary *codec_options,
                                         pthread_mutex_t* mutex);